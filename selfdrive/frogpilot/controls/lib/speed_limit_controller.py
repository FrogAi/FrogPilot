# PFEIFER - SLC - Modified by FrogAi for FrogPilot
import json
import math

from openpilot.common.conversions import Conversions as CV
from openpilot.common.params import Params

R = 6373000.0  # approximate radius of earth in meters
TO_RADIANS = math.pi / 180
TO_DEGREES = 180 / math.pi

# points should be in radians
# output is meters
def distance_to_point(ax, ay, bx, by):
  a = math.sin((bx - ax) / 2) ** 2 + math.cos(ax) * math.cos(bx) * math.sin((by - ay) / 2) ** 2
  c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
  return R * c  # in meters

class SpeedLimitController:
  def __init__(self):
    self.params = Params()
    self.params_memory = Params("/dev/shm/params")

    self.car_speed_limit = 0  # m/s
    self.map_speed_limit = 0  # m/s
    self.nav_speed_limit = 0  # m/s
    self.prv_speed_limit = 0  # m/s

    self.lat = 0  # deg
    self.lon = 0  # deg

    self.update_frogpilot_params()

  def update(self, v_ego):
    self.car_speed_limit = self.get_param_memory("CarSpeedLimit")
    self.write_map_state(v_ego)
    self.nav_speed_limit = self.get_param_memory("NavSpeedLimit")

    if self.params_memory.get_bool("FrogPilotTogglesUpdated"):
      self.update_frogpilot_params()

  def get_param_memory(self, key, is_json=False, default=None):
    try:
      data = self.params_memory.get(key)
      if not is_json and data is not None:
        return float(data.decode('utf-8'))
      return json.loads(data) if is_json else data
    except:
      return default

  def write_map_state(self, v_ego):
    self.map_speed_limit = self.get_param_memory("MapSpeedLimit")

    next_map_speed_limit = self.get_param_memory("NextMapSpeedLimit", is_json=True, default={})
    next_map_speed_limit_value = next_map_speed_limit.get("speedlimit", 0)
    next_map_speed_limit_lat = next_map_speed_limit.get("latitude", 0)
    next_map_speed_limit_lon = next_map_speed_limit.get("longitude", 0)

    position = self.get_param_memory("LastGPSPosition", is_json=True, default={})
    self.lat = position.get("latitude", 0)
    self.lon = position.get("longitude", 0)

    if self.prv_speed_limit < next_map_speed_limit_value > 1:
      d = distance_to_point(self.lat * TO_RADIANS, self.lon * TO_RADIANS,
                            next_map_speed_limit_lat * TO_RADIANS, next_map_speed_limit_lon * TO_RADIANS)
      max_d = self.map_speed_lookahead_higher * v_ego
      if d < max_d:
        self.map_speed_limit = next_map_speed_limit_value

    if self.prv_speed_limit > next_map_speed_limit_value > 1:
      d = distance_to_point(self.lat * TO_RADIANS, self.lon * TO_RADIANS,
                            next_map_speed_limit_lat * TO_RADIANS, next_map_speed_limit_lon * TO_RADIANS)
      max_d = self.map_speed_lookahead_higher_lower * v_ego
      if d < max_d:
        self.map_speed_limit = next_map_speed_limit_value

  @property
  def speed_limit(self):
    limits = [self.car_speed_limit, self.map_speed_limit, self.nav_speed_limit]
    filtered_limits = [limit for limit in limits if limit is not None and limit > 1]

    if self.highest and filtered_limits:
      return float(max(filtered_limits))
    elif self.lowest and filtered_limits:
      return float(min(filtered_limits))

    speed_limits = {
      "Dashboard": self.car_speed_limit,
      "Offline Maps": self.map_speed_limit,
      "Navigation": self.nav_speed_limit,
    }

    priorities = [
      self.speed_limit_priority1,
      self.speed_limit_priority2,
      self.speed_limit_priority3,
    ]

    for priority in priorities:
      if priority in speed_limits and speed_limits[priority] in filtered_limits:
        return float(speed_limits[priority])

    if self.use_previous_limit:
      return float(self.prv_speed_limit)

    return 0

  @property
  def offset(self):
    if self.speed_limit < 13.5:
      return self.offset1
    elif self.speed_limit < 24:
      return self.offset2
    elif self.speed_limit < 29:
      return self.offset3
    else:
      return self.offset4

  @property
  def desired_speed_limit(self):
    if self.speed_limit > 1:
      self.update_previous_limit(self.speed_limit)
      return self.speed_limit + self.offset
    else:
      return 0

  def update_previous_limit(self, speed_limit):
    if self.prv_speed_limit != speed_limit:
      self.params.put_float("PreviousSpeedLimit", speed_limit)
      self.prv_speed_limit = speed_limit

  @property
  def experimental_mode(self):
    return self.speed_limit == 0 and self.use_experimental_mode

  def update_frogpilot_params(self):
    conversion = CV.KPH_TO_MS if self.params.get_bool("IsMetric") else CV.MPH_TO_MS

    self.map_speed_lookahead_higher = self.params.get_int("SLCLookaheadHigher")
    self.map_speed_lookahead_lower = self.params.get_int("SLCLookaheadLower")

    self.offset1 = self.params.get_int("Offset1") * conversion
    self.offset2 = self.params.get_int("Offset2") * conversion
    self.offset3 = self.params.get_int("Offset3") * conversion
    self.offset4 = self.params.get_int("Offset4") * conversion

    self.speed_limit_priority1 = self.params.get("SLCPriority1", encoding='utf-8')
    self.speed_limit_priority2 = self.params.get("SLCPriority2", encoding='utf-8')
    self.speed_limit_priority3 = self.params.get("SLCPriority3", encoding='utf-8')

    self.highest = self.speed_limit_priority1 == "Highest"
    self.lowest = self.speed_limit_priority1 == "Lowest"

    slc_fallback = self.params.get_int("SLCFallback")
    self.use_experimental_mode = slc_fallback == 1
    self.use_previous_limit = slc_fallback == 2

    self.prv_speed_limit = self.params.get_float("PreviousSpeedLimit")

SpeedLimitController = SpeedLimitController()
