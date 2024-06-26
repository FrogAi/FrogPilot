from openpilot.common.params import Params

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import MovingAverageCalculator
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_variables import CITY_SPEED_LIMIT, CRUISING_SPEED, PROBABILITY, TRAJECTORY_SIZE

class ConditionalExperimentalMode:
  def __init__(self):
    self.params_memory = Params("/dev/shm/params")

    self.curve_detected = False
    self.experimental_mode = False

    self.curvature_mac = MovingAverageCalculator()

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

    if frogpilot_toggles.conditional_curves and self.curve_detected and (frogpilot_toggles.conditional_curves_lead or not tracking_lead):
      self.status_value = 12
      return True

    return False

  def update_conditions(self, lead_distance, model_length, road_curvature, slower_lead, tracking_lead, v_ego, v_lead, frogpilot_toggles):
    self.road_curvature(road_curvature, v_ego, frogpilot_toggles)

  def road_curvature(self, road_curvature, v_ego, frogpilot_toggles):
    curve_detected = (1 / road_curvature)**0.5 < v_ego
    curve_active = (0.9 / road_curvature)**0.5 < v_ego and self.curve_detected

    self.curvature_mac.add_data(curve_detected or curve_active)
    self.curve_detected = self.curvature_mac.get_moving_average() >= PROBABILITY
