#!/usr/bin/env python3
import capnp
import ctypes
import gc
import hashlib
import hmac
import json
import requests
import shutil
import time
import zstandard

from pathlib import Path

import cereal.messaging as messaging

from cereal import CEREAL_PATH, log
from openpilot.common.api import get_key_pair
from openpilot.common.params import Params
from openpilot.common.utils import LOG_COMPRESSION_LEVEL
from openpilot.system import sentry
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.system.loggerd.xattr_cache import getxattr, setxattr

from openpilot.frogpilot.common import frogpilot_api, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.system.speed_limit_capture import CAPTURES_PATH

LOG_ROOTS = [Path("/data/media/0", name) for name in ("realdata", "realdata_HD", "realdata_konik")]

MAX_LOG_SIZE = 256 * 1024 * 1024

SCHEMA_FILES = ("car.capnp", "custom.capnp", "include/c++.capnp", "legacy.capnp", "log.capnp")
SCHEMAS_PATH = Path("/data/telemetry_schemas")

UPLOAD_DONE = b"done"

EXCLUDED_MESSAGE_TYPES = frozenset((
  "androidLog",
  "audioFeedback",
  "bookmarkButton",
  "clocks",
  "customReservedRawData0",
  "customReservedRawData1",
  "customReservedRawData2",
  "deviceState",
  "driverCameraState",
  "driverEncodeIdx",
  "driverStateV2",
  "drivingModelData",
  "errorLogMessage",
  "frogpilotDeviceState",
  "gnssMeasurements",
  "gpsNMEA",
  "lightSensor",
  "logMessage",
  "magnetometer",
  "managerState",
  "mapdExtendedOut",
  "mapdIn",
  "navInstruction",
  "navRoute",
  "navThumbnail",
  "peripheralState",
  "procLog",
  "qRoadEncodeIdx",
  "qcomGnss",
  "rawAudioData",
  "roadCameraState",
  "roadEncodeIdx",
  "soundPressure",
  "temperatureSensor",
  "testJoystick",
  "thumbnail",
  "touch",
  "ubloxGnss",
  "ubloxRaw",
  "uiDebug",
  "uploaderState",
  "wideRoadCameraState",
  "wideRoadEncodeIdx",
))

REMOVED_FIELDS = {
  "carParams": ("carVin",),
  "livePose": ("debugFilterState",),
}

REPLACEMENT_FIELDS = {
  "carControl": {"orientationNED.2": 0},
  "frogpilotPlan": {"gpsBearing": 0, "slcMapboxWayId": 0},
  "gpsLocation": {"altitude": 0, "bearingDeg": 0, "latitude": 0, "longitude": 0, "unixTimestampMillis": 0, "vNED": []},
  "gpsLocationExternal": {"altitude": 0, "bearingDeg": 0, "latitude": 0, "longitude": 0, "unixTimestampMillis": 0, "vNED": []},
  "livePose": {"orientationNED.z": 0},
  "longitudinalPlan": {"processingDelay": 0},
  "mapdOut": {"roadName": "", "wayId": 0, "wayName": "", "wayRef": ""},
  "modelV2": {"rawPredictions": b""},
}

WHITELIST_FIELDS = {
  "driverMonitoringState": ("awarenessStatus", "isRHD"),
  "initData": ("deviceType", "dirty", "gitCommit", "gitCommitDate", "osVersion", "version"),
}

def filter_log(data, schema):
  car_fingerprint = None

  fingerprinting = False

  filtered_data = bytearray()

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

    filtered_data.extend(builder.to_bytes())

  return car_fingerprint, filtered_data

