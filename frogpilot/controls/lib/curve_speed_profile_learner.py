#!/usr/bin/env python3
import json
import numpy as np

from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.drive_helpers import MAX_LATERAL_ACCEL_NO_ROLL

from openpilot.frogpilot.common import frogpilot_variables

CURVE_EXIT_TIME = 0.5
MINIMUM_CURVE_TIME = 0.5

CURVE_PERCENTILE = 90
LEARNING_CURVES = 100

class CurveSpeedProfileLearner:
  def __init__(self, CurveSpeedController):
    self.csc = CurveSpeedController

    self.signalled = False

    self.curve_exit_time = 0

    self.curve_samples = []

    self.calibrated_lateral_acceleration = frogpilot_variables.DEFAULT_LATERAL_ACCELERATION

    self.curve_values = json.loads(frogpilot_variables.params.get("CurvatureData") or "{}").get("curves", [])[-LEARNING_CURVES:]

    self._update_profile()

  def update(self, long_control_active, sm):
    blinker = sm["carState"].leftBlinker or sm["carState"].rightBlinker

    valid = sm.all_checks(["carState", "liveLocationKalman", "liveParameters", "radarState"])
    valid &= sm["liveParameters"].valid and sm["liveLocationKalman"].angularVelocityCalibrated.valid
    valid &= len(sm["liveLocationKalman"].angularVelocityCalibrated.value) > 2
    valid &= not long_control_active and not sm["carState"].cruiseState.enabled and not sm["radarState"].leadOne.status
    valid &= self.csc.frogpilot_planner.frogpilot_weather.weather_id == 0 or self.csc.frogpilot_planner.frogpilot_weather.reduce_lateral_acceleration == 0
    valid &= sm["carState"].vEgo > frogpilot_variables.CRUISING_SPEED

    if valid:
      roll_compensation = self.csc.frogpilot_planner.roll_compensation

      actual_lateral_acceleration = sm["liveLocationKalman"].angularVelocityCalibrated.value[2] * sm["carState"].vEgo - roll_compensation
      in_curve = abs(actual_lateral_acceleration) >= frogpilot_variables.MINIMUM_LATERAL_ACCELERATION

      self.signalled = blinker or (self.signalled and in_curve)

      valid &= in_curve and not self.signalled
    else:
      self.signalled = self.signalled or blinker

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

        frogpilot_variables.params.put_nonblocking("CurvatureData", json.dumps({"curves": self.curve_values}))

      self.curve_exit_time = 0

      self.curve_samples = []

    self.csc.enable_training = valid

  def _update_profile(self):
    confidence = len(self.curve_values) / LEARNING_CURVES

    if self.curve_values:
      preferred_lateral_acceleration = float(np.mean(self.curve_values))
    else:
      preferred_lateral_acceleration = frogpilot_variables.DEFAULT_LATERAL_ACCELERATION

    self.calibrated_lateral_acceleration = frogpilot_variables.DEFAULT_LATERAL_ACCELERATION + confidence * (
      preferred_lateral_acceleration - frogpilot_variables.DEFAULT_LATERAL_ACCELERATION
    )

    frogpilot_variables.params.put_float_nonblocking("CalibratedLateralAcceleration", self.calibrated_lateral_acceleration)
    frogpilot_variables.params.put_float_nonblocking("CalibrationProgress", confidence * 100)
