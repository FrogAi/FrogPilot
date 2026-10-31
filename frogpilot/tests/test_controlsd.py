import time

import cereal.messaging as messaging
from cereal import car, custom
from openpilot.common.realtime import DT_CTRL
from openpilot.selfdrive.car.car_helpers import interfaces
from openpilot.selfdrive.car.mock.values import CAR as MOCK
from openpilot.selfdrive.controls.controlsd import Controls
from openpilot.selfdrive.locationd.lagd import LateralLagEstimator


def test_lat_delay_before_live_delay(mocker):
  CarInterface, CarController, CarState = interfaces[MOCK.MOCK]
  CP = CarInterface.get_non_essential_params(MOCK.MOCK)
  CP.steerActuatorDelay = 0.12
  CI = CarInterface(CP, custom.FrogPilotCarParams.new_message(), CarController, CarState)
  controlsd = Controls(CI=CI)
  controlsd.lat_smooth_seconds = 0.1

  lateral_update = mocker.patch.object(controlsd.LaC, "update", wraps=controlsd.LaC.update)

  controlsd.state_control(car.CarState.new_message(vEgo=20.0))

  msg = messaging.new_message("liveDelay")
  msg.liveDelay.lateralDelay = 0.3
  controlsd.sm.update_msgs(time.monotonic(), [msg])
  controlsd.state_control(car.CarState.new_message(vEgo=20.0))

  assert lateral_update.call_args_list[0].args[7] == LateralLagEstimator(controlsd.CP, DT_CTRL).initial_lag + 0.1
  assert lateral_update.call_args_list[1].args[7] == controlsd.sm["liveDelay"].lateralDelay + 0.1
