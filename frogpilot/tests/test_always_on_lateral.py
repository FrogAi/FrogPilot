import types

from cereal import car, custom, log
from openpilot.common.params import Params
from openpilot.selfdrive.car import gen_empty_fingerprint
from openpilot.selfdrive.car.car_helpers import interfaces
from openpilot.selfdrive.car.mock.values import CAR as MOCK
from openpilot.selfdrive.car.nissan.values import CAR as NISSAN
from openpilot.selfdrive.car.tests.test_car_interfaces import TOGGLES
from openpilot.selfdrive.controls.controlsd import Controls
from openpilot.selfdrive.controls.lib.events import ET, EventName, Events, FrogPilotEventName, always_on_lateral_waiting_alert
from openpilot.selfdrive.monitoring.helpers import DriverMonitoring
from panda import ALTERNATIVE_EXPERIENCE

from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.controls.frogpilot_card import FrogPilotCard
from openpilot.frogpilot.controls.lib.always_on_lateral import AlwaysOnLateral

ButtonType = car.CarState.ButtonEvent.Type
State = log.ControlsState.OpenpilotState


class Drive:
  def __init__(self, always_on_lateral=True, car_name="", lkas_button=False, lkas_button_forced=False, main_at_start_car=False, openpilot_longitudinal=False,
               pause_on_brake_speed=0):
    self.CP = car.CarParams.new_message(carFingerprint="TEST_CAR", carName=car_name, openpilotLongitudinalControl=openpilot_longitudinal)
    self.FPCP = custom.FrogPilotCarParams.new_message(alwaysOnLateralLKASButton=lkas_button_forced)

    self.toggles = types.SimpleNamespace(always_on_lateral=always_on_lateral, always_on_lateral_lkas_button=lkas_button or (lkas_button_forced and always_on_lateral),
                                         always_on_lateral_pause_on_brake_speed=pause_on_brake_speed)

    if main_at_start_car:
      frogpilot_variables.CRUISE_MAIN_OFF_AT_START_CARS.add("TEST_CAR")

    self.always_on_lateral = AlwaysOnLateral(self.CP, self.FPCP, self.toggles)

    frogpilot_variables.CRUISE_MAIN_OFF_AT_START_CARS.discard("TEST_CAR")

  def step(self, cruise_main=True, lkas_pressed=False, engaged=False, brake=False, standstill=False, v_ego=20.0, events=(), frogpilot_events=(),
           lateral_check=True, can_valid=True, speed_sensors_disagree=False, steering_disengage=False, rx_checks_invalid=False):
    carState = car.CarState.new_message(brakePressed=brake, canValid=can_valid, standstill=standstill, vEgo=v_ego)
    carState.cruiseState.available = cruise_main

    if lkas_pressed:
      carState.buttonEvents = [{"pressed": True, "type": ButtonType.lkas}]

    onroad_events = Events()
    for event in events:
      onroad_events.add(event)

    plan_events = Events(frogpilot=True)
    for event in frogpilot_events:
      plan_events.add(event)

    sm = {
      "carControl": car.CarControl.new_message(enabled=engaged),
      "frogpilotPlan": custom.FrogPilotPlan.new_message(frogpilotEvents=plan_events.to_msg(), lateralCheck=lateral_check),
      "onroadEvents": onroad_events.to_msg(),
    }
    frogpilotCarState = custom.FrogPilotCarState.new_message(speedSensorsDisagree=speed_sensors_disagree, steeringDisengage=steering_disengage)

    self.always_on_lateral.update(carState, frogpilotCarState, sm, self.toggles, rx_checks_invalid)

    return frogpilotCarState


def test_alternative_experience():
  off = Drive(always_on_lateral=False)
  on = Drive()
  lkas_button = Drive(lkas_button=True)

  assert off.CP.alternativeExperience == 0
  assert on.CP.alternativeExperience == ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL
  assert lkas_button.CP.alternativeExperience == ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL | ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL_LKAS_BUTTON


def test_always_on_lateral_off():
  off = Drive(always_on_lateral=False)

  off.step(cruise_main=False)

  assert not off.step().alwaysOnLateralAllowed


