#!/usr/bin/env python3
import json

from collections import deque
from random import random
from shutil import disk_usage
from time import time_ns

from cereal import messaging
from openpilot.common.file_helpers import atomic_write_in_dir
from openpilot.common.swaglog import cloudlog
from openpilot.system.hardware import HARDWARE
from openpilot.system.loggerd.xattr_cache import setxattr
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_variables

BUFFER_DURATION = 15

MAX_CAPTURES = 100

MIN_FREE_SPACE = 5 * 1024 * 1024 * 1024

POST_TRIGGER_DURATION = 5


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
    frogpilot_paths.SPEED_LIMIT_CAPTURES_PATH.mkdir(parents=True, exist_ok=True)

    if disk_usage(frogpilot_paths.SPEED_LIMIT_CAPTURES_PATH).free < MIN_FREE_SPACE:
      cloudlog.warning("Skipping speed limit capture: insufficient free space")
      return

    capture_path = frogpilot_paths.SPEED_LIMIT_CAPTURES_PATH / str(time_ns())

    with atomic_write_in_dir(str(capture_path.with_suffix(".hevc")), mode="wb") as clip:
      clip.write(frames[0].header)

      for frame in frames:
        clip.write(frame.data)

    with atomic_write_in_dir(str(capture_path.with_suffix(".json"))) as details:
      json.dump(trigger, details)

    setxattr(capture_path.with_suffix(".hevc"), frogpilot_variables.UPLOAD_ATTR_NAME, frogpilot_variables.UPLOAD_PENDING)

    for capture in sorted(frogpilot_paths.SPEED_LIMIT_CAPTURES_PATH.glob("*.hevc"))[:-MAX_CAPTURES]:
      capture.unlink()
      capture.with_suffix(".json").unlink(missing_ok=True)

  except OSError:
    cloudlog.exception("Failed to save speed limit capture")


def queue_clip(frames, pending_clips, trigger):
  pending_clips.append((frames[-1].idx.timestampEof + POST_TRIGGER_DURATION * 1_000_000_000, trigger))


def main():
  previous_reader_speed_limit = 0
  previous_speed_limit = 0

  unsure_agreement = 0
  unsure_speed_limit = 0

  build_metadata = get_build_metadata()

  device_details = {
    "device_type": HARDWARE.get_device_type(),
    "git_commit": build_metadata.openpilot.git_commit,
    "version": build_metadata.openpilot.version,
  }

  frames = deque()
  pending_clips = deque()

  road_encode_sock = messaging.sub_sock("roadEncodeData")

  sm = messaging.SubMaster(["frogpilotCarState", "frogpilotSignReading", "liveCalibration", "roadCameraState"])

  while True:
    for message in messaging.drain_sock(road_encode_sock, wait_for_one=True):
      frames.append(message.roadEncodeData)

      while frames[-1].idx.timestampEof - frames[0].idx.timestampEof > BUFFER_DURATION * 1_000_000_000:
        frames.popleft()

    sm.update(0)

    speed_limit = sm["frogpilotCarState"].dashboardSpeedLimit

    sign_reading = sm["frogpilotSignReading"]

    if sign_reading.signSpeedLimit != unsure_speed_limit:
      unsure = unsure_agreement >= frogpilot_variables.UNSURE_SIGN_AGREEMENT and sign_reading.speedLimit != unsure_speed_limit

      if unsure:
        trigger = {"trigger": "unsure", "sign_speed_limit": unsure_speed_limit, "sign_agreement": unsure_agreement}
        queue_clip(frames, pending_clips, {**trigger, **capture_details(sm, device_details)})

      unsure_agreement = 0
      unsure_speed_limit = sign_reading.signSpeedLimit

    if unsure_speed_limit > 0:
      unsure_agreement = max(unsure_agreement, sign_reading.signAgreement)

    if speed_limit != previous_speed_limit and speed_limit > 0:
      queue_clip(frames, pending_clips, {"trigger": "dashboard", "speed_limit": speed_limit, **capture_details(sm, device_details)})
    elif sign_reading.speedLimit != previous_reader_speed_limit and sign_reading.speedLimit > 0 and random() < frogpilot_variables.READER_CLIP_SHARE:
      queue_clip(frames, pending_clips, {"trigger": "reader", "speed_limit": sign_reading.speedLimit, **capture_details(sm, device_details)})

    previous_speed_limit = speed_limit
    previous_reader_speed_limit = sign_reading.speedLimit

    while pending_clips and frames[-1].idx.timestampEof >= pending_clips[0][0]:
      _, trigger = pending_clips.popleft()

      save_clip(frames, trigger)


if __name__ == "__main__":
  main()
