#!/usr/bin/env python3
import base64
import hashlib
import json
import math
import os
import re
import secrets
import shutil
import subprocess
import tempfile
import threading
import uuid

from contextlib import contextmanager
from datetime import datetime
from pathlib import Path
from PIL import Image
from werkzeug.utils import secure_filename

from openpilot.common.conversions import Conversions as CV
from openpilot.system.loggerd.config import get_available_bytes, get_used_bytes
from openpilot.system.loggerd.deleter import PRESERVE_ATTR_NAME, PRESERVE_ATTR_VALUE
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.tools.lib.route import SegmentName

from openpilot.frogpilot.assets.theme_manager import HOLIDAY_THEME_PATH
from openpilot.frogpilot.common.frogpilot_variables import THEME_SAVE_PATH, VIDEO_CACHE_PATH, params
from openpilot.frogpilot.system.the_pond.helpers import format_ordinal_date

DISTANCE_ICON_NAMES = ("traffic", "aggressive", "standard", "relaxed")
IMAGE_EXTS = (".png", ".gif", ".jpg", ".jpeg")
LOG_CANDIDATES = [
  "qlog",
  "qlog.bz2",
  "rlog",
  "rlog.bz2",
  "raw_log.bz2",
]
MAX_FILE_SIZE = 5 * 1024 * 1024
ROUTE_PROTECTED_SUFFIXES = (".hevc", ".ts", ".png", ".gif", ".lock")
SEGMENT_RE = re.compile(r"^[0-9a-fA-F]{8}--[0-9a-fA-F]{10}--\d+$")
SOUND_NAMES = ("engage", "disengage", "prompt", "startup")
XOR_KEY = "s8#pL3*Xj!aZ@dWq"

_FFMPEG_SEMAPHORE = threading.Semaphore(2)
_FFPROBE_SEMAPHORE = threading.Semaphore(2)
_VIDEO_CACHE_LOCK = threading.Lock()
_VIDEO_CACHE_KEY_LOCKS = {}
_VIDEO_CACHE_RESERVED_BYTES = 0
_THEME_MUTATION_LOCK = threading.Lock()
_FFMPEG_TIMEOUT = 600
_FFMPEG_WAIT_TIMEOUT = 5
_FFPROBE_TIMEOUT = 30
_NICE = (["nice", "-n", "19", "ionice", "-c", "3"]
         if shutil.which("nice") and shutil.which("ionice") else [])
_DURATION_CACHE: dict[str, float] = {}
_VIDEO_CACHE_MAX_BYTES = 2 * 1024 * 1024 * 1024
_VIDEO_CACHE_MIN_FREE = 500 * 1024 * 1024

def is_route_entry_protected(name):
  return name.endswith(ROUTE_PROTECTED_SUFFIXES) or name in LOG_CANDIDATES

def _evict_video_cache(required_bytes=0):
  try:
    free = shutil.disk_usage(VIDEO_CACHE_PATH).free
  except OSError as error:
    raise OSError("Could not inspect video-cache storage") from error

  cache_files = sorted(VIDEO_CACHE_PATH.glob("*.mp4"), key=lambda path: path.stat().st_atime)
  cache_bytes = sum(path.stat().st_size for path in cache_files)
  cached_requirement = min(required_bytes, _VIDEO_CACHE_MAX_BYTES)
  for cache_file in cache_files:
    if cache_bytes + cached_requirement <= _VIDEO_CACHE_MAX_BYTES and free >= _VIDEO_CACHE_MIN_FREE + required_bytes:
      break
    try:
      size = cache_file.stat().st_size
      cache_file.unlink()
      cache_bytes -= size
      free += size
    except OSError:
      pass

  if cache_bytes + cached_requirement > _VIDEO_CACHE_MAX_BYTES or free < _VIDEO_CACHE_MIN_FREE + required_bytes:
    raise OSError("Not enough storage is available for the requested video")

@contextmanager
def _video_cache_key_lock(cache_path):
  with _VIDEO_CACHE_LOCK:
    lock, users = _VIDEO_CACHE_KEY_LOCKS.get(cache_path, (threading.Lock(), 0))
    _VIDEO_CACHE_KEY_LOCKS[cache_path] = (lock, users + 1)

  try:
    with lock:
      yield
  finally:
    with _VIDEO_CACHE_LOCK:
      users = _VIDEO_CACHE_KEY_LOCKS[cache_path][1]
      if users == 1:
        del _VIDEO_CACHE_KEY_LOCKS[cache_path]
      else:
        _VIDEO_CACHE_KEY_LOCKS[cache_path] = (lock, users - 1)

