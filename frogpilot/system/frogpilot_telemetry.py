#!/usr/bin/env python3
import capnp
import hashlib
import os
import requests
import secrets
import time
import zstandard

from pathlib import Path

import cereal.messaging as messaging

from cereal import log
from openpilot.common.params import Params
from openpilot.system import sentry
from openpilot.system.hardware import PC
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.system.loggerd.xattr_cache import getxattr, setxattr

from openpilot.frogpilot.common.frogpilot_api import FrogPilotAPI, FrogPilotAPIError

NetworkType = log.DeviceState.NetworkType

COMPRESSION_LEVEL = 10

DRIVE_ATTR_NAME = "user.frogpilot_telemetry_drive"
UPLOAD_ATTR_NAME = "user.frogpilot_telemetry"

EXCLUDED_MESSAGE_TYPES = frozenset((
  "accelerometer2",
  "androidLog",
  "boot",
  "clocks",
  "customReservedRawData0",
  "customReservedRawData1",
  "customReservedRawData2",
  "deviceState",
  "driverCameraState",
  "driverEncodeData",
  "driverEncodeIdx",
  "driverStateV2",
  "drivingModelData",
  "errorLogMessage",
  "frogpilotDeviceState",
  "gnssMeasurements",
  "gpsNMEA",
  "gyroscope2",
  "lightSensor",
  "livestreamDriverEncodeData",
  "livestreamDriverEncodeIdx",
  "livestreamRoadEncodeData",
  "livestreamRoadEncodeIdx",
  "livestreamWideRoadEncodeData",
  "livestreamWideRoadEncodeIdx",
  "logMessage",
  "magnetometer",
  "managerState",
  "mapdExtendedOut",
  "mapdIn",
  "mapdOut",
  "mapRenderState",
  "microphone",
  "model",
  "navInstruction",
  "navModel",
  "navRoute",
  "navThumbnail",
  "peripheralState",
  "procLog",
  "qRoadEncodeData",
  "qRoadEncodeIdx",
  "qcomGnss",
  "roadEncodeData",
  "roadEncodeIdx",
  "temperatureSensor",
  "temperatureSensor2",
  "testJoystick",
  "thumbnail",
  "ubloxGnss",
  "ubloxRaw",
  "uiDebug",
  "uiPlan",
  "uploaderState",
  "wideRoadEncodeData",
  "wideRoadEncodeIdx",
))

REMOVED_FIELDS = {
  "liveLocationKalman": (
    "calibratedOrientationECEF",
    "filterState",
    "orientationECEF",
    "positionECEF",
    "positionGeodetic",
    "velocityECEF",
    "velocityNED",
  ),
  "livePose": ("debugFilterState",),
}

REPLACEMENT_FIELDS = {
  "carControl": {"orientationNED.2": 0},
  "carParams": {"carVin": ""},
  "frogpilotPlan": {"slcMapboxWayId": 0},
  "gpsLocation": {"altitude": 0, "bearingDeg": 0, "latitude": 0, "longitude": 0, "unixTimestampMillis": 0, "vNED": []},
  "gpsLocationExternal": {"altitude": 0, "bearingDeg": 0, "latitude": 0, "longitude": 0, "unixTimestampMillis": 0, "vNED": []},
  "liveLocationKalman": {
    "calibratedOrientationNED.value.2": 0,
    "gpsTimeOfWeek": 0,
    "gpsWeek": 0,
    "orientationNED.value.2": 0,
    "unixTimestampMillis": 0,
  },
  "livePose": {"orientationNED.z": 0},
  "modelV2": {"leads": [], "rawPredictions": b""},
  "roadCameraState": {"image": b""},
  "wideRoadCameraState": {"image": b""},
}

WHITELIST_FIELDS = {
  "driverMonitoringState": ("isRHD",),
  "initData": ("deviceType", "dirty", "gitCommit", "gitCommitDate", "osVersion", "version"),
}