class FrogPilotTelemetry:
  def __init__(self):
    self.params = Params()

    self.frogpilot_api = frogpilot_api.FrogPilotAPI(self.params)

    self.private_key = get_key_pair()[1].encode()

    self.git_commit = self.params.get("GitCommit")
    self.schemas = {self.git_commit: log}

    schema_path = SCHEMAS_PATH / self.git_commit
    if not schema_path.is_dir():
      staging_path = SCHEMAS_PATH / f".{self.git_commit}"
      for name in SCHEMA_FILES:
        (staging_path / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(Path(CEREAL_PATH, name), staging_path / name)
      staging_path.rename(schema_path)

    self.retry_at = 0

    self.car_fingerprint = None
    self.car_fingerprint_route = None

    self.decompressor = zstandard.ZstdDecompressor()

    self.session = requests.Session()

    self.sm = messaging.SubMaster(["deviceState"])

    self.route = self.params.get("CurrentRoute")
    self.first_segment = None

    if self.route is not None:
      for log_root in LOG_ROOTS:
        for lock_path in log_root.glob(f"{self.route}--*/rlog.lock"):
          self.first_segment = int(lock_path.parent.name.rpartition("--")[2]) + 1

  def mark_shared_logs(self):
    if self.route is not None and self.first_segment is not None:
      for log_root in LOG_ROOTS:
        for segment_path in log_root.glob(f"{self.route}--*"):
          finished = not (segment_path / "rlog.lock").exists()
          if finished and int(segment_path.name.rpartition("--")[2]) >= self.first_segment and getxattr(segment_path, frogpilot_variables.UPLOAD_ATTR_NAME) is None:
            setxattr(segment_path, frogpilot_variables.UPLOAD_ATTR_NAME, frogpilot_variables.UPLOAD_PENDING)

    route = self.params.get("CurrentRoute")

    if route != self.route:
      self.route = route
      self.first_segment = 0

  def pending(self):
    paths = [Path(log_root, segment) for log_root in LOG_ROOTS for segment in listdir_by_creation(log_root)]
    paths += sorted(CAPTURES_PATH.glob("*.hevc"))

    for path in paths:
      try:
        pending = getxattr(path, frogpilot_variables.UPLOAD_ATTR_NAME) == frogpilot_variables.UPLOAD_PENDING
      except OSError:
        continue

      if pending:
        yield path

  def schema(self, git_commit):
    if git_commit not in self.schemas:
      schema_path = SCHEMAS_PATH / git_commit / "log.capnp"
      self.schemas[git_commit] = capnp.SchemaParser().load(str(schema_path), imports=[str(schema_path.parent)])
    return self.schemas[git_commit]

  def upload_id(self, name):
    return hmac.new(self.private_key, name.encode(), "sha256").hexdigest()[:32]

  def upload(self, path):
    if path.suffix == ".hevc":
      data = path.read_bytes()

      descriptor = {**json.loads(path.with_suffix(".json").read_text()), "capture_id": self.upload_id(path.stem)}
      endpoint = "/v1/captures"
    else:
      try:
        raw_data = self.decompressor.decompress((path / "rlog.zst").read_bytes(), max_output_size=MAX_LOG_SIZE)
        git_commit = next(log.Event.read_multiple_bytes(raw_data)).initData.gitCommit
        schema = self.schema(git_commit)
      except (OSError, StopIteration, capnp.KjException, zstandard.ZstdError):
        return

      car_fingerprint, filtered_data = filter_log(raw_data, schema)
      data = zstandard.ZstdCompressor(level=LOG_COMPRESSION_LEVEL).compress(filtered_data)

      route, _, segment = path.name.rpartition("--")
      if car_fingerprint is not None:
        self.car_fingerprint, self.car_fingerprint_route = car_fingerprint, route
      elif route == self.car_fingerprint_route:
        car_fingerprint = self.car_fingerprint

      descriptor = {"car_fingerprint": car_fingerprint, "route_id": self.upload_id(route), "segment": int(segment)}
      endpoint = "/v1/telemetry"

    result = self.frogpilot_api.post_json(endpoint, {**descriptor, "sha256": hashlib.sha256(data).hexdigest(), "size_bytes": len(data)}, self.session)

    if result:
      if not self.can_upload():
        return False

      self.frogpilot_api.put_upload(result["upload"], data, path.name, self.session)

  def can_upload(self):
    self.sm.update(0)

    if self.sm["deviceState"].started:
      return False

    return frogpilot_utilities.is_unmetered_network(self.sm["deviceState"])

  def update(self):
    self.mark_shared_logs()

    if time.monotonic() < self.retry_at or not self.can_upload():
      return

    for path in self.pending():
      if not self.can_upload():
        return

      try:
        if self.upload(path) is False:
          return
      except (frogpilot_api.FrogPilotAPIError, requests.exceptions.RequestException) as error:
        self.retry_at = time.monotonic() + 60 * 60
        print(f"Error uploading telemetry ({error})")
        return

      if path.suffix == ".hevc" and not frogpilot_utilities.is_FrogsGoMoo():
        path.unlink()
        path.with_suffix(".json").unlink()
      else:
        setxattr(path, frogpilot_variables.UPLOAD_ATTR_NAME, UPLOAD_DONE)

    for schema_path in SCHEMAS_PATH.iterdir():
      if schema_path.name != self.git_commit:
        shutil.rmtree(schema_path)

    self.schemas = {self.git_commit: log}


def main():
  frogpilot_telemetry = FrogPilotTelemetry()

  libc = ctypes.CDLL("libc.so.6")

  while True:
    try:
      frogpilot_telemetry.update()
    except Exception as error:
      sentry.capture_exception(error)

    gc.collect()
    libc.malloc_trim(0)

    time.sleep(60)


if __name__ == "__main__":
  main()
