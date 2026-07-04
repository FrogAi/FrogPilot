#!/usr/bin/env python3
import base64
import hashlib
import json
import os
import re
import secrets
import shutil
import subprocess
import threading
import uuid

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

LOG_CANDIDATES = [
  "qlog",
  "qlog.bz2",
  "rlog",
  "rlog.bz2",
  "raw_log.bz2",
]

SEGMENT_RE = re.compile(r"^[0-9a-fA-F]{8}--[0-9a-fA-F]{10}--\d+$")

TARGET_LOUDNESS = -15.0

XOR_KEY = "s8#pL3*Xj!aZ@dWq"

MAX_FILE_SIZE = 5 * 1024 * 1024

IMAGE_EXTS = (".png", ".gif", ".jpg", ".jpeg")
SOUND_NAMES = ("engage", "disengage", "prompt", "startup")
DISTANCE_ICON_NAMES = ("traffic", "aggressive", "standard", "relaxed")

def _file_too_large(file_storage):
  stream = file_storage.stream
  pos = stream.tell()
  stream.seek(0, os.SEEK_END)
  size = stream.tell()
  stream.seek(pos)
  return size > MAX_FILE_SIZE

_FFMPEG_SEMAPHORE = threading.Semaphore(2)
_FFMPEG_TIMEOUT = 600
_FFPROBE_TIMEOUT = 30
_NICE = (["nice", "-n", "19", "ionice", "-c", "3"]
         if shutil.which("nice") and shutil.which("ionice") else [])
_DURATION_CACHE: dict[str, float] = {}
_VIDEO_CACHE_MIN_FREE = 500 * 1024 * 1024

def _evict_video_cache():
  try:
    free = shutil.disk_usage(VIDEO_CACHE_PATH).free
  except OSError:
    return
  if free >= _VIDEO_CACHE_MIN_FREE:
    return
  for cache_file in sorted(VIDEO_CACHE_PATH.glob("*.mp4"), key=lambda p: p.stat().st_atime):
    if free >= _VIDEO_CACHE_MIN_FREE:
      break
    try:
      size = cache_file.stat().st_size
      cache_file.unlink()
      free += size
    except OSError:
      pass

def _oversize_error(file):
  return f"File {file.filename} exceeds the 5MB limit." if _file_too_large(file) else None

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
  tail = ["-movflags", "faststart", "-y", str(cache_path)]
  with _FFMPEG_SEMAPHORE:
    try:
      subprocess.run([*base, "-c", "copy", *tail], check=True, timeout=_FFMPEG_TIMEOUT)
    except subprocess.CalledProcessError:
      subprocess.run([*base, "-c:v", "libx264", *tail], check=True, timeout=_FFMPEG_TIMEOUT)

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