@contextmanager
def _video_cache_reservation(required_bytes):
  global _VIDEO_CACHE_RESERVED_BYTES
  with _VIDEO_CACHE_LOCK:
    _evict_video_cache(_VIDEO_CACHE_RESERVED_BYTES + required_bytes)
    _VIDEO_CACHE_RESERVED_BYTES += required_bytes
  try:
    yield
  finally:
    with _VIDEO_CACHE_LOCK:
      _VIDEO_CACHE_RESERVED_BYTES -= required_bytes

@contextmanager
def theme_mutation_lock():
  with _THEME_MUTATION_LOCK:
    yield

def _oversize_error(file):
  stream = file.stream
  position = stream.tell()
  stream.seek(0, os.SEEK_END)
  size = stream.tell()
  stream.seek(position)
  return f"File {file.filename} exceeds the 5MB limit." if size > MAX_FILE_SIZE else None

def _image_error(file):
  expected_format = {
    ".gif": "GIF",
    ".jpeg": "JPEG",
    ".jpg": "JPEG",
    ".png": "PNG",
  }.get(Path(file.filename).suffix.lower())
  if expected_format is None:
    return f"File {file.filename} is not a supported image."

  stream = file.stream
  position = stream.tell()
  try:
    stream.seek(0)
    with Image.open(stream) as image:
      if image.format != expected_format:
        return f"File {file.filename} does not match its file extension."
      image.verify()
  except (OSError, ValueError):
    return f"File {file.filename} is not a valid image."
  finally:
    stream.seek(position)
  return None

def _resize_image(path, ext, dims):
  if ext == ".gif":
    width, height = dims
    palette_path = path.with_suffix(".palette.png")
    resized_path = path.with_suffix(".resized.gif")
    subprocess.run(["ffmpeg", "-i", str(path), "-vf", "palettegen", "-y", str(palette_path)], check=True, timeout=_FFMPEG_TIMEOUT)
    subprocess.run(["ffmpeg", "-i", str(path), "-i", str(palette_path), "-lavfi",
                    f"fps=20,scale={width}:{height}:flags=lanczos[x];[x][1:v]paletteuse", "-y", str(resized_path)],
                   check=True, timeout=_FFMPEG_TIMEOUT)
    palette_path.unlink()
    resized_path.rename(path)
    return path
  img = Image.open(path).resize(dims, Image.Resampling.LANCZOS)
  if ext != ".png":
    path.unlink()
    path = path.with_suffix(".png")
  img.save(path, "PNG")
  return path

def _ffmpeg_to_mp4(input_args, cache_path):
  base = _NICE + ["ffmpeg", "-hide_banner", "-loglevel", "error", *input_args]
  tail = ["-movflags", "faststart", "-f", "mp4", "-y", str(cache_path)]
  with _ffmpeg_slot():
    try:
      subprocess.run([*base, "-c", "copy", *tail], check=True, timeout=_FFMPEG_TIMEOUT)
    except subprocess.CalledProcessError:
      subprocess.run([*base, "-c:v", "libx264", *tail], check=True, timeout=_FFMPEG_TIMEOUT)

@contextmanager
def _ffmpeg_slot():
  if not _FFMPEG_SEMAPHORE.acquire(timeout=_FFMPEG_WAIT_TIMEOUT):
    raise subprocess.TimeoutExpired("ffmpeg queue", _FFMPEG_WAIT_TIMEOUT)
  try:
    yield
  finally:
    _FFMPEG_SEMAPHORE.release()

def first_image(directory, stem):
  for ext in IMAGE_EXTS:
    if (directory / f"{stem}{ext}").exists():
      return f"{stem}{ext}"
  return None

