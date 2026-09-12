#!/usr/bin/env python3
from openpilot.common.constants import CV
from openpilot.common.realtime import DT_MDL

from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED, THRESHOLD
from openpilot.frogpilot.controls.lib.curve_speed_controller import CurveSpeedController
from openpilot.frogpilot.controls.lib.speed_limit_controller import SpeedLimitController

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
    self.stop_distance = 0

  def update(self, long_control_active, now, time_validated, v_cruise, v_ego, sm, frogpilot_toggles):
    self.update_force_stop(long_control_active, v_ego, sm, frogpilot_toggles)

    v_cruise_cluster = max(sm["carState"].vCruiseCluster * CV.KPH_TO_MS, v_cruise)

    v_ego_cluster = max(sm["carState"].vEgoCluster, v_ego)
    v_ego_diff = v_ego_cluster - v_ego

    # FrogsGoMoo's Curve Speed Controller
    self.csc.update_lateral_acceleration(frogpilot_toggles)
    self.csc.update_max_limit(sm, frogpilot_toggles)

    self.csc.profile_learner.update(long_control_active, sm)

    if long_control_active and frogpilot_toggles.curve_speed_controller:
      self.csc_target = self.csc.update_target(v_cruise, v_ego)
    else:
      self.csc.target = None

      self.csc_target = v_cruise

    self.csc_active = self.csc_target < v_cruise

    # Pfeiferj's Speed Limit Controller
    self.slc.frogpilot_toggles = frogpilot_toggles

    if frogpilot_toggles.speed_limit_controller:
      self.slc.update_limits(self.frogpilot_planner.gps_position, now, time_validated, v_ego, sm)
      self.slc.update_override(v_cruise_cluster, v_ego_cluster, sm)

      self.slc_offset = self.slc.offset
      self.slc_target = self.slc.target
    elif frogpilot_toggles.show_speed_limits or frogpilot_toggles.speed_limit_filler:
      self.slc.update_limits(self.frogpilot_planner.gps_position, now, time_validated, v_ego, sm)

      self.slc_offset = 0
      self.slc_target = self.slc.target
    else:
      self.slc.reset()

      self.slc_offset = 0
      self.slc_target = 0

    targets = [self.csc_target if self.csc_target >= CRUISING_SPEED else v_cruise, v_cruise]
    if frogpilot_toggles.speed_limit_controller and self.slc_target > 0:
      targets.append(max(max(self.slc.overridden_speed, self.slc_target + self.slc_offset) - v_ego_diff, CRUISING_SPEED))

    v_cruise = min(targets)

    self.csc_controlling_speed = self.csc_active and self.csc_target == v_cruise

    return v_cruise

  def update_force_stop(self, long_control_active, v_ego, sm, frogpilot_toggles):
    if not long_control_active or not frogpilot_toggles.force_stops or sm["frogpilotCarState"].trafficModeEnabled:
      self.forcing_stop = False
      self.override_force_stop = False

      self.force_stop_timer = 0
      self.stop_distance = 0
      return

    stop_detected = self.frogpilot_planner.frogpilot_cem.stop_light_filter.x >= THRESHOLD and self.frogpilot_planner.model_stopped
    stop_detected &= not self.frogpilot_planner.tracking_lead

    if sm["carState"].gasPressed or sm["frogpilotCarState"].accelPressed:
      self.override_force_stop |= self.forcing_stop or stop_detected

      self.forcing_stop = False

      self.force_stop_timer = 0
      self.stop_distance = 0
      return

    if self.override_force_stop:
      if not sm["carState"].standstill and not self.frogpilot_planner.frogpilot_cem.stop_light_signal:
        self.override_force_stop = self.frogpilot_planner.frogpilot_cem.stop_light_filter.x >= THRESHOLD
      return

    if stop_detected:
      model_stop_distance = next((distance for distance, velocity in zip(sm["modelV2"].position.x, sm["modelV2"].velocity.x) if velocity < 1), self.frogpilot_planner.model_length)

    if self.forcing_stop:
      self.stop_distance = max(self.stop_distance - v_ego * DT_MDL, 0)

      if stop_detected:
        self.stop_distance = min(self.stop_distance, model_stop_distance)
      return

    if stop_detected and long_control_active and not sm["carState"].standstill:
      self.force_stop_timer += DT_MDL

      if self.force_stop_timer >= 1:
        self.forcing_stop = True

        self.force_stop_timer = 0
        self.stop_distance = model_stop_distance
    else:
      self.force_stop_timer = 0
