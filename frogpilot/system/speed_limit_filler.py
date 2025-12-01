#!/usr/bin/env python3
import capnp
import hashlib
import json
import math
import time

from collections import Counter
from pathlib import Path
from types import SimpleNamespace

from cereal import custom, messaging
from openpilot.common.gps import get_gps_location_service
from openpilot.common.params import Params
from openpilot.common.swaglog import cloudlog

from openpilot.frogpilot.common import frogpilot_api, frogpilot_utilities, frogpilot_variables

MAX_ENTRIES = 100_000

MAX_UPLOAD_SPEED_LIMIT = 40

UPLOAD_INTERVAL = 300

class SpeedLimitFiller:
  def __init__(self):
    self.params = Params(return_defaults=True)

    self.gps_service = get_gps_location_service(self.params)

    self.started_previously = False

    self.visit = None

    self.last_upload_time = -UPLOAD_INTERVAL

    self.map_schema = capnp.load(str(Path(__file__).parent / "offline_maps.capnp"))

    self.speed_limits = {(limit["segment_id"], limit["is_forward"]): limit for limit in self.params.get("SpeedLimits") or []}

    self.upload_pending = True

    self.frogpilot_api = frogpilot_api.FrogPilotAPI(self.params)

    self.sm = messaging.SubMaster(["deviceState", "frogpilotCarState", "frogpilotPlan", "mapdOut", self.gps_service], poll="deviceState")

    self.prune_speed_limits()

  def log_speed_limit(self):
    gps_location = self.sm[self.gps_service]
    gps_valid = frogpilot_utilities.is_gps_location_valid(gps_location, self.gps_service, self.sm)
    mapd_out = self.sm["mapdOut"]

    if not frogpilot_utilities.is_mapd_data_valid(mapd_out, gps_valid, self.sm):
      return

    if mapd_out.waySelectionType != custom.WaySelectionType.current:
      return

    is_forward = mapd_out.isForward
    way_id = mapd_out.wayId

    if mapd_out.conditionalSpeedLimit:
      if (way_id, is_forward) in self.speed_limits:
        del self.speed_limits[way_id, is_forward]
        self.save_speed_limits()
      return

    if self.visit is None or self.visit.key != (way_id, is_forward):
      self.finish_visit()

      self.visit = SimpleNamespace(
        key=(way_id, is_forward),
        map_speed_limit=mapd_out.speedLimit,
        readings=Counter(),
        tile=[math.floor(gps_location.latitude * 4) / 4, math.floor(gps_location.longitude * 4) / 4],
      )

    dash_speed_limit = self.sm["frogpilotCarState"].dashboardSpeedLimit
    mapbox_speed_limit = self.sm["frogpilotPlan"].slcMapboxSpeedLimit

    mapbox_matches = self.sm["frogpilotPlan"].slcMapboxWayId == way_id and self.sm["frogpilotPlan"].slcMapboxIsForward == is_forward

    if dash_speed_limit >= 1:
      self.visit.readings["Dashboard", dash_speed_limit] += gps_location.speed
    elif mapbox_matches and mapbox_speed_limit >= 1:
      self.visit.readings["Mapbox", mapbox_speed_limit] += gps_location.speed

  def finish_visit(self):
    visit = self.visit
    self.visit = None

    if visit is None or not visit.readings:
      return

    dashboard_readings = [reading for reading in visit.readings if reading[0] == "Dashboard"]
    source, new_limit = max(dashboard_readings or visit.readings, key=visit.readings.get)

    existing_limit = self.speed_limits.get(visit.key)

    if abs(new_limit - visit.map_speed_limit) > 1:
      if existing_limit is None or abs(existing_limit["speed_limit"] - new_limit) > 1:
        if existing_limit is None and len(self.speed_limits) >= MAX_ENTRIES:
          del self.speed_limits[next(iter(self.speed_limits))]

        self.speed_limits[visit.key] = {"source": source, "speed_limit": new_limit, "tile": visit.tile}

        self.save_speed_limits()
    elif visit.map_speed_limit >= 1 and existing_limit is not None:
      del self.speed_limits[visit.key]
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
        self.sm.update(0)

        if self.sm["deviceState"].started:
          return

        path = frogpilot_variables.MAPS_PATH / f"{math.floor(latitude / 2) * 2}/{math.floor(longitude / 2) * 2}"
        path /= f"{latitude:.6f}_{longitude:.6f}_{latitude + 0.25:.6f}_{longitude + 0.25:.6f}"

        if not path.is_file():
          continue

        offline_maps = self.map_schema.Offline.from_bytes_packed(path.read_bytes())

        for way in offline_maps.ways:
          for is_forward in (False, True):
            existing_limit = speed_limits.get((way.id, is_forward))

            if existing_limit is None:
              continue

            if is_forward:
              map_speed_limit = way.maxSpeedForward or way.maxSpeed
            else:
              map_speed_limit = way.maxSpeedBackward or way.maxSpeed

            if map_speed_limit > 0 and abs(existing_limit["speed_limit"] - map_speed_limit) <= 1:
              filled_limits.add((way.id, is_forward))
    except (OSError, capnp.KjException):
      cloudlog.exception("Unable to check speed limits against offline maps")
      return

    for way_key in filled_limits:
      del self.speed_limits[way_key]

    if filled_limits:
      self.save_speed_limits()

  def save_speed_limits(self):
    speed_limits = [{"is_forward": is_forward, "segment_id": way_id, **limit} for (way_id, is_forward), limit in self.speed_limits.items()]

    if self.params.put("SpeedLimits", speed_limits) != 0:
      raise RuntimeError("Unable to save SpeedLimits")

  def send_speed_limits(self):
    speed_limits = [{
      "is_forward": is_forward,
      "segment_id": way_id,
      "source": limit["source"],
      "speed_limit": limit["speed_limit"],
    } for (way_id, is_forward), limit in sorted(self.speed_limits.items()) if limit["speed_limit"] <= MAX_UPLOAD_SPEED_LIMIT]
    body = json.dumps({"speed_limits": speed_limits}, separators=(",", ":")).encode()
    digest = hashlib.sha256(body).hexdigest()

    if self.params.get("SpeedLimitsUploadedHash") == digest:
      return

    self.params.remove("SpeedLimitsUploadedHash")

    response = self.frogpilot_api.post_gzip("/v1/speed-limits", body, timeout=10)

    if response is not None and 200 <= response.status_code < 300:
      self.params.put("SpeedLimitsUploadedHash", digest)
    else:
      self.upload_pending = True
      status = "no response" if response is None else response.status_code
      cloudlog.warning(f"Unable to upload speed limits (status={status})")

  def upload_speed_limits(self):
    now = time.monotonic()

    if now - self.last_upload_time < UPLOAD_INTERVAL:
      return

    if not frogpilot_utilities.is_unmetered_network(self.sm["deviceState"]):
      return

    frogpilot_toggles = frogpilot_variables.get_frogpilot_toggles(self.sm)

    if not frogpilot_toggles.speed_limit_filler_share_data:
      return

    self.upload_pending = False
    self.send_speed_limits()

    self.last_upload_time = time.monotonic()

  def update(self):
    self.sm.update(1000)

    started = self.sm["deviceState"].started

    if started:
      self.log_speed_limit()
    else:
      if self.started_previously:
        self.finish_visit()
        self.prune_speed_limits()

        self.upload_pending = True

      if self.upload_pending:
        self.upload_speed_limits()

    self.started_previously = started

def main():
  speed_limit_filler = SpeedLimitFiller()

  while True:
    speed_limit_filler.update()


if __name__ == "__main__":
  main()
