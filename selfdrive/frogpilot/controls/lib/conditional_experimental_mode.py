from openpilot.common.params import Params

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import MovingAverageCalculator
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_variables import CITY_SPEED_LIMIT, PROBABILITY

class ConditionalExperimentalMode:
  def __init__(self, FrogPilotPlanner):
    self.params_memory = Params("/dev/shm/params")

    self.frogpilot_planner = FrogPilotPlanner

    self.experimental_mode = False

  def update(self, carState, frogpilotNavigation, modelData, v_ego, v_lead, frogpilot_toggles):
    if not carState.standstill:
      self.update_conditions(self.frogpilot_planner.tracking_lead, v_ego, v_lead, frogpilot_toggles)
      self.experimental_mode = self.check_conditions(carState, frogpilotNavigation, modelData, self.frogpilot_planner.tracking_lead, v_ego, v_lead, frogpilot_toggles)
      self.params_memory.put_int("CEStatus", self.status_value if self.experimental_mode else 0)
    else:
      self.experimental_mode = carState.standstill and self.experimental_mode

  def check_conditions(self, carState, frogpilotNavigation, modelData, tracking_lead, v_ego, v_lead, frogpilot_toggles):
    below_speed = frogpilot_toggles.conditional_limit > v_ego >= 1 and not tracking_lead
    below_speed_with_lead = frogpilot_toggles.conditional_limit_lead > v_ego >= 1 and tracking_lead
    if below_speed or below_speed_with_lead:
      self.status_value = 7 if tracking_lead else 8
      return True

    return False

  def update_conditions(self, tracking_lead, v_ego, v_lead, frogpilot_toggles):
