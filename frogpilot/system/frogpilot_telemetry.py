#!/usr/bin/env python3
import bz2
import errno
import hashlib
import json
import os
import psutil
import re
import time
import zstandard as zstd

from typing import NamedTuple

import capnp
import requests

import openpilot.system.sentry as sentry

from cereal import log, messaging
from opendbc.car.fw_query_definitions import StdQueries
from opendbc.car.vin import VIN_UNKNOWN
from openpilot.common.swaglog import cloudlog
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.system.loggerd.xattr_cache import getxattr, setxattr
from openpilot.tools.lib.helpers import RE

from openpilot.frogpilot.common import frogpilot_api

ERROR_BACKOFF = 60

COMPRESSION_LEVEL = 8

EXPECTED_MISSING_FILE_ERRNOS = {errno.ENOENT, errno.ENOTDIR}

SEGMENT_NAME_PATTERN = re.compile(rf"{RE.LOG_ID_V2}--\d+")
RLOG_NAMES = ("rlog.zst", "rlog")

TELEMETRY_XATTR = "user.frogpilot_telemetry"
TELEMETRY_XATTR_EMPTY = b"empty"
TELEMETRY_XATTR_REJECTED = b"rejected"
TELEMETRY_XATTR_UPLOADED = b"uploaded"
TELEMETRY_XATTR_TERMINAL = (b"1", TELEMETRY_XATTR_EMPTY, TELEMETRY_XATTR_REJECTED, TELEMETRY_XATTR_UPLOADED)

END_SENTINELS = (log.Sentinel.SentinelType.endOfSegment, log.Sentinel.SentinelType.endOfRoute)
ZSTD_MAGIC = b"\x28\xb5\x2f\xfd"

DEFER = object()
DROP = object()
SKIP_EMPTY = object()

CAN_ADDRESS_MASK = 0x1FFFFFFF
PANDA_BUS_OFFSET = 4

HARD_DROP_ADDRS = {
  "ford": {
    (None, 8): frozenset((0x41, 0x42, 0x47, 0x48, 0x49, 0x454)),
    (0, 8): frozenset((0x3F3, 0x462)),
    (1, 8): frozenset((0x105, 0x1F1, 0x1F3, 0x1F4)),
  },
  "gm": {
    (None, 8): frozenset((
      0x290000, 0x2A0000, 0x28C000, 0x2AA000,
      0x8F0000, 0x8F2000, 0x8F4000, 0x900000, 0x902000, 0x904000,
      0x906000, 0x908000, 0x90A000, 0x90C000, 0x90E000, 0x910000,
      0xEC4000, 0xEC8000,
    )),
    (0, 5): frozenset((0x160,)),
    (0, 8): frozenset((0x32A, 0x4E1, 0x514)),
    (1, 8): frozenset((0x483, 0x484, 0x485, 0x486, 0x487, 0x488, 0x741, 0x743)),
  },
  "hyundai": {
    (None, 5): frozenset((0x5FC,)),
    (None, 8): frozenset((0x5A1, 0x5FB)),
  },
  "tesla": {(0, 8): frozenset((0x3D8,))},
  "toyota": {(0, 8): frozenset((0x580, 0x581))},
}

PROJECTION_MASKS = {
  "ford": {
    (0x306, 0): bytes.fromhex("87 00 00 00 00 00 00 00"),
    (0x331, None): bytes.fromhex("ff ff 00 00 ff ff ff ff"),
    (0x332, None): bytes.fromhex("ff ff 00 00 ff ff ff ff"),
    (0x3F4, 0): bytes.fromhex("00 00 00 00 00 00 00 07"),
  },
  "gm": {
    (0x24A000, None): bytes.fromhex("ff 00 00 00 00 00 00"),
    (0x2C0000, None): bytes.fromhex("00 00 00 00 ff"),
    (0x304000, None): bytes.fromhex("00 00 ff"),
    (0x306000, None): bytes.fromhex("00 00 ff"),
    (0x308000, None): bytes.fromhex("00 00 ff"),
    (0x394000, None): bytes.fromhex("ff ff 00 00 ff"),
    (0x430000, None): bytes.fromhex("ff 00 00 00 00 00 00 00"),
    (0x792000, None): bytes.fromhex("00 00 00 00 ff"),
    (0x8EC000, None): bytes.fromhex("ff 00 00 00 00 00 00"),
    (0x920000, None): bytes.fromhex("ff 00 00 00 00"),
  },
  "hyundai": {(0x495, None): bytes.fromhex("ff ff 00 00 00 00 00 00")},
  "toyota": {(0x582, 0): bytes.fromhex("00 ff ff ff ff ff ff ff")},
}

