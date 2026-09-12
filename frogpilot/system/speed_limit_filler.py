#!/usr/bin/env python3
import json

from cereal import messaging

from openpilot.frogpilot.common.frogpilot_variables import params, params_memory

ENTRY_KEYS = {"segment_id", "source", "speed_limit"}

MAP_MATCH_MAX_AGE_NS = 2_000_000_000

MAX_ENTRIES = 100_000

class SpeedLimitFiller:
  def __init__(self):
    self.started_previously = False

    self.speed_limits = {limit.pop("segment_id"): limit for limit in json.loads(params.get("SpeedLimits") or "[]")[-MAX_ENTRIES:] if self.valid_entry(limit)}

    self.sm = messaging.SubMaster(["deviceState", "frogpilotCarState", "frogpilotNavigation", "frogpilotPlan"], poll="deviceState")

  def valid_entry(self, limit):
    return limit.keys() == ENTRY_KEYS

  def log_speed_limit(self):
    last_position = json.loads(params_memory.get("LastGPSPosition") or "{}")
    map_match = json.loads(params_memory.get("MapMatchedWay") or "{}")

    map_match_age = last_position.get("location_mono_time", 0) - map_match.get("location_mono_time", 0)

    way_id = map_match.get("way_id", 0)

    if not way_id or not 0 <= map_match_age <= MAP_MATCH_MAX_AGE_NS:
      return

    dash_speed_limit = self.sm["frogpilotCarState"].dashboardSpeedLimit
    map_speed_limit = map_match["speed_limit"]
    mapbox_speed_limit = self.sm["frogpilotPlan"].slcMapboxSpeedLimit
    nav_speed_limit = self.sm["frogpilotNavigation"].navigationSpeedLimit

    if self.sm["frogpilotPlan"].slcMapboxWayId == way_id and mapbox_speed_limit >= 1:
      new_limit = mapbox_speed_limit
      source = "Mapbox"
    elif dash_speed_limit >= 1:
      new_limit = dash_speed_limit
      source = "Dashboard"
    elif nav_speed_limit >= 1:
      new_limit = nav_speed_limit
      source = "Navigation"
    else:
      new_limit = 0
      source = "None"

    existing_limit = self.speed_limits.get(way_id)

    if new_limit >= 1:
      if abs(new_limit - map_speed_limit) <= 1:
        if existing_limit is not None:
          del self.speed_limits[way_id]
      elif existing_limit is None or existing_limit["source"] != source or abs(existing_limit["speed_limit"] - new_limit) > 1:
        if existing_limit is None and len(self.speed_limits) >= MAX_ENTRIES:
          del self.speed_limits[next(iter(self.speed_limits))]

        self.speed_limits[way_id] = {
          "source": source,
          "speed_limit": new_limit,
        }
    elif existing_limit is not None and map_speed_limit > 0 and abs(existing_limit["speed_limit"] - map_speed_limit) <= 1:
      del self.speed_limits[way_id]

  def update(self):
    self.sm.update(1000)

    started = self.sm["deviceState"].started

    if started:
      self.log_speed_limit()
    elif self.started_previously:
      speed_limits = json.dumps([{"segment_id": way_id, **limit} for way_id, limit in self.speed_limits.items()])

      if params.get("SpeedLimits") != speed_limits.encode():
        params.put("SpeedLimits", speed_limits)

    self.started_previously = started

def main():
  speed_limit_filler = SpeedLimitFiller()

  while True:
    speed_limit_filler.update()


if __name__ == "__main__":
  main()
