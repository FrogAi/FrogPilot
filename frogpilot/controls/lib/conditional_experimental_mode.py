#!/usr/bin/env python3
import numpy as np

from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import LEAD_T_IDXS_MODEL, STOP_DISTANCE

from openpilot.frogpilot.common import frogpilot_variables

LEAD_DECELERATION = 0.2
LEAD_GAP_RATIO = 0.75
LEAD_RELEASE_DECELERATION = 0.1

SLOWDOWN_PERCENTAGE = 0.50
SLOWDOWN_RELEASE_PERCENTAGE = 0.75

SPEED_RELEASE_MARGIN = 0.5

TURN_YAW_RATE = 0.05

CEStatus = {
  "OFF": 0,              # Off
  "USER_DISABLED": 1,    # "Experimental Mode" disabled by user
  "USER_OVERRIDDEN": 2,  # "Experimental Mode" enabled by user
  "CURVATURE": 3,        # Road curvature condition
  "LEAD": 4,             # Slower lead vehicle condition
  "SIGNAL": 5,           # Turn signal condition
  "SPEED": 6,            # Speed condition
  "SPEED_LIMIT": 7,      # Speed limit controller condition
  "STOP_LIGHT": 8,       # Stop light or sign condition
}

class ConditionalExperimentalMode:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.curvature_filter = FirstOrderFilter(0, 0.9, DT_MDL)
    self.slow_lead_filter = FirstOrderFilter(0, 0.45, DT_MDL)
    self.stop_light_filter = FirstOrderFilter(0, 0.5, DT_MDL)

    self.curve_detected = False
    self.experimental_mode = False
    self.lane_detected_left = False
    self.lane_detected_right = False
    self.slow_lead_detected = False
    self.stop_light_detected = False
    self.stop_light_signal = False

    self.status_value = CEStatus["OFF"]

    self.press_count = None

  def update(self, v_ego, sm, frogpilot_toggles):
    self.update_conditions(v_ego, sm, frogpilot_toggles)

    if self.status_value in (CEStatus["USER_DISABLED"], CEStatus["USER_OVERRIDDEN"]):
      self.experimental_mode = self.status_value == CEStatus["USER_OVERRIDDEN"]
    else:
      if sm["carState"].standstill:
        self.experimental_mode &= self.status_value != CEStatus["LEAD"]
        self.experimental_mode &= self.frogpilot_planner.model_stopped or self.frogpilot_planner.frogpilot_vcruise.forcing_stop

        lead_stopped = self.frogpilot_planner.lead_relevant and self.frogpilot_planner.lead_one.vLead < frogpilot_variables.LEAD_DEPARTURE_SPEED

        stopped_at_light = sm["modelV2"].action.shouldStop and not lead_stopped
        if frogpilot_toggles.conditional_model_stop_time != 0 and stopped_at_light:
          self.experimental_mode = True
          self.status_value = CEStatus["STOP_LIGHT"]
      else:
        self.experimental_mode = self.check_conditions(v_ego, sm, frogpilot_toggles)

      if not self.experimental_mode:
        self.status_value = CEStatus["OFF"]

  def update_override(self, sm):
    press_count = sm["frogpilotCarState"].experimentalModePressCount

    if self.frogpilot_planner.experimental_mode_pressed or (self.press_count is not None and press_count > self.press_count):
      if self.status_value in (CEStatus["USER_DISABLED"], CEStatus["USER_OVERRIDDEN"]):
        self.status_value = CEStatus["OFF"]
      elif sm["selfdriveState"].experimentalMode:
        self.status_value = CEStatus["USER_DISABLED"]
      else:
        self.status_value = CEStatus["USER_OVERRIDDEN"]

    self.press_count = press_count

  def check_conditions(self, v_ego, sm, frogpilot_toggles):
    if self.curve_detected and (not self.frogpilot_planner.frogpilot_following.following_lead or frogpilot_toggles.conditional_curves_lead or self.status_value == CEStatus["CURVATURE"]) and frogpilot_toggles.conditional_curves:
      self.status_value = CEStatus["CURVATURE"]
      return True

    if self.slow_lead_detected and frogpilot_toggles.conditional_lead:
      self.status_value = CEStatus["LEAD"]
      return True

    if (sm["carState"].leftBlinker or sm["carState"].rightBlinker) and v_ego < frogpilot_toggles.conditional_signal:
      lane_detected = self.lane_detected_left if sm["carState"].leftBlinker else self.lane_detected_right
      if not lane_detected or v_ego < frogpilot_toggles.minimum_lane_change_speed or not frogpilot_toggles.conditional_signal_lane_detection:
        self.status_value = CEStatus["SIGNAL"]
        return True

    if self.status_value == CEStatus["SIGNAL"] and abs(v_ego * sm["controlsState"].curvature) >= TURN_YAW_RATE:
      return True

    speed_limit = frogpilot_toggles.conditional_limit_lead if self.frogpilot_planner.lead_relevant else frogpilot_toggles.conditional_limit
    if (1 <= v_ego or self.experimental_mode) and v_ego < speed_limit + (SPEED_RELEASE_MARGIN if self.status_value == CEStatus["SPEED"] else 0):
      self.status_value = CEStatus["SPEED"]
      return True

    if self.frogpilot_planner.frogpilot_vcruise.slc.experimental_mode:
      self.status_value = CEStatus["SPEED_LIMIT"]
      return True

    if self.stop_light_detected and frogpilot_toggles.conditional_model_stop_time != 0:
      self.status_value = CEStatus["STOP_LIGHT"]
      return True

    return False

  def update_conditions(self, v_ego, sm, frogpilot_toggles):
    self.lane_detection(sm, frogpilot_toggles)

    if sm["carState"].standstill:
      self.slow_lead_detected = False

      self.slow_lead_filter.x = 0
      self.stop_light_filter.x = 0
    else:
      self.slow_lead(v_ego, sm, frogpilot_toggles)
      self.curve_detection(v_ego, sm)
      self.stop_sign_and_light(v_ego, sm, frogpilot_toggles.conditional_model_stop_time or frogpilot_variables.PLANNER_TIME - 2)

  def curve_detection(self, v_ego, sm):
    signaling = sm["carState"].leftBlinker or sm["carState"].rightBlinker

    self.curvature_filter.update(self.frogpilot_planner.curve_ahead or (self.frogpilot_planner.driving_in_curve and not signaling))
    self.curve_detected = self.curvature_filter.x >= (1 - frogpilot_variables.THRESHOLD if self.curve_detected else frogpilot_variables.THRESHOLD) and v_ego > frogpilot_variables.CRUISING_SPEED

  def lane_detection(self, sm, frogpilot_toggles):
    if not sm["carState"].leftBlinker:
      self.lane_detected_left = self.frogpilot_planner.lane_width_left >= frogpilot_toggles.lane_detection_width
    if not sm["carState"].rightBlinker:
      self.lane_detected_right = self.frogpilot_planner.lane_width_right >= frogpilot_toggles.lane_detection_width

  def slow_lead(self, v_ego, sm, frogpilot_toggles):
    if self.frogpilot_planner.lead_relevant:
      lead_speed = self.frogpilot_planner.lead_one.vLead + np.array(sm["modelV2"].leadsV3[0].v) - sm["modelV2"].leadsV3[0].v[0]
      lead_distance = self.frogpilot_planner.lead_one.dRel + np.cumsum((lead_speed[:-1] + lead_speed[1:]) / 2 * np.diff(LEAD_T_IDXS_MODEL))

      following_distance = STOP_DISTANCE + self.frogpilot_planner.frogpilot_following.t_follow * lead_speed[1:]
      safe_distance = np.minimum(following_distance, LEAD_GAP_RATIO * self.frogpilot_planner.lead_one.dRel)
      required_deceleration = max(2 * (v_ego * LEAD_T_IDXS_MODEL[1:] + safe_distance - lead_distance) / LEAD_T_IDXS_MODEL[1:]**2)

      slower_lead = required_deceleration >= LEAD_DECELERATION
      slower_lead &= frogpilot_toggles.conditional_slower_lead

      stopped_lead = min(lead_speed) < 1
      stopped_lead &= frogpilot_toggles.conditional_stopped_lead

      lead_departing = self.frogpilot_planner.lead_one.vLead >= v_ego + frogpilot_variables.LEAD_DEPARTURE_SPEED
      lead_settled = required_deceleration < LEAD_RELEASE_DECELERATION and self.frogpilot_planner.lead_one.aLeadK >= -LEAD_DECELERATION

      lead_held = self.slow_lead_detected and (not lead_settled or (v_ego < frogpilot_variables.CRUISING_SPEED and not lead_departing))

      self.slow_lead_filter.update(bool(slower_lead or stopped_lead or lead_held))
    else:
      self.slow_lead_filter.update(False)

    self.slow_lead_detected = self.slow_lead_filter.x >= (1 - frogpilot_variables.THRESHOLD if self.slow_lead_detected else frogpilot_variables.THRESHOLD)

  def stop_sign_and_light(self, v_ego, sm, model_time):
      model_velocities = [velocity for time, velocity in zip(sm["modelV2"].velocity.t, sm["modelV2"].velocity.x) if time < model_time]
      model_velocities.append(np.interp(model_time, sm["modelV2"].velocity.t, sm["modelV2"].velocity.x))

      slowdown_percentage = SLOWDOWN_RELEASE_PERCENTAGE if self.stop_light_filter.x >= frogpilot_variables.THRESHOLD else SLOWDOWN_PERCENTAGE
      model_slowing = min(model_velocities) <= slowdown_percentage * v_ego
      model_stopping = max(np.interp([0.5, 1.5], sm["modelV2"].velocity.t, sm["modelV2"].velocity.x)) < 1

      self.stop_light_signal = bool(model_slowing or model_stopping)
      self.stop_light_filter.update(self.stop_light_signal and not self.frogpilot_planner.lead_relevant)
      self.stop_light_detected = self.stop_light_filter.x >= frogpilot_variables.THRESHOLD
