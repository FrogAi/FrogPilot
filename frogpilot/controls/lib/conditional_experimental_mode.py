#!/usr/bin/env python3
import numpy as np

from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import STOP_DISTANCE
from openpilot.selfdrive.modeld.constants import ModelConstants

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables

LEAD_DECELERATION = 0.2
LEAD_GAP_RATIO = 0.75
LEAD_RELEASE_DECELERATION = 0.1
LEAD_T_IDXS = np.array(ModelConstants.LEAD_T_IDXS)  # [0, 2, 4, 6, 8, 10]s

SLOWDOWN_PERCENTAGE = 0.70
SLOWDOWN_RELEASE_PERCENTAGE = 0.75

SPEED_RELEASE_MARGIN = 0.5

STOPPING_TIMES = [0.5, 1.5]

class ConditionalExperimentalMode:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.curvature_filter = FirstOrderFilter(0, 0.9, DT_MDL)
    self.slow_lead_filter = FirstOrderFilter(0, 0.45, DT_MDL)
    self.stop_light_filter = FirstOrderFilter(0, 1.5, DT_MDL)

    self.curve_detected = False
    self.experimental_mode = False
    self.lane_detected_left = False
    self.lane_detected_right = False
    self.model_stopping = False
    self.prev_standstill = False
    self.slow_lead_detected = False
    self.stop_light_detected = False
    self.stop_light_signal = False

    self.status_value = frogpilot_variables.CE_STATUS["OFF"]

  def update(self, v_ego, sm, frogpilot_toggles):
    self.update_override(sm, frogpilot_toggles)

    if self.prev_standstill and not sm["carState"].standstill:
      self.stop_light_filter.x = 0

    self.prev_standstill = sm["carState"].standstill

    self.stop_sign_and_light(v_ego, sm)

    if not (sm["controlsState"].enabled and frogpilot_toggles.conditional_experimental_mode):
      self.reset()
      return

    self.update_conditions(v_ego, sm, frogpilot_toggles)

    if self.status_value in (frogpilot_variables.CE_STATUS["USER_DISABLED"], frogpilot_variables.CE_STATUS["USER_OVERRIDDEN"]):
      self.experimental_mode = self.status_value == frogpilot_variables.CE_STATUS["USER_OVERRIDDEN"]
    else:
      if sm["carState"].standstill:
        self.experimental_mode &= self.status_value != frogpilot_variables.CE_STATUS["LEAD"]
        self.experimental_mode &= self.frogpilot_planner.model_stopped or self.frogpilot_planner.frogpilot_vcruise.forcing_stop

        if frogpilot_utilities.use_classic_longitudinal(frogpilot_toggles.model):
          should_stop = self.model_stopping
        else:
          should_stop = sm["modelV2"].action.shouldStop

        stopped_at_light = should_stop and not sm["longitudinalPlan"].shouldStop
        if stopped_at_light and not sm["frogpilotCarState"].trafficModeEnabled and frogpilot_toggles.conditional_stop_lights:
          self.experimental_mode = True
          self.status_value = frogpilot_variables.CE_STATUS["STOP_LIGHT"]
      else:
        self.experimental_mode = self.check_conditions(v_ego, sm, frogpilot_toggles)

      if not self.experimental_mode:
        self.status_value = frogpilot_variables.CE_STATUS["OFF"]

  def reset(self):
    self.curve_detected = False
    self.experimental_mode = False
    self.slow_lead_detected = False

    self.curvature_filter.x = 0
    self.slow_lead_filter.x = 0

    if self.status_value not in (frogpilot_variables.CE_STATUS["USER_DISABLED"], frogpilot_variables.CE_STATUS["USER_OVERRIDDEN"]):
      self.status_value = frogpilot_variables.CE_STATUS["OFF"]

  def update_override(self, sm, frogpilot_toggles):
    if self.frogpilot_planner.experimental_mode_pressed and sm["controlsState"].enabled and frogpilot_toggles.conditional_experimental_mode:
      if self.status_value in (frogpilot_variables.CE_STATUS["USER_DISABLED"], frogpilot_variables.CE_STATUS["USER_OVERRIDDEN"]):
        self.status_value = frogpilot_variables.CE_STATUS["OFF"]
      elif sm["controlsState"].experimentalMode:
        self.status_value = frogpilot_variables.CE_STATUS["USER_DISABLED"]
      else:
        self.status_value = frogpilot_variables.CE_STATUS["USER_OVERRIDDEN"]

  def check_conditions(self, v_ego, sm, frogpilot_toggles):
    following_lead = self.frogpilot_planner.frogpilot_following.following_lead

    curve_lead_allowed = not following_lead or self.status_value == frogpilot_variables.CE_STATUS["CURVATURE"] or frogpilot_toggles.conditional_curves_lead
    if self.curve_detected and curve_lead_allowed and frogpilot_toggles.conditional_curves:
      self.status_value = frogpilot_variables.CE_STATUS["CURVATURE"]
      return True

    if self.slow_lead_detected and frogpilot_toggles.conditional_lead:
      self.status_value = frogpilot_variables.CE_STATUS["LEAD"]
      return True

    if (sm["carState"].leftBlinker or sm["carState"].rightBlinker) and v_ego < frogpilot_toggles.conditional_signal:
      lane_detected = self.lane_detected_left if sm["carState"].leftBlinker else self.lane_detected_right
      if not lane_detected or v_ego < frogpilot_toggles.minimum_lane_change_speed or not frogpilot_toggles.conditional_signal_lane_detection:
        self.status_value = frogpilot_variables.CE_STATUS["SIGNAL"]
        return True

    approaching_maneuver = sm["frogpilotNavigation"].approachingIntersection or sm["frogpilotNavigation"].approachingTurn
    if approaching_maneuver and (not following_lead or frogpilot_toggles.conditional_navigation_lead) and frogpilot_toggles.conditional_navigation:
      if sm["frogpilotNavigation"].approachingIntersection:
        self.status_value = frogpilot_variables.CE_STATUS["NAVIGATION_INTERSECTION"]
      else:
        self.status_value = frogpilot_variables.CE_STATUS["NAVIGATION_TURN"]
      return True

    speed_threshold = frogpilot_toggles.conditional_limit_lead if self.frogpilot_planner.lead_relevant else frogpilot_toggles.conditional_limit
    if self.status_value == frogpilot_variables.CE_STATUS["SPEED"]:
      speed_release_margin = SPEED_RELEASE_MARGIN
    else:
      speed_release_margin = 0

    if (v_ego >= 1 or self.experimental_mode) and v_ego < speed_threshold + speed_release_margin:
      self.status_value = frogpilot_variables.CE_STATUS["SPEED"]
      return True

    if self.frogpilot_planner.frogpilot_vcruise.slc.experimental_mode:
      self.status_value = frogpilot_variables.CE_STATUS["SPEED_LIMIT"]
      return True

    if self.stop_light_detected and frogpilot_toggles.conditional_stop_lights:
      self.status_value = frogpilot_variables.CE_STATUS["STOP_LIGHT"]
      return True

    return False

  def update_conditions(self, v_ego, sm, frogpilot_toggles):
    self.lane_detection(sm, frogpilot_toggles)

    if sm["carState"].standstill:
      self.slow_lead_detected = False

      self.slow_lead_filter.x = 0
    else:
      self.slow_lead(v_ego, sm, frogpilot_toggles)
      self.curve_detection(v_ego)

  def curve_detection(self, v_ego):
    self.curvature_filter.update(self.frogpilot_planner.curve_ahead or self.frogpilot_planner.driving_in_curve)
    if self.curve_detected:
      curve_threshold = 1 - frogpilot_variables.THRESHOLD
    else:
      curve_threshold = frogpilot_variables.THRESHOLD

    self.curve_detected = self.curvature_filter.x >= curve_threshold and v_ego > frogpilot_variables.CRUISING_SPEED

  def lane_detection(self, sm, frogpilot_toggles):
    if not sm["carState"].leftBlinker:
      self.lane_detected_left = self.frogpilot_planner.lane_width_left >= frogpilot_toggles.lane_detection_width
    if not sm["carState"].rightBlinker:
      self.lane_detected_right = self.frogpilot_planner.lane_width_right >= frogpilot_toggles.lane_detection_width

  def slow_lead(self, v_ego, sm, frogpilot_toggles):
    if self.frogpilot_planner.lead_relevant:
      model_lead = sm["modelV2"].leadsV3[0]

      if frogpilot_utilities.use_model_lead_trajectory(frogpilot_toggles.model):
        lead_speed = np.maximum(self.frogpilot_planner.lead_one.vLead + np.array(model_lead.v) - model_lead.v[0], 0)
      else:
        lead_speed = np.full(len(LEAD_T_IDXS), self.frogpilot_planner.lead_one.vLead)
      lead_distance = self.frogpilot_planner.lead_one.dRel + np.cumsum((lead_speed[:-1] + lead_speed[1:]) / 2 * np.diff(LEAD_T_IDXS))

      following_distance = STOP_DISTANCE + self.frogpilot_planner.frogpilot_following.base_t_follow * lead_speed[1:]
      safe_distance = np.minimum(following_distance, LEAD_GAP_RATIO * self.frogpilot_planner.lead_one.dRel)
      required_deceleration = max(2 * (v_ego * LEAD_T_IDXS[1:] + safe_distance - lead_distance) / LEAD_T_IDXS[1:]**2)

      slower_lead = required_deceleration >= LEAD_DECELERATION
      slower_lead &= frogpilot_toggles.conditional_slower_lead

      stopped_lead = min(lead_speed) < frogpilot_variables.LEAD_DEPARTURE_SPEED
      stopped_lead &= frogpilot_toggles.conditional_stopped_lead

      lead_departing = self.frogpilot_planner.lead_one.vLead >= v_ego + frogpilot_variables.LEAD_DEPARTURE_SPEED
      lead_settled = required_deceleration < LEAD_RELEASE_DECELERATION and self.frogpilot_planner.lead_one.aLeadK >= -LEAD_DECELERATION

      lead_held = self.slow_lead_detected and (not lead_settled or (v_ego < frogpilot_variables.CRUISING_SPEED and not lead_departing))

      self.slow_lead_filter.update(bool(slower_lead or stopped_lead or lead_held))
    else:
      self.slow_lead_filter.update(False)

    self.slow_lead_detected = self.slow_lead_filter.x >= (1 - frogpilot_variables.THRESHOLD if self.slow_lead_detected else frogpilot_variables.THRESHOLD)

  def stop_sign_and_light(self, v_ego, sm):
    if not sm["frogpilotCarState"].trafficModeEnabled:
      model_times = np.array(sm["modelV2"].velocity.t)
      model_velocities = np.array(sm["modelV2"].velocity.x)

      stop_time = frogpilot_variables.STOP_TIME
      upcoming_velocities = np.append(model_velocities[model_times < stop_time], np.interp(stop_time, model_times, model_velocities))

      slowdown_percentage = SLOWDOWN_RELEASE_PERCENTAGE if self.stop_light_detected else SLOWDOWN_PERCENTAGE
      model_slowing = upcoming_velocities.min() <= slowdown_percentage * v_ego
      self.model_stopping = np.interp(STOPPING_TIMES, model_times, model_velocities).max() < frogpilot_variables.STOPPING_SPEED

      self.stop_light_signal = bool(model_slowing or self.model_stopping)
      self.stop_light_filter.update(self.stop_light_signal and not self.frogpilot_planner.lead_relevant)
      self.stop_light_detected = self.stop_light_filter.x >= frogpilot_variables.THRESHOLD
    else:
      self.stop_light_detected = False
      self.stop_light_signal = False

      self.stop_light_filter.x = 0
