#!/usr/bin/env python3
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.drive_helpers import MAX_LATERAL_ACCEL_NO_ROLL

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.controls.lib.curve_speed_profile_learner import CurveSpeedProfileLearner
from openpilot.frogpilot.controls.lib.max_lateral_acceleration_learner import MaxLateralAccelerationLearner

CURVE_SPEED_LATERAL_ACCELERATIONS = {
  frogpilot_variables.CURVE_SPEED_PROFILES["GENTLE"]: 1.75,
  frogpilot_variables.CURVE_SPEED_PROFILES["STANDARD"]: frogpilot_variables.DEFAULT_LATERAL_ACCELERATION
}

CURVE_AHEAD_ENTER = 0.6
CURVE_AHEAD_EXIT = 0.4
TARGET_RISE_RATE = 1.2

class CurveSpeedController:
  def __init__(self, FrogPilotVCruise):
    self.frogpilot_planner = FrogPilotVCruise.frogpilot_planner

    self.lateral_acceleration = frogpilot_variables.DEFAULT_LATERAL_ACCELERATION
    self.max_limit = frogpilot_variables.DEFAULT_LATERAL_ACCELERATION

    self.enable_training = False

    self.decel_rate = 0

    self.target = None

    self.max_limit_learner = MaxLateralAccelerationLearner(self)
    self.profile_learner = CurveSpeedProfileLearner(self)

  def update_lateral_acceleration(self, frogpilot_toggles):
    if frogpilot_toggles.curve_speed_profile == frogpilot_variables.CURVE_SPEED_PROFILES["AUTO"]:
      self.lateral_acceleration = self.profile_learner.calibrated_lateral_acceleration
    elif frogpilot_toggles.curve_speed_profile == frogpilot_variables.CURVE_SPEED_PROFILES["SPORT"]:
      self.lateral_acceleration = self.max_limit
    else:
      self.lateral_acceleration = CURVE_SPEED_LATERAL_ACCELERATIONS[frogpilot_toggles.curve_speed_profile]

    self.lateral_acceleration = min(self.lateral_acceleration, self.max_limit)

    if self.frogpilot_planner.frogpilot_weather.weather_id != 0:
      self.lateral_acceleration -= self.lateral_acceleration * self.frogpilot_planner.frogpilot_weather.reduce_lateral_acceleration

  def update_max_limit(self, sm, frogpilot_toggles):
    if sm["controlsState"].lateralControlState.which() == "angleState":
      self.max_limit_learner.update(sm, frogpilot_toggles)
    else:
      self.max_limit = frogpilot_toggles.maxLateralAccel

    self.max_limit = min(self.max_limit, MAX_LATERAL_ACCEL_NO_ROLL)

  def update_target(self, v_cruise, v_ego):
    self.decel_rate = 0

    if v_ego**2 * self.frogpilot_planner.road_curvature_peak <= (CURVE_AHEAD_ENTER if self.target is None else CURVE_AHEAD_EXIT):
      self.target = None
      return v_cruise

    csc_speed = float(frogpilot_utilities.calculate_curve_speed(self.frogpilot_planner.road_curvature, self.lateral_acceleration, self.frogpilot_planner.roll_compensation))

    if self.target is None:
      self.target = max(v_ego, csc_speed)

    if csc_speed < self.target:
      self.decel_rate = max(v_ego - csc_speed, 0) / max(self.frogpilot_planner.time_to_curve - frogpilot_variables.DECEL_TIME_MARGIN, 1)

      self.target = max(min(self.target, v_ego) - self.decel_rate * DT_MDL, csc_speed)
    elif abs(self.frogpilot_planner.lateral_acceleration - self.frogpilot_planner.roll_compensation) < self.lateral_acceleration:
      self.target = min(self.target + TARGET_RISE_RATE * DT_MDL, csc_speed)

      if self.target >= v_cruise:
        self.target = csc_speed

    return self.target