def check_theme_components(theme_path):
  components = {
    "hasColors": False,
    "hasIcons": False,
    "hasSounds": False,
    "hasTurnSignals": False,
    "hasDistanceIcons": False,
    "hasSteeringWheel": False
  }

  colors_path = theme_path / "colors" / "colors.json"
  if colors_path.exists():
    components["hasColors"] = True

  icons_path = theme_path / "icons"
  if icons_path.exists() and any(icons_path.iterdir()):
    components["hasIcons"] = True

  sounds_path = theme_path / "sounds"
  if sounds_path.exists() and any(sounds_path.iterdir()):
    components["hasSounds"] = True

  signals_path = theme_path / "signals"
  if signals_path.exists() and any(signals_path.iterdir()):
    components["hasTurnSignals"] = True

  distance_icons_path = theme_path / "distance_icons"
  if distance_icons_path.exists() and any(distance_icons_path.iterdir()):
    components["hasDistanceIcons"] = True

  is_holiday_theme = str(HOLIDAY_THEME_PATH) in str(theme_path)

  if is_holiday_theme:
    wheel_path = theme_path / "steering_wheel"
    if wheel_path.exists() and any(f.name.startswith("wheel.") for f in wheel_path.iterdir()):
      components["hasSteeringWheel"] = True
  else:
    wheel_path = THEME_SAVE_PATH / "steering_wheels"
    if wheel_path.exists():
      theme_name = theme_path.name.replace("-user_created", "")
      if any(wheel_path.glob(f"{theme_name}-user_created.*")):
        components["hasSteeringWheel"] = True

  return components

def convert_audio(input_file):
  from pydub import AudioSegment
  sound = AudioSegment.from_file(input_file)
  sound = sound.set_frame_rate(48000)
  sound = sound.set_channels(1)

  output_filename = os.path.splitext(input_file)[0] + ".wav"
  sound.export(output_filename, format="wav", parameters=["-acodec", "pcm_s16le"])

  if input_file != output_filename:
    os.remove(input_file)

def _create_theme_in_place(form_data, files, sane_theme_name, save_checklist, base_path, wheels_dir):
  theme_path = (base_path / f"{sane_theme_name}-user_created") if base_path else None
  if theme_path:
    theme_path.mkdir(parents=True, exist_ok=True)

  if save_checklist.get("colors"):
    (theme_path / "colors").mkdir(exist_ok=True)
    colors_str = form_data.get("colors")
    if colors_str:
      color_data = json.loads(colors_str)
      colors_file = theme_path / "colors" / "colors.json"
      with open(colors_file, "w") as f:
        json.dump(color_data, f, indent=2)

  if save_checklist.get("turn_signals"):
    signals_path = theme_path / "signals"
    signals_path.mkdir(exist_ok=True)

    if turn_signal_length := form_data.get("turnSignalLength"):
      style = form_data.get("turnSignalStyle", "Traditional").lower()
      for marker in signals_path.iterdir():
        if marker.is_file() and marker.suffix.lower() not in IMAGE_EXTS:
          marker.unlink()
      (signals_path / f"{style}_{turn_signal_length}").touch()

    turn_signal_type = form_data.get("turnSignalType", "Single Image").lower()

    if turn_signal_type == "single image":
      for f in signals_path.glob("turn_signal*"):
        f.unlink()

      file = files.get("turnSignal")
      if file and file.filename:
        ext = Path(file.filename).suffix.lower()
        file.save(signals_path / f"turn_signal{ext}")

      file = files.get("turnSignalBlindspot")
      if file and file.filename:
        ext = Path(file.filename).suffix.lower()
        file.save(signals_path / f"turn_signal_blindspot{ext}")

    elif turn_signal_type == "sequential":
      for f in signals_path.glob("turn_signal_*"):
        f.unlink()

      file = files.get("turnSignalBlindspot")
      if file and file.filename:
        ext = Path(file.filename).suffix.lower()
        file.save(signals_path / f"turn_signal_blindspot{ext}")

      for f in signals_path.glob("turn_signal.*"):
        f.unlink()

      sequential_keys = sorted(
        [k for k in files if k.startswith("turn_signal_")],
        key=lambda name: int(name.split("_")[-1])
      )

      frame_width = max((len(k.split("_")[-1]) for k in sequential_keys), default=1)

      for key in sequential_keys:
        file = files.get(key)
        if file and file.filename:
          idx = key.split("_")[-1].zfill(frame_width)
          ext = Path(file.filename).suffix.lower()
          file.save(signals_path / f"turn_signal_{idx}{ext}")

  if save_checklist.get("icons"):
    (theme_path / "icons").mkdir(exist_ok=True)

    icon_map = {
      "settingsButton": (theme_path / "icons", "button_settings", (169, 104)),
      "homeButton": (theme_path / "icons", "button_home", (250, 250)),
    }

    for field, (dest_path, base_name, resize_dims) in icon_map.items():
      file = files.get(field)
      if file and file.filename:
        for f in dest_path.glob(f"{base_name}.*"):
          f.unlink()

        ext = Path(file.filename).suffix.lower()
        save_path = dest_path / f"{base_name}{ext}"
        file.save(save_path)

        if resize_dims:
          _resize_image(save_path, ext, resize_dims)

  if save_checklist.get("steering_wheel") and wheels_dir:
    wheels_dir.mkdir(parents=True, exist_ok=True)
    file = files.get("steeringWheel")
    if file and file.filename:
      for f in wheels_dir.glob(f"{sane_theme_name}-user_created.*"):
        f.unlink()
      ext = Path(file.filename).suffix.lower()
      dest = wheels_dir / f"{sane_theme_name}-user_created{ext}"
      file.save(dest)
      _resize_image(dest, ext, (250, 250))

  if save_checklist.get("distance_icons"):
    dist_path = theme_path / "distance_icons"
    dist_path.mkdir(exist_ok=True)
    for name in DISTANCE_ICON_NAMES:
      file = files.get(f"distanceIcons_{name}")
      if file and file.filename:
        for f in dist_path.glob(f"{name}.*"):
          f.unlink()

        ext = Path(file.filename).suffix.lower()
        save_path = dist_path / f"{name}{ext}"
        file.save(save_path)
        _resize_image(save_path, ext, (250, 250))

  if save_checklist.get("sounds"):
    sounds_path = theme_path / "sounds"
    sounds_path.mkdir(exist_ok=True)
    for name in SOUND_NAMES:
      file = files.get(name)
      if file and file.filename:
        save_path = sounds_path / f"{name}{Path(file.filename).suffix}"
        file.save(save_path)
        convert_audio(str(save_path))

  return theme_path