def create_theme(form_data, files, temporary=False):
  theme_name = form_data.get("themeName")
  if not theme_name:
    return None, "Theme name is required."

  sane_theme_name = secure_filename(theme_name.replace(" ", "_"))

  save_checklist_str = form_data.get("saveChecklist", "{}")
  save_checklist = json.loads(save_checklist_str)

  needs_theme_pack = any([
    save_checklist.get("colors"),
    save_checklist.get("icons"),
    save_checklist.get("sounds"),
    save_checklist.get("turn_signals"),
    save_checklist.get("distance_icons"),
  ])

  if temporary:
    base_path = Path(f"/tmp/{sane_theme_name}_{secrets.token_hex(8)}")
  else:
    base_path = THEME_SAVE_PATH / "theme_packs" if needs_theme_pack else None

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
      (signals_path / f"{style}_{turn_signal_length}").touch()

    turn_signal_type = form_data.get("turnSignalType", "Single Image").lower()

    if turn_signal_type == "single image":
      for f in signals_path.glob("turn_signal.*"):
        f.unlink()
      for f in signals_path.glob("turn_signal_blindspot.*"):
        f.unlink()

      file = files.get("turnSignal")
      if file and file.filename:
        if err := _oversize_error(file):
          return None, err
        ext = Path(file.filename).suffix
        file.save(signals_path / f"turn_signal{ext}")

      file = files.get("turnSignalBlindspot")
      if file and file.filename:
        if err := _oversize_error(file):
          return None, err
        ext = Path(file.filename).suffix
        file.save(signals_path / f"turn_signal_blindspot{ext}")

    elif turn_signal_type == "sequential":
      for f in signals_path.glob("turn_signal_*"):
        f.unlink()

      signal_map = {
        "turnSignal": "turn_signal",
        "turnSignalBlindspot": "turn_signal_blindspot",
      }
      for field, base_name in signal_map.items():
        file = files.get(field)
        if file and file.filename:
          if err := _oversize_error(file):
            return None, err
          for f in signals_path.glob(f"{base_name}.*"):
            f.unlink()
          ext = Path(file.filename).suffix.lower()
          file.save(signals_path / f"{base_name}{ext}")

      for f in signals_path.glob("turn_signal.*"):
        f.unlink()
      for f in signals_path.glob("turn_signal_blindspot.*"):
        f.unlink()

      sequential_keys = sorted(
        [k for k in files if k.startswith("turn_signal_")],
        key=lambda name: int(name.split("_")[-1])
      )

      for key in sequential_keys:
        file = files.get(key)
        if file and file.filename:
          if err := _oversize_error(file):
            return None, err
          idx = key.split("_")[-1]
          ext = Path(file.filename).suffix
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
        if err := _oversize_error(file):
          return None, err

        for f in dest_path.glob(f"{base_name}.*"):
          f.unlink()

        ext = Path(file.filename).suffix.lower()
        save_path = dest_path / f"{base_name}{ext}"
        file.save(save_path)

        if resize_dims:
          _resize_image(save_path, ext, resize_dims)

  if save_checklist.get("steering_wheel"):
    wheels_dir = THEME_SAVE_PATH / "steering_wheels"
    wheels_dir.mkdir(parents=True, exist_ok=True)
    file = files.get("steeringWheel")
    saved_wheel_path = None
    if file and file.filename:
      if err := _oversize_error(file):
        return None, err
      for f in wheels_dir.glob(f"{sane_theme_name}-user_created.*"):
        f.unlink()
      ext = Path(file.filename).suffix.lower()
      saved_wheel_path = wheels_dir / f"{sane_theme_name}-user_created{ext}"
      file.save(saved_wheel_path)
      saved_wheel_path = _resize_image(saved_wheel_path, ext, (250, 250))
    if temporary and (theme_path is not None):
      existing = saved_wheel_path if saved_wheel_path is not None else next(wheels_dir.glob(f"{sane_theme_name}-user_created.*"), None)
      if existing:
        wheel_icon_dir = theme_path / "WheelIcon"
        wheel_icon_dir.mkdir(parents=True, exist_ok=True)
        dest = wheel_icon_dir / f"wheel{existing.suffix.lower()}"
        if dest.exists():
          dest.unlink()
        dest.symlink_to(existing)

  if save_checklist.get("distance_icons"):
    dist_path = theme_path / "distance_icons"
    dist_path.mkdir(exist_ok=True)
    for name in DISTANCE_ICON_NAMES:
      file = files.get(f"distanceIcons_{name}")
      if file and file.filename:
        if err := _oversize_error(file):
          return None, err

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
        if err := _oversize_error(file):
          return None, err

        save_path = sounds_path / f"{name}{Path(file.filename).suffix}"
        file.save(save_path)
        convert_audio(str(save_path))

  return theme_path, None

def decode_parameters(encoded_string):
  obfuscated_data = base64.b64decode(encoded_string.encode("utf-8")).decode("utf-8")
  decrypted_data = xor_encrypt_decrypt(obfuscated_data, XOR_KEY)
  return json.loads(decrypted_data)

def ffmpeg_concat_segments_to_mp4(input_files, cache_key=None):
  if not input_files:
    raise ValueError("No input files provided for concatenation")

  VIDEO_CACHE_PATH.mkdir(exist_ok=True)

  key_str = "|".join(str(p) for p in input_files)
  if cache_key:
    key_str = f"{cache_key}|{key_str}"
  file_hash = hashlib.md5(key_str.encode()).hexdigest()
  cache_path = VIDEO_CACHE_PATH / f"{file_hash}.mp4"

  if cache_path.exists() and all(cache_path.stat().st_mtime > Path(f).stat().st_mtime for f in input_files):
    return cache_path

  _evict_video_cache()

  list_file = VIDEO_CACHE_PATH / f"{file_hash}.txt"
  with open(list_file, "w") as f:
    for seg in input_files:
      f.write(f"file '{Path(seg)}'\n")

  try:
    _ffmpeg_to_mp4(["-f", "concat", "-safe", "0", "-i", str(list_file)], cache_path)
  except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
    if cache_path.exists():
      cache_path.unlink()
    raise ValueError(f"Cannot process concatenated video segments: {input_files}")
  finally:
    if list_file.exists():
      list_file.unlink()

  return cache_path

