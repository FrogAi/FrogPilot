from cereal import car, custom, log
from openpilot.selfdrive.car.car_helpers import interfaces
from openpilot.selfdrive.car.mock.values import CAR as MOCK
from openpilot.selfdrive.controls.controlsd import Controls
from openpilot.selfdrive.controls.lib.events import EventName

State = log.ControlsState.OpenpilotState


def test_forced_deceleration(mocker):
  for awareness, enabled, openpilot_longitudinal, override, state, force_decel, long_active in ((0.0, True, True, False, State.enabled, False, False),
                                                                                                (-0.1, True, True, False, State.enabled, True, True),
                                                                                                (1.0, True, True, False, State.softDisabling, True, True),
                                                                                                (-0.1, True, True, True, State.enabled, True, False),
                                                                                                (-0.1, False, True, False, State.disabled, True, False),
                                                                                                (-0.1, True, False, False, State.enabled, True, False)):
    CarInterface, CarController, CarState = interfaces[MOCK.MOCK]
    CP = CarInterface.get_non_essential_params(MOCK.MOCK)
    CP.openpilotLongitudinalControl = openpilot_longitudinal
    CI = CarInterface(CP, custom.FrogPilotCarParams.new_message(), CarController, CarState)
    controlsd = Controls(CI=CI)
    send = mocker.patch.object(controlsd.pm, "send")

    controlsd.enabled = enabled
    controlsd.sm.data["driverMonitoringState"] = log.DriverMonitoringState.new_message(awarenessStatus=awareness)
    controlsd.sm.data["frogpilotCarState"] = custom.FrogPilotCarState.new_message(pauseLongitudinal=True)
    controlsd.sm.data["longitudinalPlan"] = log.LongitudinalPlan.new_message(aTarget=-1.0)
    controlsd.state = state

    if override:
      controlsd.events.add(EventName.gasPressedOverride)

    CS = car.CarState.new_message(vEgo=20.0)

    CC, lac_log = controlsd.state_control(CS)
    controlsd.publish_logs(CS, 0.0, CC, lac_log)

    assert CC.longActive == long_active, (awareness, enabled, openpilot_longitudinal, override, state)
    assert (CC.actuators.accel < 0) == long_active, (awareness, enabled, openpilot_longitudinal, override, state)
    assert send.call_args_list[0].args[1].controlsState.forceDecel == force_decel, (awareness, enabled, openpilot_longitudinal, override, state)
