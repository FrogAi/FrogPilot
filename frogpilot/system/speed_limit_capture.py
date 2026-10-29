#!/usr/bin/env python3
import json

from collections import deque, namedtuple
from random import random
from shutil import disk_usage
from time import time_ns

from cereal import messaging
from openpilot.common.swaglog import cloudlog
from openpilot.common.utils import atomic_write
from openpilot.system.hardware import HARDWARE
from openpilot.system.loggerd.xattr_cache import setxattr
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system import speed_limit_reader

BUFFER_DURATION = 10

GPS_INTERVAL = 1_000_000_000

MAX_CAPTURES = 100

MIN_FREE_SPACE = 5 * 1024 * 1024 * 1024

Frame = namedtuple("Frame", ["encode_data", "wall_ns", "numbers", "text"])


def capture_details(sm, device_details):
  return {
    **device_details,
    "cal_status": str(sm["liveCalibration"].calStatus),
    "rpy_calib": list(sm["liveCalibration"].rpyCalib),
    "sensor": str(sm["roadCameraState"].sensor),
  }


def reader_details(frames, gps, inputs, record_gps):
  details = {
    "number_fields": inputs["numbers"],
    "text_fields": inputs["text"],
    "first_frame_wall_ns": frames[0].wall_ns,
    "frames": [{
      "frame_id": frame.encode_data.idx.frameId,
      "timestamp_eof": frame.encode_data.idx.timestampEof,
      "numbers": frame.numbers,
      "text": frame.text,
    } for frame in frames],
  }

  if record_gps:
    details["gps"] = [location for _, location in gps]

  return details


def save_clip(frames, gps, inputs, record_gps, trigger):
  while frames and not frames[0].encode_data.header:
    frames.popleft()

  if not frames:
    return

  try:
    frogpilot_paths.SPEED_LIMIT_CAPTURES_PATH.mkdir(parents=True, exist_ok=True)

    if disk_usage(frogpilot_paths.SPEED_LIMIT_CAPTURES_PATH).free < MIN_FREE_SPACE:
      cloudlog.warning("Skipping speed limit capture: insufficient free space")
      return

    capture_path = frogpilot_paths.SPEED_LIMIT_CAPTURES_PATH / str(time_ns())

    with atomic_write(capture_path.with_suffix(".hevc"), mode="wb") as clip:
      clip.write(frames[0].encode_data.header)

      for frame in frames:
        clip.write(frame.encode_data.data)

    with atomic_write(capture_path.with_suffix(".json")) as details:
      json.dump({**trigger, **reader_details(frames, gps, inputs, record_gps)}, details)

    setxattr(capture_path.with_suffix(".hevc"), frogpilot_variables.UPLOAD_ATTR_NAME, frogpilot_variables.UPLOAD_PENDING)

    frames.clear()
    gps.clear()

    for capture in sorted(frogpilot_paths.SPEED_LIMIT_CAPTURES_PATH.glob("*.hevc"))[:-MAX_CAPTURES]:
      capture.unlink()
      capture.with_suffix(".json").unlink(missing_ok=True)

  except OSError:
    cloudlog.exception("Failed to save speed limit capture")


def main():
  previous_lkas_button_press_count = 0
  previous_reader_speed_limit = 0
  previous_speed_limit = 0

  unsure_agreement = 0
  unsure_speed_limit = 0

  build_metadata = get_build_metadata()

  development_build = frogpilot_utilities.is_FrogsGoMoo_development(build_metadata)

  device_details = {
    "device_type": HARDWARE.get_device_type(),
    "git_commit": build_metadata.openpilot.git_commit,
    "version": build_metadata.openpilot.version,
  }

  inputs = frogpilot_utilities.get_speed_limit_reader_inputs()

  frames = deque()
  gps = deque()

  road_encode_sock = messaging.sub_sock("roadEncodeData")

  services = {"frogpilotCarState", "frogpilotSignReading", "liveCalibration", "roadCameraState"} | {field.split(".")[0] for field in inputs["numbers"] + inputs["text"]}
  if development_build:
    services |= {"gpsLocation", "gpsLocationExternal"}

  sm = messaging.SubMaster(sorted(services))

  while True:
    messages = messaging.drain_sock(road_encode_sock, wait_for_one=True)

    sm.update(0)

    now = time_ns()

    for message in messages:
      numbers = [float(speed_limit_reader.read(sm, field)) for field in inputs["numbers"]]
      text = [str(speed_limit_reader.read(sm, field)) for field in inputs["text"]]
      frames.append(Frame(message.roadEncodeData, now, numbers, text))

      while frames[-1].encode_data.idx.timestampEof - frames[0].encode_data.idx.timestampEof > BUFFER_DURATION * 1_000_000_000:
        frames.popleft()

    if development_build:
      for gps_service in ("gpsLocationExternal", "gpsLocation"):
        if frogpilot_utilities.is_gps_location_valid(gps_service, sm):
          if not gps or now - gps[-1][0] >= GPS_INTERVAL:
            location = sm[gps_service]
            gps.append((now, {
              "timestamp": location.unixTimestampMillis,
              "latitude": location.latitude,
              "longitude": location.longitude,
              "bearing": location.bearingDeg,
              "accuracy": location.horizontalAccuracy,
            }))
          break

      while gps and frames and gps[0][0] < frames[0].wall_ns:
        gps.popleft()

    speed_limit = sm["frogpilotCarState"].dashboardSpeedLimit
    lkas_button_press_count = sm["frogpilotCarState"].lkasButtonPressCount

    sign_reading = sm["frogpilotSignReading"]

    if sign_reading.signSpeedLimit != unsure_speed_limit:
      unsure = unsure_agreement >= frogpilot_variables.UNSURE_SIGN_AGREEMENT and sign_reading.speedLimit != unsure_speed_limit

      if unsure:
        save_clip(frames, gps, inputs, development_build, {"trigger": "unsure", "sign_speed_limit": unsure_speed_limit, "sign_agreement": unsure_agreement, **capture_details(sm, device_details)})

      unsure_agreement = 0
      unsure_speed_limit = sign_reading.signSpeedLimit

    if unsure_speed_limit > 0:
      unsure_agreement = max(unsure_agreement, sign_reading.signAgreement)

    if speed_limit != previous_speed_limit and speed_limit > 0:
      save_clip(frames, gps, inputs, development_build, {"trigger": "dashboard", "speed_limit": speed_limit, **capture_details(sm, device_details)})
    elif sign_reading.speedLimit != previous_reader_speed_limit and sign_reading.speedLimit > 0 and random() < frogpilot_variables.READER_CLIP_SHARE:
      save_clip(frames, gps, inputs, development_build, {"trigger": "reader", "speed_limit": sign_reading.speedLimit, **capture_details(sm, device_details)})
    elif development_build and lkas_button_press_count > previous_lkas_button_press_count:
      save_clip(frames, gps, inputs, development_build, {"trigger": "lkas", **capture_details(sm, device_details)})

    previous_speed_limit = speed_limit
    previous_lkas_button_press_count = lkas_button_press_count
    previous_reader_speed_limit = sign_reading.speedLimit


if __name__ == "__main__":
  main()
