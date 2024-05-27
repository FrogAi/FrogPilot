from openpilot.common.conversions import Conversions as CV
from openpilot.common.numpy_fast import interp
from openpilot.common.params import Params

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import MovingAverageCalculator
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_variables import CITY_SPEED_LIMIT, CRUISING_SPEED, PROBABILITY

class ConditionalExperimentalMode:
  def __init__(self):
    self.params_memory = Params("/dev/shm/params")

    self.experimental_mode = False

    self.lead_detection_mac = MovingAverageCalculator()

  def update(self, carState, enabled, frogpilotNavigation, lead_distance, lead, modelData, road_curvature, slower_lead, v_ego, v_lead, frogpilot_toggles):
    self.update_conditions(lead_distance, lead.status, modelData, road_curvature, slower_lead, carState.standstill, v_ego, v_lead, frogpilot_toggles)

    condition_met = self.check_conditions(carState, frogpilotNavigation, lead, modelData, v_ego, frogpilot_toggles) and enabled
    self.experimental_mode = condition_met

    self.params_memory.put_int("CEStatus", self.status_value if condition_met else 0)

  def check_conditions(self, carState, frogpilotNavigation, lead, modelData, v_ego, frogpilot_toggles):
    if carState.standstill:
      self.status_value = 0
      return self.experimental_mode

    if (not self.lead_detected and v_ego <= frogpilot_toggles.conditional_limit) or (self.lead_detected and v_ego <= frogpilot_toggles.conditional_limit_lead):
      self.status_value = 11 if self.lead_detected else 12
      return True

    return False

  def update_conditions(self, lead_distance, lead_status, modelData, road_curvature, slower_lead, standstill, v_ego, v_lead, frogpilot_toggles):
    self.lead_detection(lead_status)

  def lead_detection(self, lead_status):
    self.lead_detection_mac.add_data(lead_status)
    self.lead_detected = self.lead_detection_mac.get_moving_average() >= PROBABILITY
