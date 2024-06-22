from openpilot.common.params import Params

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import MovingAverageCalculator
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_variables import CITY_SPEED_LIMIT, PROBABILITY

class ConditionalExperimentalMode:
  def __init__(self, FrogPilotPlanner):
    self.params_memory = Params("/dev/shm/params")

    self.frogpilot_planner = FrogPilotPlanner

    self.curvature_mac = MovingAverageCalculator()

    self.curve_detected = False
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

    approaching_maneuver = modelData.navEnabled and (frogpilotNavigation.approachingIntersection or frogpilotNavigation.approachingTurn)
    if frogpilot_toggles.conditional_navigation and approaching_maneuver and (frogpilot_toggles.conditional_navigation_lead or not tracking_lead):
      self.status_value = 10 if frogpilotNavigation.approachingIntersection else 11
      return True

    if frogpilot_toggles.conditional_curves and self.curve_detected and (frogpilot_toggles.conditional_curves_lead or not tracking_lead):
      self.status_value = 12
      return True

    return False

  def update_conditions(self, tracking_lead, v_ego, v_lead, frogpilot_toggles):
    self.curve_detection(v_ego, frogpilot_toggles)

  def curve_detection(self, v_ego, frogpilot_toggles):
    curve_detected = (1 / self.frogpilot_planner.road_curvature)**0.5 < v_ego
    curve_active = (0.9 / self.frogpilot_planner.road_curvature)**0.5 < v_ego and self.curve_detected

    self.curvature_mac.add_data(curve_detected or curve_active)
    self.curve_detected = self.curvature_mac.get_moving_average() >= PROBABILITY