KNOWN_VIN_ENDPOINTS = frozenset((
  (0x24B, 0, None), (0x64B, 0, None),
  (0x797, 0, None), (0x79A, 0, None),
  (0x74F, 0, None), (0x7B9, 0, None),
))
KNOWN_DIAGNOSTIC_ENDPOINTS = {
  "toyota": frozenset((
    (0x701, 0, None), (0x713, 0, None), (0x716, 0, None), (0x724, 0, None), (0x730, 0, None), (0x747, 0, None),
    (0x780, 0, None), (0x784, 0, None), (0x7A0, 0, None), (0x7A1, 0, None), (0x7B3, 0, None), (0x7C0, 0, None),
    (0x7C4, 0, None), (0x7D2, 0, None), (0x7E1, 0, None), (0x7E2, 0, None),
    (0x750, 0, 0x0F), (0x750, 0, 0x2C), (0x750, 0, 0x40),
    (0x750, 0, 0x5F), (0x750, 0, 0x6D), (0x750, 0, 0xC7),
  )),
}
SENSITIVE_DIAGNOSTIC_PREFIXES = (
  StdQueries.GM_VIN_RESPONSE, StdQueries.KWP_VIN_RESPONSE,
  StdQueries.OBD_VIN_RESPONSE, StdQueries.UDS_VIN_RESPONSE,
  b"\x2e\xf1\x8c", b"\x62\xf1\x8c",
  b"\x2e\xf1\x98", b"\x62\xf1\x98",
  b"\x2e\xf1\x9a", b"\x62\xf1\x9a",
)
SENSITIVE_DIAGNOSTIC_SERVICES = frozenset((0x27, 0x29, 0x67, 0x69))
TESLA_BOSCH_RADAR_FINGERPRINTS = frozenset(("TESLA_AP1_MODELS", "TESLA_AP2_MODELS"))
VIN_RUN_LENGTH = 6


class LogInfo(NamedTuple):
  vin: bytes
  brand: str
  car_fingerprint: str
  diagnostic_endpoints: frozenset
  diagnostic_streams: dict


def clear_filter_state(filter_state):
  filter_state.valid = False

  filter_state.value = []
  filter_state.std = []


def clear_xyz_measurement(measurement):
  measurement.valid = False

  measurement.x = 0
  measurement.y = 0
  measurement.z = 0
  measurement.xStd = 0
  measurement.yStd = 0
  measurement.zStd = 0


def anonymize_segment_name(segment):
  segment_number = segment.rsplit("--", 1)[-1]
  return f"segment-{segment_number}" if segment_number.isdigit() else "segment"


def carries_vin(data, vin):
  return bool(vin) and any(data[index:index + VIN_RUN_LENGTH] in vin for index in range(len(data) - VIN_RUN_LENGTH + 1))


def logical_can_bus(src):
  return src % PANDA_BUS_OFFSET


def diagnostic_sub_addresses(address, bus, log_info):
  sub_addresses = {
    sub_address for endpoint, endpoint_bus, sub_address in log_info.diagnostic_endpoints
    if endpoint == address and endpoint_bus == bus
  }

  if address == 0x7DF or 0x7E0 <= address <= 0x7EF or 0x18DA0000 <= address <= 0x18DBFFFF:
    sub_addresses.add(None)

  return sub_addresses


def is_sensitive_diagnostic_payload(payload):
  return (
    any(payload.startswith(prefix) for prefix in SENSITIVE_DIAGNOSTIC_PREFIXES)
    or bool(payload) and payload[0] in SENSITIVE_DIAGNOSTIC_SERVICES
  )