def ffmpeg_mp4_wrap_process_builder(filename):
  input_path = Path(filename)

  if not input_path.exists():
    raise FileNotFoundError(f"Input file does not exist: {input_path}")

  if input_path.stat().st_size == 0:
    raise ValueError(f"Input file is empty: {input_path}")

  lock_file = input_path.parent / "rlog.lock"
  if lock_file.exists():
    raise ValueError(f"File is still being recorded: {input_path}")

  VIDEO_CACHE_PATH.mkdir(exist_ok=True)

  _evict_video_cache()

  file_hash = hashlib.md5(str(input_path).encode()).hexdigest()
  cache_path = VIDEO_CACHE_PATH / f"{file_hash}.mp4"

  if cache_path.exists() and cache_path.stat().st_mtime > input_path.stat().st_mtime:
    return cache_path

  try:
    _ffmpeg_to_mp4(["-i", str(input_path)], cache_path)
  except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
    if cache_path.exists():
      cache_path.unlink()
    raise ValueError(f"Cannot process video file: {input_path}")

  return cache_path

def format_git_date(raw_date: str):
  from openpilot.frogpilot.system.the_pond.helpers import format_ordinal_date
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
  stats = json.loads(params.get("ApiCache_DriveStats", encoding="utf-8") or "{}")
  frogpilot_stats = json.loads(params.get("FrogPilotStats") or "{}")

  is_metric = params.get_bool("IsMetric")
  unit = "kilometers" if is_metric else "miles"

  def process(timeframe):
    data = stats.get(timeframe, {})
    return {
      "distance": data.get("distance", 0) * (1 if is_metric else CV.KPH_TO_MPH),
      "drives": data.get("routes", 0),
      "hours": data.get("minutes", 0) / 60,
      "unit": unit
    }

  stats["all"] = process("all")
  stats["week"] = process("week")
  stats["frogpilot"] = {
    "distance": frogpilot_stats.get("FrogPilotMeters", 0) * (0.001 if is_metric else CV.METER_TO_MILE),
    "hours": frogpilot_stats.get("FrogPilotSeconds", 0) / (60 * 60),
    "drives": frogpilot_stats.get("FrogPilotDrives", 0),
    "unit": unit
  }

  return stats

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
    result = subprocess.run([
      "ffprobe", "-v", "error", "-show_entries", "format=duration",
      "-of", "default=noprint_wrappers=1:nokey=1", str(input_path)
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True, timeout=_FFPROBE_TIMEOUT)
    duration = float(result.stdout)
  except (ValueError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
    duration = 60
  if key:
    _DURATION_CACHE[key] = duration
  return duration

def has_preserve_attr(path: str):
  return PRESERVE_ATTR_NAME in os.listxattr(path) and os.getxattr(path, PRESERVE_ATTR_NAME) == PRESERVE_ATTR_VALUE

def list_file(path):
  return sorted(os.listdir(path), reverse=True)

def normalize_theme_name(name, for_path=False):
  name = name.replace("-user_created", "")
  if for_path:
    return name.lower().replace(" (", "-").replace(")", "").replace(" ", "-").replace("'", "").replace(".", "")

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
      if not item.endswith((".hevc", ".ts", ".png", ".gif")) and item not in LOG_CANDIDATES:
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
    datetime.strptime(stem.name, "%B_%d_%Y-%I-%M%p")
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
  with _FFMPEG_SEMAPHORE:
    process = subprocess.Popen(_NICE + ["ffmpeg", "-hide_banner", "-loglevel", "error"] + args, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
      stdout, _ = process.communicate(timeout=_FFMPEG_TIMEOUT)
    except subprocess.TimeoutExpired:
      process.kill()
      process.communicate()
      raise
  return stdout

def segment_to_segment_name(data_dir, segment):
  full_path = os.path.join(data_dir, f"FakeDongleID1337|{segment}")
  return SegmentName(full_path)

def video_to_gif(input_path, output_path):
  output_path = Path(output_path)
  sped_up_path = output_path.with_suffix(f".{uuid.uuid4()}.spedup.mp4")

  run_ffmpeg(["-i", str(input_path), "-an", "-vf", "setpts=PTS/35", str(sped_up_path)])
  run_ffmpeg(["-i", str(sped_up_path), "-loop", "0", str(output_path)])

  if os.path.exists(sped_up_path):
    os.remove(sped_up_path)

def video_to_png(input_path, output_path):
  run_ffmpeg([
    "-ss", str(get_video_duration(input_path) / 2),
    "-i", str(input_path),
    "-frames:v", "1",
    str(output_path)
  ])

def xor_encrypt_decrypt(data, key):
  return "".join(chr(ord(c) ^ ord(key[i % len(key)])) for i, c in enumerate(data))
