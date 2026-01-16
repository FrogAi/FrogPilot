#!/usr/bin/env python3
from openpilot.common.conversions import Conversions as CV
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import COMFORT_BRAKE

from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED, LOW_SPEED_CRUISE_THRESHOLD, PLANNER_TIME
from openpilot.frogpilot.controls.lib.curve_speed_controller import CurveSpeedController
from openpilot.frogpilot.controls.lib.speed_limit_controller import SpeedLimitController

class FrogPilotVCruise:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.csc = CurveSpeedController(self)
    self.slc = SpeedLimitController()

    self.forcing_stop = False
    self.override_force_stop = False

    self.override_force_stop_timer = 0

    # Low-speed cruise mode: when below PCM's 28 mph floor, OP owns the target
    self.low_speed_cruise_mode = False
    self.low_speed_target = 0.0  # m/s - OP-owned target when below threshold

  def update(self, gps_position, now, time_validated, v_cruise, v_ego, sm, frogpilot_toggles):
    # Auto-detect low-speed cruise mode from incoming v_cruise
    # When VCruiseHelper is in low_speed_cruise_mode, v_cruise will be below threshold
    was_in_low_speed_mode = self.low_speed_cruise_mode
    if v_cruise < LOW_SPEED_CRUISE_THRESHOLD and v_cruise > 0:
      if not was_in_low_speed_mode:
        # Entering low-speed mode
        self.low_speed_cruise_mode = True
        self.low_speed_target = v_cruise
      else:
        # Update target in low-speed mode
        self.low_speed_target = v_cruise
    elif v_cruise >= LOW_SPEED_CRUISE_THRESHOLD:
      # Exit low-speed mode
      if was_in_low_speed_mode:
        self.low_speed_cruise_mode = False
        self.low_speed_target = 0.0

    force_stop = self.frogpilot_planner.cem.stop_light_detected and sm["controlsState"].enabled and frogpilot_toggles.force_stops
    force_stop &= self.frogpilot_planner.model_stopped
    force_stop &= self.override_force_stop_timer <= 0

    self.force_stop_timer = self.force_stop_timer + DT_MDL if force_stop else 0

    force_stop_enabled = self.force_stop_timer >= 1

    self.override_force_stop |= sm["carState"].gasPressed
    self.override_force_stop |= sm["frogpilotCarState"].accelPressed
    self.override_force_stop &= force_stop_enabled

    if self.override_force_stop:
      self.override_force_stop_timer = 10
    elif self.override_force_stop_timer > 0:
      self.override_force_stop_timer -= DT_MDL

    v_cruise_cluster = max(sm["controlsState"].vCruiseCluster * CV.KPH_TO_MS, v_cruise)
    v_cruise_diff = v_cruise_cluster - v_cruise

    v_ego_cluster = max(sm["carState"].vEgoCluster, v_ego)
    v_ego_diff = v_ego_cluster - v_ego

    # FrogsGoMoo's Curve Speed Controller
    if v_ego > CRUISING_SPEED and sm["controlsState"].enabled and self.frogpilot_planner.road_curvature_detected and frogpilot_toggles.curve_speed_controller:
      self.csc.update_target(v_ego)

      self.csc_controlling_speed = True

      self.csc_target = self.csc.target
    else:
      self.csc.log_data(v_ego, sm)

      self.csc_controlling_speed = False
      self.csc.target_set = False

      self.csc_target = v_cruise

    # Pfeiferj's Speed Limit Controller
    self.slc.frogpilot_toggles = frogpilot_toggles

    if frogpilot_toggles.speed_limit_controller:
      self.slc.update_limits(sm["frogpilotCarState"].dashboardSpeedLimit, gps_position, sm["frogpilotNavigation"].navigationSpeedLimit, now, time_validated, v_cruise, v_ego, sm)
      self.slc.update_override(v_cruise, v_cruise_diff, v_ego, v_ego_diff, sm)

      self.slc_offset = self.slc.offset
      self.slc_target = self.slc.target
    elif frogpilot_toggles.show_speed_limits:
      self.slc.update_limits(sm["frogpilotCarState"].dashboardSpeedLimit, gps_position, sm["frogpilotNavigation"].navigationSpeedLimit, now, time_validated, v_cruise, v_ego, sm)

      self.slc_offset = 0
      self.slc_target = self.slc.target
    else:
      self.slc_offset = 0
      self.slc_target = 0

    if force_stop_enabled and not self.override_force_stop:
      self.forcing_stop |= not sm["carState"].standstill

      self.tracked_model_length = max(self.tracked_model_length - (v_ego * DT_MDL), 0)
      v_cruise = min((self.tracked_model_length // PLANNER_TIME), v_cruise)

    else:
      self.forcing_stop = False

      self.tracked_model_length = self.frogpilot_planner.model_length

      targets = [self.csc_target, v_cruise]
      if frogpilot_toggles.speed_limit_controller:
        targets.append(max(self.slc.overridden_speed, self.slc_target + self.slc_offset) - v_ego_diff)

      # Low-speed cruise mode handling:
      # When v_cruise is below Toyota PCM's floor (~28 mph), OP owns the target
      # and we allow targets below CRUISING_SPEED to be used
      if self.low_speed_cruise_mode:
        # In low-speed mode: use all targets including those below CRUISING_SPEED
        # Use low_speed_target as the base if it's set, otherwise use incoming v_cruise
        effective_cruise = self.low_speed_target if self.low_speed_target > 0 else v_cruise
        targets_with_low = [self.csc_target, effective_cruise]
        if frogpilot_toggles.speed_limit_controller:
          targets_with_low.append(max(self.slc.overridden_speed, self.slc_target + self.slc_offset) - v_ego_diff)
        # Allow all targets, no CRUISING_SPEED floor
        v_cruise = min([t for t in targets_with_low if t > 0] or [effective_cruise])
      else:
        # Normal mode: filter out targets below CRUISING_SPEED
        v_cruise = min([target if target >= CRUISING_SPEED else v_cruise for target in targets])

    return v_cruise

  def update_low_speed_cruise(self, v_cruise_kph, is_metric, frogpilot_toggles):
    """
    Update low-speed cruise mode state based on current set speed.
    Called from drive_helpers.py after button handling.

    Args:
      v_cruise_kph: Current cruise set speed in kph
      is_metric: Whether display is metric
      frogpilot_toggles: FrogPilot toggle settings

    Returns:
      Tuple of (adjusted_v_cruise_kph, low_speed_mode_active)
    """
    from openpilot.common.conversions import Conversions as CV
    from openpilot.selfdrive.controls.lib.drive_helpers import V_CRUISE_MIN

    v_cruise_ms = v_cruise_kph * CV.KPH_TO_MS
    threshold_kph = LOW_SPEED_CRUISE_THRESHOLD * CV.MS_TO_KPH

    # Detect mode transitions
    was_in_low_speed_mode = self.low_speed_cruise_mode

    # Enter low-speed mode when set speed drops below threshold
    if v_cruise_kph < threshold_kph:
      if not was_in_low_speed_mode:
        # Entering low-speed mode: initialize target from current v_cruise
        self.low_speed_cruise_mode = True
        self.low_speed_target = v_cruise_ms
      else:
        # Already in low-speed mode: update target
        self.low_speed_target = v_cruise_ms
    else:
      # Above threshold: exit low-speed mode
      if was_in_low_speed_mode:
        self.low_speed_cruise_mode = False
        self.low_speed_target = 0.0

    return v_cruise_kph, self.low_speed_cruise_mode

  def adjust_low_speed_target(self, delta_kph):
    """
    Adjust the low-speed target by delta_kph when in low-speed mode.
    Called from drive_helpers.py during button handling.

    Args:
      delta_kph: Speed change in kph (positive for increase, negative for decrease)

    Returns:
      New target speed in kph
    """
    from openpilot.common.conversions import Conversions as CV
    from openpilot.selfdrive.controls.lib.drive_helpers import V_CRUISE_MIN, V_CRUISE_MAX

    if not self.low_speed_cruise_mode:
      return None

    # Convert current target to kph, apply delta, convert back
    current_kph = self.low_speed_target * CV.MS_TO_KPH
    new_kph = current_kph + delta_kph

    # Clip to valid range (allow down to V_CRUISE_MIN which is ~5 mph)
    new_kph = max(V_CRUISE_MIN, min(new_kph, LOW_SPEED_CRUISE_THRESHOLD * CV.MS_TO_KPH))

    self.low_speed_target = new_kph * CV.KPH_TO_MS
    return new_kph
