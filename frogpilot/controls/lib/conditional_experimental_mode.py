#!/usr/bin/env python3
import numpy as np

from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import STOP_DISTANCE

from openpilot.frogpilot.common.frogpilot_utilities import calculate_lane_width
from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED, THRESHOLD

KINEMATIC_LEAD_DECELERATION = 2.0
LEAD_SPEED_DIFFERENCE = 2.0

SLOWDOWN_PERCENTAGE = 0.50
SLOWDOWN_RELEASE_PERCENTAGE = 0.75

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
    self.slow_lead_filter = FirstOrderFilter(0, 0.6, DT_MDL)
    self.stop_light_filter = FirstOrderFilter(0, 0.5, DT_MDL)

    self.curve_detected = False
    self.experimental_mode = False
    self.slow_lead_detected = False
    self.stop_light_detected = False
    self.stop_light_signal = False

    self.status_value = CEStatus["OFF"]

  def update(self, v_ego, sm, frogpilot_toggles):
    self.status_value = self.frogpilot_planner.params_memory.get("CEStatus")

    if not sm["carState"].standstill:
      self.update_conditions(v_ego, sm, frogpilot_toggles)

    if self.status_value not in (CEStatus["USER_DISABLED"], CEStatus["USER_OVERRIDDEN"]) and not sm["carState"].standstill:
      self.experimental_mode = self.check_conditions(v_ego, sm, frogpilot_toggles)
      self.frogpilot_planner.params_memory.put("CEStatus", self.status_value if self.experimental_mode else CEStatus["OFF"])
    else:
      stop_required = self.frogpilot_planner.model_stopped or self.frogpilot_planner.frogpilot_vcruise.forcing_stop
      self.experimental_mode &= sm["carState"].standstill and stop_required
      self.experimental_mode &= self.status_value != CEStatus["USER_DISABLED"]
      self.experimental_mode |= self.status_value == CEStatus["USER_OVERRIDDEN"]

      self.stop_light_detected &= self.status_value not in (CEStatus["USER_DISABLED"], CEStatus["USER_OVERRIDDEN"])

      if sm["carState"].standstill:
        self.stop_light_filter.x = 0

  def check_conditions(self, v_ego, sm, frogpilot_toggles):
    if self.curve_detected and (not self.frogpilot_planner.frogpilot_following.following_lead or frogpilot_toggles.conditional_curves_lead) and frogpilot_toggles.conditional_curves:
      self.status_value = CEStatus["CURVATURE"]
      return True

    if self.slow_lead_detected and frogpilot_toggles.conditional_lead:
      self.status_value = CEStatus["LEAD"]
      return True

    if (sm["carState"].leftBlinker or sm["carState"].rightBlinker) and v_ego < frogpilot_toggles.conditional_signal:
      desired_lane = 0
      if frogpilot_toggles.conditional_signal_lane_detection and v_ego >= frogpilot_toggles.minimum_lane_change_speed:
        current_lane = sm["modelV2"].laneLines[1 if sm["carState"].leftBlinker else 2]
        lane = sm["modelV2"].laneLines[0 if sm["carState"].leftBlinker else 3]
        road_edge = sm["modelV2"].roadEdges[0 if sm["carState"].leftBlinker else 1]
        desired_lane = min(calculate_lane_width(lane, current_lane), calculate_lane_width(road_edge, current_lane))

      if desired_lane < frogpilot_toggles.lane_detection_width or not frogpilot_toggles.conditional_signal_lane_detection:
        self.status_value = CEStatus["SIGNAL"]
        return True

    if 1 <= v_ego < (frogpilot_toggles.conditional_limit_lead if self.frogpilot_planner.frogpilot_following.following_lead else frogpilot_toggles.conditional_limit):
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
    self.curve_detection(v_ego, frogpilot_toggles)
    self.slow_lead(v_ego, sm, frogpilot_toggles)
    self.stop_sign_and_light(v_ego, sm, frogpilot_toggles.conditional_model_stop_time)

  def curve_detection(self, v_ego, frogpilot_toggles):
    self.curvature_filter.update(self.frogpilot_planner.driving_in_curve or self.frogpilot_planner.road_curvature_detected)
    self.curve_detected = self.curvature_filter.x >= THRESHOLD and v_ego > CRUISING_SPEED

  def slow_lead(self, v_ego, sm, frogpilot_toggles):
    if self.frogpilot_planner.tracking_lead and self.frogpilot_planner.lead_one.status:
      predicted_lead_speed = self.frogpilot_planner.lead_one.vLead
      if sm["modelV2"].leadsV3[0].prob > frogpilot_toggles.lead_detection_probability:
        predicted_lead_speed = max(self.frogpilot_planner.lead_one.vLead + min(sm["modelV2"].leadsV3[0].v) - sm["modelV2"].leadsV3[0].v[0], 0)

      required_deceleration = (v_ego**2 - self.frogpilot_planner.lead_one.vLead**2) / (2 * max(self.frogpilot_planner.lead_one.dRel - STOP_DISTANCE, 1))

      slower_lead = v_ego - self.frogpilot_planner.lead_one.vLead >= LEAD_SPEED_DIFFERENCE
      slower_lead |= self.frogpilot_planner.lead_one.vLead - predicted_lead_speed >= LEAD_SPEED_DIFFERENCE
      slower_lead |= required_deceleration >= KINEMATIC_LEAD_DECELERATION and self.frogpilot_planner.lead_one.vLead >= 1 and v_ego > CRUISING_SPEED
      slower_lead &= frogpilot_toggles.conditional_slower_lead

      stopped_lead = predicted_lead_speed < (2.0 if self.slow_lead_detected else 1)
      stopped_lead &= frogpilot_toggles.conditional_stopped_lead

      self.slow_lead_filter.update(slower_lead or stopped_lead)
    elif not self.frogpilot_planner.tracking_lead:
      self.slow_lead_filter.x = 0

    self.slow_lead_detected = self.slow_lead_filter.x >= (1 - THRESHOLD if self.slow_lead_detected else THRESHOLD)

  def stop_sign_and_light(self, v_ego, sm, model_time):
    if not sm["frogpilotCarState"].trafficModeEnabled:
      model_velocities = [velocity for time, velocity in zip(sm["modelV2"].velocity.t, sm["modelV2"].velocity.x) if time < model_time]
      model_velocities.append(np.interp(model_time, sm["modelV2"].velocity.t, sm["modelV2"].velocity.x))

      slowdown_percentage = SLOWDOWN_RELEASE_PERCENTAGE if self.stop_light_filter.x >= THRESHOLD else SLOWDOWN_PERCENTAGE
      model_slowing = min(model_velocities) <= slowdown_percentage * v_ego and not self.curve_detected
      model_stopping = max(np.interp([0.5, 1.5], sm["modelV2"].velocity.t, sm["modelV2"].velocity.x)) < 1

      self.stop_light_signal = bool(model_slowing or model_stopping)
      self.stop_light_filter.update(self.stop_light_signal)
      self.stop_light_detected = self.stop_light_filter.x >= THRESHOLD and not self.frogpilot_planner.tracking_lead
    else:
      self.stop_light_detected = False
      self.stop_light_signal = False

      self.stop_light_filter.x = 0