def _preflight_theme(form_data, files):
  theme_name = form_data.get("themeName")
  if not theme_name:
    return None, None, "Theme name is required."

  sane_theme_name = secure_filename(theme_name.replace(" ", "_"))
  if not sane_theme_name:
    return None, None, "Theme name is invalid."

  try:
    save_checklist = json.loads(form_data.get("saveChecklist", "{}"))
  except (json.JSONDecodeError, TypeError):
    return None, None, "Save checklist is invalid."
  if not isinstance(save_checklist, dict):
    return None, None, "Save checklist is invalid."

  if save_checklist.get("colors") and (colors := form_data.get("colors")):
    try:
      json.loads(colors)
    except (json.JSONDecodeError, TypeError):
      return None, None, "Theme colors are invalid."

  consumed_files = set()
  if save_checklist.get("turn_signals"):
    consumed_files.update(("turnSignal", "turnSignalBlindspot"))
    if form_data.get("turnSignalType", "Single Image").lower() == "sequential":
      for key in files:
        if key.startswith("turn_signal_"):
          if not key.rsplit("_", 1)[-1].isdecimal():
            return None, None, "Sequential turn signal frame names are invalid."
          consumed_files.add(key)
  if save_checklist.get("icons"):
    consumed_files.update(("settingsButton", "homeButton"))
  if save_checklist.get("steering_wheel"):
    consumed_files.add("steeringWheel")
  if save_checklist.get("distance_icons"):
    consumed_files.update(f"distanceIcons_{name}" for name in DISTANCE_ICON_NAMES)
  if save_checklist.get("sounds"):
    consumed_files.update(SOUND_NAMES)

  for field in consumed_files:
    file = files.get(field)
    if file and file.filename:
      if error := _oversize_error(file):
        return None, None, error
      if field not in SOUND_NAMES and (error := _image_error(file)):
        return None, None, error

  return sane_theme_name, save_checklist, None

def _remove_theme_staging(path):
  try:
    if path.is_symlink():
      path.unlink()
    elif path.exists():
      shutil.rmtree(path)
    return not os.path.lexists(path)
  except OSError:
    return False

def _ensure_real_theme_directory(path):
  if path.is_symlink() or (os.path.lexists(path) and not path.is_dir()):
    raise OSError(f"Theme path is not a real directory: {path}")
  path.mkdir(parents=True, exist_ok=True)