def test_brake_pause():
  drive = Drive(pause_on_brake_speed=10)
  drive.step(cruise_main=False)
  drive.step()

  assert drive.step(brake=True, v_ego=15).alwaysOnLateralEnabled
  assert not drive.step(brake=True, v_ego=5).alwaysOnLateralEnabled
  assert drive.step(brake=True, standstill=True, v_ego=0).alwaysOnLateralEnabled
  assert drive.step(v_ego=5).alwaysOnLateralEnabled


def test_card_rx_checks_invalid():
  CP = car.CarParams.new_message(carName="toyota", safetyConfigs=[{"safetyModel": car.CarParams.SafetyModel.toyota}])
  toggles = types.SimpleNamespace(always_on_lateral=True, always_on_lateral_lkas_button=False, always_on_lateral_pause_on_brake_speed=0)
  car_interface = types.SimpleNamespace(CS=types.SimpleNamespace())
  card = FrogPilotCard(types.SimpleNamespace(CI=car_interface, CP=CP, FPCP=custom.FrogPilotCarParams.new_message(), frogpilot_toggles=toggles))

  def step(cruise_main=True, rx_checks_invalid_models=()):
    carState = car.CarState.new_message(canValid=True, vEgo=20.0)
    carState.cruiseState.available = cruise_main

    pandaStates = [{"safetyModel": model, "safetyRxChecksInvalid": True} for model in rx_checks_invalid_models]
    sm = {
      "carControl": car.CarControl.new_message(),
      "frogpilotPlan": custom.FrogPilotPlan.new_message(lateralCheck=True),
      "onroadEvents": [],
      "pandaStates": [log.PandaState.new_message(**panda_state) for panda_state in pandaStates],
    }

    return card.update(carState, custom.FrogPilotCarState.new_message(), sm)

  step(cruise_main=False)

  assert step().alwaysOnLateralEnabled
  assert not car_interface.CS.car_safety_mode_set
  assert not step(rx_checks_invalid_models=[car.CarParams.SafetyModel.toyota]).alwaysOnLateralEnabled
  assert car_interface.CS.car_safety_mode_set
  assert step(rx_checks_invalid_models=[car.CarParams.SafetyModel.elm327]).alwaysOnLateralEnabled
  assert not car_interface.CS.car_safety_mode_set


def test_controlsd_engaged_steering_uses_car_params():
  CarInterface, CarController, CarState = interfaces[MOCK.MOCK]
  CP = CarInterface.get_non_essential_params(MOCK.MOCK)
  CP.lateralTuning.pid.kpBP, CP.lateralTuning.pid.kpV = [0.0], [0.1]
  CP.lateralTuning.pid.kiBP, CP.lateralTuning.pid.kiV = [0.0], [0.0]
  CI = CarInterface(CP, custom.FrogPilotCarParams.new_message(), CarController, CarState)
  controlsd = Controls(CI=CI)

  for car_params_bit, lat_active in ((0, True), (ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL, False)):
    controlsd.CP.alternativeExperience = car_params_bit
    controlsd.frogpilot_toggles.always_on_lateral = True
    controlsd.sm.data["frogpilotPlan"] = custom.FrogPilotPlan.new_message(lateralCheck=True)
    controlsd.state = State.enabled
    controlsd.enabled = controlsd.active = True

    CC, _ = controlsd.state_control(car.CarState.new_message(vEgo=20.0))

    assert CC.latActive == lat_active, car_params_bit


def test_controlsd_warning_alerts():
  CarInterface, CarController, CarState = interfaces[MOCK.MOCK]
  CI = CarInterface(CarInterface.get_non_essential_params(MOCK.MOCK), custom.FrogPilotCarParams.new_message(), CarController, CarState)
  controlsd = Controls(CI=CI)

  for always_on_lateral_enabled in (False, True):
    controlsd.sm.data["frogpilotCarState"] = custom.FrogPilotCarState.new_message(alwaysOnLateralEnabled=always_on_lateral_enabled)
    controlsd.sm.data["frogpilotPlan"] = custom.FrogPilotPlan.new_message()
    controlsd.state = State.disabled

    controlsd.state_transition(car.CarState.new_message())

    assert (ET.WARNING in controlsd.current_alert_types) == always_on_lateral_enabled


