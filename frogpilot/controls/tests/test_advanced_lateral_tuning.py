import types

from cereal import car
from openpilot.selfdrive.locationd.lagd import LateralLagEstimator

from openpilot.frogpilot.common import frogpilot_variables

DT = 0.05


def test_custom_steer_delay_includes_lateral_delay_offset():
  estimator = LateralLagEstimator(car.CarParams(steerActuatorDelay=0.3), DT)
  estimator.frogpilot_toggles = types.SimpleNamespace(steerActuatorDelay=0.3, use_custom_steerActuatorDelay=True)

  msg = estimator.get_msg(True)

  assert abs(msg.liveDelay.lateralDelay - (0.3 + frogpilot_variables.LATERAL_DELAY_OFFSET)) < 1e-6
  assert abs(msg.liveDelay.lateralDelay - estimator.initial_lag) < 1e-6
