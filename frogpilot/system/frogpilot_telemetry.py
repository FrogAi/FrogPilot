#!/usr/bin/env python3
import capnp
import hashlib
import hmac
import json
import requests
import secrets
import shutil
import time
import zstandard

from pathlib import Path

import cereal.messaging as messaging

from cereal import CEREAL_PATH, log
from openpilot.common.api import get_key_pair
from openpilot.common.params import Params
from openpilot.system import sentry
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.system.loggerd.xattr_cache import getxattr, setxattr

from openpilot.frogpilot.common import frogpilot_api, frogpilot_paths, frogpilot_utilities, frogpilot_variables

COMPRESSION_LEVEL = 10

DRIVE_ATTR_NAME = "user.frogpilot_telemetry_drive"
LEGACY_UPLOAD_ATTR_VALUE = b"1"

SCHEMA_FILES = ("car.capnp", "custom.capnp", "include/c++.capnp", "legacy.capnp", "log.capnp")

EXCLUDED_CAN_ADDRESSES = {
  "chrysler": frozenset((0x3E0, 0x4D3)),
  "ford": frozenset((0x5, 0x6, 0x84, 0x21E, 0x306, 0x3F3, 0x3F4, 0x40A, 0x454, 0x462, 0x463, 0x464, 0x465, 0x466, 0x467, 0x473, 0x47E)),
  "gm": frozenset((0x137, 0x260, 0x261, 0x262, 0x264, 0x32A, 0x4E1, 0x514, 0x741, 0x743)),
  "honda": frozenset((0x40C, 0x651)),
  "hyundai": frozenset((0x2D2, 0x332, 0x333, 0x374)),
  "mazda": frozenset((0x40A, 0x439)),
  "psa": frozenset((0x492, 0x4B2)),
  "rivian": frozenset((0x342, 0x349)),
  "subaru": frozenset((0x6A1, 0x6BC)),
  "tesla": frozenset((0x4F, 0x309, 0x324, 0x3B9, 0x3D8, 0x405, 0x528, 0x718)),
  "toyota": frozenset((0x3D2, 0x413, 0x580, 0x581, 0x582, 0x675, 0x676)),
  "volkswagen": frozenset((0x36F, 0x374, 0x378, 0x37B, 0x486, 0x5D2, 0x6B2, 0x6B4, 0x7D0, 0x1A5555B8, 0x1A555628)),
}

EXCLUDED_MESSAGE_TYPES = frozenset((
  "accelerometer2",
  "androidLog",
  "boot",
  "cameraOdometry",
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
  "frogpilotProcessState",
  "frogpilotSignReading",
  "frogpilotUIEvent",
  "frogpilotUIRequest",
  "gnssMeasurements",
  "gpsLocation",
  "gpsLocationExternal",
  "gpsNMEA",
  "gyroscope2",
  "lightSensor",
  "livePose",
  "liveTracks",
  "livestreamDriverEncodeData",
  "livestreamDriverEncodeIdx",
  "livestreamRoadEncodeData",
  "livestreamRoadEncodeIdx",
  "livestreamWideRoadEncodeData",
  "livestreamWideRoadEncodeIdx",
  "logMessage",
  "magnetometer",
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
  "roadCameraState",
  "roadEncodeData",
  "roadEncodeIdx",
  "temperatureSensor",
  "temperatureSensor2",
  "testJoystick",
  "thePondRequest",
  "thumbnail",
  "ubloxGnss",
  "ubloxRaw",
  "uiDebug",
  "uiPlan",
  "uploaderState",
  "userFlag",
  "wideRoadCameraState",
  "wideRoadEncodeData",
  "wideRoadEncodeIdx",
))

REMOVED_FIELDS = {
  "carParams": ("carVin",),
  "liveLocationKalman": (
    "accelerationCalibrated",
    "accelerationDevice",
    "angularVelocityDevice",
    "calibratedOrientationECEF",
    "filterState",
    "orientationECEF",
    "positionECEF",
    "positionGeodetic",
    "velocityCalibrated",
    "velocityDevice",
    "velocityECEF",
    "velocityNED",
  ),
}

REPLACEMENT_FIELDS = {
  "carControl": {"orientationNED.2": 0},
  "controlsState": {"alertText1": "", "alertText2": ""},
  "frogpilotControlsState": {"alertText1": "", "alertText2": ""},
  "frogpilotPlan": {"gpsBearing": 0, "themeUpdateCount": 0, "togglesUpdateTime": 0, "weatherDaytime": False, "wheelImageUpdateCount": 0},
  "liveLocationKalman": {
    "calibratedOrientationNED.value.2": 0,
    "excessiveResets": False,
    "gpsTimeOfWeek": 0,
    "gpsWeek": 0,
    "orientationNED.value.2": 0,
    "timeSinceReset": 0,
    "timeToFirstFix": 0,
    "unixTimestampMillis": 0,
  },
  "modelV2": {"leads": [], "rawPredictions": b""},
}

