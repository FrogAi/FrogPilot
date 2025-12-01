#!/usr/bin/env python3
import json
import time

import cereal.messaging as messaging

from openpilot.common.constants import ACCELERATION_DUE_TO_GRAVITY, CV
from openpilot.common.gps import get_gps_location_service
from openpilot.common.params import Params
from openpilot.selfdrive.car.cruise import V_CRUISE_MAX
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import A_CHANGE_COST, DANGER_ZONE_COST, J_EGO_COST, STOP_DISTANCE

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.controls.lib.conditional_experimental_mode import ConditionalExperimentalMode
from openpilot.frogpilot.controls.lib.frogpilot_acceleration import FrogPilotAcceleration
from openpilot.frogpilot.controls.lib.frogpilot_events import FrogPilotEvents
from openpilot.frogpilot.controls.lib.frogpilot_following import FrogPilotFollowing
from openpilot.frogpilot.controls.lib.frogpilot_vcruise import FrogPilotVCruise

class FrogPilotPlanner:
  def __init__(self):
    self.params = Params(return_defaults=True)

    self.frogpilot_acceleration = FrogPilotAcceleration(self)
    self.frogpilot_cem = ConditionalExperimentalMode(self)
    self.frogpilot_events = FrogPilotEvents(self)
    self.frogpilot_following = FrogPilotFollowing(self)
    self.frogpilot_vcruise = FrogPilotVCruise(self)

    self.accel_pressed = False
    self.curve_ahead = False
    self.decel_pressed = False
    self.driving_in_curve = False
    self.experimental_mode_pressed = False
    self.gps_valid = False
    self.lead_relevant = False
    self.model_stopped = False

    self.accel_press_count = 0
    self.decel_press_count = 0
    self.experimental_mode_press_count = None
    self.lateral_acceleration = 0
    self.model_length = 0
    self.next_gps_position_save = 0
    self.peak_lateral_acceleration = 0
    self.road_curvature = 0
    self.road_curvature_peak = 0
    self.roll_compensation = 0
    self.time_to_curve = 0
    self.v_cruise = 0

    last_gps_position = json.loads(self.params.get("LastGPSPosition") or "{}")
    self.gps_bearing = last_gps_position.get("bearing", 0.)
    self.gps_position = None
    self.last_gps_position = None

    self.gps_location_service = get_gps_location_service(self.params)

    self.ui_event_sock = messaging.sub_sock("frogpilotUIEvent")

  def update(self, now, time_validated, sm, frogpilot_toggles):
    self.accel_pressed = sm["frogpilotCarState"].accelPressCount > self.accel_press_count
    self.decel_pressed = sm["frogpilotCarState"].decelPressCount > self.decel_press_count

    if self.experimental_mode_press_count is not None:
      self.experimental_mode_pressed = sm["frogpilotCarState"].experimentalModePressCount > self.experimental_mode_press_count

    self.accel_press_count = sm["frogpilotCarState"].accelPressCount
    self.decel_press_count = sm["frogpilotCarState"].decelPressCount
    self.experimental_mode_press_count = sm["frogpilotCarState"].experimentalModePressCount

    for msg in messaging.drain_sock(self.ui_event_sock):
      ui_event = msg.frogpilotUIEvent
      if ui_event.which() == "experimentalModePressed":
        self.experimental_mode_pressed = True

    self.lead_one = sm["radarState"].leadOne

    long_control_active = sm["carControl"].longActive

    v_cruise = min(sm["carState"].vCruise, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    self.model_length = sm["modelV2"].position.x[-1]

    self.lead_relevant = self.is_lead_relevant(self.lead_one, sm["carState"].standstill, v_ego)

    self.frogpilot_acceleration.update(v_ego, sm, frogpilot_toggles)

    self.frogpilot_cem.update(v_ego, sm, frogpilot_toggles)

    self.driving_in_curve = abs(self.lateral_acceleration) >= frogpilot_variables.MINIMUM_LATERAL_ACCELERATION

    self.frogpilot_events.update(long_control_active, sm, frogpilot_toggles)

    self.frogpilot_following.update(v_ego, sm, frogpilot_toggles)

    gps_location = sm[self.gps_location_service]
    self.gps_valid = frogpilot_utilities.is_gps_location_valid(self.gps_location_service, sm)
    if self.gps_valid:
      self.gps_bearing = gps_location.bearingDeg
      self.gps_position = {
        "latitude": gps_location.latitude,
        "longitude": gps_location.longitude,
        "bearing": gps_location.bearingDeg,
      }
      self.last_gps_position = self.gps_position

      if time.monotonic() >= self.next_gps_position_save:
        self.params.put_nonblocking("LastGPSPosition", json.dumps(self.gps_position))
        self.next_gps_position_save = time.monotonic() + frogpilot_variables.GPS_POSITION_SAVE_INTERVAL
    else:
      self.gps_position = None

    self.lateral_acceleration = v_ego**2 * sm["controlsState"].curvature if sm.all_checks(service_list=["carState", "controlsState"]) else 0

    self.model_stopped = self.model_length < frogpilot_variables.CRUISING_SPEED * frogpilot_variables.PLANNER_TIME

    self.roll_compensation = sm["liveParameters"].roll * ACCELERATION_DUE_TO_GRAVITY

    self.frogpilot_vcruise.csc.update_max_limit(sm, frogpilot_toggles)
    self.frogpilot_vcruise.csc.update_lateral_acceleration(frogpilot_toggles)

    self.road_curvature, self.time_to_curve, self.road_curvature_peak = frogpilot_utilities.calculate_road_curvature(
      sm["modelV2"], v_ego, self.frogpilot_vcruise.csc.lateral_acceleration, self.roll_compensation
    )
    self.peak_lateral_acceleration = v_ego**2 * self.road_curvature_peak

    if self.curve_ahead or self.frogpilot_cem.curve_detected:
      self.curve_ahead = self.peak_lateral_acceleration > frogpilot_variables.CURVE_AHEAD_EXIT
    else:
      self.curve_ahead = self.peak_lateral_acceleration > frogpilot_variables.CURVE_AHEAD_ENTER and self.time_to_curve > 1
    self.curve_ahead &= v_ego > frogpilot_variables.CRUISING_SPEED
    self.curve_ahead &= not (sm["carState"].leftBlinker or sm["carState"].rightBlinker)

    self.v_cruise = self.frogpilot_vcruise.update(long_control_active, v_cruise, v_ego, sm, frogpilot_toggles)

  def is_lead_relevant(self, lead, standstill, v_ego):
    following_speed = max(v_ego, frogpilot_variables.CRUISING_SPEED)
    lead_speed = lead.vLead + min(lead.aLeadK, 0) * frogpilot_variables.PLANNER_TIME / 2

    relevant_lead = lead.status
    relevant_lead &= lead.dRel < (0 if standstill else self.model_length) + STOP_DISTANCE + frogpilot_variables.LEAD_STOP_MARGIN
    relevant_lead &= lead.dRel < STOP_DISTANCE + following_speed * frogpilot_variables.MAX_T_FOLLOW + max(following_speed - lead_speed, 0) * frogpilot_variables.PLANNER_TIME
    return relevant_lead

  def publish(self, theme_update_count, sm, pm, frogpilot_toggles):
    frogpilot_plan_send = messaging.new_message("frogpilotPlan")
    frogpilot_plan_send.valid = sm.all_checks(service_list=["carState", "controlsState", "selfdriveState", "radarState"])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    frogpilotPlan.accelerationJerk = float(A_CHANGE_COST * self.frogpilot_following.acceleration_jerk)
    frogpilotPlan.dangerJerk = float(DANGER_ZONE_COST * self.frogpilot_following.danger_jerk)
    frogpilotPlan.speedJerk = float(J_EGO_COST * self.frogpilot_following.speed_jerk)

    frogpilotPlan.ceStatus = self.frogpilot_cem.status_value

    frogpilotPlan.cscActive = self.frogpilot_vcruise.csc_active
    frogpilotPlan.cscControllingSpeed = self.frogpilot_vcruise.csc_controlling_speed
    frogpilotPlan.cscLateralAcceleration = float(self.frogpilot_vcruise.csc.lateral_acceleration)
    frogpilotPlan.cscSpeed = float(self.frogpilot_vcruise.csc_target)
    frogpilotPlan.cscTraining = self.frogpilot_vcruise.csc.enable_training

    frogpilotPlan.experimentalMode = self.frogpilot_cem.experimental_mode
    frogpilotPlan.experimentalMode &= not frogpilot_toggles.conditional_experimental_mode or self.frogpilot_cem.status_value != frogpilot_variables.CE_STATUS["USER_DISABLED"]

    frogpilotPlan.forcingStop = self.frogpilot_vcruise.forcing_stop
    frogpilotPlan.forcingStopLength = self.frogpilot_vcruise.stop_distance

    frogpilotPlan.frogpilotEvents = self.frogpilot_events.events.to_msg()

    frogpilotPlan.gpsBearing = self.gps_bearing

    frogpilotPlan.increasedStoppedDistance = frogpilot_toggles.increase_stopped_distance

    frogpilotPlan.maxAcceleration = float(self.frogpilot_acceleration.max_accel)
    frogpilotPlan.minAcceleration = float(self.frogpilot_acceleration.min_accel)

    frogpilotPlan.redLight = self.frogpilot_cem.stop_light_detected

    frogpilotPlan.roadCurvature = self.road_curvature

    frogpilotPlan.tFollow = float(self.frogpilot_following.t_follow)

    frogpilotPlan.themeUpdateCount = theme_update_count

    frogpilotPlan.vCruise = float(self.v_cruise)

    pm.send("frogpilotPlan", frogpilot_plan_send)