def _publish_staged_theme(stage_root, staged_theme, live_theme, publish_pack,
                          staged_wheels, live_wheels, wheel_pattern, publish_wheel):
  moved_new_wheels = []
  moved_old_wheels = []
  previous_theme = stage_root / "previous_theme"
  previous_wheels = stage_root / "previous_wheels"
  previous_theme_moved = False
  published_theme = False

  try:
    if publish_pack:
      _ensure_real_theme_directory(live_theme.parent)
      if os.path.lexists(live_theme):
        if live_theme.is_symlink() or not live_theme.is_dir():
          raise OSError("Existing theme pack is not a real directory")
        live_theme.rename(previous_theme)
        previous_theme_moved = True
      staged_theme.rename(live_theme)
      published_theme = True

    if publish_wheel:
      _ensure_real_theme_directory(live_wheels)
      previous_wheels.mkdir(mode=0o700)
      for old_wheel in live_wheels.glob(wheel_pattern):
        backup = previous_wheels / old_wheel.name
        old_wheel.rename(backup)
        moved_old_wheels.append((backup, old_wheel))

      new_wheels = list(staged_wheels.glob(wheel_pattern))
      if not new_wheels:
        raise OSError("Staged steering wheel is missing")
      for staged_wheel in new_wheels:
        live_wheel = live_wheels / staged_wheel.name
        staged_wheel.rename(live_wheel)
        moved_new_wheels.append((live_wheel, staged_wheel))
  except Exception as exception:
    rollback_failed = False
    for live_wheel, staged_wheel in reversed(moved_new_wheels):
      try:
        if os.path.lexists(live_wheel):
          live_wheel.rename(staged_wheel)
      except OSError:
        rollback_failed = True
    for backup, live_wheel in reversed(moved_old_wheels):
      try:
        if os.path.lexists(backup):
          backup.rename(live_wheel)
      except OSError:
        rollback_failed = True
    try:
      if published_theme and os.path.lexists(live_theme):
        live_theme.rename(staged_theme)
      if previous_theme_moved and os.path.lexists(previous_theme):
        previous_theme.rename(live_theme)
    except OSError:
      rollback_failed = True
    if rollback_failed:
      raise RuntimeError("Theme publication failed and rollback was incomplete") from exception
    raise

def create_theme(form_data, files, temporary=False):
  sane_theme_name, save_checklist, error = _preflight_theme(form_data, files)
  if error:
    return None, error

  if temporary:
    base_path = Path(f"/tmp/{sane_theme_name}_{secrets.token_hex(8)}")
    try:
      return _create_theme_in_place(form_data, files, sane_theme_name, save_checklist, base_path, None), None
    except Exception:
      _remove_theme_staging(base_path)
      raise

  needs_theme_pack = any(save_checklist.get(component) for component in ("colors", "icons", "sounds", "turn_signals", "distance_icons"))
  stage_root = THEME_SAVE_PATH / f".theme_{secrets.token_hex(8)}_in_progress"
  live_theme = THEME_SAVE_PATH / "theme_packs" / f"{sane_theme_name}-user_created"
  staged_theme = stage_root / "theme_packs" / live_theme.name
  live_wheels = THEME_SAVE_PATH / "steering_wheels"
  staged_wheels = stage_root / "steering_wheels"
  wheel_pattern = f"{sane_theme_name}-user_created.*"
  wheel_file = files.get("steeringWheel")
  publish_wheel = bool(save_checklist.get("steering_wheel") and wheel_file and wheel_file.filename)

  with theme_mutation_lock():
    _ensure_real_theme_directory(THEME_SAVE_PATH)
    for stale_root in THEME_SAVE_PATH.glob(".theme_*_in_progress"):
      stale_name = next((staged.name for staged in (stale_root / "theme_packs").glob("*")), None)
      if stale_name and (stale_root / "previous_theme").is_dir() and not os.path.lexists(live_theme.parent / stale_name):
        (stale_root / "previous_theme").rename(live_theme.parent / stale_name)
      for stale_wheel in (stale_root / "previous_wheels").glob("*"):
        if not os.path.lexists(live_wheels / stale_wheel.name):
          stale_wheel.rename(live_wheels / stale_wheel.name)
      if not _remove_theme_staging(stale_root):
        print(f"Unable to remove theme staging directory: {stale_root}")
    stage_root.mkdir(mode=0o700)
    try:
      if needs_theme_pack and os.path.lexists(live_theme):
        if live_theme.is_symlink() or not live_theme.is_dir():
          raise OSError("Existing theme pack is not a real directory")
        shutil.copytree(live_theme, staged_theme, symlinks=True)
        if any(path.is_symlink() for path in staged_theme.rglob("*")):
          raise OSError("Existing theme pack contains symbolic links")

      theme_path = _create_theme_in_place(
        form_data,
        files,
        sane_theme_name,
        save_checklist,
        stage_root / "theme_packs" if needs_theme_pack else None,
        staged_wheels,
      )

      _publish_staged_theme(
        stage_root,
        staged_theme,
        live_theme,
        needs_theme_pack,
        staged_wheels,
        live_wheels,
        wheel_pattern,
        publish_wheel,
      )
      return live_theme if needs_theme_pack else theme_path, None
    finally:
      if not _remove_theme_staging(stage_root):
        print(f"Unable to remove theme staging directory: {stage_root}")