def test_cruise_main():
  drive = Drive()
  drive.step(cruise_main=False)

  assert drive.step(cruise_main=True).alwaysOnLateralEnabled
  assert not drive.step(cruise_main=False).alwaysOnLateralAllowed


def test_cruise_main_on_at_start():
  drive = Drive()

  started = drive.step()
  switched_off = drive.step(cruise_main=False)

  assert not started.alwaysOnLateralAllowed
  assert started.alwaysOnLateralWaiting
  assert not switched_off.alwaysOnLateralWaiting


def test_cruise_main_on_after_invalid_can():
  drive = Drive()
  drive.step(cruise_main=False, can_valid=False)

  frogpilotCarState = drive.step()

  assert not frogpilotCarState.alwaysOnLateralAllowed
  assert frogpilotCarState.alwaysOnLateralWaiting


def test_driver_monitoring_while_paused():
  for allowed in (False, True):
    driver_monitoring = DriverMonitoring()
    driver_monitoring.awareness = 0.5

    sm = {
      "carState": car.CarState.new_message(vEgo=20.0),
      "controlsState": log.ControlsState.new_message(enabled=False),
      "driverStateV2": log.DriverStateV2.new_message(),
      "frogpilotCarState": custom.FrogPilotCarState.new_message(alwaysOnLateralAllowed=allowed, alwaysOnLateralEnabled=False),
      "liveCalibration": log.LiveCalibrationData.new_message(rpyCalib=[0.0, 0.0, 0.0]),
      "modelV2": log.ModelDataV2.new_message(),
    }
    sm["modelV2"].meta.disengagePredictions.brakeDisengageProbs = [0.0]

    driver_monitoring.run_step(sm)

    assert (driver_monitoring.awareness < 1.0) == allowed


def test_engagement():
  drive = Drive()
  drive.step()

  engaged = drive.step(engaged=True)
  disengaged = drive.step()

  assert engaged.alwaysOnLateralAllowed
  assert not engaged.alwaysOnLateralWaiting
  assert disengaged.alwaysOnLateralAllowed


def test_lkas_button():
  for lkas_button in (False, True):
    drive = Drive(lkas_button=lkas_button)
    drive.step(cruise_main=False)

    pressed = drive.step(cruise_main=False, lkas_pressed=True)
    pressed_again = drive.step(cruise_main=False, lkas_pressed=True)

    assert pressed.alwaysOnLateralAllowed == lkas_button
    assert not pressed_again.alwaysOnLateralAllowed


def test_lkas_button_forced():
  for always_on_lateral in (False, True):
    drive = Drive(always_on_lateral=always_on_lateral, lkas_button_forced=True)
    drive.step(cruise_main=False)

    pressed = drive.step(cruise_main=False, lkas_pressed=True)

    assert bool(drive.CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL_LKAS_BUTTON) == always_on_lateral
    assert pressed.alwaysOnLateralAllowed == always_on_lateral


def test_lkas_button_with_cruise_main_off():
  drive = Drive(lkas_button=True)
  drive.step(cruise_main=False)
  drive.step()

  frogpilotCarState = drive.step(cruise_main=False, lkas_pressed=True)

  assert not frogpilotCarState.alwaysOnLateralAllowed


def test_lkas_switch_toggles(monkeypatch):
  params = Params()
  monkeypatch.setattr(frogpilot_variables, "params", params)

  CarInterface, _, _ = interfaces[NISSAN.NISSAN_ROGUE]
  CP = CarInterface.get_params(NISSAN.NISSAN_ROGUE, gen_empty_fingerprint(), [], False, TOGGLES, docs=False)
  params.put("CarParamsPersistent", CP.to_bytes())
  params.put_bool("AlwaysOnLateral", True)
  params.put_bool("AlwaysOnLateralLKASButton", True)

  toggles = frogpilot_variables.FrogPilotVariables().frogpilot_toggles

  assert toggles.has_lkas_button
  assert not toggles.can_use_always_on_lateral_lkas_button and not toggles.always_on_lateral_lkas_button


