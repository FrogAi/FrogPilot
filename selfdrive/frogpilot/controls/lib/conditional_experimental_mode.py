from openpilot.common.params import Params

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import MovingAverageCalculator
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_variables import CITY_SPEED_LIMIT, CRUISING_SPEED, PROBABILITY, TRAJECTORY_SIZE

class ConditionalExperimentalMode:
  def __init__(self):
    self.params_memory = Params("/dev/shm/params")

    self.experimental_mode = False

  def update(self, carState, frogpilotNavigation, lead, modelData, model_length, road_curvature, slower_lead, tracking_lead, v_ego, v_lead, frogpilot_toggles):
    if not carState.standstill:
      self.update_conditions(lead.dRel, model_length, road_curvature, slower_lead, tracking_lead, v_ego, v_lead, frogpilot_toggles)
      self.experimental_mode = self.check_conditions(carState, frogpilotNavigation, modelData, tracking_lead, v_ego, v_lead, frogpilot_toggles)
      self.params_memory.put_int("CEStatus", self.status_value if self.experimental_mode else 0)
    else:
      self.experimental_mode = carState.standstill and self.experimental_mode

  def check_conditions(self, carState, frogpilotNavigation, modelData, tracking_lead, v_ego, v_lead, frogpilot_toggles):
    if (tracking_lead and v_ego <= frogpilot_toggles.conditional_limit_lead) or (not tracking_lead and v_ego <= frogpilot_toggles.conditional_limit):
      self.status_value = 7 if tracking_lead else 8
      return True

    return False

  def update_conditions(self, lead_distance, model_length, road_curvature, slower_lead, tracking_lead, v_ego, v_lead, frogpilot_toggles):
