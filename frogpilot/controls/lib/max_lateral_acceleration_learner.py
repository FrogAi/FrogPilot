#!/usr/bin/env python3
from openpilot.common.realtime import DT_MDL

from openpilot.frogpilot.common import frogpilot_variables

TRACKING_RATIO = 0.9

MIN_LEARNING_TIME = 1.0
LEARNING_RATE = 0.05


class MaxLateralAccelerationLearner:
  def __init__(self, CurveSpeedController):
    self.csc = CurveSpeedController

    self.initialized = False

    self.tracking_time = 0

  def update(self, sm, frogpilot_toggles):
    if not self.initialized:
      self.csc.max_limit = frogpilot_toggles.maxLateralAccel

      self._update_profile(frogpilot_toggles.car_model)

      self.initialized = True

    valid = sm.all_checks(["carControl", "carState", "controlsState", "liveParameters"])
    valid &= sm["controlsState"].lateralControlState.angleState.active and sm["liveParameters"].valid and len(sm["carControl"].angularVelocity) > 2
    valid &= sm["carState"].vEgo > frogpilot_variables.CRUISING_SPEED and not sm["carState"].steeringPressed
    valid &= not (sm["carState"].leftBlinker or sm["carState"].rightBlinker)

    if valid:
      roll_compensation = self.csc.frogpilot_planner.roll_compensation

      actual_lateral_acceleration = sm["carControl"].angularVelocity[2] * sm["carState"].vEgo - roll_compensation
      desired_lateral_acceleration = sm["controlsState"].desiredCurvature * sm["carState"].vEgo**2 - roll_compensation
      modeled_lateral_acceleration = sm["controlsState"].curvature * sm["carState"].vEgo**2 - roll_compensation

      demonstrated_limit = min(abs(actual_lateral_acceleration), abs(modeled_lateral_acceleration), abs(desired_lateral_acceleration))

      valid &= actual_lateral_acceleration * desired_lateral_acceleration > 0
      valid &= modeled_lateral_acceleration * desired_lateral_acceleration > 0
      valid &= demonstrated_limit >= TRACKING_RATIO * abs(desired_lateral_acceleration)
      valid &= demonstrated_limit > self.csc.max_limit

    if valid:
      self.tracking_time += DT_MDL
    else:
      if self.tracking_time >= MIN_LEARNING_TIME:
        self._update_profile(frogpilot_toggles.car_model)

      self.tracking_time = 0.0

    if self.tracking_time >= MIN_LEARNING_TIME:
      self.csc.max_limit = min(self.csc.max_limit + LEARNING_RATE * DT_MDL, demonstrated_limit)

  def _update_profile(self, car_model):
    self.csc.frogpilot_planner.params.put_nonblocking("MaxLateralAcceleration", {
      "car_fingerprint": car_model,
      "value": self.csc.max_limit,
    })
