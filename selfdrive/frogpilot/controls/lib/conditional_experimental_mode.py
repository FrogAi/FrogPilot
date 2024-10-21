from openpilot.selfdrive.frogpilot.frogpilot_utilities import MovingAverageCalculator
from openpilot.selfdrive.frogpilot.frogpilot_variables import CITY_SPEED_LIMIT, CRUISING_SPEED, THRESHOLD, params_memory

class ConditionalExperimentalMode:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.curvature_mac = MovingAverageCalculator()
    self.slow_lead_mac = MovingAverageCalculator()
    self.stop_light_mac = MovingAverageCalculator()

    self.curve_detected = False
    self.experimental_mode = False
    self.stop_light_detected = False

  def update(self, carState, frogpilotCarState, frogpilotNavigation, modelData, v_ego, v_lead, frogpilot_toggles):
    if frogpilot_toggles.experimental_mode_via_press:
      self.status_value = params_memory.get_int("CEStatus")
    else:
      self.status_value = 0

    if self.status_value not in {1, 2, 3, 4, 5, 6} and not carState.standstill:
      self.update_conditions(frogpilotCarState, self.frogpilot_planner.tracking_lead, v_ego, v_lead, frogpilot_toggles)
      self.experimental_mode = self.check_conditions(carState, frogpilotNavigation, modelData, self.frogpilot_planner.frogpilot_following.following_lead, v_ego, v_lead, frogpilot_toggles)
      params_memory.put_int("CEStatus", self.status_value if self.experimental_mode else 0)
    else:
      self.experimental_mode = self.status_value in {2, 4, 6} or carState.standstill and self.experimental_mode
      self.stop_light_detected &= self.status_value not in {1, 2, 3, 4, 5, 6}

  def check_conditions(self, carState, frogpilotNavigation, modelData, following_lead, v_ego, v_lead, frogpilot_toggles):
    below_speed = frogpilot_toggles.conditional_limit > v_ego >= 1 and not following_lead
    below_speed_with_lead = frogpilot_toggles.conditional_limit_lead > v_ego >= 1 and following_lead
    if below_speed or below_speed_with_lead:
      self.status_value = 7 if following_lead else 8
      return True

    desired_lane = self.frogpilot_planner.lane_width_left if carState.leftBlinker else self.frogpilot_planner.lane_width_right
    lane_available = desired_lane >= frogpilot_toggles.lane_detection_width or not frogpilot_toggles.conditional_signal_lane_detection or not frogpilot_toggles.lane_detection
    if v_ego < frogpilot_toggles.conditional_signal and (carState.leftBlinker or carState.rightBlinker) and not lane_available:
      self.status_value = 9
      return True

    approaching_maneuver = modelData.navEnabled and (frogpilotNavigation.approachingIntersection or frogpilotNavigation.approachingTurn)
    if frogpilot_toggles.conditional_navigation and approaching_maneuver and (frogpilot_toggles.conditional_navigation_lead or not following_lead):
      self.status_value = 10 if frogpilotNavigation.approachingIntersection else 11
      return True

    if frogpilot_toggles.conditional_curves and self.curve_detected and (frogpilot_toggles.conditional_curves_lead or not following_lead):
      self.status_value = 12
      return True

    if frogpilot_toggles.conditional_lead and self.slow_lead_detected:
      self.status_value = 13 if v_lead < 1 else 14
      return True

    if frogpilot_toggles.conditional_model_stop_time != 0 and self.stop_light_detected:
      self.status_value = 15 if not self.frogpilot_planner.frogpilot_vcruise.forcing_stop else 16
      return True

    if self.frogpilot_planner.frogpilot_vcruise.slc.experimental_mode:
      self.status_value = 17
      return True
    return False

  def update_conditions(self, frogpilotCarState, tracking_lead, v_ego, v_lead, frogpilot_toggles):
    self.curve_detection(tracking_lead, v_ego, frogpilot_toggles)
    self.slow_lead(tracking_lead, v_lead, frogpilot_toggles)
    self.stop_sign_and_light(frogpilotCarState, tracking_lead, v_ego, frogpilot_toggles)

  def curve_detection(self, tracking_lead, v_ego, frogpilot_toggles):
    if v_ego > CRUISING_SPEED:
      curve_detected = (1 / self.frogpilot_planner.road_curvature)**0.5 < v_ego
      curve_active = (0.9 / self.frogpilot_planner.road_curvature)**0.5 < v_ego and self.curve_detected

      self.curvature_mac.add_data(curve_detected or curve_active)
      self.curve_detected = self.curvature_mac.get_moving_average() >= THRESHOLD
    else:
      self.curvature_mac.reset_data()
      self.curve_detected = False

  def slow_lead(self, tracking_lead, v_lead, frogpilot_toggles):
    if tracking_lead:
      slower_lead = frogpilot_toggles.conditional_slower_lead and self.frogpilot_planner.frogpilot_following.slower_lead
      stopped_lead = frogpilot_toggles.conditional_stopped_lead and v_lead < 1

      self.slow_lead_mac.add_data(slower_lead or stopped_lead)
      self.slow_lead_detected = self.slow_lead_mac.get_moving_average() >= THRESHOLD
    else:
      self.slow_lead_mac.reset_data()
      self.slow_lead_detected = False

  def stop_sign_and_light(self, frogpilotCarState, tracking_lead, v_ego, frogpilot_toggles):
    if not (self.curve_detected or tracking_lead or frogpilotCarState.trafficModeActive):
      model_stopping = self.frogpilot_planner.model_length < v_ego * frogpilot_toggles.conditional_model_stop_time

      self.stop_light_mac.add_data(self.frogpilot_planner.model_stopped or model_stopping)
      self.stop_light_detected = self.stop_light_mac.get_moving_average() >= THRESHOLD
    else:
      self.stop_light_mac.reset_data()
      self.stop_light_detected = False
