#!/usr/bin/env python3
import cereal.messaging as messaging
import math

from openpilot.common.constants import CV
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.gps import get_gps_location_service
from openpilot.common.params import Params
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.selfdrive.car.cruise import V_CRUISE_MAX
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import A_CHANGE_COST, DANGER_ZONE_COST, J_EGO_COST, STOP_DISTANCE

from openpilot.frogpilot.controls.lib.frogpilot_acceleration import FrogPilotAcceleration
from openpilot.frogpilot.controls.lib.frogpilot_events import FrogPilotEvents
from openpilot.frogpilot.controls.lib.frogpilot_following import FrogPilotFollowing
from openpilot.frogpilot.controls.lib.frogpilot_vcruise import FrogPilotVCruise


class FrogPilotPlanner:
  def __init__(self):
    self.params = Params(return_defaults=True)

    self.frogpilot_acceleration = FrogPilotAcceleration(self)
    self.frogpilot_events = FrogPilotEvents(self)
    self.frogpilot_following = FrogPilotFollowing(self)
    self.frogpilot_vcruise = FrogPilotVCruise(self)

    self.lateral_check = False
    self.model_stopped = False
    self.tracking_lead = False

    self.model_length = 0
    self.v_cruise = 0

    self.gps_position = None

    self.gps_location_service = get_gps_location_service(self.params)

    self.tracking_lead_filter = FirstOrderFilter(0, 0.5, DT_MDL)

  def update(self, now, time_validated, sm):
    self.lead_one = sm["radarState"].leadOne

    long_control_active = sm["carControl"].longActive

    v_cruise = min(sm["carState"].vCruise, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    if long_control_active:
      self.frogpilot_acceleration.update(v_ego, sm)
    else:
      self.frogpilot_acceleration.max_accel = 0
      self.frogpilot_acceleration.min_accel = 0

    self.frogpilot_events.update(long_control_active, sm)

    self.frogpilot_following.update(long_control_active, v_ego, sm)

    gps_location = sm[self.gps_location_service]
    self.gps_position = {
      "bearing": gps_location.bearingDeg,
      "latitude": gps_location.latitude,
      "longitude": gps_location.longitude,
      "valid": bool(sm.alive[self.gps_location_service] and sm.valid[self.gps_location_service] and gps_location.hasFix),
    }

    self.lateral_check |= sm["carState"].standstill

    self.model_length = sm["modelV2"].position.x[-1]

    self.model_stopped = self.model_length < frogpilot_variables.CRUISING_SPEED * frogpilot_variables.PLANNER_TIME

    if not sm["carState"].standstill:
      self.tracking_lead = self.update_lead_status()

    self.v_cruise = self.frogpilot_vcruise.update(long_control_active, now, time_validated, v_cruise, v_ego, sm)

  def update_lead_status(self):
    closing_lead = self.lead_one.status
    closing_lead &= self.lead_one.vRel < 0
    closing_lead &= self.lead_one.dRel + (self.lead_one.vRel * frogpilot_variables.PLANNER_TIME) < self.model_length + STOP_DISTANCE

    following_lead = self.lead_one.status
    following_lead &= self.lead_one.dRel < self.model_length + STOP_DISTANCE

    self.tracking_lead_filter.update(following_lead)
    return closing_lead or self.tracking_lead_filter.x >= frogpilot_variables.THRESHOLD

  def publish(self, sm, pm):
    frogpilot_plan_send = messaging.new_message("frogpilotPlan")
    frogpilot_plan_send.valid = sm.all_checks(service_list=["carState", "controlsState", "modelV2", "selfdriveState", "radarState"])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    frogpilotPlan.accelerationJerk = float(A_CHANGE_COST * self.frogpilot_following.acceleration_jerk)
    frogpilotPlan.dangerFactor = float(self.frogpilot_following.danger_factor)
    frogpilotPlan.dangerJerk = float(DANGER_ZONE_COST * self.frogpilot_following.danger_jerk)
    frogpilotPlan.desiredFollowDistance = int(self.frogpilot_following.desired_follow_distance)
    frogpilotPlan.speedJerk = float(J_EGO_COST * self.frogpilot_following.speed_jerk)
    frogpilotPlan.tFollow = float(self.frogpilot_following.t_follow)
    frogpilotPlan.trackingLead = self.tracking_lead

    frogpilotPlan.frogpilotEvents = self.frogpilot_events.events.to_msg()

    frogpilotPlan.lateralCheck = self.lateral_check

    frogpilotPlan.maxAcceleration = float(self.frogpilot_acceleration.max_accel)
    frogpilotPlan.minAcceleration = float(self.frogpilot_acceleration.min_accel)

    frogpilotPlan.vCruise = float(self.v_cruise)

    pm.send("frogpilotPlan", frogpilot_plan_send)