def is_sensitive_diagnostic_frame(address, bus, log_info, direction, data):
  for sub_address in diagnostic_sub_addresses(address, bus, log_info):
    pci_index = 1 if sub_address is not None else 0
    if len(data) <= pci_index or sub_address is not None and data[0] != sub_address:
      continue

    pci = data[pci_index]
    frame_type = pci >> 4
    stream_key = (direction, bus, address, sub_address)

    if frame_type == 0:
      payload_length = pci & 0xF
      payload_start = pci_index + 1
      if payload_length == 0:
        if len(data) <= payload_start:
          continue
        payload_length = data[payload_start]
        payload_start += 1

      if 0 < payload_length <= len(data) - payload_start:
        log_info.diagnostic_streams.pop(stream_key, None)
        return is_sensitive_diagnostic_payload(data[payload_start:payload_start + payload_length])

    elif frame_type == 1:
      if len(data) <= pci_index + 1:
        continue

      payload_length = ((pci & 0xF) << 8) | data[pci_index + 1]
      payload_start = pci_index + 2
      if payload_length == 0:
        if len(data) < pci_index + 6:
          continue
        payload_length = int.from_bytes(data[pci_index + 2:pci_index + 6], "big")
        payload_start = pci_index + 6

      payload = data[payload_start:]
      if payload_length <= len(payload):
        continue

      log_info.diagnostic_streams.pop(stream_key, None)
      if not is_sensitive_diagnostic_payload(payload):
        continue

      log_info.diagnostic_streams[stream_key] = (payload_length - len(payload), 1)
      return True

    elif frame_type == 2:
      stream = log_info.diagnostic_streams.get(stream_key)
      sequence = pci & 0xF
      if stream is None or sequence != stream[1]:
        log_info.diagnostic_streams.pop(stream_key, None)
        continue

      remaining = stream[0] - (len(data) - pci_index - 1)
      if remaining > 0:
        log_info.diagnostic_streams[stream_key] = (remaining, (sequence + 1) & 0xF)
      else:
        log_info.diagnostic_streams.pop(stream_key, None)
      return True

  return False


def sanitize_can_frame(can_frame, log_info, direction):
  address = can_frame.address & CAN_ADDRESS_MASK
  bus = logical_can_bus(can_frame.src)
  data = bytes(can_frame.dat)

  if not log_info.brand:
    return None

  drops = HARD_DROP_ADDRS.get(log_info.brand, {})
  if address in drops.get((None, len(data)), ()) or address in drops.get((bus, len(data)), ()):
    return None

  if log_info.brand == "volkswagen" and bus == 0 and address in (0x5D2, 0x6B4) and len(data) == 8 and data[0] & 0x3 != 3:
    return None

  if (
    direction == "can" and log_info.brand == "tesla" and log_info.car_fingerprint in TESLA_BOSCH_RADAR_FINGERPRINTS
    and address == 0x2B9 and bus == 1 and len(data) == 8 and data[0] in (0x10, 0x11, 0x12)
  ):
    return None

  if is_sensitive_diagnostic_frame(address, bus, log_info, direction, data) or carries_vin(data, log_info.vin):
    return None

  masks = PROJECTION_MASKS.get(log_info.brand, {})
  mask = masks.get((address, bus), masks.get((address, None)))
  if mask is not None and len(data) == len(mask):
    data = bytes(value & allowed for value, allowed in zip(data, mask, strict=True))

  return {"address": can_frame.address, "dat": data, "src": can_frame.src}


def filter_can(messages, log_info, direction):
  sanitized = (sanitize_can_frame(can_frame, log_info, direction) for can_frame in messages)
  return [can_frame for can_frame in sanitized if can_frame is not None]


def sanitize_alert_debug(event, log_info):
  event.alertDebug.alertText1 = ""
  event.alertDebug.alertText2 = ""


def sanitize_can(event, log_info):
  event.can = filter_can(event.can, log_info, "can")
  return bool(event.can)


def sanitize_car_control(event, log_info):
  orientation = event.carControl.orientationNED
  if len(orientation) >= 3:
    orientation[2] = 0.0


def sanitize_car_params(event, log_info):
  event.carParams.carVin = ""


def sanitize_controls_state(event, log_info):
  event.controlsState.alertText1DEPRECATED = ""
  event.controlsState.alertText2DEPRECATED = ""


def sanitize_device_state(event, log_info):
  event.deviceState.init("networkInfo")
  event.deviceState.init("networkStats")
  event.deviceState.lastAthenaPingTime = 0


def sanitize_frogpilot_plan(event, log_info):
  event.frogpilotPlan.weatherDaytime = False


def sanitize_frogpilot_selfdrive_state(event, log_info):
  event.frogpilotSelfdriveState.alertText1 = ""
  event.frogpilotSelfdriveState.alertText2 = ""


def sanitize_live_pose(event, log_info):
  clear_xyz_measurement(event.livePose.orientationNED)
  clear_filter_state(event.livePose.debugFilterState)


def sanitize_model(event, log_info):
  event.modelV2.rawPredictions = b""


def sanitize_selfdrive_state(event, log_info):
  event.selfdriveState.alertText1 = ""
  event.selfdriveState.alertText2 = ""


