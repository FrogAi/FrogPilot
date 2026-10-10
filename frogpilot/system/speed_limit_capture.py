#!/usr/bin/env python3
import json

from collections import deque
from pathlib import Path
from shutil import disk_usage
from time import time_ns

from cereal import messaging
from openpilot.common.swaglog import cloudlog
from openpilot.common.utils import atomic_write
from openpilot.system.hardware import HARDWARE
from openpilot.system.loggerd.xattr_cache import setxattr
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables

CAPTURES_PATH = Path("/data/media/speed_limit_captures")

BUFFER_DURATION = 10

MAX_CAPTURES = 100

MIN_FREE_SPACE = 5 * 1024 * 1024 * 1024

def capture_details(sm, device_details):
  return {
    **device_details,
    "cal_status": str(sm["liveCalibration"].calStatus),
    "rpy_calib": list(sm["liveCalibration"].rpyCalib),
    "sensor": str(sm["roadCameraState"].sensor),
  }

def save_clip(frames, trigger):
  while frames and not frames[0].header:
    frames.popleft()

  if not frames:
    return

  try:
    CAPTURES_PATH.mkdir(parents=True, exist_ok=True)

    if disk_usage(CAPTURES_PATH).free < MIN_FREE_SPACE:
      cloudlog.warning("Skipping speed limit capture: insufficient free space")
      return

    capture_path = CAPTURES_PATH / str(time_ns())

    with atomic_write(capture_path.with_suffix(".hevc"), mode="wb") as clip:
      clip.write(frames[0].header)

      for frame in frames:
        clip.write(frame.data)

    with atomic_write(capture_path.with_suffix(".json")) as details:
      json.dump(trigger, details)

    setxattr(capture_path.with_suffix(".hevc"), frogpilot_variables.UPLOAD_ATTR_NAME, frogpilot_variables.UPLOAD_PENDING)

    frames.clear()

    for capture in sorted(CAPTURES_PATH.glob("*.hevc"))[:-MAX_CAPTURES]:
      capture.unlink()
      capture.with_suffix(".json").unlink(missing_ok=True)

  except OSError:
    cloudlog.exception("Failed to save speed limit capture")

def main():
  previous_lkas_button_press_count = 0
  previous_speed_limit = 0

  build_metadata = get_build_metadata()

  capture_via_lkas = build_metadata.channel == "FrogPilot-Development" and frogpilot_utilities.is_FrogsGoMoo()

  device_details = {
    "device_type": HARDWARE.get_device_type(),
    "git_commit": build_metadata.openpilot.git_commit,
    "version": build_metadata.openpilot.version,
  }

  frames = deque()

  road_encode_sock = messaging.sub_sock("roadEncodeData")

  sm = messaging.SubMaster(["frogpilotCarState", "liveCalibration", "roadCameraState"])

  while True:
    for message in messaging.drain_sock(road_encode_sock, wait_for_one=True):
      frames.append(message.roadEncodeData)

      while frames[-1].idx.timestampEof - frames[0].idx.timestampEof > BUFFER_DURATION * 1_000_000_000:
        frames.popleft()

    sm.update(0)

    speed_limit = sm["frogpilotCarState"].dashboardSpeedLimit
    lkas_button_press_count = sm["frogpilotCarState"].lkasButtonPressCount

    if speed_limit != previous_speed_limit and speed_limit > 0:
      save_clip(frames, {"trigger": "dashboard", "speed_limit": speed_limit, **capture_details(sm, device_details)})
    elif capture_via_lkas and lkas_button_press_count > previous_lkas_button_press_count:
      save_clip(frames, {"trigger": "lkas", **capture_details(sm, device_details)})

    previous_speed_limit = speed_limit
    previous_lkas_button_press_count = lkas_button_press_count


if __name__ == "__main__":
  main()
