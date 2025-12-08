#!/usr/bin/env python3
import cereal.messaging as messaging

from openpilot.common.constants import CV
from openpilot.common.gps import get_gps_location_service
from openpilot.common.params import Params
from openpilot.selfdrive.car.cruise import V_CRUISE_MAX
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import A_CHANGE_COST, DANGER_ZONE_COST, J_EGO_COST, STOP_DISTANCE

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
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

    self.accel_pressed = False
    self.decel_pressed = False
    self.gps_valid = False
    self.lead_relevant = False
    self.model_stopped = False

    self.accel_press_count = 0
    self.decel_press_count = 0
    self.model_length = 0
    self.v_cruise = 0

    self.gps_position = None
    self.last_gps_position = None

    self.gps_location_service = get_gps_location_service(self.params)

    self.ui_event_sock = messaging.sub_sock("frogpilotUIEvent")

  def update(self, now, time_validated, sm):
    self.accel_pressed = sm["frogpilotCarState"].accelPressCount > self.accel_press_count
    self.decel_pressed = sm["frogpilotCarState"].decelPressCount > self.decel_press_count

    self.accel_press_count = sm["frogpilotCarState"].accelPressCount
    self.decel_press_count = sm["frogpilotCarState"].decelPressCount

    for msg in messaging.drain_sock(self.ui_event_sock):
      ui_event = msg.frogpilotUIEvent

    self.lead_one = sm["radarState"].leadOne

    long_control_active = sm["carControl"].longActive

    v_cruise = min(sm["carState"].vCruise, V_CRUISE_MAX) * CV.KPH_TO_MS
    v_ego = max(sm["carState"].vEgo, 0)

    self.model_length = sm["modelV2"].position.x[-1]

    self.lead_relevant = self.is_lead_relevant(self.lead_one, sm["carState"].standstill, v_ego)

    self.frogpilot_acceleration.update(v_ego, sm)

    self.frogpilot_events.update(long_control_active, sm, frogpilot_toggles)

    self.frogpilot_following.update(sm)

    gps_location = sm[self.gps_location_service]
    self.gps_valid = frogpilot_utilities.is_gps_location_valid(self.gps_location_service, sm)
    if self.gps_valid:
      self.gps_position = {
        "latitude": gps_location.latitude,
        "longitude": gps_location.longitude,
        "bearing": gps_location.bearingDeg,
      }
      self.last_gps_position = self.gps_position
    else:
      self.gps_position = None

    self.model_stopped = self.model_length < frogpilot_variables.CRUISING_SPEED * frogpilot_variables.PLANNER_TIME

    self.v_cruise = self.frogpilot_vcruise.update(v_cruise, v_ego, sm)

  def is_lead_relevant(self, lead, standstill, v_ego):
    following_speed = max(v_ego, frogpilot_variables.CRUISING_SPEED)
    lead_speed = lead.vLead + min(lead.aLeadK, 0) * frogpilot_variables.PLANNER_TIME / 2

    relevant_lead = lead.status
    relevant_lead &= lead.dRel < (0 if standstill else self.model_length) + STOP_DISTANCE + frogpilot_variables.LEAD_STOP_MARGIN
    relevant_lead &= lead.dRel < STOP_DISTANCE + following_speed * frogpilot_variables.MAX_T_FOLLOW + max(following_speed - lead_speed, 0) * frogpilot_variables.PLANNER_TIME
    return relevant_lead

  def publish(self, sm, pm):
    frogpilot_plan_send = messaging.new_message("frogpilotPlan")
    frogpilot_plan_send.valid = sm.all_checks(service_list=["carState", "controlsState", "selfdriveState", "radarState"])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    frogpilotPlan.accelerationJerk = float(A_CHANGE_COST * self.frogpilot_following.acceleration_jerk)
    frogpilotPlan.dangerJerk = float(DANGER_ZONE_COST * self.frogpilot_following.danger_jerk)
    frogpilotPlan.speedJerk = float(J_EGO_COST * self.frogpilot_following.speed_jerk)

    frogpilotPlan.frogpilotEvents = self.frogpilot_events.events.to_msg()

    frogpilotPlan.maxAcceleration = float(self.frogpilot_acceleration.max_accel)
    frogpilotPlan.minAcceleration = float(self.frogpilot_acceleration.min_accel)

    frogpilotPlan.tFollow = float(self.frogpilot_following.t_follow)

    frogpilotPlan.vCruise = float(self.v_cruise)

    pm.send("frogpilotPlan", frogpilot_plan_send)