def decode_parameters(encoded_string):
  obfuscated_data = base64.b64decode(encoded_string.encode("utf-8")).decode("utf-8")
  decrypted_data = xor_encrypt_decrypt(obfuscated_data, XOR_KEY)
  return json.loads(decrypted_data)

def ffmpeg_concat_segments_to_mp4(input_files, cache_key=None, cache_identity_files=None):
  if not input_files:
    raise ValueError("No input files provided for concatenation")

  identity_files = input_files if cache_identity_files is None else cache_identity_files
  if len(identity_files) != len(input_files):
    raise ValueError("Cache identity count does not match input count")

  key_str = "|".join(str(p) for p in identity_files)
  if cache_key:
    key_str = f"{cache_key}|{key_str}"
  file_hash = hashlib.md5(key_str.encode()).hexdigest()
  cache_path = VIDEO_CACHE_PATH / f"{file_hash}.mp4"

  with _video_cache_key_lock(cache_path):
    with _VIDEO_CACHE_LOCK:
      VIDEO_CACHE_PATH.mkdir(exist_ok=True)
      if cache_path.exists() and all(cache_path.stat().st_mtime > Path(f).stat().st_mtime for f in input_files):
        return cache_path.open("rb")
      cache_path.unlink(missing_ok=True)
    required_bytes = sum(Path(path).stat().st_size for path in input_files)

    with _video_cache_reservation(required_bytes):
      unique_suffix = uuid.uuid4().hex
      list_file = VIDEO_CACHE_PATH / f".{file_hash}.{unique_suffix}.txt"
      partial_path = VIDEO_CACHE_PATH / f".{file_hash}.{unique_suffix}.partial"
      with open(list_file, "w") as f:
        for seg in input_files:
          f.write(f"file '{Path(seg)}'\n")

      try:
        _ffmpeg_to_mp4(["-f", "concat", "-safe", "0", "-i", str(list_file)], partial_path)
        with _VIDEO_CACHE_LOCK:
          output_size = partial_path.stat().st_size
          extra_bytes = max(0, output_size - required_bytes)
          if extra_bytes:
            _evict_video_cache(_VIDEO_CACHE_RESERVED_BYTES + extra_bytes)
          partial_path.replace(cache_path)
          output = cache_path.open("rb")
          if output_size > _VIDEO_CACHE_MAX_BYTES:
            cache_path.unlink()
          return output
      except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exception:
        raise ValueError(f"Cannot process concatenated video segments: {input_files}") from exception
      finally:
        if partial_path.exists():
          partial_path.unlink()
        if list_file.exists():
          list_file.unlink()

