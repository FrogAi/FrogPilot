#!/usr/bin/env python3
import numpy as np

from openpilot.common.constants import ACCELERATION_DUE_TO_GRAVITY
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.drive_helpers import MAX_LATERAL_ACCEL_NO_ROLL

from openpilot.frogpilot.common.frogpilot_variables import DEFAULT_LATERAL_ACCELERATION

CURVE_THRESHOLD = 1.3

CURVE_EXIT_TIME = 0.5
MINIMUM_CURVE_TIME = 0.5

CURVE_PERCENTILE = 90
LEARNING_CURVES = 100


class CurveSpeedProfileLearner:
  def __init__(self, CurveSpeedController):
    self.csc = CurveSpeedController

    self.curve_exit_time = 0

    self.curve_samples = []

    self.calibrated_lateral_acceleration = DEFAULT_LATERAL_ACCELERATION

    curvature_data = self.csc.frogpilot_planner.params.get("CurvatureData")
    self.curve_values = curvature_data.get("curves", [])[-LEARNING_CURVES:]

    self._update_profile()

  def update(self, long_control_active, sm):
    valid = sm.all_checks(["carControl", "carState", "liveParameters", "radarState"])
    valid &= sm["liveParameters"].valid and len(sm["carControl"].angularVelocity) > 2
    valid &= not long_control_active and not sm["carState"].cruiseState.enabled and not sm["radarState"].leadOne.status
    valid &= not sm["carState"].espActive
    valid &= self.csc.frogpilot_planner.frogpilot_weather.weather_id == 0 or self.csc.frogpilot_planner.frogpilot_weather.reduce_lateral_acceleration == 0
    valid &= self.csc.frogpilot_planner.road_curvature_detected

    if valid:
      roll_compensation = sm["liveParameters"].roll * ACCELERATION_DUE_TO_GRAVITY
      actual_lateral_acceleration = sm["carControl"].angularVelocity[2] * sm["carState"].vEgo - roll_compensation

      valid &= abs(actual_lateral_acceleration) >= CURVE_THRESHOLD

    if valid:
      self.curve_exit_time = 0

      self.curve_samples.append(min(abs(actual_lateral_acceleration), MAX_LATERAL_ACCEL_NO_ROLL))
    elif self.curve_samples:
      self.curve_exit_time += DT_MDL

    if self.curve_exit_time >= CURVE_EXIT_TIME:
      if len(self.curve_samples) * DT_MDL >= MINIMUM_CURVE_TIME:
        self.curve_values.append(float(np.percentile(self.curve_samples, CURVE_PERCENTILE)))
        self.curve_values = self.curve_values[-LEARNING_CURVES:]

        self._update_profile()

        self.csc.frogpilot_planner.params.put_nonblocking("CurvatureData", {"curves": self.curve_values})

      self.curve_exit_time = 0

      self.curve_samples = []

    self.csc.enable_training = valid

  def _update_profile(self):
    confidence = len(self.curve_values) / LEARNING_CURVES

    if self.curve_values:
      preferred_lateral_acceleration = float(np.percentile(self.curve_values, CURVE_PERCENTILE))
    else:
      preferred_lateral_acceleration = DEFAULT_LATERAL_ACCELERATION

    self.calibrated_lateral_acceleration = DEFAULT_LATERAL_ACCELERATION + confidence * (preferred_lateral_acceleration - DEFAULT_LATERAL_ACCELERATION)

    self.csc.frogpilot_planner.params.put_nonblocking("CalibratedLateralAcceleration", self.calibrated_lateral_acceleration)
    self.csc.frogpilot_planner.params.put_nonblocking("CalibrationProgress", confidence * 100)