def sanitize_sendcan(event, log_info):
  event.sendcan = filter_can(event.sendcan, log_info, "sendcan")
  return bool(event.sendcan)


SANITIZERS = {
  "alertDebug": sanitize_alert_debug,
  "androidLog": DROP,
  "audioFeedback": DROP,
  "boot": DROP,
  "can": sanitize_can,
  "carControl": sanitize_car_control,
  "carParams": sanitize_car_params,
  "controlsState": sanitize_controls_state,
  "customReservedRawData0": DROP,
  "customReservedRawData1": DROP,
  "customReservedRawData2": DROP,
  "deviceState": sanitize_device_state,
  "driverCameraState": DROP,
  "driverEncodeData": DROP,
  "driverEncodeIdx": DROP,
  "driverMonitoringState": DROP,
  "driverStateV2": DROP,
  "errorLogMessage": DROP,
  "frogpilotPlan": sanitize_frogpilot_plan,
  "frogpilotSelfdriveState": sanitize_frogpilot_selfdrive_state,
  "gnssMeasurements": DROP,
  "gpsLocation": DROP,
  "gpsLocationExternal": DROP,
  "gpsNMEA": DROP,
  "initData": DROP,
  "livePose": sanitize_live_pose,
  "livestreamDriverEncodeData": DROP,
  "livestreamDriverEncodeIdx": DROP,
  "livestreamRoadEncodeData": DROP,
  "livestreamRoadEncodeIdx": DROP,
  "livestreamWideRoadEncodeData": DROP,
  "livestreamWideRoadEncodeIdx": DROP,
  "logMessage": DROP,
  "magnetometer": DROP,
  "mapdExtendedOut": DROP,
  "mapdIn": DROP,
  "mapdOut": DROP,
  "mapRenderState": DROP,
  "modelV2": sanitize_model,
  "navInstruction": DROP,
  "navRoute": DROP,
  "navThumbnail": DROP,
  "qRoadEncodeData": DROP,
  "qRoadEncodeIdx": DROP,
  "qcomGnss": DROP,
  "rawAudioData": DROP,
  "roadCameraState": DROP,
  "roadEncodeData": DROP,
  "roadEncodeIdx": DROP,
  "selfdriveState": sanitize_selfdrive_state,
  "sendcan": sanitize_sendcan,
  "thumbnail": DROP,
  "ubloxGnss": DROP,
  "ubloxRaw": DROP,
  "uploaderState": DROP,
  "wideRoadCameraState": DROP,
  "wideRoadEncodeData": DROP,
  "wideRoadEncodeIdx": DROP,
}


def car_fw_diagnostic_endpoints(brand, car_fw):
  endpoints = set(KNOWN_VIN_ENDPOINTS | KNOWN_DIAGNOSTIC_ENDPOINTS.get(brand, frozenset()))
  for fw in car_fw:
    sub_address = fw.subAddress or None
    bus = logical_can_bus(fw.bus)
    for address in (fw.address, fw.responseAddress):
      if address:
        endpoints.add((address & CAN_ADDRESS_MASK, bus, sub_address))

  return frozenset(endpoints)


def find_log_info(payload):
  try:
    for event in log.Event.read_multiple_bytes(payload):
      try:
        if event.which() == "carParams":
          car_params = event.carParams
          vin = car_params.carVin
          return LogInfo(
            vin.encode() if vin != VIN_UNKNOWN else b"",
            car_params.brand,
            car_params.carFingerprint,
            car_fw_diagnostic_endpoints(car_params.brand, car_params.carFw),
            {},
          )
      except capnp.KjException:
        continue
  except capnp.KjException:
    pass

  return LogInfo(b"", "", "", KNOWN_VIN_ENDPOINTS, {})


def read_rlog(rlog):
  with open(rlog, "rb") as log_file:
    magic = log_file.read(4)
    log_file.seek(0)
    if magic == ZSTD_MAGIC:
      return zstd.ZstdDecompressor().stream_reader(log_file).read()

    payload = log_file.read()
    return bz2.decompress(payload) if payload.startswith(b"BZh") else payload


def is_uploadable(service):
  return not service.endswith("DEPRECATED") and SANITIZERS.get(service) is not DROP


