#!/usr/bin/env python3
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.drive_helpers import MAX_LATERAL_ACCEL_NO_ROLL

from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED, CURVE_SPEED_PROFILES, DECEL_TIME_MARGIN, DEFAULT_LATERAL_ACCELERATION
from openpilot.frogpilot.controls.lib.curve_speed_profile_learner import CurveSpeedProfileLearner
from openpilot.frogpilot.controls.lib.max_lateral_acceleration_learner import MaxLateralAccelerationLearner

CURVE_SPEED_LATERAL_ACCELERATIONS = {
  CURVE_SPEED_PROFILES["GENTLE"]: 1.75,
  CURVE_SPEED_PROFILES["STANDARD"]: DEFAULT_LATERAL_ACCELERATION
}

MAX_LIMIT_STORE_THRESHOLD = 0.05
TARGET_RISE_RATE = 1.2

class CurveSpeedController:
  def __init__(self, FrogPilotVCruise):
    self.frogpilot_planner = FrogPilotVCruise.frogpilot_planner

    self.lateral_acceleration = DEFAULT_LATERAL_ACCELERATION
    self.max_limit = DEFAULT_LATERAL_ACCELERATION

    self.enable_training = False
    self.stored_max_limit = None
    self.target_set = False

    self.max_limit_learner = MaxLateralAccelerationLearner(self)
    self.profile_learner = CurveSpeedProfileLearner(self)

  def update_lateral_acceleration(self, frogpilot_toggles):
    if frogpilot_toggles.curve_speed_profile == CURVE_SPEED_PROFILES["AUTO"]:
      self.lateral_acceleration = self.profile_learner.calibrated_lateral_acceleration
    elif frogpilot_toggles.curve_speed_profile == CURVE_SPEED_PROFILES["SPORT"]:
      self.lateral_acceleration = self.max_limit
    else:
      self.lateral_acceleration = CURVE_SPEED_LATERAL_ACCELERATIONS.get(frogpilot_toggles.curve_speed_profile, DEFAULT_LATERAL_ACCELERATION)

    self.lateral_acceleration = min(self.lateral_acceleration, self.max_limit)

    if self.frogpilot_planner.frogpilot_weather.weather_id != 0:
      self.lateral_acceleration -= self.lateral_acceleration * self.frogpilot_planner.frogpilot_weather.reduce_lateral_acceleration

  def update_max_limit(self, sm, frogpilot_toggles):
    learned = False

    if sm["controlsState"].lateralControlState.which() == "angleState":
      self.max_limit_learner.update(sm, frogpilot_toggles)
      max_limit = self.max_limit
    elif sm["controlsState"].lateralControlState.which() == "torqueState":
      use_live_parameters = sm["liveTorqueParameters"].useParams or frogpilot_toggles.force_auto_tune
      use_live_parameters &= sm["liveTorqueParameters"].latAccelFactorFiltered > 0

      if use_live_parameters:
        max_limit = sm["liveTorqueParameters"].latAccelFactorFiltered
        learned = True
      else:
        max_limit = frogpilot_toggles.maxLateralAccel
    else:
      max_limit = frogpilot_toggles.maxLateralAccel

    self.max_limit = min(max_limit, MAX_LATERAL_ACCEL_NO_ROLL)

    if learned and (self.stored_max_limit is None or abs(self.max_limit - self.stored_max_limit) >= MAX_LIMIT_STORE_THRESHOLD):
      self.stored_max_limit = self.max_limit

      self.frogpilot_planner.params.put_nonblocking("MaxLateralAcceleration", {
        "car_fingerprint": frogpilot_toggles.car_model,
        "value": self.max_limit,
      })

  def update_target(self, v_ego):
    csc_speed = max((self.lateral_acceleration / abs(self.frogpilot_planner.road_curvature))**0.5, CRUISING_SPEED)

    if not self.target_set:
      self.target_set = True
      self.target = max(v_ego, csc_speed)

    if csc_speed < self.target:
      decel_rate = max(v_ego - csc_speed, 0) / max(self.frogpilot_planner.time_to_curve - DECEL_TIME_MARGIN, 1)

      self.target = max(min(self.target, v_ego) - decel_rate * DT_MDL, csc_speed)
    elif v_ego <= self.target + 1 and abs(self.frogpilot_planner.lateral_acceleration) < self.lateral_acceleration:
      self.target = min(self.target + TARGET_RISE_RATE * DT_MDL, csc_speed)
