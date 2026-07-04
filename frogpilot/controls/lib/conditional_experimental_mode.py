#!/usr/bin/env python3
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import get_safe_obstacle_distance, get_stopped_equivalence_factor
from openpilot.selfdrive.modeld.constants import ModelConstants

from openpilot.frogpilot.common.frogpilot_variables import CITY_SPEED_LIMIT, CRUISING_SPEED, SLOWDOWN_PERCENTAGE, THRESHOLD, params_memory

PREDICTED_LEAD_OBSTACLE_BUFFER = 15.0
PREDICTED_LEAD_SPEED_DROP = 2.0

class ConditionalExperimentalMode:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.curvature_filter = FirstOrderFilter(0, 1, DT_MDL)
    self.slow_lead_filter = FirstOrderFilter(0, 0.5, DT_MDL)
    self.stop_light_filter = FirstOrderFilter(0, 0.5, DT_MDL)

    self.curve_detected = False
    self.experimental_mode = False
    self.stop_light_detected = False

  def update(self, v_ego, sm, frogpilot_toggles):
    if frogpilot_toggles.experimental_mode_via_press:
      self.status_value = params_memory.get_int("CEStatus")
    else:
      self.status_value = 0

    if self.status_value not in (1, 2) and not sm["carState"].standstill:
      self.update_conditions(v_ego, sm, frogpilot_toggles)

      self.experimental_mode = self.check_conditions(v_ego, sm, frogpilot_toggles)

      params_memory.put_int("CEStatus", self.status_value if self.experimental_mode else 0)
    else:
      self.experimental_mode = self.status_value == 2 or sm["carState"].standstill and self.experimental_mode and self.frogpilot_planner.model_stopped
      self.stop_light_detected &= self.status_value not in (1, 2)
      self.stop_light_filter.x = 0

  def check_conditions(self, v_ego, sm, frogpilot_toggles):
    below_speed = not self.frogpilot_planner.frogpilot_following.following_lead and 1 <= v_ego < frogpilot_toggles.conditional_limit
    below_speed_with_lead = self.frogpilot_planner.frogpilot_following.following_lead and 1 <= v_ego < frogpilot_toggles.conditional_limit_lead
    if below_speed or below_speed_with_lead:
      self.status_value = 3 if self.frogpilot_planner.frogpilot_following.following_lead else 4
      return True

    desired_lane = self.frogpilot_planner.lane_width_left if sm["carState"].leftBlinker else self.frogpilot_planner.lane_width_right
    lane_available = desired_lane >= frogpilot_toggles.lane_detection_width or not frogpilot_toggles.conditional_signal_lane_detection
    if v_ego < frogpilot_toggles.conditional_signal and (sm["carState"].leftBlinker or sm["carState"].rightBlinker) and not lane_available:
      self.status_value = 5
      return True

    approaching_maneuver = sm["frogpilotNavigation"].approachingIntersection or sm["frogpilotNavigation"].approachingTurn
    if approaching_maneuver and (not self.frogpilot_planner.frogpilot_following.following_lead or frogpilot_toggles.conditional_navigation_lead) and frogpilot_toggles.conditional_navigation:
      self.status_value = 6 if sm["frogpilotNavigation"].approachingIntersection else 7
      return True

    if self.curve_detected and (not self.frogpilot_planner.frogpilot_following.following_lead or frogpilot_toggles.conditional_curves_lead) and frogpilot_toggles.conditional_curves:
      self.status_value = 8
      return True

    if self.slow_lead_detected and frogpilot_toggles.conditional_lead:
      self.status_value = 9 if self.frogpilot_planner.lead_one.vLead < 1 else 10
      return True

    if self.stop_light_detected and frogpilot_toggles.conditional_model_stop_time != 0:
      self.status_value = 11 if not self.frogpilot_planner.frogpilot_vcruise.forcing_stop else 12
      return True

    if self.frogpilot_planner.frogpilot_vcruise.slc.experimental_mode:
      self.status_value = 13
      return True

    return False

  def update_conditions(self, v_ego, sm, frogpilot_toggles):
    self.curve_detection(v_ego, frogpilot_toggles)
    self.slow_lead(v_ego, sm, frogpilot_toggles)
    self.stop_sign_and_light(v_ego, sm, frogpilot_toggles.conditional_model_stop_time)

  def curve_detection(self, v_ego, frogpilot_toggles):
    self.curvature_filter.update(self.frogpilot_planner.road_curvature_detected or self.frogpilot_planner.driving_in_curve)
    self.curve_detected = self.curvature_filter.x >= THRESHOLD and v_ego > CRUISING_SPEED

  def slow_lead(self, v_ego, sm, frogpilot_toggles):
    if self.frogpilot_planner.tracking_lead:
      slower_lead = (v_ego - self.frogpilot_planner.lead_one.vLead) > CRUISING_SPEED and frogpilot_toggles.conditional_slower_lead
      stopped_lead = self.frogpilot_planner.lead_one.vLead < 1 and frogpilot_toggles.conditional_stopped_lead

      if sm["modelV2"].leadsV3[0].prob > frogpilot_toggles.lead_detection_probability and frogpilot_toggles.model_version == "v9":
        lead_distances = [self.frogpilot_planner.lead_one.dRel + distance - sm["modelV2"].leadsV3[0].x[0] for distance in sm["modelV2"].leadsV3[0].x]
        lead_velocities = [max(self.frogpilot_planner.lead_one.vLead + velocity - sm["modelV2"].leadsV3[0].v[0], 0) for velocity in sm["modelV2"].leadsV3[0].v]

        future_lead_speed = min(lead_velocities[1:])

        lead_obstacles = [lead_distances[index] + get_stopped_equivalence_factor(velocity) for index, velocity in enumerate(lead_velocities)]
        relevant_lead = min(lead_obstacles[1:]) < get_safe_obstacle_distance(v_ego, self.frogpilot_planner.frogpilot_following.t_follow) + PREDICTED_LEAD_OBSTACLE_BUFFER

        predicted_slower_lead = lead_velocities[0] - future_lead_speed >= PREDICTED_LEAD_SPEED_DROP and relevant_lead and frogpilot_toggles.conditional_slower_lead
        predicted_stopped_lead = future_lead_speed < 1 and relevant_lead and frogpilot_toggles.conditional_stopped_lead
      else:
        predicted_slower_lead = False
        predicted_stopped_lead = False

      self.slow_lead_filter.update(slower_lead or stopped_lead or predicted_slower_lead or predicted_stopped_lead)
      self.slow_lead_detected = self.slow_lead_filter.x >= THRESHOLD
    else:
      self.slow_lead_filter.x = 0
      self.slow_lead_detected = False

  def stop_sign_and_light(self, v_ego, sm, model_time):
    if not sm["frogpilotCarState"].trafficModeEnabled:
      model_stopping = self.frogpilot_planner.model_length < v_ego * model_time

      slow_hint_window = [velocity for time_index, velocity in zip(ModelConstants.T_IDXS, sm["modelV2"].velocity.x) if time_index > model_time]
      slow_hint_detected = any(velocity <= SLOWDOWN_PERCENTAGE * v_ego for velocity in slow_hint_window) and not self.curve_detected

      self.stop_light_filter.update(self.frogpilot_planner.model_stopped or model_stopping or slow_hint_detected)
      self.stop_light_detected = self.stop_light_filter.x >= THRESHOLD and not self.frogpilot_planner.tracking_lead
    else:
      self.stop_light_filter.x = 0
      self.stop_light_detected = False
