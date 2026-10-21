from cereal import car, custom, log
from opendbc.car.car_helpers import interfaces
from opendbc.car.honda.values import CAR as HONDA
from openpilot.common.params import Params
from openpilot.selfdrive.controls.controlsd import Controls

State = log.SelfdriveState.OpenpilotState


def test_forced_deceleration(mocker):
  for awareness, enabled, openpilot_longitudinal, override, state, force_decel, long_active in ((0.0, True, True, False, State.enabled, False, False),
                                                                                                (-0.1, True, True, False, State.enabled, True, True),
                                                                                                (1.0, True, True, False, State.softDisabling, True, True),
                                                                                                (-0.1, True, True, True, State.enabled, True, False),
                                                                                                (-0.1, False, True, False, State.enabled, True, False),
                                                                                                (-0.1, True, False, False, State.enabled, True, False)):
    CP = interfaces[HONDA.HONDA_CIVIC].get_non_essential_params(HONDA.HONDA_CIVIC)
    CP.openpilotLongitudinalControl = openpilot_longitudinal
    params = Params()
    params.put("CarParams", CP.to_bytes())
    params.put("FrogPilotCarParams", custom.FrogPilotCarParams.new_message().to_bytes())
    controlsd = Controls()
    send = mocker.patch.object(controlsd.pm, "send")

    controlsd.sm.data["carState"] = car.CarState.new_message(vEgo=20.0)
    controlsd.sm.data["driverMonitoringState"] = log.DriverMonitoringState.new_message(awarenessStatus=awareness)
    controlsd.sm.data["frogpilotCarState"] = custom.FrogPilotCarState.new_message(pauseLongitudinal=True)
    controlsd.sm.data["longitudinalPlan"] = log.LongitudinalPlan.new_message(aTarget=-1.0)
    controlsd.sm.data["onroadEvents"] = [log.OnroadEvent.new_message(overrideLongitudinal=override)]
    controlsd.sm.data["selfdriveState"] = log.SelfdriveState.new_message(enabled=enabled, state=state)

    CC, lac_log = controlsd.state_control()
    controlsd.publish(CC, lac_log)

    assert CC.longActive == long_active, (awareness, enabled, openpilot_longitudinal, override, state)
    assert (CC.actuators.accel < 0) == long_active, (awareness, enabled, openpilot_longitudinal, override, state)
    assert send.call_args_list[0].args[1].controlsState.forceDecel == force_decel, (awareness, enabled, openpilot_longitudinal, override, state)