WHITELIST_FIELDS = {
  "driverMonitoringState": (),
  "initData": ("gitCommit", "version"),
}

WHITELIST_PARAMS = ("Model", "ModelRandomizer", "RandomizedModel")

def get_car_make(data, schema, default_car_make):
  try:
    for event in schema.Event.read_multiple_bytes(data):
      try:
        if event.which() == "carParams":
          return event.carParams.carName
      except capnp.KjException:
        continue
  except capnp.KjException:
    pass

  return default_car_make

def filter_log(data, schema, default_car_make):
  car_fingerprint = None

  fingerprinting = False

  excluded_can_addresses = EXCLUDED_CAN_ADDRESSES.get(get_car_make(data, schema, default_car_make))

  filtered_data = bytearray()

  try:
    for event in schema.Event.read_multiple_bytes(data):
      try:
        which = event.which()
      except capnp.KjException:
        continue

      if which == "sentinel" and event.sentinel.type == "startOfRoute":
        fingerprinting = True
      elif which == "carParams":
        car_fingerprint = event.carParams.carFingerprint
        fingerprinting = False
      elif which == "carState":
        fingerprinting = False

      if which in EXCLUDED_MESSAGE_TYPES or which.endswith("DEPRECATED"):
        continue

      if which in ("can", "sendcan") and fingerprinting:
        continue

      builder = event.as_builder()

      if which == "can" and excluded_can_addresses:
        kept_frames = [frame for frame in event.can if frame.address not in excluded_can_addresses]

        if len(kept_frames) != len(event.can):
          builder.can = kept_frames

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
          elif hasattr(target, field):
            setattr(target, field, value)

      elif which in WHITELIST_FIELDS:
        kept = builder.init(which)
        source = getattr(event, which)

        for field in WHITELIST_FIELDS[which]:
          setattr(kept, field, getattr(source, field))

        if which == "initData":
          kept.params.entries = [{"key": entry.key, "value": entry.value} for entry in source.params.entries if entry.key in WHITELIST_PARAMS]

      filtered_data.extend(builder.to_bytes())
  except capnp.KjException:
    pass

  return filtered_data, car_fingerprint

