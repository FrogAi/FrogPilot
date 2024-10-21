# PFEIFER - SLC - Modified by FrogAi for FrogPilot
import json

from openpilot.selfdrive.frogpilot.frogpilot_utilities import calculate_distance_to_point
from openpilot.selfdrive.frogpilot.frogpilot_variables import TO_RADIANS, params, params_memory

class SpeedLimitController:
  def __init__(self):
    self.experimental_mode = False

    self.desired_speed_limit = 0
    self.offset = 0
    self.speed_limit = 0

    self.previous_speed_limit = params.get_float("PreviousSpeedLimit")

  def update(self, dashboard_speed_limit, enabled, navigation_speed_limit, v_cruise, v_ego, frogpilot_toggles):
    map_speed_limit = self.get_map_speed_limit(v_ego, frogpilot_toggles)
    max_speed_limit = v_cruise if enabled else 0

    self.speed_limit = self.get_speed_limit(dashboard_speed_limit, map_speed_limit, max_speed_limit, navigation_speed_limit, frogpilot_toggles)
    self.offset = self.get_offset(frogpilot_toggles)
    self.desired_speed_limit = self.get_desired_speed_limit()

    self.experimental_mode = self.speed_limit == 0 and frogpilot_toggles.slc_fallback_experimental_mode

  def get_desired_speed_limit(self):
    if self.speed_limit > 1:
      if self.previous_speed_limit != self.speed_limit:
        params.put_float_nonblocking("PreviousSpeedLimit", self.speed_limit)
        self.previous_speed_limit = self.speed_limit
      return self.speed_limit + self.offset
    return 0

  def get_map_speed_limit(self, v_ego, frogpilot_toggles):
    map_speed_limit = params_memory.get_float("MapSpeedLimit")

    next_map_speed_limit = json.loads(params_memory.get("NextMapSpeedLimit", "{}"))
    next_map_speed_limit_lat = next_map_speed_limit.get("latitude", 0)
    next_map_speed_limit_lon = next_map_speed_limit.get("longitude", 0)
    next_map_speed_limit_value = next_map_speed_limit.get("speedlimit", 0)

    position = json.loads(params_memory.get("LastGPSPosition", "{}"))
    lat = position.get("latitude", 0)
    lon = position.get("longitude", 0)

    if next_map_speed_limit_value > 1:
      d = calculate_distance_to_point(lat * TO_RADIANS, lon * TO_RADIANS, next_map_speed_limit_lat * TO_RADIANS, next_map_speed_limit_lon * TO_RADIANS)

      if self.previous_speed_limit < next_map_speed_limit_value:
        max_d = frogpilot_toggles.map_speed_lookahead_higher * v_ego
      else:
        max_d = frogpilot_toggles.map_speed_lookahead_lower * v_ego

      if d < max_d:
        map_speed_limit = next_map_speed_limit_value

    return map_speed_limit

  def get_offset(self, frogpilot_toggles):
    if self.speed_limit < 13.5:
      return frogpilot_toggles.speed_limit_offset1
    if self.speed_limit < 24:
      return frogpilot_toggles.speed_limit_offset2
    if self.speed_limit < 29:
      return frogpilot_toggles.speed_limit_offset3
    return frogpilot_toggles.speed_limit_offset4

  def get_speed_limit(self, dashboard_speed_limit, map_speed_limit, max_speed_limit, navigation_speed_limit, frogpilot_toggles):
    limits = [dashboard_speed_limit, map_speed_limit, navigation_speed_limit]
    filtered_limits = [float(limit) for limit in limits if limit > 1]

    if frogpilot_toggles.speed_limit_priority_highest and filtered_limits:
      return max(filtered_limits)
    if frogpilot_toggles.speed_limit_priority_lowest and filtered_limits:
      return min(filtered_limits)

    speed_limits = {
      "Dashboard": dashboard_speed_limit,
      "Offline Maps": map_speed_limit,
      "Navigation": navigation_speed_limit,
    }

    for priority in [
      frogpilot_toggles.speed_limit_priority1,
      frogpilot_toggles.speed_limit_priority2,
      frogpilot_toggles.speed_limit_priority3,
    ]:
      if speed_limits.get(priority) in filtered_limits:
        return speed_limits[priority]

    if frogpilot_toggles.slc_fallback_previous_speed_limit:
      return self.previous_speed_limit

    if frogpilot_toggles.slc_fallback_set_speed:
      return max_speed_limit

    return 0
