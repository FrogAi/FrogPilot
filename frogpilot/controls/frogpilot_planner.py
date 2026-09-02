#!/usr/bin/env python3
import json

import cereal.messaging as messaging

from openpilot.common.constants import CV
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.gps import get_gps_location_service
from openpilot.common.params import Params
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.car.cruise import V_CRUISE_MAX
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import A_CHANGE_COST, DANGER_ZONE_COST, J_EGO_COST, STOP_DISTANCE

from openpilot.frogpilot.common.frogpilot_utilities import calculate_lane_width, is_gps_location_valid, select_road_curvature
from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED, MINIMUM_LATERAL_ACCELERATION, PLANNER_TIME, THRESHOLD
from openpilot.frogpilot.controls.lib.conditional_experimental_mode import ConditionalExperimentalMode
from openpilot.frogpilot.controls.lib.frogpilot_acceleration import FrogPilotAcceleration
from openpilot.frogpilot.controls.lib.frogpilot_events import FrogPilotEvents
from openpilot.frogpilot.controls.lib.frogpilot_following import FrogPilotFollowing
from openpilot.frogpilot.controls.lib.frogpilot_vcruise import FrogPilotVCruise
from openpilot.frogpilot.controls.lib.weather_checker import WeatherChecker

CURVE_DETECTION_ENTER = 1.0
CURVE_DETECTION_EXIT = 0.7

