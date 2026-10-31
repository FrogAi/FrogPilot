#!/usr/bin/env python3
import json
import math

import cereal.messaging as messaging
import numpy as np

from cereal import log
from openpilot.common.conversions import Conversions as CV
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.drive_helpers import V_CRUISE_MAX
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import A_CHANGE_COST, DANGER_ZONE_COST, J_EGO_COST, STOP_DISTANCE
from openpilot.selfdrive.controls.lib.vehicle_model import ACCELERATION_DUE_TO_GRAVITY

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.controls.lib.conditional_experimental_mode import ConditionalExperimentalMode
from openpilot.frogpilot.controls.lib.frogpilot_acceleration import FrogPilotAcceleration
from openpilot.frogpilot.controls.lib.frogpilot_events import FrogPilotEvents
from openpilot.frogpilot.controls.lib.frogpilot_following import FrogPilotFollowing
from openpilot.frogpilot.controls.lib.frogpilot_vcruise import FrogPilotVCruise
from openpilot.frogpilot.controls.lib.speed_limit_filler import SpeedLimitFiller
from openpilot.frogpilot.controls.lib.weather_checker import WeatherChecker

class FrogPilotPlanner:
  def __init__(self, ThemeManager, frogpilot_api):
    self.cem = ConditionalExperimentalMode(self)
    self.frogpilot_acceleration = FrogPilotAcceleration(self)
    self.frogpilot_events = FrogPilotEvents(self, ThemeManager)
    self.frogpilot_following = FrogPilotFollowing(self)
    self.frogpilot_vcruise = FrogPilotVCruise(self)
    self.frogpilot_weather = WeatherChecker(frogpilot_api)
    self.speed_limit_filler = SpeedLimitFiller()

    self.accel_pressed = False
    self.curve_ahead = False
    self.decel_pressed = False
    self.driving_in_curve = False
    self.experimental_mode_pressed = False
    self.lateral_check = False
    self.lead_relevant = False
    self.model_stopped = False

    self.accel_press_count = 0
    self.decel_press_count = 0
    self.experimental_mode_press_count = None
    self.lane_width_left = 0
    self.lane_width_right = 0
    self.lateral_acceleration = 0
    self.model_length = 0
    self.peak_lateral_acceleration = 0
    self.road_curvature = 0
    self.roll_compensation = 0
    self.time_to_curve = 0
    self.v_cruise = 0

    last_gps_position = json.loads(frogpilot_variables.params.get("LastGPSPosition") or "{}")
    self.gps_bearing = last_gps_position.get("bearing", 0.)

    self.lane_width_filter = FirstOrderFilter(np.zeros(2), 0.15, DT_MDL)
    self.road_edge_filter = FirstOrderFilter(np.zeros(2), 0.15, DT_MDL)

    self.ui_event_sock = messaging.sub_sock("frogpilotUIEvent")

  def update(self, now, time_validated, sm, frogpilot_toggles):
    self.accel_pressed = sm["frogpilotCarState"].accelPressCount > self.accel_press_count
    self.decel_pressed = sm["frogpilotCarState"].decelPressCount > self.decel_press_count

    if self.experimental_mode_press_count is not None:
      self.experimental_mode_pressed = sm["frogpilotCarState"].experimentalModePressCount > self.experimental_mode_press_count

    self.accel_press_count = sm["frogpilotCarState"].accelPressCount
    self.decel_press_count = sm["frogpilotCarState"].decelPressCount
    self.experimental_mode_press_count = sm["frogpilotCarState"].experimentalModePressCount

    self.frogpilot_vcruise.slc.speed_limit_accepted = False
    for msg in messaging.drain_sock(self.ui_event_sock):
      ui_event = msg.frogpilotUIEvent
      if ui_event.which() == "experimentalModePressed":
        self.experimental_mode_pressed = True
      elif ui_event.which() == "screenRecorderEvent":
        self.frogpilot_events.screen_recorder_events.append(ui_event.screenRecorderEvent.raw)
      elif ui_event.which() == "speedLimitAccepted":
        self.frogpilot_vcruise.slc.speed_limit_accepted = True

    self.lead_one = sm["radarState"].leadOne

    long_control_active = sm["carControl"].longActive

    v_cruise = min(sm["controlsState"].vCruise, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    self.model_length = sm["modelV2"].position.x[-1]

    self.lead_relevant = self.is_lead_relevant(self.lead_one, sm["carState"].standstill, v_ego)

    self.frogpilot_acceleration.update(v_ego, sm, frogpilot_toggles)

    self.cem.update(v_ego, sm, frogpilot_toggles)

    self.driving_in_curve = abs(self.lateral_acceleration) >= frogpilot_variables.MINIMUM_LATERAL_ACCELERATION

    self.frogpilot_events.update(long_control_active, sm, frogpilot_toggles)

    self.frogpilot_following.update(long_control_active, v_ego, sm, frogpilot_toggles)

    localizer_valid = (sm["liveLocationKalman"].status == log.LiveLocationKalman.Status.valid) and sm["liveLocationKalman"].positionGeodetic.valid
    if sm.alive["liveLocationKalman"] and sm["liveLocationKalman"].gpsOK and localizer_valid:
      gps_position = {
        "latitude": sm["liveLocationKalman"].positionGeodetic.value[0],
        "longitude": sm["liveLocationKalman"].positionGeodetic.value[1],
        "bearing": math.degrees(sm["liveLocationKalman"].calibratedOrientationNED.value[2]),
        "location_mono_time": sm.logMonoTime["liveLocationKalman"],
      }
      self.gps_bearing = gps_position["bearing"]

      frogpilot_variables.params_memory.put("LastGPSPosition", json.dumps(gps_position))
    else:
      gps_position = None

      frogpilot_variables.params_memory.remove("LastGPSPosition")

    map_match = {}
    if gps_position and (frogpilot_toggles.speed_limit_controller or frogpilot_toggles.show_speed_limits or frogpilot_toggles.speed_limit_filler):
      map_match = json.loads(frogpilot_variables.params_memory.get("MapMatchedWay") or "{}")

      if not frogpilot_utilities.valid_map_match(map_match, gps_position["location_mono_time"]):
        map_match = {}

    if frogpilot_toggles.speed_limit_filler:
      self.speed_limit_filler.update(gps_position, map_match, v_ego, sm, frogpilot_toggles)

    if v_ego >= frogpilot_toggles.minimum_lane_change_speed:
      lane_widths, road_edge_distances = frogpilot_utilities.calculate_lane_widths(sm["modelV2"])
      self.lane_width_filter.update(lane_widths)
      self.road_edge_filter.update(road_edge_distances)

      self.lane_width_left, self.lane_width_right = np.where(self.road_edge_filter.x >= self.lane_width_filter.x, self.lane_width_filter.x, 0).tolist()
    else:
      self.lane_width_left = 0
      self.lane_width_right = 0

    self.lateral_acceleration = v_ego**2 * sm["controlsState"].curvature

    self.lateral_check = v_ego >= frogpilot_toggles.pause_lateral_below_speed
    self.lateral_check |= not (sm["carState"].leftBlinker or sm["carState"].rightBlinker) and frogpilot_toggles.pause_lateral_below_signal
    self.lateral_check |= sm["carState"].standstill
    self.lateral_check &= not sm["frogpilotCarState"].pauseLateral

    self.model_stopped = self.model_length < frogpilot_variables.CRUISING_SPEED * frogpilot_variables.PLANNER_TIME

    self.roll_compensation = sm["liveParameters"].roll * ACCELERATION_DUE_TO_GRAVITY

    self.frogpilot_vcruise.csc.update_max_limit(sm, frogpilot_toggles)
    self.frogpilot_vcruise.csc.update_lateral_acceleration(frogpilot_toggles)

    self.road_curvature, self.time_to_curve, road_curvature_peak = frogpilot_utilities.calculate_road_curvature(
      sm["modelV2"], v_ego, self.frogpilot_vcruise.csc.lateral_acceleration, self.roll_compensation
    )
    self.peak_lateral_acceleration = v_ego**2 * road_curvature_peak

    if self.curve_ahead or self.cem.curve_detected:
      self.curve_ahead = self.peak_lateral_acceleration > frogpilot_variables.CURVE_AHEAD_EXIT
    else:
      self.curve_ahead = self.peak_lateral_acceleration > frogpilot_variables.CURVE_AHEAD_ENTER and self.time_to_curve > 1
    self.curve_ahead &= v_ego > frogpilot_variables.CRUISING_SPEED
    self.curve_ahead &= not (sm["carState"].leftBlinker or sm["carState"].rightBlinker)

    self.v_cruise = self.frogpilot_vcruise.update(gps_position, map_match, now, time_validated, v_cruise, v_ego, sm, frogpilot_toggles)

    if gps_position and time_validated and frogpilot_toggles.weather_presets:
      self.frogpilot_weather.update_weather(gps_position, now, frogpilot_toggles)
    elif time_validated and frogpilot_toggles.weather_presets:
      self.frogpilot_weather.weather_id = 0
    else:
      self.frogpilot_weather.invalidate()

  def is_lead_relevant(self, lead, standstill, v_ego):
    following_speed = max(v_ego, frogpilot_variables.CRUISING_SPEED)
    lead_speed = lead.vLead + min(lead.aLeadK, 0) * frogpilot_variables.PLANNER_TIME / 2

    relevant_lead = lead.status
    relevant_lead &= lead.dRel < (0 if standstill else self.model_length) + STOP_DISTANCE + frogpilot_variables.LEAD_STOP_MARGIN
    relevant_lead &= lead.dRel < STOP_DISTANCE + following_speed * frogpilot_variables.MAX_T_FOLLOW + max(following_speed - lead_speed, 0) * frogpilot_variables.PLANNER_TIME
    return relevant_lead

  def publish(self, theme_update_count, sm, pm, frogpilot_toggles, toggles_update_time):
    frogpilot_plan_send = messaging.new_message("frogpilotPlan")
    frogpilot_plan_send.valid = sm.all_checks(service_list=["carState", "controlsState"])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    frogpilotPlan.accelerationJerk = float(A_CHANGE_COST * self.frogpilot_following.acceleration_jerk)
    frogpilotPlan.dangerJerk = float(DANGER_ZONE_COST * self.frogpilot_following.danger_jerk)
    frogpilotPlan.speedJerk = float(J_EGO_COST * self.frogpilot_following.speed_jerk)
    frogpilotPlan.tFollow = float(self.frogpilot_following.t_follow)

    frogpilotPlan.ceStatus = self.cem.status_value

    frogpilotPlan.cscActive = self.frogpilot_vcruise.csc_active
    frogpilotPlan.cscControllingSpeed = self.frogpilot_vcruise.csc_controlling_speed
    frogpilotPlan.cscMaxLateralAcceleration = self.frogpilot_vcruise.csc.max_limit
    frogpilotPlan.cscSpeed = float(self.frogpilot_vcruise.csc_target)
    frogpilotPlan.cscTraining = self.frogpilot_vcruise.csc.enable_training

    frogpilotPlan.desiredFollowDistance = self.frogpilot_following.desired_follow_distance

    user_disabled = frogpilot_toggles.conditional_experimental_mode and self.cem.status_value == frogpilot_variables.CE_STATUS["USER_DISABLED"]
    frogpilotPlan.experimentalMode = (self.cem.experimental_mode or self.frogpilot_vcruise.slc.experimental_mode) and not user_disabled

    frogpilotPlan.forcingStop = self.frogpilot_vcruise.forcing_stop
    frogpilotPlan.forcingStopLength = self.frogpilot_vcruise.stop_distance

    frogpilotPlan.frogpilotEvents = self.frogpilot_events.events.to_msg()

    frogpilotPlan.gpsBearing = self.gps_bearing

    frogpilotPlan.increasedStoppedDistance = frogpilot_toggles.increase_stopped_distance if not sm["frogpilotCarState"].trafficModeEnabled else 0
    if self.frogpilot_weather.weather_id != 0 and not sm["frogpilotCarState"].trafficModeEnabled:
      frogpilotPlan.increasedStoppedDistance += self.frogpilot_weather.increase_stopped_distance

    frogpilotPlan.laneWidthLeft = self.lane_width_left
    frogpilotPlan.laneWidthRight = self.lane_width_right

    frogpilotPlan.lateralCheck = self.lateral_check

    frogpilotPlan.maxAcceleration = float(self.frogpilot_acceleration.max_accel)
    frogpilotPlan.minAcceleration = float(self.frogpilot_acceleration.min_accel)

    frogpilotPlan.redLight = self.cem.stop_light_detected

    frogpilotPlan.roadCurvature = self.road_curvature

    frogpilotPlan.slcMapSpeedLimit = self.frogpilot_vcruise.slc.map_speed_limit
    frogpilotPlan.slcMapboxSpeedLimit = self.frogpilot_vcruise.slc.mapbox_speed_limit
    frogpilotPlan.slcNextSpeedLimit = self.frogpilot_vcruise.slc.next_speed_limit
    frogpilotPlan.slcOverriddenSpeed = self.frogpilot_vcruise.slc.overridden_speed
    frogpilotPlan.slcSpeedLimit = self.frogpilot_vcruise.slc_target
    frogpilotPlan.slcSpeedLimitOffset = self.frogpilot_vcruise.slc_offset
    frogpilotPlan.slcSpeedLimitSource = self.frogpilot_vcruise.slc.source
    frogpilotPlan.speedLimitChanged = self.frogpilot_vcruise.slc.speed_limit_changed_timer > DT_MDL
    frogpilotPlan.unconfirmedSlcSpeedLimit = self.frogpilot_vcruise.slc.unconfirmed_speed_limit

    frogpilotPlan.themeUpdateCount = theme_update_count

    frogpilotPlan.togglesUpdateTime = toggles_update_time

    frogpilotPlan.vCruise = float(self.v_cruise)

    frogpilotPlan.weatherDaytime = self.frogpilot_weather.is_daytime
    frogpilotPlan.weatherId = self.frogpilot_weather.weather_id

    frogpilotPlan.wheelImageUpdateCount = self.frogpilot_events.wheel_image_update_count

    pm.send("frogpilotPlan", frogpilot_plan_send)
