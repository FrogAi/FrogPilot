import importlib.util
import types

from cereal import car, custom, log
from opendbc.car.car_helpers import interfaces
from opendbc.car.chrysler.values import CAR as CHRYSLER
from opendbc.car.ford.values import CAR as FORD
from opendbc.car.gm.values import CAR as GM
from opendbc.car.honda.values import CAR as HONDA
from opendbc.car.hyundai.values import CAR as HYUNDAI
from opendbc.car.toyota.values import CAR as TOYOTA, ToyotaFlags
from opendbc.safety import ALTERNATIVE_EXPERIENCE
from openpilot.common.params import Params
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.controlsd import Controls
from openpilot.selfdrive.monitoring.helpers import DriverMonitoring
from openpilot.selfdrive.selfdrived import events
from openpilot.selfdrive.selfdrived.events import ET, EventName, Events, FrogPilotEventName, always_on_lateral_waiting_alert
from openpilot.selfdrive.selfdrived.state import State, StateMachine
from openpilot.system.hardware import HARDWARE

from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.controls.frogpilot_card import FrogPilotCard
from openpilot.frogpilot.controls.lib.always_on_lateral import AlwaysOnLateral
from openpilot.frogpilot.controls.lib.frogpilot_events import ALWAYS_ON_LATERAL_WAITING_ALERT_TIME, FrogPilotEvents

GearShifter = car.CarState.GearShifter

FROGPILOT_EVENTS_PLANNER = types.SimpleNamespace(frogpilot_vcruise=types.SimpleNamespace(forcing_stop=False,
                                                                                         slc=types.SimpleNamespace(speed_limit_changed_timer=0)),
                                                lead_one=types.SimpleNamespace(status=False), lead_relevant=False,
                                                params=types.SimpleNamespace(get=lambda key: None))
FROGPILOT_EVENTS_TOGGLES = types.SimpleNamespace(current_holiday_theme="stock", nnff=False, random_events=False, speed_limit_changed_alert=False)


class SubMaster(dict):
  def __init__(self, messages, alive=True):
    super().__init__(messages)
    self.alive = dict.fromkeys(messages, alive)


class Drive:
  def __init__(self, always_on_lateral=True, brand="", fingerprint="TEST_CAR", lkas_button=False, lkas_button_forced=False, main_at_start_car=False,
               openpilot_longitudinal=False, passive=False, pause_on_brake_speed=0):
    self.CP = car.CarParams.new_message(brand=brand, carFingerprint=fingerprint, openpilotLongitudinalControl=openpilot_longitudinal, passive=passive)
    self.FPCP = custom.FrogPilotCarParams.new_message(alwaysOnLateralLKASButton=lkas_button_forced)

    self.toggles = types.SimpleNamespace(always_on_lateral=always_on_lateral, always_on_lateral_lkas_button=lkas_button,
                                         always_on_lateral_pause_on_brake_speed=pause_on_brake_speed)

    if main_at_start_car:
      frogpilot_variables.CRUISE_MAIN_OFF_AT_START_CAR.add("TEST_CAR")

    self.always_on_lateral = AlwaysOnLateral(self.CP, self.FPCP, self.toggles)

    frogpilot_variables.CRUISE_MAIN_OFF_AT_START_CAR.discard("TEST_CAR")

  def step(self, cruise_main=True, lkas_pressed=False, engaged=False, brake=False, standstill=False, v_ego=20.0, gear=GearShifter.drive,
           events=(), has_disable_events=False, lateral_check=True, can_valid=True, steering_disengage=False, rx_checks_invalid=False,
           selfdrived_alive=True, speed_sensors_disagree=False):
    carState = car.CarState.new_message(brakePressed=brake, canValid=can_valid, gearShifter=gear, standstill=standstill, steeringDisengage=steering_disengage,
                                        vEgo=v_ego)
    carState.cruiseState.available = cruise_main

    if lkas_pressed:
      carState.buttonEvents = [{"pressed": True, "type": car.CarState.ButtonEvent.Type.lkas}]

    onroad_events = Events()
    for event in events:
      onroad_events.add(event)

    sm = SubMaster({
      "carControl": car.CarControl.new_message(enabled=engaged),
      "frogpilotPlan": custom.FrogPilotPlan.new_message(lateralCheck=lateral_check),
      "frogpilotSelfdriveState": custom.FrogPilotSelfdriveState.new_message(hasDisableEvents=has_disable_events),
      "onroadEvents": onroad_events.to_msg(),
    }, alive=selfdrived_alive)
    frogpilotCarState = custom.FrogPilotCarState.new_message(speedSensorsDisagree=speed_sensors_disagree)

    self.always_on_lateral.update(carState, frogpilotCarState, sm, self.toggles, rx_checks_invalid)

    return frogpilotCarState