def test_main_at_start():
  listed_brand = Drive(car_name="chrysler")
  listed_car = Drive(main_at_start_car=True)

  assert listed_brand.step().alwaysOnLateralAllowed
  assert listed_car.step().alwaysOnLateralAllowed


def test_main_at_start_hyundai_openpilot_longitudinal():
  for car_name, openpilot_longitudinal, allowed in (("hyundai", False, False), ("hyundai", True, True), ("toyota", True, False)):
    drive = Drive(car_name=car_name, openpilot_longitudinal=openpilot_longitudinal)

    frogpilotCarState = drive.step()

    assert frogpilotCarState.alwaysOnLateralAllowed == allowed, (car_name, openpilot_longitudinal)
    assert frogpilotCarState.alwaysOnLateralWaiting != allowed, (car_name, openpilot_longitudinal)


def test_no_cruise_main():
  drive = Drive(car_name="tesla")

  started = drive.step()
  drive.step(cruise_main=False)
  cruise_main_on = drive.step()
  engaged = drive.step(engaged=True)
  cruise_main_off = drive.step(cruise_main=False)
  braked = drive.step(brake=True)

  assert not started.alwaysOnLateralWaiting
  assert not cruise_main_on.alwaysOnLateralAllowed
  assert engaged.alwaysOnLateralAllowed
  assert cruise_main_off.alwaysOnLateralEnabled
  assert not braked.alwaysOnLateralAllowed


def test_no_cruise_main_brake_while_engaged():
  drive = Drive(car_name="tesla")
  drive.step(engaged=True)

  braked = drive.step(engaged=True, brake=True)
  disengaged = drive.step(brake=True)

  assert not braked.alwaysOnLateralAllowed
  assert not disengaged.alwaysOnLateralAllowed


def test_no_cruise_main_held_brake():
  drive = Drive(car_name="tesla")
  drive.step(brake=True, standstill=True, v_ego=0)

  engaged = drive.step(engaged=True, brake=True, standstill=True, v_ego=0)
  cancelled = drive.step(brake=True, standstill=True, v_ego=0)
  drive.step()
  braked = drive.step(brake=True)

  assert engaged.alwaysOnLateralEnabled
  assert cancelled.alwaysOnLateralAllowed
  assert not braked.alwaysOnLateralAllowed


def test_no_cruise_main_quick_restart():
  drive = Drive(car_name="tesla")
  drive.step(engaged=True)
  drive.step()
  drive.step(can_valid=False)

  restarted = drive.step()

  assert not restarted.alwaysOnLateralAllowed
  assert not restarted.alwaysOnLateralWaiting


def test_paused():
  for step in ({"events": (EventName.steerUnavailable,)}, {"events": (EventName.wrongGear,)}, {"events": (EventName.tooDistracted,)},
               {"frogpilot_events": (FrogPilotEventName.openpilotCrashed,)}, {"lateral_check": False}, {"speed_sensors_disagree": True}):
    drive = Drive()
    drive.step(cruise_main=False)
    drive.step()

    paused = drive.step(**step)
    resumed = drive.step()

    assert paused.alwaysOnLateralAllowed, step
    assert not paused.alwaysOnLateralEnabled, step
    assert resumed.alwaysOnLateralEnabled, step


def test_quick_restart():
  main_on = Drive()
  main_on.step(cruise_main=False)
  main_on.step(cruise_main=False, can_valid=False)

  main_off = Drive()
  main_off.step(cruise_main=False)
  main_off.step()
  main_off.step(can_valid=False)

  armed = Drive()
  armed.step(cruise_main=False)
  armed.step()
  armed.step(can_valid=False)

  listed_car = Drive(main_at_start_car=True)
  listed_car.step(can_valid=False)

  armed_restarted = armed.step()
  main_on_restarted = main_on.step()
  main_off_restarted = main_off.step(cruise_main=False)

  assert not armed_restarted.alwaysOnLateralAllowed
  assert armed_restarted.alwaysOnLateralWaiting
  assert not main_on_restarted.alwaysOnLateralAllowed
  assert main_on_restarted.alwaysOnLateralWaiting
  assert not main_off_restarted.alwaysOnLateralAllowed
  assert not main_off_restarted.alwaysOnLateralWaiting
  assert listed_car.step().alwaysOnLateralAllowed


