import json

from openpilot.common.conversions import Conversions as CV
from openpilot.common.params import Params


params = Params()
params_memory = Params("/dev/shm/params")

class SpeedLimitController:
  car_speed_limit: float = 0  # m/s
  map_speed_limit: float = 0  # m/s
  nav_speed_limit: float = 0  # m/s
  prv_speed_limit: float = 0  # m/s

  def __init__(self) -> None:
    self.update_frogpilot_params()

  def update_speed_limits(self):
    self.car_speed_limit = json.loads(params_memory.get("CarSpeedLimit") or '0')
    self.map_speed_limit = json.loads(params_memory.get("MapSpeedLimit") or '0')
    self.nav_speed_limit = json.loads(params_memory.get("NavSpeedLimit") or '0')

    if params_memory.get_bool("FrogPilotTogglesUpdated"):
      self.update_frogpilot_params()

  @property
  def offset(self) -> float:
    if self.speed_limit < 14:
      return self.offset1
    elif self.speed_limit < 24:
      return self.offset2
    elif self.speed_limit < 29:
      return self.offset3
    else:
      return self.offset4

  @property
  def speed_limit(self) -> float:
    limits = [self.car_speed_limit, self.map_speed_limit, self.nav_speed_limit]
    filtered_limits = [limit for limit in limits if limit >= 1]

    if self.highest and filtered_limits:
      return max(filtered_limits)
    elif self.lowest and filtered_limits:
      return min(filtered_limits)

    speed_limits = {
      "Dashboard": self.car_speed_limit,
      "Navigation": self.nav_speed_limit,
      "Offline Maps": self.map_speed_limit,
    }

    priorities = [
      self.speed_limit_priority1,
      self.speed_limit_priority2,
      self.speed_limit_priority3,
    ]

    for priority in priorities:
      speed_limit_value = speed_limits.get(priority, 0)
      if speed_limit_value >= 1:
        return speed_limit_value

    if self.use_previous_limit:
      return self.prv_speed_limit

    return 0

  def update_previous_limit(self, speed_limit: float):
    if self.prv_speed_limit != speed_limit:
      params.put_float("PreviousSpeedLimit", speed_limit)
      self.prv_speed_limit = speed_limit

  @property
  def desired_speed_limit(self) -> float:
    if self.speed_limit != 0:
      self.update_previous_limit(self.speed_limit)
      return self.speed_limit + self.offset
    else:
      return 0

  @property
  def experimental_mode(self) -> bool:
    return self.speed_limit == 0 and self.use_experimental_mode

  def write_car_state(self):
    params_memory.put("CarSpeedLimit", json.dumps(self.car_speed_limit))

  def write_map_state(self):
    params_memory.put("MapSpeedLimit", json.dumps(self.map_speed_limit))

  def write_nav_state(self):
    params_memory.put("NavSpeedLimit", json.dumps(self.nav_speed_limit))

  def update_frogpilot_params(self):
    conversion = CV.KPH_TO_MS if params.get_bool("IsMetric") else CV.MPH_TO_MS

    self.offset1 = params.get_int("Offset1") * conversion
    self.offset2 = params.get_int("Offset2") * conversion
    self.offset3 = params.get_int("Offset3") * conversion
    self.offset4 = params.get_int("Offset4") * conversion

    self.speed_limit_priority1 = params.get("SLCPriority1", encoding='utf-8')
    self.speed_limit_priority2 = params.get("SLCPriority2", encoding='utf-8')
    self.speed_limit_priority3 = params.get("SLCPriority3", encoding='utf-8')

    self.highest = self.speed_limit_priority1 == "Highest"
    self.lowest = self.speed_limit_priority1 == "Lowest"

    slc_fallback = params.get_int("SLCFallback")
    self.use_experimental_mode = slc_fallback == 1
    self.use_previous_limit = slc_fallback == 2

    self.prv_speed_limit = params.get_float("PreviousSpeedLimit")

SpeedLimitController = SpeedLimitController()
