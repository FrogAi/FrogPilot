import types

import pytest
from cereal import car
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.locationd.lagd import BLOCK_NUM_NEEDED, LateralLagEstimator

CP = car.CarParams.new_message(steerActuatorDelay=0.12)


def get_lateral_delay(use_custom, steer_actuator_delay=CP.steerActuatorDelay, learned_delay=None):
  estimator = LateralLagEstimator(CP, DT_MDL)
  estimator.frogpilot_toggles = types.SimpleNamespace(steerActuatorDelay=steer_actuator_delay, use_custom_steerActuatorDelay=use_custom)

  if learned_delay is not None:
    estimator.reset(learned_delay, BLOCK_NUM_NEEDED + 1)

  return estimator.initial_lag, estimator.get_msg(True).liveDelay.lateralDelay


def test_custom_delay():
  initial_lag, custom_delay = get_lateral_delay(True, CP.steerActuatorDelay + 0.1)
  _, custom_delay_while_learned = get_lateral_delay(True, CP.steerActuatorDelay + 0.1, learned_delay=0.25)

  assert custom_delay == pytest.approx(initial_lag + 0.1)
  assert custom_delay_while_learned == custom_delay


def test_default_delay():
  initial_lag, default_delay = get_lateral_delay(False)
  _, custom_delay_at_default = get_lateral_delay(True)

  assert default_delay == pytest.approx(initial_lag)
  assert custom_delay_at_default == default_delay


def test_learned_delay():
  _, learned_delay = get_lateral_delay(False, learned_delay=0.25)

  assert learned_delay == pytest.approx(0.25)