class FrogPilotPlanner:
  def __init__(self, error_log, ThemeManager):
    self.params = Params(return_defaults=True)
    self.params_memory = Params(memory=True)

    self.frogpilot_acceleration = FrogPilotAcceleration(self)
    self.frogpilot_cem = ConditionalExperimentalMode(self)
    self.frogpilot_events = FrogPilotEvents(self, error_log, ThemeManager)
    self.frogpilot_following = FrogPilotFollowing(self)
    self.frogpilot_vcruise = FrogPilotVCruise(self)
    self.frogpilot_weather = WeatherChecker()

    self.driving_in_curve = False
    self.gps_valid = False
    self.lateral_check = False
    self.model_stopped = False
    self.road_curvature_detected = False
    self.tracking_lead = False

    self.lane_width_left = 0
    self.lane_width_right = 0
    self.lateral_acceleration = 0
    self.model_length = 0
    self.road_curvature = 0
    self.time_to_curve = 0
    self.v_cruise = 0

    self.gps_position = None

    self.gps_location_service = get_gps_location_service(self.params)

    self.tracking_lead_filter = FirstOrderFilter(0, 0.5, DT_MDL)

  def update(self, now, time_validated, sm, frogpilot_toggles):
    self.lead_one = sm["radarState"].leadOne

    long_control_active = sm["carControl"].longActive

    v_cruise = min(sm["carState"].vCruise, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    if long_control_active:
      self.frogpilot_acceleration.update(v_ego, sm, frogpilot_toggles)
    else:
      self.frogpilot_acceleration.max_accel = 0
      self.frogpilot_acceleration.min_accel = 0

    if long_control_active and frogpilot_toggles.conditional_experimental_mode:
      self.frogpilot_cem.update(v_ego, sm, frogpilot_toggles)
    else:
      self.frogpilot_cem.experimental_mode = False
      self.frogpilot_cem.stop_sign_and_light(v_ego, sm, PLANNER_TIME - 2)

    self.driving_in_curve = abs(self.lateral_acceleration) >= MINIMUM_LATERAL_ACCELERATION

    self.frogpilot_events.update(long_control_active, v_cruise, sm, frogpilot_toggles)

    self.frogpilot_following.update(long_control_active, v_ego, sm, frogpilot_toggles)

    gps_location = sm[self.gps_location_service]
    self.gps_position = {
      "latitude": gps_location.latitude,
      "longitude": gps_location.longitude,
      "bearing": gps_location.bearingDeg,
    }
    self.gps_valid = is_gps_location_valid(gps_location, self.gps_location_service, sm)
    self.params_memory.put("LastGPSPosition", json.dumps(self.gps_position))

    if v_ego >= frogpilot_toggles.minimum_lane_change_speed:
      self.lane_width_left = calculate_lane_width(sm["modelV2"].laneLines[0], sm["modelV2"].laneLines[1], sm["modelV2"].roadEdges[0])
      self.lane_width_right = calculate_lane_width(sm["modelV2"].laneLines[3], sm["modelV2"].laneLines[2], sm["modelV2"].roadEdges[1])
    else:
      self.lane_width_left = 0
      self.lane_width_right = 0

    self.lateral_acceleration = v_ego**2 * sm["controlsState"].curvature if sm.all_checks(service_list=["carState", "controlsState"]) else 0

    self.lateral_check = v_ego >= frogpilot_toggles.pause_lateral_below_speed
    self.lateral_check |= not (sm["carState"].leftBlinker or sm["carState"].rightBlinker) and frogpilot_toggles.pause_lateral_below_signal
    self.lateral_check |= sm["carState"].standstill
    self.lateral_check &= not sm["frogpilotCarState"].pauseLateral

    self.model_length = sm["modelV2"].position.x[-1]

    self.model_stopped = self.model_length < CRUISING_SPEED * PLANNER_TIME

    self.road_curvature, self.time_to_curve, road_curvature_peak = select_road_curvature(sm["modelV2"], v_ego, self.frogpilot_vcruise.csc.lateral_acceleration)

    self.road_curvature_detected = v_ego**2 * road_curvature_peak > (CURVE_DETECTION_EXIT if self.road_curvature_detected else CURVE_DETECTION_ENTER)
    self.road_curvature_detected &= v_ego > CRUISING_SPEED
    self.road_curvature_detected &= not (sm["carState"].leftBlinker or sm["carState"].rightBlinker)

    if not sm["carState"].standstill:
      self.tracking_lead = self.update_lead_status()

    self.v_cruise = self.frogpilot_vcruise.update(long_control_active, now, time_validated, v_cruise, v_ego, sm, frogpilot_toggles)

    if self.gps_valid and time_validated and frogpilot_toggles.weather_presets:
      self.frogpilot_weather.update_weather(self.gps_position, now, frogpilot_toggles)
    else:
      self.frogpilot_weather.invalidate()

  def update_lead_status(self):
    closing_lead = self.lead_one.status
    closing_lead &= self.lead_one.vRel < 0
    closing_lead &= self.lead_one.dRel + (self.lead_one.vRel * PLANNER_TIME) < self.model_length + STOP_DISTANCE

    following_lead = self.lead_one.status
    following_lead &= self.lead_one.dRel < self.model_length + STOP_DISTANCE

    self.tracking_lead_filter.update(following_lead)
    return closing_lead or self.tracking_lead_filter.x >= THRESHOLD

  def publish(self, theme_updated, sm, pm, frogpilot_toggles):
    frogpilot_plan_send = messaging.new_message("frogpilotPlan")
    frogpilot_plan_send.valid = sm.all_checks(service_list=["carState", "controlsState", "selfdriveState", "radarState"])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    frogpilotPlan.accelerationJerk = float(A_CHANGE_COST * self.frogpilot_following.acceleration_jerk)
    frogpilotPlan.dangerJerk = float(DANGER_ZONE_COST * self.frogpilot_following.danger_jerk)
    frogpilotPlan.speedJerk = float(J_EGO_COST * self.frogpilot_following.speed_jerk)
    frogpilotPlan.tFollow = float(self.frogpilot_following.t_follow)
    frogpilotPlan.trackingLead = self.tracking_lead

    frogpilotPlan.cscActive = self.frogpilot_vcruise.csc_active
    frogpilotPlan.cscControllingSpeed = self.frogpilot_vcruise.csc_controlling_speed
    frogpilotPlan.cscMaxLateralAcceleration = float(self.frogpilot_vcruise.csc.max_limit)
    frogpilotPlan.cscSpeed = float(self.frogpilot_vcruise.csc_target)
    frogpilotPlan.cscTraining = self.frogpilot_vcruise.csc.enable_training

    frogpilotPlan.desiredFollowDistance = int(self.frogpilot_following.desired_follow_distance)

    frogpilotPlan.experimentalMode = self.frogpilot_cem.experimental_mode or self.frogpilot_vcruise.slc.experimental_mode

    frogpilotPlan.forcingStop = self.frogpilot_vcruise.forcing_stop
    frogpilotPlan.forcingStopLength = self.frogpilot_vcruise.tracked_model_length

    frogpilotPlan.frogpilotEvents = self.frogpilot_events.events.to_msg()

    frogpilotPlan.frogpilotToggles = json.dumps(vars(frogpilot_toggles))

    if sm["frogpilotCarState"].trafficModeEnabled:
      frogpilotPlan.increasedStoppedDistance = 0
    else:
      frogpilotPlan.increasedStoppedDistance = frogpilot_toggles.increase_stopped_distance
      if self.frogpilot_weather.weather_id != 0:
        frogpilotPlan.increasedStoppedDistance += self.frogpilot_weather.increase_stopped_distance

    frogpilotPlan.laneWidthLeft = self.lane_width_left
    frogpilotPlan.laneWidthRight = self.lane_width_right

    frogpilotPlan.lateralCheck = self.lateral_check

    frogpilotPlan.maxAcceleration = float(self.frogpilot_acceleration.max_accel)
    frogpilotPlan.minAcceleration = float(self.frogpilot_acceleration.min_accel)

    frogpilotPlan.redLight = self.frogpilot_cem.stop_light_detected

    frogpilotPlan.roadCurvature = self.road_curvature

    frogpilotPlan.slcMapSpeedLimit = self.frogpilot_vcruise.slc.map_speed_limit
    frogpilotPlan.slcMapboxSpeedLimit = self.frogpilot_vcruise.slc.mapbox_speed_limit
    frogpilotPlan.slcMapboxWayId = self.frogpilot_vcruise.slc.mapbox_way_id
    frogpilotPlan.slcNextSpeedLimit = self.frogpilot_vcruise.slc.next_speed_limit
    frogpilotPlan.slcOverriddenSpeed = self.frogpilot_vcruise.slc.overridden_speed
    frogpilotPlan.slcSpeedLimit = self.frogpilot_vcruise.slc_target
    frogpilotPlan.slcSpeedLimitOffset = self.frogpilot_vcruise.slc_offset
    frogpilotPlan.slcSpeedLimitSource = self.frogpilot_vcruise.slc.source
    frogpilotPlan.speedLimitChanged = self.frogpilot_vcruise.slc.speed_limit_changed_timer > DT_MDL
    frogpilotPlan.unconfirmedSlcSpeedLimit = self.frogpilot_vcruise.slc.unconfirmed_speed_limit

    frogpilotPlan.themeUpdated = theme_updated

    frogpilotPlan.vCruise = float(self.v_cruise)

    frogpilotPlan.weatherDaytime = self.frogpilot_weather.is_daytime
    frogpilotPlan.weatherId = self.frogpilot_weather.weather_id

    pm.send("frogpilotPlan", frogpilot_plan_send)
