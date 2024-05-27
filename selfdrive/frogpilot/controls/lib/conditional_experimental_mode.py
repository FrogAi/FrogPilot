from openpilot.common.conversions import Conversions as CV
from openpilot.common.numpy_fast import interp
from openpilot.common.params import Params

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import MovingAverageCalculator
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_variables import CITY_SPEED_LIMIT, CRUISING_SPEED, PROBABILITY

class ConditionalExperimentalMode:
  def __init__(self):
    self.params_memory = Params("/dev/shm/params")

    self.curve_detected = False
    self.experimental_mode = False

    self.curvature_mac = MovingAverageCalculator()
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

    approaching_maneuver = modelData.navEnabled and (frogpilotNavigation.approachingIntersection or frogpilotNavigation.approachingTurn)
    if frogpilot_toggles.conditional_navigation and approaching_maneuver and (frogpilot_toggles.conditional_navigation_lead or not self.lead_detected):
      self.status_value = 7 if frogpilotNavigation.approachingIntersection else 8
      return True

    if (not self.lead_detected and v_ego <= frogpilot_toggles.conditional_limit) or (self.lead_detected and v_ego <= frogpilot_toggles.conditional_limit_lead):
      self.status_value = 11 if self.lead_detected else 12
      return True

    if frogpilot_toggles.conditional_curves and self.curve_detected:
      self.status_value = 15
      return True

    return False

  def update_conditions(self, lead_distance, lead_status, modelData, road_curvature, slower_lead, standstill, v_ego, v_lead, frogpilot_toggles):
    self.lead_detection(lead_status)
    self.road_curvature(road_curvature, v_ego, frogpilot_toggles)

  def lead_detection(self, lead_status):
    self.lead_detection_mac.add_data(lead_status)
    self.lead_detected = self.lead_detection_mac.get_moving_average() >= PROBABILITY

  def road_curvature(self, road_curvature, v_ego, frogpilot_toggles):
    if frogpilot_toggles.conditional_curves_lead or not self.lead_detected:
      curve_detected = (1 / road_curvature)**0.5 < v_ego
      curve_active = (0.9 / road_curvature)**0.5 < v_ego and self.curve_detected

      self.curvature_mac.add_data(curve_detected or curve_active)
      self.curve_detected = self.curvature_mac.get_moving_average() >= PROBABILITY
    else:
      self.curvature_mac.reset_data()
      self.curve_detected = False