def strip_rlog(rlog):
  payload = read_rlog(rlog)
  log_info = find_log_info(payload)

  scrubbed_messages = []
  try:
    for event in log.Event.read_multiple_bytes(payload):
      try:
        service = event.which()
      except capnp.KjException:
        continue

      if service == "sentinel" and event.sentinel.type in END_SENTINELS:
        return scrubbed_messages, True

      if not is_uploadable(service):
        continue

      scrubbed_event = event.as_builder()
      sanitizer = SANITIZERS.get(service)
      try:
        if sanitizer is not None and sanitizer(scrubbed_event, log_info) is False:
          continue
      except Exception:
        continue

      scrubbed_messages.append(scrubbed_event.to_bytes())
  except capnp.KjException:
    pass

  return scrubbed_messages, False


def get_rlog_path(segment_directory):
  if os.path.exists(os.path.join(segment_directory, "rlog.lock")):
    return None

  for name in RLOG_NAMES:
    rlog = os.path.join(segment_directory, name)
    if os.path.isfile(rlog):
      return rlog

  return None


def should_skip_log(rlog):
  try:
    return getxattr(rlog, TELEMETRY_XATTR) in TELEMETRY_XATTR_TERMINAL
  except OSError:
    return True


class FrogPilotTelemetry:
  def __init__(self):
    self.log_roots = list(dict.fromkeys((Paths.log_root(), Paths.log_root(HD=True), Paths.log_root(konik=True))))

    self.session = requests.Session()
    self.sm = messaging.SubMaster(["deviceState"])

  def at_home(self):
    device_state = self.sm["deviceState"]
    network_type = device_state.networkType
    return not device_state.started and not device_state.networkMetered and network_type in (log.DeviceState.NetworkType.ethernet, log.DeviceState.NetworkType.wifi)

  def get_pending_logs(self):
    pending_logs = []
    for log_root in self.log_roots:
      for segment in listdir_by_creation(log_root):
        if not SEGMENT_NAME_PATTERN.fullmatch(segment):
          continue

        rlog = get_rlog_path(os.path.join(log_root, segment))
        if rlog is not None and not should_skip_log(rlog):
          pending_logs.append(rlog)

    return pending_logs

  def update(self):
    self.sm.update(0)
    if not self.at_home():
      return

    api_info = frogpilot_api.get_info()

    for rlog in self.get_pending_logs():
      self.sm.update(0)
      if not self.at_home():
        return

      try:
        response = self.upload(rlog, api_info)

        if response is DEFER:
          continue
        if response is SKIP_EMPTY:
          setxattr(rlog, TELEMETRY_XATTR, TELEMETRY_XATTR_EMPTY)
          continue
        if response is not None and 200 <= response.status_code < 300:
          setxattr(rlog, TELEMETRY_XATTR, TELEMETRY_XATTR_UPLOADED)
          continue
        if response is not None and response.status_code in (400, 413, 415, 422):
          setxattr(rlog, TELEMETRY_XATTR, TELEMETRY_XATTR_REJECTED)
          cloudlog.error(f"Telemetry upload rejected with HTTP status {response.status_code}")
          continue
      except OSError as error:
        if error.errno not in EXPECTED_MISSING_FILE_ERRNOS:
          sentry.capture_exception(error, crash_log=False)
        continue

      return

  def upload(self, rlog, api_info):
    scrubbed_messages, complete = strip_rlog(rlog)
    if not complete and os.path.getmtime(rlog) >= psutil.boot_time():
      return DEFER
    if not scrubbed_messages:
      return SKIP_EMPTY

    segment = os.path.basename(os.path.dirname(rlog))
    anonymized_segment = anonymize_segment_name(segment)
    payload = zstd.ZstdCompressor(level=COMPRESSION_LEVEL).compress(b"".join(scrubbed_messages))
    idempotency_key = hashlib.sha256(anonymized_segment.encode() + payload).hexdigest()

    data = {
      "build_metadata": json.dumps(api_info["build_metadata"]),
      "device": api_info["device_type"],
      "segment": anonymized_segment,
    }
    files = {"log": (f"{anonymized_segment}.zst", payload, "application/zstd")}

    return frogpilot_api.post(
      "/telemetry",
      session=self.session,
      timeout=60,
      data=data,
      files=files,
      headers={"Idempotency-Key": idempotency_key, "User-Agent": "frogpilot-api/1.0"},
    )


def main():
  frogpilot_telemetry = FrogPilotTelemetry()

  while True:
    try:
      frogpilot_telemetry.update()
    except Exception as error:
      sentry.capture_exception(error)
    time.sleep(ERROR_BACKOFF)


if __name__ == "__main__":
  main()
