from openpilot.common.params import Params
from openpilot.selfdrive.modeld.constants import ModelConstants

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import MovingAverageCalculator
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_variables import CITY_SPEED_LIMIT, PROBABILITY
from openpilot.selfdrive.frogpilot.controls.lib.speed_limit_controller import SpeedLimitController

MODEL_LENGTH = ModelConstants.IDX_N
PLANNER_TIME = ModelConstants.T_IDXS[MODEL_LENGTH - 1]

class ConditionalExperimentalMode:
  def __init__(self, FrogPilotPlanner):
    self.params_memory = Params("/dev/shm/params")

    self.frogpilot_planner = FrogPilotPlanner

    self.curvature_mac = MovingAverageCalculator()
    self.slow_lead_mac = MovingAverageCalculator()
    self.stop_light_mac = MovingAverageCalculator()

    self.curve_detected = False
    self.experimental_mode = False
    self.stop_light_detected = False

  def update(self, carState, frogpilotNavigation, modelData, v_ego, v_lead, frogpilot_toggles):
    if frogpilot_toggles.experimental_mode_via_press:
      self.status_value = self.params_memory.get_int("CEStatus")
    else:
      self.status_value = 0

    if self.status_value not in {1, 2, 3, 4, 5, 6} and not carState.standstill:
      self.update_conditions(self.frogpilot_planner.tracking_lead, v_ego, v_lead, frogpilot_toggles)
      self.experimental_mode = self.check_conditions(carState, frogpilotNavigation, modelData, self.frogpilot_planner.tracking_lead, v_ego, v_lead, frogpilot_toggles)
      self.params_memory.put_int("CEStatus", self.status_value if self.experimental_mode else 0)
    else:
      self.experimental_mode = self.status_value in {2, 4, 6} or carState.standstill and self.experimental_mode

  def check_conditions(self, carState, frogpilotNavigation, modelData, tracking_lead, v_ego, v_lead, frogpilot_toggles):
    below_speed = frogpilot_toggles.conditional_limit > v_ego >= 1 and not tracking_lead
    below_speed_with_lead = frogpilot_toggles.conditional_limit_lead > v_ego >= 1 and tracking_lead
    if below_speed or below_speed_with_lead:
      self.status_value = 7 if tracking_lead else 8
      return True

    if frogpilot_toggles.conditional_signal and v_ego < CITY_SPEED_LIMIT and (carState.leftBlinker or carState.rightBlinker):
      self.status_value = 9
      return True

    approaching_maneuver = modelData.navEnabled and (frogpilotNavigation.approachingIntersection or frogpilotNavigation.approachingTurn)
    if frogpilot_toggles.conditional_navigation and approaching_maneuver and (frogpilot_toggles.conditional_navigation_lead or not tracking_lead):
      self.status_value = 10 if frogpilotNavigation.approachingIntersection else 11
      return True

    if frogpilot_toggles.conditional_curves and self.curve_detected and (frogpilot_toggles.conditional_curves_lead or not tracking_lead):
      self.status_value = 12
      return True

    if frogpilot_toggles.conditional_lead and self.slow_lead_detected:
      self.status_value = 13 if v_lead < 1 else 14
      return True

    if frogpilot_toggles.conditional_stop_lights and self.stop_light_detected:
      self.status_value = 15 if not self.frogpilot_planner.forcing_stop else 16
      return True

    if SpeedLimitController.experimental_mode:
      self.status_value = 17
      return True

    return False

  def update_conditions(self, tracking_lead, v_ego, v_lead, frogpilot_toggles):
    self.curve_detection(v_ego, frogpilot_toggles)
    self.slow_lead(tracking_lead, v_lead, frogpilot_toggles)
    self.stop_sign_and_light(tracking_lead, v_ego, frogpilot_toggles)

  def curve_detection(self, v_ego, frogpilot_toggles):
    curve_detected = (1 / self.frogpilot_planner.road_curvature)**0.5 < v_ego
    curve_active = (0.9 / self.frogpilot_planner.road_curvature)**0.5 < v_ego and self.curve_detected

    self.curvature_mac.add_data(curve_detected or curve_active)
    self.curve_detected = self.curvature_mac.get_moving_average() >= PROBABILITY

  def slow_lead(self, tracking_lead, v_lead, frogpilot_toggles):
    if tracking_lead:
      slower_lead = self.frogpilot_planner.slower_lead and frogpilot_toggles.conditional_slower_lead
      stopped_lead = frogpilot_toggles.conditional_stopped_lead and v_lead < 1

      self.slow_lead_mac.add_data(slower_lead or stopped_lead)
      self.slow_lead_detected = self.slow_lead_mac.get_moving_average() >= PROBABILITY
    else:
      self.slow_lead_mac.reset_data()
      self.slow_lead_detected = False

  def stop_sign_and_light(self, tracking_lead, v_ego, frogpilot_toggles):
    model_projection = PLANNER_TIME - (5 if frogpilot_toggles.less_sensitive_lights else 3)
    model_stopping = self.frogpilot_planner.model_length < v_ego * model_projection

    self.stop_light_mac.add_data((self.frogpilot_planner.model_stopped or model_stopping) and not (self.curve_detected or tracking_lead))
    self.stop_light_detected = self.stop_light_mac.get_moving_average() >= PROBABILITY