def test_alternative_experience():
  off = Drive(always_on_lateral=False)
  on = Drive()
  lkas_button = Drive(lkas_button=True)
  passive = Drive(lkas_button=True, passive=True)

  assert off.CP.alternativeExperience == 0
  assert on.CP.alternativeExperience == ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL
  assert lkas_button.CP.alternativeExperience == ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL | ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL_LKAS_BUTTON
  assert passive.CP.alternativeExperience == 0


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
  CP = car.CarParams.new_message(brand="toyota", safetyConfigs=[{"safetyModel": car.CarParams.SafetyModel.toyota}])
  toggles = types.SimpleNamespace(always_on_lateral=True, always_on_lateral_lkas_button=False, always_on_lateral_pause_on_brake_speed=0)
  card = FrogPilotCard(CP, custom.FrogPilotCarParams.new_message(), toggles)

  def step(cruise_main=True, rx_checks_invalid_models=()):
    carState = car.CarState.new_message(canValid=True, gearShifter=GearShifter.drive, vEgo=20.0)
    carState.cruiseState.available = cruise_main

    pandaStates = [{"safetyModel": model, "safetyRxChecksInvalid": True} for model in rx_checks_invalid_models]
    sm = SubMaster({
      "carControl": car.CarControl.new_message(),
      "frogpilotPlan": custom.FrogPilotPlan.new_message(lateralCheck=True),
      "frogpilotSelfdriveState": custom.FrogPilotSelfdriveState.new_message(),
      "onroadEvents": [],
      "pandaStates": [log.PandaState.new_message(**panda_state) for panda_state in pandaStates],
    })

    return card.update(carState, custom.FrogPilotCarState.new_message(), sm, toggles)

  step(cruise_main=False)

  assert step().alwaysOnLateralEnabled
  assert not card.rx_checks_invalid
  assert not step(rx_checks_invalid_models=[car.CarParams.SafetyModel.toyota]).alwaysOnLateralEnabled
  assert card.rx_checks_invalid
  assert step(rx_checks_invalid_models=[car.CarParams.SafetyModel.elm327]).alwaysOnLateralEnabled
  assert not card.rx_checks_invalid


def test_card_safety_mode_set():
  CP = car.CarParams.new_message(brand="volkswagen", safetyConfigs=[{"safetyModel": car.CarParams.SafetyModel.volkswagen}])
  toggles = types.SimpleNamespace(always_on_lateral=True, always_on_lateral_lkas_button=False, always_on_lateral_pause_on_brake_speed=0)
  card = FrogPilotCard(CP, custom.FrogPilotCarParams.new_message(), toggles)

  for models, safety_mode_set in (((), False), ((car.CarParams.SafetyModel.elm327,), False), ((car.CarParams.SafetyModel.volkswagen,), True)):
    sm = SubMaster({
      "carControl": car.CarControl.new_message(),
      "frogpilotPlan": custom.FrogPilotPlan.new_message(lateralCheck=True),
      "frogpilotSelfdriveState": custom.FrogPilotSelfdriveState.new_message(),
      "onroadEvents": [],
      "pandaStates": [log.PandaState.new_message(safetyModel=model) for model in models],
    })

    card.update(car.CarState.new_message(canValid=True), custom.FrogPilotCarState.new_message(), sm, toggles)

    assert card.car_safety_mode_set == safety_mode_set, models


def test_card_speed_sensors_disagree_needs_valid_can():
  CP = car.CarParams.new_message(brand="ford", safetyConfigs=[{"safetyModel": car.CarParams.SafetyModel.ford}])
  toggles = types.SimpleNamespace(always_on_lateral=True, always_on_lateral_lkas_button=False, always_on_lateral_pause_on_brake_speed=0)
  card = FrogPilotCard(CP, custom.FrogPilotCarParams.new_message(), toggles)

  sm = SubMaster({
    "carControl": car.CarControl.new_message(),
    "frogpilotPlan": custom.FrogPilotPlan.new_message(lateralCheck=True),
    "frogpilotSelfdriveState": custom.FrogPilotSelfdriveState.new_message(),
    "onroadEvents": [],
    "pandaStates": [],
  })

  for can_valid in (False, True):
    carState = car.CarState.new_message(canValid=can_valid, gearShifter=GearShifter.drive, vEgo=20.0)

    frogpilotCarState = card.update(carState, custom.FrogPilotCarState.new_message(speedSensorsDisagree=True), sm, toggles)

    assert frogpilotCarState.speedSensorsDisagree == can_valid


