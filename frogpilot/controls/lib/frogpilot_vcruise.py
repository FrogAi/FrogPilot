#!/usr/bin/env python3
from openpilot.common.constants import CV
from openpilot.common.realtime import DT_MDL

from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED, PLANNER_TIME
from openpilot.frogpilot.controls.lib.curve_speed_controller import CurveSpeedController
from openpilot.frogpilot.controls.lib.speed_limit_controller import SpeedLimitController

OVERRIDE_FORCE_STOP_TIMER = 10

class FrogPilotVCruise:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.csc = CurveSpeedController(self)
    self.slc = SpeedLimitController(self)

    self.csc_active = False
    self.csc_controlling_speed = False
    self.forcing_stop = False
    self.override_force_stop = False

    self.force_stop_timer = 0
    self.override_force_stop_timer = 0

  def update(self, long_control_active, now, time_validated, v_cruise, v_ego, sm, frogpilot_toggles):
    force_stop = self.frogpilot_planner.frogpilot_cem.stop_light_detected and long_control_active and frogpilot_toggles.force_stops
    force_stop &= self.frogpilot_planner.model_stopped
    force_stop &= self.override_force_stop_timer <= 0

    self.force_stop_timer = self.force_stop_timer + DT_MDL if force_stop else 0

    force_stop_enabled = self.force_stop_timer >= 1

    self.override_force_stop |= sm["carState"].gasPressed
    self.override_force_stop |= sm["frogpilotCarState"].accelPressed
    self.override_force_stop &= force_stop_enabled

    if self.override_force_stop:
      self.override_force_stop_timer = OVERRIDE_FORCE_STOP_TIMER
    elif self.override_force_stop_timer > 0:
      self.override_force_stop_timer -= DT_MDL

    v_cruise_cluster = max(sm["carState"].vCruiseCluster * CV.KPH_TO_MS, v_cruise)

    v_ego_cluster = max(sm["carState"].vEgoCluster, v_ego)
    v_ego_diff = v_ego_cluster - v_ego

    # FrogsGoMoo's Curve Speed Controller
    self.csc.profile_learner.update(long_control_active, sm)
    self.csc.update_max_limit(sm, frogpilot_toggles)
    self.csc.update_lateral_acceleration(frogpilot_toggles)

    self.csc_active = long_control_active and v_ego > CRUISING_SPEED and self.frogpilot_planner.road_curvature_detected and frogpilot_toggles.curve_speed_controller

    if self.csc_active:
      self.csc.update_target(v_ego)

      self.csc_controlling_speed = True

      self.csc_target = self.csc.target
    else:
      self.csc_controlling_speed = False
      self.csc.target_set = False

      self.csc_target = v_cruise

    # Pfeiferj's Speed Limit Controller
    self.slc.frogpilot_toggles = frogpilot_toggles

    if frogpilot_toggles.speed_limit_controller:
      self.slc.update_limits(now, time_validated, v_ego, sm)
      self.slc.update_override(v_cruise_cluster, v_ego, v_ego_cluster, sm)

      self.slc_offset = self.slc.offset
      self.slc_target = self.slc.target
    elif frogpilot_toggles.show_speed_limits or frogpilot_toggles.speed_limit_filler:
      self.slc.update_limits(now, time_validated, v_ego, sm)

      self.slc_offset = 0
      self.slc_target = self.slc.target
    else:
      self.slc.reset()

      self.slc_offset = 0
      self.slc_target = 0

    if force_stop_enabled and not self.override_force_stop:
      self.csc_controlling_speed = False

      self.forcing_stop |= not sm["carState"].standstill

      self.tracked_model_length = max(self.tracked_model_length - (v_ego * DT_MDL), 0)
      v_cruise = min((self.tracked_model_length // PLANNER_TIME), v_cruise)
      targets = [v_cruise]

    else:
      self.forcing_stop = False

      self.tracked_model_length = self.frogpilot_planner.model_length

      targets = [self.csc_target if self.csc_target >= CRUISING_SPEED else v_cruise, v_cruise]
      if frogpilot_toggles.speed_limit_controller and self.slc_target > 0:
        targets.append(max(max(self.slc.overridden_speed, self.slc_target + self.slc_offset) - v_ego_diff, 0))

      self.csc_controlling_speed &= targets[0] < min(targets[1:])

    return min(targets)