def test_rx_checks_invalid():
  drive = Drive(main_at_start_car=True)

  armed = drive.step()
  paused = drive.step(rx_checks_invalid=True)
  resumed = drive.step()

  assert armed.alwaysOnLateralEnabled
  assert paused.alwaysOnLateralAllowed and not paused.alwaysOnLateralEnabled
  assert resumed.alwaysOnLateralEnabled


def test_steering_off_only_from_lkas_press():
  main_off = Drive()
  main_off.step()
  main_off.step(engaged=True)

  no_cruise_main = Drive(car_name="tesla")
  no_cruise_main.step(engaged=True)

  assert not main_off.step(engaged=True, cruise_main=False).alwaysOnLateralSteeringOff
  assert not no_cruise_main.step(engaged=True, brake=True).alwaysOnLateralSteeringOff


def test_steering_off_while_engaged():
  drive = Drive(lkas_button=True)
  drive.step()

  engaged = drive.step(engaged=True)
  turned_off = drive.step(engaged=True, lkas_pressed=True)
  turned_on = drive.step(engaged=True, lkas_pressed=True)

  drive.step(engaged=True, lkas_pressed=True)
  disengaged = drive.step()

  assert not engaged.alwaysOnLateralSteeringOff
  assert turned_off.alwaysOnLateralSteeringOff
  assert not turned_on.alwaysOnLateralSteeringOff
  assert not disengaged.alwaysOnLateralSteeringOff


def test_steering_disengage():
  drive = Drive()
  drive.step(cruise_main=False)
  drive.step()

  assert not drive.step(steering_disengage=True).alwaysOnLateralAllowed


def test_steering_disengage_while_engaged():
  drive = Drive()
  drive.step(cruise_main=False)
  drive.step(engaged=True)

  overridden = drive.step(engaged=True, steering_disengage=True)
  still_engaged = drive.step(engaged=True)
  disengaged = drive.step()

  assert overridden.alwaysOnLateralEnabled
  assert still_engaged.alwaysOnLateralEnabled
  assert not disengaged.alwaysOnLateralAllowed


def test_toggle_changed_mid_drive():
  switched_on = Drive(always_on_lateral=False)
  switched_on.toggles.always_on_lateral = True
  switched_on.step(cruise_main=False)

  switched_off = Drive()
  switched_off.step(cruise_main=False)
  armed = switched_off.step()
  switched_off.toggles.always_on_lateral = False

  lkas_button_switched_off = Drive(lkas_button=True)
  lkas_button_switched_off.step(cruise_main=False)
  lkas_button_switched_off.toggles.always_on_lateral_lkas_button = False

  assert not switched_on.step().alwaysOnLateralAllowed
  assert armed.alwaysOnLateralEnabled
  assert not switched_off.step().alwaysOnLateralAllowed
  assert not switched_off.step().alwaysOnLateralEnabled
  assert switched_off.step(engaged=True).alwaysOnLateralAllowed
  assert not lkas_button_switched_off.step(cruise_main=False, lkas_pressed=True).alwaysOnLateralAllowed


def test_waiting_alert():
  for lkas_button in (False, True):
    toggles = types.SimpleNamespace(always_on_lateral_lkas_button=lkas_button)

    alert = always_on_lateral_waiting_alert(car.CarParams.new_message(), car.CarState.new_message(), {}, False, 0, toggles)

    assert "cruise" in alert.alert_text_2, lkas_button
    assert ("LKAS" in alert.alert_text_2) == lkas_button, lkas_button
    assert "engage" in alert.alert_text_2.lower(), lkas_button
