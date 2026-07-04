#!/usr/bin/env python3
import bz2
import io
import json
import os
import random
import re
import time

import capnp
import requests
import zstandard as zstd

import cereal.messaging as messaging

from cereal import log as capnp_log
from openpilot.system.hardware.hw import Paths
from openpilot.system.loggerd.uploader import listdir_by_creation
from openpilot.system.loggerd.xattr_cache import getxattr, setxattr
from openpilot.tools.lib.helpers import RE

from openpilot.frogpilot.common.frogpilot_utilities import get_frogpilot_api_info
from openpilot.frogpilot.common.frogpilot_variables import FROGPILOT_API

NetworkType = capnp_log.DeviceState.NetworkType

RLOG_NAMES = ("rlog.zst", "rlog")

SEGMENT_NAME_PATTERN = re.compile(rf"^{RE.LOG_ID_V2}--\d+$")

TELEMETRY_ATTR_NAME = "user.frogpilot_telemetry"
TELEMETRY_ATTR_VALUE = b"1"

ZSTD_MAGIC = b"\x28\xb5\x2f\xfd"

KEEP_TYPES = frozenset({
  "accelerometer", "accelerometer2", "cameraOdometry", "can", "carControl", "carOutput", "carParams", "carState",
  "controlsState", "frogpilotCarControl", "frogpilotCarParams", "frogpilotCarState", "frogpilotControlsState",
  "frogpilotModelV2", "frogpilotOnroadEvents", "frogpilotPlan", "frogpilotRadarState", "gyroscope", "gyroscope2",
  "liveCalibration", "liveDelay", "liveParameters", "livePose", "liveTorqueParameters", "liveTracks",
  "longitudinalPlan", "modelV2", "onroadEvents", "pandaStates", "peripheralState", "radarState", "sendcan",
})

VIN_ADDRESSES = frozenset(range(0x7DF, 0x7F0)) | frozenset((0x64B, 0x760, 0x768, 0x79A, 0x7C6, 0x7CE))
VIN_PATTERNS = (b"\x09\x02", b"\x22\xf1\x90", b"\x49\x02\x01", b"\x5a\x90", b"\x61\x81", b"\x62\xf1\x90")


def is_vin_frame(frame):
  if frame.address in VIN_ADDRESSES or 0x18DA0000 <= frame.address <= 0x18DBFFFF:
    return True
  return any(pattern in bytes(frame.dat) for pattern in VIN_PATTERNS)


def anonymize_segment_name(segment):
  number = segment.rsplit("--", 1)[-1]
  return f"segment-{number}" if number.isdigit() else "segment"


def sanitize(builder, which):
  if which in ("can", "sendcan"):
    setattr(builder, which, [
      {"address": frame.address, "dat": bytes(frame.dat), "src": frame.src}
      for frame in getattr(builder, which) if not is_vin_frame(frame)
    ])
  elif which == "carControl":
    builder.carControl.orientationNED = []
  elif which == "carParams":
    builder.carParams.carVin = ""
    builder.carParams.init("carFw", 0)
  elif which == "frogpilotPlan":
    target = builder.frogpilotPlan
    for field in ("cscSpeed", "laneWidthLeft", "laneWidthRight", "roadCurvature", "slcMapSpeedLimit", "slcMapboxSpeedLimit",
                  "slcNextSpeedLimit", "slcOverriddenSpeed", "slcSpeedLimit", "slcSpeedLimitOffset", "unconfirmedSlcSpeedLimit", "weatherId"):
      setattr(target, field, 0)
    target.cscControllingSpeed = False
    target.slcSpeedLimitSource = ""
    target.speedLimitChanged = False
    target.weatherDaytime = False
  elif which == "livePose":
    builder.livePose.init("filterState")
    builder.livePose.init("orientationNED")


def already_uploaded(segment):
  try:
    return getxattr(segment, TELEMETRY_ATTR_NAME) == TELEMETRY_ATTR_VALUE
  except OSError:
    return True


def rlog_path(segment_dir):
  if os.path.exists(os.path.join(segment_dir, "rlog.lock")):
    return None
  for name in RLOG_NAMES:
    rlog = os.path.join(segment_dir, name)
    if os.path.isfile(rlog):
      return rlog
  return None


def strip_rlog(rlog):
  with open(rlog, "rb") as f:
    data = f.read()

  if data[:4] == ZSTD_MAGIC:
    data = zstd.ZstdDecompressor().stream_reader(io.BytesIO(data)).read()

  kept = []
  try:
    for event in capnp_log.Event.read_multiple_bytes(data):
      try:
        which = event.which()
      except capnp.KjException:
        continue
      if which not in KEEP_TYPES:
        continue

      builder = event.as_builder()
      try:
        sanitize(builder, which)
      except Exception:
        continue
      kept.append(builder.to_bytes())
  except capnp.KjException:
    pass

  return b"".join(kept)


def upload_log(rlog):
  api_info = get_frogpilot_api_info()
  if not api_info.api_token:
    return False

  segment = anonymize_segment_name(os.path.basename(os.path.dirname(rlog)))
  payload = bz2.compress(strip_rlog(rlog))

  data = {
    "api_token": api_info.api_token,
    "build_metadata": json.dumps(api_info.build_metadata),
    "device": api_info.device_type,
    "segment": segment,
  }
  files = {"log": (f"{segment}.bz2", payload, "application/x-bzip2")}

  try:
    response = requests.post(
      f"{FROGPILOT_API}/telemetry",
      data=data,
      files=files,
      headers={"User-Agent": "frogpilot-api/1.0"},
      timeout=60,
    )
    if response.ok:
      setxattr(rlog, TELEMETRY_ATTR_NAME, TELEMETRY_ATTR_VALUE)
      return True
  except requests.exceptions.RequestException as error:
    print(f"Failed to upload telemetry: {error}")

  return False


class FrogPilotTelemetry:
  def __init__(self):
    self.log_roots = list(dict.fromkeys([Paths.log_root(raw=True), Paths.log_root(HD=True, raw=True), Paths.log_root(konik=True, raw=True)]))

    self.sm = messaging.SubMaster(["deviceState"])

    self.backoff = 10

  def get_pending_logs(self):
    pending = []
    for log_root in self.log_roots:
      for segment in listdir_by_creation(log_root):
        if SEGMENT_NAME_PATTERN.fullmatch(segment):
          rlog = rlog_path(os.path.join(log_root, segment))
          if rlog is not None and not already_uploaded(rlog):
            pending.append(rlog)

    return pending

  def update(self):
    self.sm.update(0)

    network_type = self.sm["deviceState"].networkType
    offroad = not self.sm["deviceState"].started

    at_home = offroad and network_type in (NetworkType.ethernet, NetworkType.wifi)
    if not at_home:
      time.sleep(60 if offroad else 5)
      return

    pending = self.get_pending_logs()
    if not pending:
      time.sleep(60)
      return

    for rlog in pending:
      try:
        uploaded = upload_log(rlog)
      except FileNotFoundError:
        continue

      if not uploaded:
        time.sleep(self.backoff * random.uniform(0.5, 1.5))
        self.backoff = min(self.backoff * 2, 120)
        return

    self.backoff = 10


def main():
  frogpilot_telemetry = FrogPilotTelemetry()

  while True:
    frogpilot_telemetry.update()


if __name__ == "__main__":
  main()
