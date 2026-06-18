#!/usr/bin/env python3
from cereal import custom, messaging

from openpilot.common.gps import get_gps_location_service
from openpilot.common.params import Params
from openpilot.frogpilot.common.frogpilot_utilities import is_gps_location_valid, is_mapd_data_valid

ENTRY_KEYS = {"segment_id", "source", "speed_limit"}

MAX_ENTRIES = 100_000

class SpeedLimitFiller:
  def __init__(self):
    self.params = Params()

    self.gps_service = get_gps_location_service(self.params)

    self.started_previously = False

    self.speed_limits = {limit.pop("segment_id"): limit for limit in (self.params.get("SpeedLimits") or [])[-MAX_ENTRIES:] if self.valid_entry(limit)}

    self.sm = messaging.SubMaster(["deviceState", "frogpilotCarState", "frogpilotPlan", "mapdOut", self.gps_service], poll="deviceState")

  def valid_entry(self, limit):
    return limit.keys() == ENTRY_KEYS

  def log_speed_limit(self):
    gps_location = self.sm[self.gps_service]
    gps_valid = is_gps_location_valid(gps_location, self.gps_service, self.sm)

    if not is_mapd_data_valid(self.sm["mapdOut"], gps_valid, self.sm):
      return

    if self.sm["mapdOut"].waySelectionType != custom.WaySelectionType.current:
      return

    dash_speed_limit = self.sm["frogpilotCarState"].dashboardSpeedLimit
    map_speed_limit = self.sm["mapdOut"].speedLimit
    mapbox_speed_limit = self.sm["frogpilotPlan"].slcMapboxSpeedLimit

    way_id = self.sm["mapdOut"].wayId

    if self.sm["frogpilotPlan"].slcMapboxWayId == way_id and mapbox_speed_limit >= 1:
      new_limit = mapbox_speed_limit
      source = "Mapbox"
    elif dash_speed_limit >= 1:
      new_limit = dash_speed_limit
      source = "Dashboard"
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
      speed_limits = [{"segment_id": way_id, **limit} for way_id, limit in self.speed_limits.items()]

      if self.params.get("SpeedLimits") != speed_limits:
        self.params.put("SpeedLimits", speed_limits)

    self.started_previously = started

def main():
  speed_limit_filler = SpeedLimitFiller()

  while True:
    speed_limit_filler.update()


if __name__ == "__main__":
  main()
