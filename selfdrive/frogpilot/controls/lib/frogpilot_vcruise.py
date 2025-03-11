#!/usr/bin/env python3
import numpy as np

from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.drive_helpers import V_CRUISE_UNSET

from openpilot.selfdrive.frogpilot.controls.lib.map_turn_speed_controller import MapTurnSpeedController
from openpilot.selfdrive.frogpilot.controls.lib.speed_limit_controller import SpeedLimitController
from openpilot.selfdrive.frogpilot.frogpilot_variables import CRUISING_SPEED, PLANNER_TIME

TARGET_LAT_A = 2.0

class FrogPilotVCruise:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.mtsc = MapTurnSpeedController()
    self.slc = SpeedLimitController()

    self.forcing_stop = False
    self.override_force_stop = False

    self.force_stop_timer = 0
    self.mtsc_target = 0
    self.override_force_stop_timer = 0
    self.tracked_model_length = 0
    self.vtsc_target = 0

  def update(self, carControl, carState, controlsState, frogpilotCarControl, frogpilotCarState, frogpilotNavigation, gps_position, v_cruise, v_ego, frogpilot_toggles):
    force_stop = frogpilot_toggles.force_stops and self.frogpilot_planner.cem.stop_light_detected and controlsState.enabled
    force_stop &= self.frogpilot_planner.model_length < 100
    force_stop &= self.override_force_stop_timer <= 0

    self.force_stop_timer = self.force_stop_timer + DT_MDL if force_stop else 0

    force_stop_enabled = self.force_stop_timer >= 1

    self.override_force_stop |= not frogpilot_toggles.force_standstill and carState.standstill and self.frogpilot_planner.tracking_lead
    self.override_force_stop |= carState.gasPressed
    self.override_force_stop |= frogpilotCarControl.accelPressed
    self.override_force_stop &= force_stop_enabled

    if self.override_force_stop:
      self.override_force_stop_timer = 10
    elif self.override_force_stop_timer > 0:
      self.override_force_stop_timer -= DT_MDL

    # Pfeiferj's Map Turn Speed Controller
    if frogpilot_toggles.map_turn_speed_controller and v_ego > CRUISING_SPEED and carControl.longActive:
      mtsc_active = self.mtsc_target < v_cruise
      mtsc_speed = ((TARGET_LAT_A * frogpilot_toggles.turn_aggressiveness) / (self.mtsc.get_map_curvature(gps_position, v_ego) * frogpilot_toggles.curve_sensitivity))**0.5
      self.mtsc_target = float(np.clip(mtsc_speed, CRUISING_SPEED, v_cruise))

      if self.frogpilot_planner.road_curvature_detected and mtsc_active:
        self.mtsc_target = self.frogpilot_planner.v_cruise
      elif not self.frogpilot_planner.road_curvature_detected and frogpilot_toggles.mtsc_curvature_check:
        self.mtsc_target = v_cruise
    else:
      self.mtsc_target = v_cruise if v_cruise != V_CRUISE_UNSET else 0

    # Pfeiferj's Speed Limit Controller
    if frogpilot_toggles.speed_limit_controller:
      self.slc.frogpilot_toggles = frogpilot_toggles

      self.slc.update_limits(controlsState, frogpilotCarState.dashboardSpeedLimit, frogpilotCarControl, gps_position, frogpilotNavigation.navigationSpeedLimit, v_cruise, v_ego)
      self.slc.update_override(carState, controlsState, v_cruise, v_ego)

      self.slc_offset = self.slc.offset
      self.slc_target = self.slc.target
    elif frogpilot_toggles.show_speed_limits:
      self.slc.frogpilot_toggles = frogpilot_toggles

      self.slc.update_limits(carControl, controlsState, frogpilotCarState.dashboardSpeedLimit, frogpilotCarControl, gps_position, frogpilotNavigation.navigationSpeedLimit, v_cruise, v_ego)

      self.slc_offset = 0
      self.slc_target = self.slc.target
    else:
      self.slc_offset = 0
      self.slc_target = 0

    # Pfeiferj's Vision Turn Controller
    if frogpilot_toggles.vision_turn_speed_controller and carControl.longActive and self.frogpilot_planner.road_curvature_detected:
      self.vtsc_target = ((TARGET_LAT_A * frogpilot_toggles.turn_aggressiveness) / (abs(self.frogpilot_planner.road_curvature) * frogpilot_toggles.curve_sensitivity))**0.5
      self.vtsc_target = float(np.clip(self.vtsc_target, CRUISING_SPEED, v_cruise))
    else:
      self.vtsc_target = v_cruise if v_cruise != V_CRUISE_UNSET else 0

    if frogpilot_toggles.force_standstill and carState.standstill and not self.override_force_stop and controlsState.enabled:
      self.forcing_stop = True
      v_cruise = -1

    elif force_stop_enabled and not self.override_force_stop:
      self.forcing_stop |= not carState.standstill
      self.tracked_model_length = max(self.tracked_model_length - v_ego * DT_MDL, 0)
      v_cruise = min((self.tracked_model_length // PLANNER_TIME), v_cruise)

    else:
      if not self.frogpilot_planner.cem.stop_light_detected:
        self.override_force_stop = False

      self.forcing_stop = False

      self.tracked_model_length = self.frogpilot_planner.model_length

      if frogpilot_toggles.speed_limit_controller:
        targets = [self.mtsc_target, max(self.slc.overridden_speed, self.slc.target + self.slc_offset), self.vtsc_target]
      else:
        targets = [self.mtsc_target, self.vtsc_target]
      v_cruise = min([target if target > CRUISING_SPEED else v_cruise for target in targets])

    return v_cruise
