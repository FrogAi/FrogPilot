#!/usr/bin/env python3
import base64
import json
import os
import re
import subprocess
import time
import uuid

from datetime import datetime
from pathlib import Path
from typing import List

from openpilot.common.conversions import Conversions as CV
from openpilot.system.loggerd.config import get_available_bytes, get_used_bytes
from openpilot.system.loggerd.deleter import PRESERVE_ATTR_NAME, PRESERVE_ATTR_VALUE
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.tools.lib.route import SegmentName

from openpilot.frogpilot.common.frogpilot_variables import params, params_tracking

LOG_CANDIDATES = [
  "qlog",
  "qlog.bz2",
  "rlog",
  "rlog.bz2",
  "raw_log.bz2",
]

SEGMENT_RE = re.compile(r"^[0-9a-fA-F]{8}--[0-9a-fA-F]{10}--\d+$")

XOR_KEY = "s8#pL3*Xj!aZ@dWq"

def decode_parameters(encoded_string):
  obfuscated_data = base64.b64decode(encoded_string.encode("utf-8")).decode("utf-8")
  decrypted_data = xor_encrypt_decrypt(obfuscated_data, XOR_KEY)
  return json.loads(decrypted_data)

def encode_parameters(params_dict):
  serialized_data = json.dumps(params_dict)
  obfuscated_data = xor_encrypt_decrypt(serialized_data, XOR_KEY)
  encoded_data = base64.b64encode(obfuscated_data.encode("utf-8")).decode("utf-8")
  return encoded_data

def ffmpeg_mp4_wrap_process_builder(filename):
  is_raw_hevc = filename.rsplit(".", 1)[-1] == "hevc"

  command = [
    "ffmpeg",
    "-hide_banner",
    "-loglevel", "error",
    "-probesize", "1M",
    "-analyzeduration", "1M",
    *(["-f", "hevc"] if is_raw_hevc else []),
    "-i", filename,
    "-c", "copy",
    "-map", "0",
    *(["-vtag", "hvc1"] if is_raw_hevc else []),
    "-f", "mp4",
    "-movflags", "empty_moov",
    "-"
  ]

  return subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

def format_git_date(raw_date: str):
  date_object = datetime.strptime(raw_date.split()[1], "%Y-%m-%d")

  day = date_object.day
  suffix = "th" if 11 <= day <= 13 else {1: "st", 2: "nd", 3: "rd"}.get(day % 10, "th")

  return date_object.strftime(f"%B {day}{suffix}, %Y")

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
    "usedPercentage": f"{(used / total) * 100:.2f}%"
  }]

def get_drive_stats():
  stats = json.loads(params.get("ApiCache_DriveStats", encoding="utf-8") or "{}")

  is_metric = params.get_bool("IsMetric")
  conversion = 1 if is_metric else CV.KPH_TO_MPH
  unit = "kilometers" if is_metric else "miles"

  def process(timeframe):
    data = stats.get(timeframe, {})
    return {
      "distance": data.get("distance", 0) * conversion,
      "drives": data.get("routes", 0),
      "hours": data.get("minutes", 0) / 60,
      "unit": unit
    }

  stats["all"] = process("all")
  stats["week"] = process("week")
  stats["frogpilot"] = {
    "distance": params_tracking.get_int("FrogPilotKilometers") * conversion,
    "hours": params_tracking.get_int("FrogPilotMinutes") / 60,
    "drives": params_tracking.get_int("FrogPilotDrives"),
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
    result = subprocess.run([
      "ffprobe", "-v", "error", "-show_entries", "format=duration",
      "-of", "default=noprint_wrappers=1:nokey=1", str(input_path)
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
    return float(result.stdout)
  except (ValueError, subprocess.CalledProcessError):
    return 60

def has_preserve_attr(path: str):
  return PRESERVE_ATTR_NAME in os.listxattr(path) and os.getxattr(path, PRESERVE_ATTR_NAME) == PRESERVE_ATTR_VALUE

def list_file(path):
  return sorted(os.listdir(path), reverse=True)

def process_route(footage_path, route_name):
  segment_path = f"{footage_path}{route_name}--0"
  qcamera_path = f"{segment_path}/qcamera.ts"
  rlog_path = f"{segment_path}/rlog"

  png_output_path = os.path.join(segment_path, "preview.png")
  if not os.path.exists(png_output_path):
    video_to_png(qcamera_path, png_output_path)

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

def process_route_gif(footage_path, route_name):
  segment_path = f"{footage_path}{route_name}--0"
  qcamera_path = f"{segment_path}/qcamera.ts"
  gif_output_path = os.path.join(segment_path, "preview.gif")

  if not os.path.exists(gif_output_path):
    video_to_gif(qcamera_path, gif_output_path)

def process_screen_recording(mp4):
  stem = mp4.with_suffix("")
  png_path = stem.with_suffix(".png")

  if not png_path.exists():
    video_to_png(mp4, png_path)

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

def process_screen_recording_gif(mp4):
  stem = mp4.with_suffix("")
  gif_path = stem.with_suffix(".gif")
  if not gif_path.exists():
    video_to_gif(mp4, gif_path)

def run_ffmpeg(args):
  process = subprocess.Popen(["ffmpeg", "-hide_banner", "-loglevel", "error"] + args, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
  stdout, stderr = process.communicate()
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