def ffmpeg_mp4_wrap_process_builder(filename, cache_key=None):
  input_path = Path(filename)

  if not input_path.exists():
    raise FileNotFoundError(f"Input file does not exist: {input_path}")

  if input_path.stat().st_size == 0:
    raise ValueError(f"Input file is empty: {input_path}")

  file_hash = hashlib.md5(str(cache_key or input_path).encode()).hexdigest()
  cache_path = VIDEO_CACHE_PATH / f"{file_hash}.mp4"

  with _video_cache_key_lock(cache_path):
    with _VIDEO_CACHE_LOCK:
      VIDEO_CACHE_PATH.mkdir(exist_ok=True)
      if cache_path.exists() and cache_path.stat().st_mtime > input_path.stat().st_mtime:
        return cache_path.open("rb")
      cache_path.unlink(missing_ok=True)
    required_bytes = input_path.stat().st_size

    with _video_cache_reservation(required_bytes):
      partial_path = VIDEO_CACHE_PATH / f".{file_hash}.{uuid.uuid4().hex}.partial"
      try:
        _ffmpeg_to_mp4(["-i", str(input_path)], partial_path)
        with _VIDEO_CACHE_LOCK:
          output_size = partial_path.stat().st_size
          extra_bytes = max(0, output_size - required_bytes)
          if extra_bytes:
            _evict_video_cache(_VIDEO_CACHE_RESERVED_BYTES + extra_bytes)
          partial_path.replace(cache_path)
          output = cache_path.open("rb")
          if output_size > _VIDEO_CACHE_MAX_BYTES:
            cache_path.unlink()
          return output
      except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exception:
        raise ValueError(f"Cannot process video file: {input_path}") from exception
      finally:
        if partial_path.exists():
          partial_path.unlink()

def format_git_date(raw_date: str):
  return format_ordinal_date(datetime.strptime(raw_date.split()[1], "%Y-%m-%d"))

def get_all_segment_names(footage_path):
  entries = listdir_by_creation(footage_path)

  segment_names = []
  for entry in entries:
    if not SEGMENT_RE.fullmatch(entry):
      continue

    segment_names.append(segment_to_segment_name(footage_path, entry))
  return segment_names

def get_available_cameras(segment_path):
  segment_path = Path(segment_path)
  return [
    name for name, file in {
      "driver": "dcamera.hevc",
      "forward": "fcamera.hevc",
      "wide": "ecamera.hevc"
    }.items() if (segment_path / file).exists()
  ]

def get_disk_usage():
  free = get_available_bytes()
  used = get_used_bytes()
  total = used + free

  def to_gb(b):
    return f"{b // (2**30)} GB"

  return [{
    "free": to_gb(free),
    "size": to_gb(total),
    "used": to_gb(used),
    "usedPercentage": f"{(used / total * 100) if total else 0:.2f}%"
  }]

def get_drive_stats():
  def load_stats(key, encoding=None):
    try:
      value = params.get(key, encoding=encoding) if encoding else params.get(key)
      stats = json.loads(value or "{}")
      return stats if isinstance(stats, dict) else {}
    except (TypeError, ValueError):
      return {}

  def number(value):
    try:
      return value if isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) else 0
    except OverflowError:
      return 0

  stats = load_stats("ApiCache_DriveStats", "utf-8")
  frogpilot_stats = load_stats("FrogPilotStats")

  is_metric = params.get_bool("IsMetric")
  unit = "kilometers" if is_metric else "miles"

  def process(timeframe):
    data = stats.get(timeframe, {})
    data = data if isinstance(data, dict) else {}
    return {
      "distance": number(data.get("distance")) * (CV.MPH_TO_KPH if is_metric else 1),
      "drives": number(data.get("routes")),
      "hours": number(data.get("minutes")) / 60,
      "unit": unit
    }

  return {
    "all": process("all"),
    "week": process("week"),
    "frogpilot": {
      "distance": number(frogpilot_stats.get("FrogPilotMeters")) * (0.001 if is_metric else CV.METER_TO_MILE),
      "hours": number(frogpilot_stats.get("FrogPilotSeconds")) / 3600,
      "drives": number(frogpilot_stats.get("FrogPilotDrives")),
      "unit": unit
    }
  }

def get_repo_owner(git_normalized_origin):
  parts = git_normalized_origin.split("/")
  return parts[1] if len(parts) >= 2 else "unknown"

def get_route_start_time(log_file_path):
  if not os.path.exists(log_file_path):
    return None

  creation_time = os.path.getctime(log_file_path)
  return datetime.fromtimestamp(creation_time)

def get_routes_names(footage_path):
  segments = get_all_segment_names(footage_path)
  route_times = {segment.route_name.time_str for segment in segments}
  return sorted(route_times, reverse=True)

def get_segments_in_route(route_time_str, footage_path):
  return [
    f"{segment.time_str}--{segment.segment_num}"
    for segment in get_all_segment_names(footage_path)
    if segment.time_str == route_time_str
  ]

