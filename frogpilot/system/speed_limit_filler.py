#!/usr/bin/env python3
import capnp
import gzip
import hashlib
import json
import math
import threading
import time

from pathlib import Path

from cereal import log, messaging
from openpilot.common.params import Params
from openpilot.common.swaglog import cloudlog

from openpilot.frogpilot.common.frogpilot_api import FrogPilotAPI
from openpilot.frogpilot.common.frogpilot_variables import MAPS_PATH, get_frogpilot_toggles

NetworkType = log.DeviceState.NetworkType

ENTRY_KEYS = {"is_forward", "segment_id", "source", "speed_limit", "tile"}

MAP_MATCH_MAX_AGE_NS = 2_000_000_000

MAX_ENTRIES = 100_000

UPLOAD_INTERVAL = 300

class SpeedLimitFiller:
  def __init__(self):
    self.params = Params(return_defaults=True)
    self.params_memory = Params(memory=True)

    self.started_previously = False

    self.upload_thread = None

    self.last_upload_time = -UPLOAD_INTERVAL

    self.map_schema = capnp.load(str(Path(__file__).parent / "offline_maps.capnp"))

    stored_speed_limits = self.params.get("SpeedLimits") or []
    valid_storage = isinstance(stored_speed_limits, list)
    if not valid_storage:
      stored_speed_limits = []
    self.speed_limits = {
      (limit["segment_id"], limit["is_forward"]): limit
      for limit in stored_speed_limits[-MAX_ENTRIES:] if self.valid_entry(limit)
    }
    if not valid_storage or len(stored_speed_limits) != len(self.speed_limits):
      self.params.put("SpeedLimits", list(self.speed_limits.values()))

    self.upload_pending = bool(self.speed_limits)

    self.frogpilot_api = FrogPilotAPI(self.params)

    self.sm = messaging.SubMaster(["deviceState", "frogpilotCarState", "frogpilotPlan"], poll="deviceState")

  def valid_entry(self, limit):
    return isinstance(limit, dict) and limit.keys() == ENTRY_KEYS

  def log_speed_limit(self):
    last_position = json.loads(self.params_memory.get("LastGPSPosition") or "{}")
    map_match = self.params_memory.get("MapMatchedWay") or {}

    map_match_age = last_position.get("location_mono_time", 0) - map_match.get("location_mono_time", 0)

    way_id = map_match.get("way_id", 0)

    if not way_id or not isinstance(map_match.get("is_forward"), bool) or not 0 <= map_match_age <= MAP_MATCH_MAX_AGE_NS:
      return

    is_forward = map_match["is_forward"]

    dash_speed_limit = self.sm["frogpilotCarState"].dashboardSpeedLimit
    map_speed_limit = map_match["speed_limit"]
    mapbox_speed_limit = self.sm["frogpilotPlan"].slcMapboxSpeedLimit

    mapbox_matches = self.sm["frogpilotPlan"].slcMapboxWayId == way_id and self.sm["frogpilotPlan"].slcMapboxIsForward == is_forward

    if mapbox_matches and mapbox_speed_limit >= 1:
      new_limit = mapbox_speed_limit
      source = "Mapbox"
    elif dash_speed_limit >= 1:
      new_limit = dash_speed_limit
      source = "Dashboard"
    else:
      new_limit = 0
      source = "None"

    existing_limit = self.speed_limits.get((way_id, is_forward))

    if new_limit >= 1 and abs(new_limit - map_speed_limit) > 1:
      if existing_limit is None or existing_limit["source"] != source or abs(existing_limit["speed_limit"] - new_limit) > 1:
        if existing_limit is None and len(self.speed_limits) >= MAX_ENTRIES:
          del self.speed_limits[next(iter(self.speed_limits))]

        self.speed_limits[way_id, is_forward] = {
          "source": source,
          "speed_limit": new_limit,
          "tile": [math.floor(last_position["latitude"] * 4) / 4, math.floor(last_position["longitude"] * 4) / 4],
        }

        self.save_speed_limits()
    elif new_limit >= 1 and map_speed_limit >= 1 and existing_limit is not None:
      del self.speed_limits[way_id, is_forward]
      self.save_speed_limits()

  def prune_speed_limits(self):
    if not self.speed_limits:
      return

    tiles = {}
    for way_key, limit in self.speed_limits.items():
      tiles.setdefault(tuple(limit["tile"]), {})[way_key] = limit

    filled_limits = set()
    try:
      for (latitude, longitude), speed_limits in tiles.items():
        if self.params.get_bool("IsOnroad"):
          return

        path = MAPS_PATH / f"{math.floor(latitude / 2) * 2}/{math.floor(longitude / 2) * 2}"
        path /= f"{latitude:.6f}_{longitude:.6f}_{latitude + 0.25:.6f}_{longitude + 0.25:.6f}"

        if not path.is_file():
          continue

        offline_maps = self.map_schema.Offline.from_bytes_packed(path.read_bytes())

        for way in offline_maps.ways:
          for is_forward in (False, True):
            existing_limit = speed_limits.get((way.id, is_forward))

            if existing_limit is None:
              continue

            if is_forward != way.osmNodeOrderReversed:
              map_speed_limit = way.maxSpeedForward or way.maxSpeed
            else:
              map_speed_limit = way.maxSpeedBackward or way.maxSpeed

            matches = map_speed_limit > 0 and abs(existing_limit["speed_limit"] - map_speed_limit) <= 1
            matches &= not (way.maxSpeedBackwardConditional or way.maxSpeedConditional or way.maxSpeedForwardConditional)

            if matches:
              filled_limits.add((way.id, is_forward))
    except (OSError, capnp.KjException):
      cloudlog.exception("Unable to check speed limits against offline maps")
      return

    for way_key in filled_limits:
      del self.speed_limits[way_key]

    if filled_limits:
      self.save_speed_limits()

  def save_speed_limits(self):
    self.params.put("SpeedLimits", [{"is_forward": is_forward, "segment_id": way_id, **limit} for (way_id, is_forward), limit in self.speed_limits.items()])

    self.upload_pending = True

  def send_speed_limits(self, speed_limits):
    speed_limits = [{
      "is_forward": is_forward,
      "segment_id": way_id,
      "source": limit["source"],
      "speed_limit": limit["speed_limit"],
    } for (way_id, is_forward), limit in sorted(speed_limits.items())]
    body = json.dumps({"speed_limits": speed_limits}, separators=(",", ":")).encode()
    digest = hashlib.sha256(body).hexdigest()

    if self.params.get("SpeedLimitsUploadedHash") == digest:
      return

    response = self.frogpilot_api.post("/v1/speed-limits", data=gzip.compress(body, compresslevel=9, mtime=0), headers={
      "Content-Encoding": "gzip",
      "Content-Type": "application/json",
    })

    if response is not None and 200 <= response.status_code < 300:
      self.params.put("SpeedLimitsUploadedHash", digest)
    else:
      self.upload_pending = True
      status = "no response" if response is None else response.status_code
      cloudlog.warning(f"Unable to upload speed limits (status={status})")

  def upload_speed_limits(self):
    if self.upload_thread is not None and self.upload_thread.is_alive():
      return

    now = time.monotonic()

    if now - self.last_upload_time < UPLOAD_INTERVAL:
      return

    if self.sm["deviceState"].networkMetered or self.sm["deviceState"].networkType not in (NetworkType.ethernet, NetworkType.wifi):
      return

    frogpilot_toggles = get_frogpilot_toggles()

    if self.params.get_bool("IsOnroad") or not self.speed_limits or not frogpilot_toggles.speed_limit_filler_share_data:
      return

    self.last_upload_time = now

    self.upload_thread = threading.Thread(target=self.send_speed_limits, args=(self.speed_limits.copy(),), daemon=True)
    self.upload_pending = False
    self.upload_thread.start()

  def update(self):
    self.sm.update(1000)

    started = self.params.get_bool("IsOnroad")

    if started:
      self.log_speed_limit()
    else:
      if self.started_previously:
        self.prune_speed_limits()

      if self.upload_pending:
        self.upload_speed_limits()

    self.started_previously = started

def main():
  speed_limit_filler = SpeedLimitFiller()

  while True:
    speed_limit_filler.update()


if __name__ == "__main__":
  main()