class FrogPilotTelemetry:
  def __init__(self):
    self.params = Params()

    self.log_roots = frogpilot_utilities.get_log_roots()

    self.frogpilot_api = frogpilot_api.FrogPilotAPI(self.params)

    self.private_key = get_key_pair()[1].encode()

    self.git_commit = self.params.get("GitCommit", encoding="utf-8")
    self.schemas = {self.git_commit: log}

    schema_path = frogpilot_paths.TELEMETRY_SCHEMAS_PATH / self.git_commit
    if not schema_path.is_dir():
      staging_path = frogpilot_paths.TELEMETRY_SCHEMAS_PATH / f".{self.git_commit}"
      for name in SCHEMA_FILES:
        (staging_path / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(Path(CEREAL_PATH, name), staging_path / name)
      staging_path.rename(schema_path)

    self.retry_at = 0

    self.compressor = zstandard.ZstdCompressor(level=COMPRESSION_LEVEL)

    self.session = requests.Session()

    self.sm = messaging.SubMaster(["deviceState"])

    self.car_fingerprint = None
    self.car_fingerprint_route = None

    self.route = self.params.get("CurrentRoute", encoding="utf-8")
    self.first_segment = None

    if self.route is not None:
      for log_root in self.log_roots:
        for lock_path in Path(log_root).glob(f"{self.route}--*/rlog.lock"):
          self.first_segment = int(lock_path.parent.name.rpartition("--")[2]) + 1

  def mark_shared_logs(self):
    if self.route is not None and self.first_segment is not None:
      for log_root in self.log_roots:
        for log_path in Path(log_root).glob(f"{self.route}--*/rlog"):
          if log_path.with_suffix(".lock").exists():
            continue

          if int(log_path.parent.name.rpartition("--")[2]) < self.first_segment:
            continue

          if getxattr(log_path, frogpilot_variables.UPLOAD_ATTR_NAME) is None:
            setxattr(log_path, frogpilot_variables.UPLOAD_ATTR_NAME, frogpilot_variables.UPLOAD_PENDING)

    route = self.params.get("CurrentRoute", encoding="utf-8")

    if route != self.route:
      self.route = route
      self.first_segment = 0

  def pending_logs(self):
    drive_ids = {}
    pending = []

    for log_root in self.log_roots:
      for segment in listdir_by_creation(log_root):
        log_path = Path(log_root, segment, "rlog")

        if not log_path.is_file():
          continue

        try:
          drive_id = getxattr(log_path, DRIVE_ATTR_NAME)
          uploaded = getxattr(log_path, frogpilot_variables.UPLOAD_ATTR_NAME)
        except OSError:
          continue

        route = segment.rpartition("--")[0]

        if uploaded == frogpilot_variables.UPLOAD_PENDING:
          if drive_id and drive_id != LEGACY_UPLOAD_ATTR_VALUE:
            drive_ids.setdefault(route, drive_id.decode())

          pending.append((log_path, route, drive_id))
        elif uploaded and uploaded != LEGACY_UPLOAD_ATTR_VALUE:
          drive_ids[route] = uploaded.decode()

    for log_path, route, existing_id in pending:
      if route not in drive_ids:
        drive_ids[route] = secrets.token_hex(16)

      drive_id = drive_ids[route].encode()

      if existing_id != drive_id:
        try:
          setxattr(log_path, DRIVE_ATTR_NAME, drive_id)
        except FileNotFoundError:
          continue

      if not log_path.with_suffix(".lock").is_file():
        yield log_path, drive_ids[route]

  def pending_captures(self):
    for capture_path in sorted(frogpilot_paths.SPEED_LIMIT_CAPTURES_PATH.glob("*.hevc")):
      if getxattr(capture_path, frogpilot_variables.UPLOAD_ATTR_NAME) == frogpilot_variables.UPLOAD_PENDING:
        yield capture_path

  def schema(self, git_commit):
    if git_commit not in self.schemas:
      schema_path = frogpilot_paths.TELEMETRY_SCHEMAS_PATH / git_commit / "log.capnp"
      self.schemas[git_commit] = capnp.SchemaParser().load(str(schema_path), imports=[str(schema_path.parent)])
    return self.schemas[git_commit]

  def upload(self, log_path, drive_id):
    data = log_path.read_bytes()

    git_commit = next(log.Event.read_multiple_bytes(data)).initData.gitCommit
    schema = self.schema(git_commit)

    filtered_data, car_fingerprint = filter_log(data, schema, frogpilot_variables.get_frogpilot_toggles().car_make)
    data = self.compressor.compress(filtered_data)

    route = log_path.parent.name.rpartition("--")[0]
    if car_fingerprint is not None:
      self.car_fingerprint = car_fingerprint
      self.car_fingerprint_route = route
    elif route == self.car_fingerprint_route:
      car_fingerprint = self.car_fingerprint

    submission = self.frogpilot_api.post_json("/v1/telemetry", {
      "car_fingerprint": car_fingerprint,
      "route_id": drive_id,
      "segment": int(log_path.parent.name.rpartition("--")[2]),
      "sha256": hashlib.sha256(data).hexdigest(),
      "size_bytes": len(data),
    }, self.session)

    if upload := submission.get("upload"):
      self.frogpilot_api.put_upload(upload, data, log_path.name, "telemetry", self.session)

  def upload_capture(self, capture_path):
    data = capture_path.read_bytes()

    submission = self.frogpilot_api.post_json("/v1/captures", {
      **json.loads(capture_path.with_suffix(".json").read_text()),
      "capture_id": self.upload_id(capture_path.stem),
      "sha256": hashlib.sha256(data).hexdigest(),
      "size_bytes": len(data),
    }, self.session)

    if upload := submission.get("upload"):
      self.frogpilot_api.put_upload(upload, data, capture_path.name, "capture", self.session)

  def upload_id(self, name):
    return hmac.new(self.private_key, name.encode(), "sha256").hexdigest()[:32]

  def can_upload(self):
    self.sm.update(0)

    if self.sm["deviceState"].started:
      return False

    return frogpilot_utilities.on_unmetered_network(self.sm["deviceState"])

  def update(self):
    self.mark_shared_logs()

    if time.monotonic() < self.retry_at or not self.frogpilot_api.get_token() or not self.can_upload():
      return

    for log_path, drive_id in self.pending_logs():
      try:
        self.upload(log_path, drive_id)
        setxattr(log_path, frogpilot_variables.UPLOAD_ATTR_NAME, drive_id.encode())
      except (frogpilot_api.FrogPilotAPIError, requests.exceptions.RequestException):
        self.retry_at = time.monotonic() + 60 * 60
        return
      except FileNotFoundError:
        pass
      except Exception as error:
        sentry.capture_exception(error, crash_log=False)

      if not self.can_upload():
        return

    self.schemas = {self.git_commit: log}

    for capture_path in self.pending_captures():
      try:
        self.upload_capture(capture_path)

        if frogpilot_utilities.is_FrogsGoMoo():
          setxattr(capture_path, frogpilot_variables.UPLOAD_ATTR_NAME, frogpilot_variables.UPLOAD_DONE)
        else:
          capture_path.unlink()
          capture_path.with_suffix(".json").unlink()
      except (frogpilot_api.FrogPilotAPIError, requests.exceptions.RequestException):
        self.retry_at = time.monotonic() + 60 * 60
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
      sentry.capture_exception(error, crash_log=False)

    time.sleep(60)


if __name__ == "__main__":
  main()