def test_comma_four_alerts(monkeypatch):
  monkeypatch.setattr(HARDWARE, "get_device_type", lambda: "mici")
  spec = importlib.util.spec_from_file_location("comma_four_events", events.__file__)
  comma_four_events = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(comma_four_events)

  steering_off = comma_four_events.FROGPILOT_EVENTS[FrogPilotEventName.alwaysOnLateralSteeringOff][ET.PERMANENT]
  waiting_alert = comma_four_events.FROGPILOT_EVENTS[FrogPilotEventName.alwaysOnLateralWaiting][ET.PERMANENT]

  for lkas_button in (False, True):
    toggles = types.SimpleNamespace(always_on_lateral_lkas_button=lkas_button)

    waiting = waiting_alert(car.CarParams.new_message(), car.CarState.new_message(), {}, False, 0, None, toggles)

    assert len(waiting.alert_text_1) <= 16, lkas_button
    assert ("LKAS" in waiting.alert_text_2) == lkas_button, lkas_button
    assert ("Cruise" in waiting.alert_text_2) != lkas_button, lkas_button

  assert len(steering_off.alert_text_1) <= 16
  assert "LKAS" in steering_off.alert_text_2

  speed_sensors = comma_four_events.FROGPILOT_EVENTS[FrogPilotEventName.speedSensorsDisagree]

  assert len(speed_sensors[ET.PERMANENT].alert_text_1) <= 16

  speed_sensors_events = comma_four_events.Events(frogpilot=True)
  speed_sensors_events.add(FrogPilotEventName.speedSensorsDisagree)
  enable = Events()
  enable.add(EventName.buttonEnable)

  engaged = StateMachine()
  engaged.state = State.enabled
  engaged.update(Events(), speed_sensors_events, False)

  engaging = StateMachine()
  engaging.update(enable, speed_sensors_events, False)

  assert engaged.state == State.disabled
  assert engaging.state == State.disabled


def test_comma_four_waiting_alert_time(monkeypatch):
  def shown(events_tracker, waiting, frogpilot_startup_alert=False, stock_alert=False):
    frogpilotSelfdriveState = custom.FrogPilotSelfdriveState.new_message()
    if frogpilot_startup_alert:
      frogpilotSelfdriveState.alertType = "customStartupAlert/permanent"
    elif FrogPilotEventName.alwaysOnLateralWaiting in events_tracker.events.names:
      frogpilotSelfdriveState.alertType = "alwaysOnLateralWaiting/permanent"

    selfdriveState = log.SelfdriveState.new_message()
    if stock_alert:
      selfdriveState.alertSize = log.SelfdriveState.AlertSize.small

    sm = {
      "carControl": car.CarControl.new_message(),
      "carState": car.CarState.new_message(),
      "frogpilotCarState": custom.FrogPilotCarState.new_message(alwaysOnLateralWaiting=waiting),
      "frogpilotModelV2": custom.FrogPilotModelDataV2.new_message(),
      "frogpilotSelfdriveState": frogpilotSelfdriveState,
      "selfdriveState": selfdriveState,
    }
    events_tracker.update(False, sm, FROGPILOT_EVENTS_TOGGLES)

    return FrogPilotEventName.alwaysOnLateralWaiting in events_tracker.events.names

  for comma_four in (False, True):
    monkeypatch.setattr(frogpilot_variables, "IS_COMMA_FOUR", comma_four)
    events_tracker = FrogPilotEvents(FROGPILOT_EVENTS_PLANNER, None)

    shown_under_startup_alert = [shown(events_tracker, True, frogpilot_startup_alert=True) for _ in range(round(5 / DT_MDL))]
    shown_under_stock_alert = [shown(events_tracker, True, stock_alert=True) for _ in range(round(5 / DT_MDL))]
    shown_while_waiting = [shown(events_tracker, True) for _ in range(round((ALWAYS_ON_LATERAL_WAITING_ALERT_TIME + 1) / DT_MDL))]
    stopped = shown(events_tracker, False)
    waiting_again = shown(events_tracker, True)

    assert all(shown_under_startup_alert), comma_four
    assert all(shown_under_stock_alert), comma_four
    assert all(shown_while_waiting[:round((ALWAYS_ON_LATERAL_WAITING_ALERT_TIME - 0.1) / DT_MDL)]), comma_four
    assert shown_while_waiting[-1] != comma_four, comma_four
    assert not stopped, comma_four
    assert waiting_again, comma_four


