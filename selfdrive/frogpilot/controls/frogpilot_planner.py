#!/usr/bin/env python3
import cereal.messaging as messaging

from openpilot.common.conversions import Conversions as CV
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.realtime import DT_MDL

from openpilot.selfdrive.controls.lib.drive_helpers import V_CRUISE_UNSET
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import A_CHANGE_COST, DANGER_ZONE_COST, J_EGO_COST, STOP_DISTANCE
from openpilot.selfdrive.controls.lib.longitudinal_planner import Lead

from openpilot.selfdrive.frogpilot.controls.lib.conditional_experimental_mode import ConditionalExperimentalMode
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_acceleration import FrogPilotAcceleration
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_events import FrogPilotEvents
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_following import FrogPilotFollowing
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_vcruise import FrogPilotVCruise
from openpilot.selfdrive.frogpilot.frogpilot_utilities import calculate_lane_width, calculate_road_curvature
from openpilot.selfdrive.frogpilot.frogpilot_variables import CRUISING_SPEED, MODEL_LENGTH, NON_DRIVING_GEARS, PLANNER_TIME, THRESHOLD

class FrogPilotPlanner:
  def __init__(self):
    self.cem = ConditionalExperimentalMode(self)
    self.frogpilot_acceleration = FrogPilotAcceleration(self)
    self.frogpilot_events = FrogPilotEvents(self)
    self.frogpilot_following = FrogPilotFollowing(self)
    self.frogpilot_vcruise = FrogPilotVCruise(self)
    self.lead_one = Lead()

    self.tracking_lead_filter = FirstOrderFilter(0, 1, DT_MDL)

    self.lateral_check = False
    self.model_stopped = False
    self.road_curvature_detected = False
    self.slower_lead = False
    self.tracking_lead = False

    self.model_length = 0
    self.road_curvature = 1
    self.v_cruise = 0

  def update(self, carControl, carState, controlsState, frogpilotCarControl, frogpilotCarState, frogpilotNavigation, modelData, radarless_model, radarState, frogpilot_toggles):
    if radarless_model:
      model_leads = list(modelData.leadsV3)
      if len(model_leads) > 0:
        distance_offset = frogpilot_toggles.increased_stopped_distance if not frogpilotCarState.trafficModeActive else 0
        model_lead = model_leads[0]
        self.lead_one.update(model_lead.x[0] - distance_offset, model_lead.y[0], model_lead.v[0], model_lead.a[0], model_lead.prob)
      else:
        self.lead_one.reset()
    else:
      self.lead_one = radarState.leadOne

    v_cruise = min(controlsState.vCruise, V_CRUISE_UNSET) * CV.KPH_TO_MS
    v_ego = max(carState.vEgo, 0)
    v_lead = self.lead_one.vLead

    self.frogpilot_acceleration.update(frogpilotCarState, v_ego, frogpilot_toggles)

    if frogpilot_toggles.conditional_experimental_mode and controlsState.enabled and carState.gearShifter not in NON_DRIVING_GEARS:
      self.cem.update(carState, frogpilotCarState, frogpilotNavigation, v_ego, v_lead, frogpilot_toggles)
    elif frogpilot_toggles.force_stops or frogpilot_toggles.green_light_alert or frogpilot_toggles.show_stopping_point:
      self.cem.curve_detected = False
      self.cem.stop_sign_and_light(frogpilotCarState, v_ego, frogpilot_toggles)
    else:
      self.cem.stop_light_detected = False

    self.frogpilot_events.update(carState, controlsState, frogpilotCarControl, frogpilotCarState, self.lead_one.dRel, modelData, v_lead, frogpilot_toggles)
    self.frogpilot_following.update(carState.aEgo, controlsState, frogpilotCarState, self.lead_one.dRel, v_ego, v_lead, frogpilot_toggles)

    check_lane_width = frogpilot_toggles.adjacent_paths or frogpilot_toggles.adjacent_path_metrics or frogpilot_toggles.blind_spot_path or frogpilot_toggles.lane_detection
    if check_lane_width and v_ego >= frogpilot_toggles.minimum_lane_change_speed:
      self.lane_width_left = calculate_lane_width(modelData.laneLines[0], modelData.laneLines[1], modelData.roadEdges[0])
      self.lane_width_right = calculate_lane_width(modelData.laneLines[3], modelData.laneLines[2], modelData.roadEdges[1])
    else:
      self.lane_width_left = 0
      self.lane_width_right = 0

    self.lateral_check = v_ego >= frogpilot_toggles.pause_lateral_below_speed
    self.lateral_check |= frogpilot_toggles.pause_lateral_below_signal and not (carState.leftBlinker or carState.rightBlinker)
    self.lateral_check |= carState.standstill

    self.model_length = modelData.position.x[MODEL_LENGTH - 1]
    self.model_stopped = self.model_length < CRUISING_SPEED * PLANNER_TIME
    self.model_stopped |= self.frogpilot_vcruise.forcing_stop

    self.road_curvature = calculate_road_curvature(modelData, v_ego) if v_ego > CRUISING_SPEED else 1
    self.road_curvature_detected = (1 / self.road_curvature)**0.5 < v_ego

    self.tracking_lead = self.set_lead_status(carState, v_lead)
    self.v_cruise = self.frogpilot_vcruise.update(carControl, carState, controlsState, frogpilotCarControl, frogpilotCarState, frogpilotNavigation, v_cruise, v_ego, frogpilot_toggles)

  def set_lead_status(self, carState, v_lead):
    following_lead = self.lead_one.status
    following_lead &= self.lead_one.dRel < self.model_length + STOP_DISTANCE
    following_lead &= not carState.standstill or self.tracking_lead

    self.tracking_lead_filter.update(following_lead)
    return self.tracking_lead_filter.x >= THRESHOLD**2

  def publish(self, sm, pm, toggles_updated):
    frogpilot_plan_send = messaging.new_message("frogpilotPlan")
    frogpilot_plan_send.valid = sm.all_checks(service_list=["carState", "controlsState"])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    frogpilotPlan.accelerationJerk = float(A_CHANGE_COST * self.frogpilot_following.acceleration_jerk)
    frogpilotPlan.accelerationJerkStock = float(A_CHANGE_COST * self.frogpilot_following.base_acceleration_jerk)
    frogpilotPlan.dangerJerk = float(DANGER_ZONE_COST * self.frogpilot_following.danger_jerk)
    frogpilotPlan.speedJerk = float(J_EGO_COST * self.frogpilot_following.speed_jerk)
    frogpilotPlan.speedJerkStock = float(J_EGO_COST * self.frogpilot_following.base_speed_jerk)
    frogpilotPlan.tFollow = float(self.frogpilot_following.t_follow)

    frogpilotPlan.mtscSpeed = float(self.frogpilot_vcruise.mtsc_target)
    frogpilotPlan.vtscControllingCurve = bool(self.frogpilot_vcruise.mtsc_target > self.frogpilot_vcruise.vtsc_target)
    frogpilotPlan.vtscSpeed = float(self.frogpilot_vcruise.vtsc_target)

    frogpilotPlan.desiredFollowDistance = self.frogpilot_following.desired_follow_distance

    frogpilotPlan.experimentalMode = self.cem.experimental_mode or self.frogpilot_vcruise.slc.experimental_mode

    frogpilotPlan.forcingStop = self.frogpilot_vcruise.forcing_stop
    frogpilotPlan.forcingStopLength = self.frogpilot_vcruise.tracked_model_length

    frogpilotPlan.frogpilotEvents = self.frogpilot_events.events.to_msg()

    frogpilotPlan.laneWidthLeft = self.lane_width_left
    frogpilotPlan.laneWidthRight = self.lane_width_right

    frogpilotPlan.lateralCheck = self.lateral_check

    frogpilotPlan.maxAcceleration = float(self.frogpilot_acceleration.max_accel)
    frogpilotPlan.minAcceleration = float(self.frogpilot_acceleration.min_accel)

    frogpilotPlan.redLight = bool(self.cem.stop_light_detected)

    frogpilotPlan.slcMapSpeedLimit = self.frogpilot_vcruise.slc.map_speed_limit
    frogpilotPlan.slcOverridden = bool(self.frogpilot_vcruise.override_slc)
    frogpilotPlan.slcOverriddenSpeed = float(self.frogpilot_vcruise.overridden_speed)
    frogpilotPlan.slcSpeedLimit = self.frogpilot_vcruise.slc_target
    frogpilotPlan.slcSpeedLimitOffset = self.frogpilot_vcruise.slc_offset
    frogpilotPlan.slcSpeedLimitSource = self.frogpilot_vcruise.slc.source
    frogpilotPlan.speedLimitChanged = self.frogpilot_vcruise.slc.speed_limit_changed
    frogpilotPlan.unconfirmedSlcSpeedLimit = self.frogpilot_vcruise.slc.desired_speed_limit
    frogpilotPlan.upcomingSLCSpeedLimit = self.frogpilot_vcruise.slc.upcoming_speed_limit

    frogpilotPlan.togglesUpdated = toggles_updated

    frogpilotPlan.vCruise = self.v_cruise

    pm.send("frogpilotPlan", frogpilot_plan_send)