def filter_log(data):
  filtered_data = bytearray()

  try:
    for event in log.Event.read_multiple_bytes(data):
      try:
        which = event.which()
      except capnp.KjException:
        continue

      if which in EXCLUDED_MESSAGE_TYPES or which.endswith("DEPRECATED"):
        continue

      builder = event.as_builder()

      if which in REMOVED_FIELDS:
        message = getattr(builder, which)

        for field in REMOVED_FIELDS[which]:
          message.disown(field)

      if which in REPLACEMENT_FIELDS:
        message = getattr(builder, which)

        for field, value in REPLACEMENT_FIELDS[which].items():
          target = message
          *parents, field = field.split(".")

          for parent in parents:
            target = getattr(target, parent)

          if field.isdigit():
            index = int(field)
            if index < len(target):
              target[index] = value
          else:
            setattr(target, field, value)

      elif which in WHITELIST_FIELDS:
        kept = builder.init(which)
        source = getattr(event, which)

        for field in WHITELIST_FIELDS[which]:
          setattr(kept, field, getattr(source, field))

      filtered_data.extend(builder.to_bytes())
  except capnp.KjException:
    pass

  return filtered_data

class FrogPilotTelemetry:
  def __init__(self):
    selected_log_root = Path(Paths.log_root())
    if os.environ.get("LOG_ROOT") or PC:
      self.log_roots = (str(selected_log_root),)
    else:
      media_root = selected_log_root.parent
      self.log_roots = tuple(dict.fromkeys(str(media_root / name) for name in ("realdata", "realdata_HD", "realdata_konik")))

    self.frogpilot_api = FrogPilotAPI(Params())

    self.compressor = zstandard.ZstdCompressor(level=COMPRESSION_LEVEL)
    self.decompressor = zstandard.ZstdDecompressor()

    self.session = requests.Session()

    self.sm = messaging.SubMaster(["deviceState"])

  def pending_logs(self):
    drive_ids = {}
    pending = []

    for log_root in self.log_roots:
      for segment in listdir_by_creation(log_root):
        log_path = next((path for name in ("rlog.zst", "rlog") if (path := Path(log_root, segment, name)).is_file()), None)
        if log_path is None:
          continue

        try:
          drive_id = getxattr(log_path, DRIVE_ATTR_NAME)
          uploaded = getxattr(log_path, UPLOAD_ATTR_NAME)
        except OSError:
          continue

        route = segment.rpartition("--")[0]

        if uploaded:
          drive_ids[route] = uploaded.decode()
        else:
          if drive_id:
            drive_ids.setdefault(route, drive_id.decode())

          pending.append((log_path, route, drive_id))

    for log_path, route, existing_id in pending:
      if route not in drive_ids:
        drive_ids[route] = secrets.token_hex(16)

      drive_id = drive_ids[route].encode()

      if existing_id != drive_id:
        try:
          setxattr(log_path, DRIVE_ATTR_NAME, drive_id)
        except FileNotFoundError:
          continue

    for log_path, route, _ in pending:
      if not log_path.with_suffix(".lock").is_file():
        yield log_path, drive_ids[route]

  def upload(self, log_path, drive_id):
    if log_path.suffix == ".zst":
      with log_path.open("rb") as compressed, self.decompressor.stream_reader(compressed) as reader:
        raw_data = reader.read()
    else:
      raw_data = log_path.read_bytes()
    data = self.compressor.compress(filter_log(raw_data))

    submission = self.frogpilot_api.post_json("/v1/telemetry", {
      "route_id": drive_id,
      "segment": int(log_path.parent.name.rpartition("--")[2]),
      "sha256": hashlib.sha256(data).hexdigest(),
      "size_bytes": len(data),
    }, self.session)

    if upload := submission.get("upload"):
      self.frogpilot_api.put_upload(upload, data, log_path.name, self.session)

  def can_upload(self):
    self.sm.update(0)

    if self.sm["deviceState"].started:
      return False

    return not self.sm["deviceState"].networkMetered and self.sm["deviceState"].networkType in (NetworkType.ethernet, NetworkType.wifi)

  def update(self):
    if not self.can_upload():
      return

    for log_path, drive_id in self.pending_logs():
      try:
        self.upload(log_path, drive_id)
        setxattr(log_path, UPLOAD_ATTR_NAME, drive_id.encode())
      except (FrogPilotAPIError, requests.exceptions.RequestException):
        return
      except Exception as error:
        sentry.capture_exception(error, crash_log=False)

      if not self.can_upload():
        return


def main():
  frogpilot_telemetry = FrogPilotTelemetry()

  while True:
    try:
      frogpilot_telemetry.update()
    except Exception as error:
      sentry.capture_exception(error)

    time.sleep(60)


if __name__ == "__main__":
  main()
