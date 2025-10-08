#!/usr/bin/env python3
import cereal.messaging as messaging

from cereal import car, log
from openpilot.common.constants import CV
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.gps import get_gps_location_service
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.car.cruise import V_CRUISE_MAX
from openpilot.selfdrive.controls.lib.longcontrol import LongCtrlState
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import A_CHANGE_COST, DANGER_ZONE_COST, J_EGO_COST, STOP_DISTANCE

from openpilot.frogpilot.common.frogpilot_utilities import calculate_road_curvature
from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED, MINIMUM_LATERAL_ACCELERATION, PLANNER_TIME, THRESHOLD
from openpilot.frogpilot.controls.lib.conditional_experimental_mode import ConditionalExperimentalMode
from openpilot.frogpilot.controls.lib.frogpilot_acceleration import FrogPilotAcceleration
from openpilot.frogpilot.controls.lib.frogpilot_events import FrogPilotEvents
from openpilot.frogpilot.controls.lib.frogpilot_following import FrogPilotFollowing
from openpilot.frogpilot.controls.lib.frogpilot_vcruise import FrogPilotVCruise

class FrogPilotPlanner:
  def __init__(self, error_log, ThemeManager, params):
    self.frogpilot_acceleration = FrogPilotAcceleration(self)
    self.frogpilot_cem = ConditionalExperimentalMode(self)
    self.frogpilot_events = FrogPilotEvents(self, error_log, ThemeManager)
    self.frogpilot_following = FrogPilotFollowing(self)
    self.frogpilot_vcruise = FrogPilotVCruise(self, params)

    self.driving_in_curve = False
    self.lateral_check = False
    self.model_stopped = False
    self.road_curvature_detected = False
    self.slower_lead = False
    self.tracking_lead = False

    self.lateral_acceleration = 0
    self.model_length = 0
    self.road_curvature = 0
    self.time_to_curve = 0
    self.v_cruise = 0

    self.gps_location_service = get_gps_location_service(params)

    self.tracking_lead_filter = FirstOrderFilter(0, 0.5, DT_MDL)

    with car.CarParams.from_bytes(params.get("CarParams", block=True)) as msg:
      self.CP = msg

  def update(self, now, time_validated, params, params_memory, sm, frogpilot_toggles):
    self.lead_one = sm["radarState"].leadOne

    long_control_active = sm["controlsState"].longControlState != LongCtrlState.off

    v_cruise = min(sm["carState"].vCruise, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    if long_control_active:
      self.frogpilot_acceleration.update(v_ego, sm, frogpilot_toggles)
    else:
      self.frogpilot_acceleration.max_accel = 0
      self.frogpilot_acceleration.min_accel = 0

    if long_control_active and frogpilot_toggles.conditional_experimental_mode:
      self.frogpilot_cem.update(v_ego, params_memory, sm, frogpilot_toggles)
    else:
      self.frogpilot_cem.curve_detected = False
      self.frogpilot_cem.stop_sign_and_light(v_ego, sm, PLANNER_TIME - 2)

    self.driving_in_curve = abs(self.lateral_acceleration) >= MINIMUM_LATERAL_ACCELERATION

    self.frogpilot_events.update(v_cruise, sm, frogpilot_toggles)

    self.frogpilot_following.update(long_control_active, v_ego, sm, frogpilot_toggles)

    gps_location = sm[self.gps_location_service]
    if gps_location.flags % 2 == 1:
      gps_position = {
        "latitude": gps_location.latitude,
        "longitude": gps_location.longitude,
        "bearing": gps_location.bearingDeg,
      }
      params_memory.put("LastGPSPosition", gps_position)
    else:
      gps_position = None
      params_memory.remove("LastGPSPosition")

    self.lateral_acceleration = v_ego**2 * (sm["carState"].steeringAngleDeg - sm["liveParameters"].angleOffsetDeg) * CV.DEG_TO_RAD / (self.CP.steerRatio * self.CP.wheelbase)

    self.model_length = sm["modelV2"].position.x[-1]

    self.model_stopped = self.model_length < CRUISING_SPEED * PLANNER_TIME
    self.model_stopped |= self.frogpilot_vcruise.forcing_stop

    self.road_curvature, self.time_to_curve = calculate_road_curvature(sm["modelV2"], v_ego)

    self.road_curvature_detected = (1 / abs(self.road_curvature))**0.5 < v_ego > CRUISING_SPEED and not (sm["carState"].leftBlinker or sm["carState"].rightBlinker)

    if not sm["carState"].standstill:
      self.tracking_lead = self.update_lead_status()

    self.v_cruise = self.frogpilot_vcruise.update(gps_position, long_control_active, now, time_validated, v_cruise, v_ego, params, params_memory, sm, frogpilot_toggles)

  def update_lead_status(self):
    following_lead = self.lead_one.status
    following_lead &= self.lead_one.dRel < self.model_length + STOP_DISTANCE

    self.tracking_lead_filter.update(following_lead)
    return self.tracking_lead_filter.x >= THRESHOLD

  def publish(self, theme_updated, toggles_updated, params_memory, sm, pm, frogpilot_toggles):
    frogpilot_plan_send = messaging.new_message("frogpilotPlan")
    frogpilot_plan_send.valid = sm.all_checks(service_list=["carState", "controlsState", "selfdriveState", "radarState"])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    frogpilotPlan.accelerationJerk = A_CHANGE_COST * self.frogpilot_following.acceleration_jerk
    frogpilotPlan.dangerJerk = DANGER_ZONE_COST * self.frogpilot_following.danger_jerk
    frogpilotPlan.speedJerk = J_EGO_COST * self.frogpilot_following.speed_jerk
    frogpilotPlan.tFollow = self.frogpilot_following.t_follow

    frogpilotPlan.cscControllingSpeed = self.frogpilot_vcruise.csc_controlling_speed
    frogpilotPlan.cscSpeed = self.frogpilot_vcruise.csc_target
    frogpilotPlan.cscTraining = self.frogpilot_vcruise.csc.enable_training

    frogpilotPlan.experimentalMode = self.frogpilot_cem.experimental_mode

    frogpilotPlan.forcingStop = self.frogpilot_vcruise.forcing_stop
    frogpilotPlan.forcingStopLength = self.frogpilot_vcruise.tracked_model_length

    frogpilotPlan.frogpilotEvents = self.frogpilot_events.events.to_msg()

    frogpilotPlan.lateralCheck = self.lateral_check

    frogpilotPlan.maxAcceleration = self.frogpilot_acceleration.max_accel
    frogpilotPlan.minAcceleration = self.frogpilot_acceleration.min_accel

    frogpilotPlan.redLight = self.frogpilot_cem.stop_light_detected

    frogpilotPlan.roadCurvature = self.road_curvature

    frogpilotPlan.themeUpdated = theme_updated or params_memory.get_bool("UseActiveTheme")

    frogpilotPlan.togglesUpdated = toggles_updated

    frogpilotPlan.vCruise = self.v_cruise

    pm.send("frogpilotPlan", frogpilot_plan_send)