def test_controlsd_engaged_steering_uses_car_params():
  always_on_lateral = ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL
  for car_params_bit, active, allowed, enabled, lat_active in ((0, True, False, False, True), (always_on_lateral, True, False, False, False),
                                                               (always_on_lateral, True, True, False, True), (always_on_lateral, False, True, True, True),
                                                               (always_on_lateral, False, True, False, False)):
    CP = interfaces[HONDA.HONDA_CIVIC].get_non_essential_params(HONDA.HONDA_CIVIC)
    CP.alternativeExperience = car_params_bit
    params = Params()
    params.put("CarParams", CP.to_bytes())
    params.put("FrogPilotCarParams", custom.FrogPilotCarParams.new_message().to_bytes())
    controlsd = Controls()

    controlsd.sm.data["carState"] = car.CarState.new_message(vEgo=20.0)
    controlsd.sm.data["frogpilotCarState"] = custom.FrogPilotCarState.new_message(alwaysOnLateralAllowed=allowed, alwaysOnLateralEnabled=enabled)
    controlsd.sm.data["frogpilotPlan"] = custom.FrogPilotPlan.new_message(lateralCheck=True)
    controlsd.sm.data["selfdriveState"] = log.SelfdriveState.new_message(active=active, enabled=active)

    CC, _ = controlsd.state_control()

    assert CC.latActive == lat_active, (car_params_bit, active, allowed, enabled)


def test_cruise_main():
  drive = Drive()
  drive.step(cruise_main=False)

  assert drive.step(cruise_main=True).alwaysOnLateralEnabled
  assert not drive.step(cruise_main=False).alwaysOnLateralAllowed


def test_cruise_main_off_beats_lkas_press():
  drive = Drive(lkas_button=True)
  drive.step(cruise_main=False)
  drive.step()

  frogpilotCarState = drive.step(cruise_main=False, lkas_pressed=True)

  assert not frogpilotCarState.alwaysOnLateralAllowed


def test_cruise_main_on_after_invalid_can():
  drive = Drive()
  drive.step(cruise_main=False, can_valid=False)

  frogpilotCarState = drive.step()

  assert not frogpilotCarState.alwaysOnLateralAllowed
  assert frogpilotCarState.alwaysOnLateralWaiting


def test_cruise_main_on_at_start():
  drive = Drive()

  started = drive.step()
  switched_off = drive.step(cruise_main=False)

  assert not started.alwaysOnLateralAllowed
  assert started.alwaysOnLateralWaiting
  assert not switched_off.alwaysOnLateralWaiting


