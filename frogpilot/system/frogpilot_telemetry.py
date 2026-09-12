#!/usr/bin/env python3
import capnp
import hashlib
import requests
import secrets
import time
import zstandard

from pathlib import Path

import cereal.messaging as messaging

from cereal import log
from openpilot.system import sentry
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.system.loggerd.xattr_cache import getxattr, setxattr

from openpilot.frogpilot.common import frogpilot_api

NetworkType = log.DeviceState.NetworkType

COMPRESSION_LEVEL = 10

DISOWN = object()

UPLOAD_ATTR_NAME = "user.frogpilot_telemetry"

BLACKLIST_FIELDS = {
  "carParams": {"carVin": ""},
  "frogpilotPlan": {"slcMapboxWayId": 0},
  "gpsLocation": {"altitude": 0, "bearingDeg": 0, "latitude": 0, "longitude": 0, "unixTimestampMillis": 0, "vNED": []},
  "gpsLocationExternal": {"altitude": 0, "bearingDeg": 0, "latitude": 0, "longitude": 0, "unixTimestampMillis": 0, "vNED": []},
  "liveLocationKalman": {
    "calibratedOrientationECEF": DISOWN,
    "filterState": DISOWN,
    "gpsTimeOfWeek": 0,
    "gpsWeek": 0,
    "orientationECEF": DISOWN,
    "positionECEF": DISOWN,
    "positionGeodetic": DISOWN,
    "unixTimestampMillis": 0,
    "velocityECEF": DISOWN,
  },
  "livePose": {"filterState": DISOWN},
  "modelV2": {"leads": [], "rawPredictions": b""},
  "roadCameraState": {"image": b""},
  "wideRoadCameraState": {"image": b""},
}

EXCLUDED_MESSAGE_TYPES = frozenset((
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
  "errorLogMessage",
  "frogpilotDeviceState",
  "gnssMeasurements",
  "gpsNMEA",
  "lightSensor",
  "livestreamDriverEncodeData",
  "livestreamDriverEncodeIdx",
  "livestreamRoadEncodeData",
  "livestreamRoadEncodeIdx",
  "livestreamWideRoadEncodeData",
  "livestreamWideRoadEncodeIdx",
  "logMessage",
  "managerState",
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

WHITELIST_FIELDS = {
  "driverMonitoringState": ("isRHD",),
  "initData": ("deviceType", "dirty", "gitBranch", "gitCommit", "gitCommitDate", "osVersion", "version"),
}

def scrub(event, builder, which):
  if which in BLACKLIST_FIELDS:
    message = getattr(builder, which)

    for field, empty in BLACKLIST_FIELDS[which].items():
      if empty is DISOWN:
        message.disown(field)
      else:
        setattr(message, field, empty)

  elif which in WHITELIST_FIELDS:
    kept = builder.init(which)
    source = getattr(event, which)

    for field in WHITELIST_FIELDS[which]:
      setattr(kept, field, getattr(source, field))


def filter_log(data):
  events = []

  try:
    for event in log.Event.read_multiple_bytes(data):
      try:
        which = event.which()
      except capnp.KjException:
        continue

      if which in EXCLUDED_MESSAGE_TYPES:
        continue

      builder = event.as_builder()
      scrub(event, builder, which)
      events.append(builder.to_bytes())
  except capnp.KjException:
    pass

  return b"".join(events)


class FrogPilotTelemetry:
  def __init__(self):
    self.log_roots = dict.fromkeys((Paths.log_root(raw=True), Paths.log_root(HD=True, raw=True), Paths.log_root(konik=True, raw=True)))

    self.compressor = zstandard.ZstdCompressor(level=COMPRESSION_LEVEL)
    self.session = requests.Session()

    self.sm = messaging.SubMaster(["deviceState"])

  def pending_logs(self):
    drive_ids = {}
    pending = []

    for log_root in self.log_roots:
      for segment in listdir_by_creation(log_root):
        log_path = Path(log_root, segment, "rlog")

        if not log_path.is_file() or log_path.with_suffix(".lock").is_file():
          continue

        try:
          uploaded = getxattr(log_path, UPLOAD_ATTR_NAME)
        except OSError:
          continue

        route = segment.rpartition("--")[0]

        if uploaded:
          drive_ids[route] = uploaded.decode()
        else:
          pending.append((log_path, route))

    for log_path, route in pending:
      yield log_path, drive_ids.setdefault(route, secrets.token_hex(16))

  def upload(self, log_path, drive_id):
    data = self.compressor.compress(filter_log(log_path.read_bytes()))

    submission = frogpilot_api.post_json("/v1/telemetry", {
      "route_id": drive_id,
      "segment": int(log_path.parent.name.rpartition("--")[2]),
      "sha256": hashlib.sha256(data).hexdigest(),
      "size_bytes": len(data),
    }, self.session)

    if upload := submission.get("upload"):
      frogpilot_api.put_upload(upload, data, log_path.name, self.session)

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
      except (frogpilot_api.FrogPilotAPIError, requests.exceptions.RequestException):
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