def get_video_duration(input_path):
  try:
    key = f"{input_path}:{os.path.getmtime(input_path)}"
  except OSError:
    key = None
  if key and key in _DURATION_CACHE:
    return _DURATION_CACHE[key]
  try:
    with _FFPROBE_SEMAPHORE:
      result = subprocess.run([
        "ffprobe", "-v", "error", "-show_entries", "format=duration",
        "-of", "default=noprint_wrappers=1:nokey=1", str(input_path)
      ], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True, timeout=_FFPROBE_TIMEOUT)
    duration = float(result.stdout)
  except (OSError, ValueError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
    duration = 0
  if key:
    _DURATION_CACHE[key] = duration
  return duration

def has_preserve_attr(path: str):
  return PRESERVE_ATTR_NAME in os.listxattr(path) and os.getxattr(path, PRESERVE_ATTR_NAME) == PRESERVE_ATTR_VALUE

def list_file(path):
  return sorted(os.listdir(path), reverse=True)

def normalize_theme_name(name):
  name = name.replace("-user_created", "")

  parts = re.split(r"[-_]", name)
  normalized_parts = [part.capitalize() for part in parts]

  if "-" in name and len(normalized_parts) > 1:
    return f"{normalized_parts[0]} ({' '.join(normalized_parts[1:])})".replace(" Week", "")
  return " ".join(normalized_parts).replace(" Week", "")

def route_metadata(footage_path, route_name):
  segment_path = os.path.join(footage_path, f"{route_name}--0")
  rlog_path = os.path.join(segment_path, "rlog")

  custom_name = None
  if os.path.isdir(segment_path):
    for item in os.listdir(segment_path):
      if not is_route_entry_protected(item):
        custom_name = item
        break

  route_timestamp_str = custom_name
  if not custom_name:
    route_timestamp_dt = get_route_start_time(rlog_path)
    route_timestamp_str = route_timestamp_dt.isoformat() if route_timestamp_dt else None

  return {
    "name": route_name,
    "gif": f"/thumbnails/{route_name}--0/preview.gif",
    "png": f"/thumbnails/{route_name}--0/preview.png",
    "timestamp": route_timestamp_str,
    "is_preserved": has_preserve_attr(segment_path)
  }

def screen_recording_metadata(mp4):
  stem = mp4.with_suffix("")
  png_path = stem.with_suffix(".png")

  is_custom_name = False
  try:
    generated_name = re.sub(r"-\d+$", "", stem.name)
    datetime.strptime(generated_name, "%B_%d_%Y-%I-%M-%S%p")
  except ValueError:
    is_custom_name = True

  return {
    "filename": mp4.name,
    "gif": f"/screen_recordings/{stem.with_suffix('.gif').name}",
    "png": f"/screen_recordings/{png_path.name}",
    "timestamp": datetime.fromtimestamp(mp4.stat().st_mtime).isoformat(),
    "is_custom_name": is_custom_name
  }

def run_ffmpeg(args):
  command = _NICE + ["ffmpeg", "-hide_banner", "-loglevel", "error"] + args
  with _ffmpeg_slot():
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
      _, stderr = process.communicate(timeout=_FFMPEG_TIMEOUT)
    except subprocess.TimeoutExpired:
      process.kill()
      process.communicate()
      raise
  if process.returncode:
    raise subprocess.CalledProcessError(process.returncode, command, stderr=stderr)

def segment_to_segment_name(data_dir, segment):
  full_path = os.path.join(data_dir, f"FakeDongleID1337|{segment}")
  return SegmentName(full_path)

def video_to_gif(input_path, output_path):
  output_path = Path(output_path)

  with tempfile.TemporaryDirectory() as scratch:
    sped_up_path = Path(scratch) / "spedup.mp4"
    run_ffmpeg(["-i", str(input_path), "-an", "-vf", "setpts=PTS/35", str(sped_up_path)])
    run_ffmpeg(["-i", str(sped_up_path), "-loop", "0", str(output_path)])

def video_to_png(input_path, output_path):
  run_ffmpeg([
    "-ss", str(get_video_duration(input_path) / 2),
    "-i", str(input_path),
    "-frames:v", "1",
    str(output_path)
  ])

def xor_encrypt_decrypt(data, key):
  return "".join(chr(ord(c) ^ ord(key[i % len(key)])) for i, c in enumerate(data))