def test_driver_monitoring_while_paused():
  for allowed in (False, True):
    driver_monitoring = DriverMonitoring()
    driver_monitoring.awareness = 0.5

    sm = {
      "carState": car.CarState.new_message(gearShifter=GearShifter.drive, vEgo=20.0),
      "driverStateV2": log.DriverStateV2.new_message(),
      "frogpilotCarState": custom.FrogPilotCarState.new_message(alwaysOnLateralAllowed=allowed, alwaysOnLateralEnabled=False),
      "liveCalibration": log.LiveCalibrationData.new_message(rpyCalib=[0.0, 0.0, 0.0]),
      "modelV2": log.ModelDataV2.new_message(),
      "selfdriveState": log.SelfdriveState.new_message(enabled=False),
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
    drive.toggles.always_on_lateral_lkas_button = always_on_lateral
    drive.step(cruise_main=False)

    pressed = drive.step(cruise_main=False, lkas_pressed=True)

    assert bool(drive.CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL_LKAS_BUTTON) == always_on_lateral
    assert pressed.alwaysOnLateralAllowed == always_on_lateral


def test_lkas_button_function():
  params = Params()
  params.put("LKASButtonControl", frogpilot_variables.BUTTON_FUNCTIONS["PAUSE_LATERAL"])
  params.put("TuningLevel", 3)
  params.put_bool("TuningLevelConfirmed", True)

  def toggles(fingerprint, always_on_lateral, lkas_button, forced=False):
    CP = interfaces[fingerprint].get_non_essential_params(fingerprint)
    if fingerprint == TOYOTA.TOYOTA_RAV4_TSS2:
      CP.flags |= ToyotaFlags.TSS2.value

    params.put("CarParamsPersistent", CP.to_bytes())
    params.put("FrogPilotCarParamsPersistent", custom.FrogPilotCarParams.new_message(alwaysOnLateralLKASButton=forced).to_bytes())
    params.put_bool("AlwaysOnLateral", always_on_lateral)
    params.put_bool("AlwaysOnLateralLKASButton", lkas_button)

    variables = frogpilot_variables.FrogPilotVariables()
    variables.update(started=False)

    return variables.frogpilot_toggles

  toyota_lkas_button = toggles(TOYOTA.TOYOTA_RAV4_TSS2, True, True)
  bronco_sport = toggles(FORD.FORD_BRONCO_SPORT_MK1, True, True)
  chrysler = toggles(CHRYSLER.RAM_1500_5TH_GEN, True, True)
  ford = toggles(FORD.FORD_EXPLORER_MK6, True, True)
  toyota_lkas_button_off = toggles(TOYOTA.TOYOTA_RAV4_TSS2, True, False)
  toyota_always_on_lateral_off = toggles(TOYOTA.TOYOTA_RAV4_TSS2, False, True)
  gm = toggles(GM.CHEVROLET_BOLT_EUV, True, True)
  honda = toggles(HONDA.HONDA_CIVIC, True, True)
  honda_forced = toggles(HONDA.HONDA_CIVIC, True, False, forced=True)
  honda_forced_always_on_lateral_off = toggles(HONDA.HONDA_CIVIC, False, False, forced=True)
  prius = toggles(TOYOTA.TOYOTA_PRIUS, True, True)
  prius_v = toggles(TOYOTA.TOYOTA_PRIUS_V, True, True)
  rav4_prime = toggles(TOYOTA.TOYOTA_RAV4_PRIME, True, True)

  assert toyota_lkas_button.can_use_always_on_lateral_lkas_button and toyota_lkas_button.always_on_lateral_lkas_button
  assert toyota_lkas_button.lkas_button_function == frogpilot_variables.BUTTON_FUNCTIONS["NOTHING"]
  assert toyota_lkas_button_off.lkas_button_function == frogpilot_variables.BUTTON_FUNCTIONS["PAUSE_LATERAL"]
  assert not toyota_always_on_lateral_off.always_on_lateral_lkas_button
  assert toyota_always_on_lateral_off.lkas_button_function == frogpilot_variables.BUTTON_FUNCTIONS["PAUSE_LATERAL"]
  assert not bronco_sport.can_use_always_on_lateral_lkas_button and not bronco_sport.always_on_lateral_lkas_button
  assert chrysler.can_use_always_on_lateral_lkas_button and chrysler.always_on_lateral_lkas_button
  assert ford.can_use_always_on_lateral_lkas_button and ford.always_on_lateral_lkas_button
  assert not gm.has_lkas_button and not gm.always_on_lateral_lkas_button
  assert gm.lkas_button_function == frogpilot_variables.BUTTON_FUNCTIONS["NOTHING"]
  assert not honda.can_use_always_on_lateral_lkas_button and not honda.always_on_lateral_lkas_button
  assert honda.lkas_button_function == frogpilot_variables.BUTTON_FUNCTIONS["PAUSE_LATERAL"]
  assert honda_forced.always_on_lateral_lkas_button_forced and honda_forced.always_on_lateral_lkas_button
  assert honda_forced.lkas_button_function == frogpilot_variables.BUTTON_FUNCTIONS["NOTHING"]
  assert not honda_forced_always_on_lateral_off.always_on_lateral_lkas_button
  assert honda_forced_always_on_lateral_off.lkas_button_function == frogpilot_variables.BUTTON_FUNCTIONS["PAUSE_LATERAL"]
  assert prius.always_on_lateral_lkas_button
  assert not prius_v.has_lkas_button and not prius_v.always_on_lateral_lkas_button
  assert rav4_prime.always_on_lateral_lkas_button


def test_main_at_start():
  frogpilot_variables.CRUISE_MAIN_OFF_AT_START_BRANDS.add("test_brand")

  listed_brand = Drive(brand="test_brand")
  listed_car = Drive(main_at_start_car=True)

  frogpilot_variables.CRUISE_MAIN_OFF_AT_START_BRANDS.discard("test_brand")

  assert listed_brand.step().alwaysOnLateralAllowed
  assert listed_car.step().alwaysOnLateralAllowed


def test_main_at_start_chrysler():
  drive = Drive(brand="chrysler", fingerprint=CHRYSLER.RAM_1500_5TH_GEN)

  frogpilotCarState = drive.step()

  assert frogpilotCarState.alwaysOnLateralAllowed
  assert not frogpilotCarState.alwaysOnLateralWaiting


def test_main_at_start_hyundai():
  for fingerprint, allowed in ((HYUNDAI.HYUNDAI_IONIQ_5, True), (HYUNDAI.KIA_EV6, True), (HYUNDAI.HYUNDAI_SONATA, False), (HYUNDAI.HYUNDAI_PALISADE, False)):
    drive = Drive(brand="hyundai", fingerprint=fingerprint)

    frogpilotCarState = drive.step()

    assert frogpilotCarState.alwaysOnLateralAllowed == allowed, fingerprint
    assert frogpilotCarState.alwaysOnLateralWaiting != allowed, fingerprint


def test_main_at_start_hyundai_openpilot_longitudinal():
  for brand, openpilot_longitudinal, allowed in (("hyundai", False, False), ("hyundai", True, True), ("toyota", True, False)):
    drive = Drive(brand=brand, openpilot_longitudinal=openpilot_longitudinal)

    frogpilotCarState = drive.step()

    assert frogpilotCarState.alwaysOnLateralAllowed == allowed, (brand, openpilot_longitudinal)
    assert frogpilotCarState.alwaysOnLateralWaiting != allowed, (brand, openpilot_longitudinal)


def test_main_at_start_toyota():
  for fingerprint, allowed in ((TOYOTA.LEXUS_ES_TSS2, True), (TOYOTA.LEXUS_IS, False), (TOYOTA.TOYOTA_PRIUS, True), (TOYOTA.TOYOTA_RAV4_PRIME, True)):
    drive = Drive(brand="toyota", fingerprint=fingerprint)

    frogpilotCarState = drive.step()

    assert frogpilotCarState.alwaysOnLateralAllowed == allowed, fingerprint
    assert frogpilotCarState.alwaysOnLateralWaiting != allowed, fingerprint


def test_no_cruise_main():
  for brand in frogpilot_variables.NO_CRUISE_SWITCH_BRANDS:
    drive = Drive(brand=brand)

    started = drive.step()
    drive.step(cruise_main=False)
    cruise_main_on = drive.step()
    engaged = drive.step(engaged=True)
    cruise_main_off = drive.step(cruise_main=False)
    braked = drive.step(brake=True)

    assert not started.alwaysOnLateralWaiting, brand
    assert not cruise_main_on.alwaysOnLateralAllowed, brand
    assert engaged.alwaysOnLateralAllowed, brand
    assert cruise_main_off.alwaysOnLateralEnabled, brand
    assert not braked.alwaysOnLateralAllowed, brand


def test_no_cruise_main_brake_while_engaged():
  drive = Drive(brand="tesla")
  drive.step(engaged=True)

  braked = drive.step(engaged=True, brake=True)
  disengaged = drive.step(brake=True)

  assert not braked.alwaysOnLateralAllowed
  assert not disengaged.alwaysOnLateralAllowed


def test_no_cruise_main_held_brake():
  drive = Drive(brand="tesla")
  drive.step(brake=True, standstill=True, v_ego=0)

  engaged = drive.step(engaged=True, brake=True, standstill=True, v_ego=0)
  cancelled = drive.step(brake=True, standstill=True, v_ego=0)
  drive.step()
  braked = drive.step(brake=True)

  assert engaged.alwaysOnLateralEnabled
  assert cancelled.alwaysOnLateralAllowed
  assert not braked.alwaysOnLateralAllowed


def test_no_cruise_main_quick_restart():
  drive = Drive(brand="tesla")
  drive.step(engaged=True)
  drive.step()
  drive.step(can_valid=False)

  restarted = drive.step()

  assert not restarted.alwaysOnLateralAllowed
  assert not restarted.alwaysOnLateralWaiting


def test_paused():
  for step in ({"events": (EventName.tooDistracted,)}, {"gear": GearShifter.reverse}, {"has_disable_events": True}, {"lateral_check": False},
               {"rx_checks_invalid": True}, {"selfdrived_alive": False}, {"speed_sensors_disagree": True}):
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


def test_speed_sensors_disagree_event():
  events_tracker = FrogPilotEvents(FROGPILOT_EVENTS_PLANNER, None)
  enable = Events()
  enable.add(EventName.buttonEnable)

  for speed_sensors_disagree in (False, True, False):
    sm = {
      "carControl": car.CarControl.new_message(),
      "carState": car.CarState.new_message(),
      "frogpilotCarState": custom.FrogPilotCarState.new_message(speedSensorsDisagree=speed_sensors_disagree),
      "frogpilotModelV2": custom.FrogPilotModelDataV2.new_message(),
      "frogpilotSelfdriveState": custom.FrogPilotSelfdriveState.new_message(),
      "selfdriveState": log.SelfdriveState.new_message(),
    }
    events_tracker.update(False, sm, FROGPILOT_EVENTS_TOGGLES)

    engaged = StateMachine()
    engaged.state = State.enabled
    engaged.update(Events(), events_tracker.events, False)

    engaging = StateMachine()
    engaging.update(enable, events_tracker.events, False)

    assert (engaged.state == State.disabled) == speed_sensors_disagree
    assert (engaging.state == State.disabled) == speed_sensors_disagree


def test_state_machine_warning_alerts():
  for always_on_lateral_enabled in (False, True):
    state_machine = StateMachine()

    state_machine.update(Events(), Events(frogpilot=True), always_on_lateral_enabled)

    assert (ET.WARNING in state_machine.current_alert_types) == always_on_lateral_enabled


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


def test_steering_off_event():
  events_tracker = FrogPilotEvents(FROGPILOT_EVENTS_PLANNER, None)

  for steering_off in (False, True, False):
    sm = {
      "carControl": car.CarControl.new_message(),
      "carState": car.CarState.new_message(),
      "frogpilotCarState": custom.FrogPilotCarState.new_message(alwaysOnLateralSteeringOff=steering_off),
      "frogpilotModelV2": custom.FrogPilotModelDataV2.new_message(),
      "frogpilotSelfdriveState": custom.FrogPilotSelfdriveState.new_message(),
      "selfdriveState": log.SelfdriveState.new_message(),
    }
    events_tracker.update(False, sm, FROGPILOT_EVENTS_TOGGLES)

    assert (FrogPilotEventName.alwaysOnLateralSteeringOff in events_tracker.events.names) == steering_off


def test_steering_off_only_from_lkas_press():
  main_off = Drive()
  main_off.step()
  main_off.step(engaged=True)

  no_cruise_main = Drive(brand="tesla")
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

  lkas_button_switched_on = Drive()
  lkas_button_switched_on.step(cruise_main=False)
  lkas_button_switched_on.toggles.always_on_lateral_lkas_button = True

  assert not switched_on.step().alwaysOnLateralAllowed
  assert armed.alwaysOnLateralEnabled
  assert not switched_off.step().alwaysOnLateralAllowed
  assert not switched_off.step().alwaysOnLateralEnabled
  assert switched_off.step(engaged=True).alwaysOnLateralAllowed
  assert not lkas_button_switched_off.step(cruise_main=False, lkas_pressed=True).alwaysOnLateralAllowed
  assert not lkas_button_switched_on.step(cruise_main=False, lkas_pressed=True).alwaysOnLateralAllowed


def test_waiting_alert():
  for lkas_button in (False, True):
    toggles = types.SimpleNamespace(always_on_lateral_lkas_button=lkas_button)

    alert = always_on_lateral_waiting_alert(car.CarParams.new_message(), car.CarState.new_message(), {}, False, 0, None, toggles)

    assert "cruise" in alert.alert_text_2, lkas_button
    assert ("LKAS" in alert.alert_text_2) == lkas_button, lkas_button
    assert "engage" in alert.alert_text_2.lower(), lkas_button


def test_waiting_switched_off():
  drive = Drive()
  drive.toggles.always_on_lateral = False

  assert not drive.step().alwaysOnLateralWaiting
