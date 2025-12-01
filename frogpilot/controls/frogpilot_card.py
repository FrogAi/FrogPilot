#!/usr/bin/env python3
import cereal.messaging as messaging

from opendbc.car import create_button_events, structs
from openpilot.common.params import Params
from openpilot.selfdrive.car.cruise import CRUISE_LONG_PRESS, ButtonType

from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.controls.lib.always_on_lateral import AlwaysOnLateral

class FrogPilotCard:
  def __init__(self, CP, FPCP, frogpilot_toggles):
    self.CP = CP

    self.params = Params(return_defaults=True)

    self.car_safety_mode_set = False
    self.distance_pressed_previously = False
    self.manual_transmission = CP.transmissionType == structs.CarParams.TransmissionType.manual
    self.onroad_distance_button = False
    self.rx_checks_invalid = False

    self.accel_press_count = 0
    self.decel_press_count = 0
    self.experimental_mode_press_count = 0
    self.gap_counter = 0
    self.lkas_button_press_count = 0

    self.long_press_threshold = CRUISE_LONG_PRESS * (1.5 if self.CP.brand == "gm" else 1)
    self.very_long_press_threshold = CRUISE_LONG_PRESS * 5

    self.always_on_lateral = AlwaysOnLateral(CP, FPCP, frogpilot_toggles)

    self.ui_event_sock = messaging.sub_sock("frogpilotUIEvent")

  def handle_button_event(self, function, sm, frogpilot_toggles):
    if sm["carControl"].longActive and function == frogpilot_variables.BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]:
      self.handle_experimental_mode(sm, frogpilot_toggles)

  def handle_experimental_mode(self, sm, frogpilot_toggles):
    if frogpilot_toggles.conditional_experimental_mode:
      self.experimental_mode_press_count += 1
    else:
      self.params.put_bool_nonblocking("ExperimentalMode", not sm["selfdriveState"].experimentalMode)

  def update(self, carState, frogpilotCarState, sm, frogpilot_toggles):
    onroad_distance_button_events = []
    for msg in messaging.drain_sock(self.ui_event_sock):
      if msg.frogpilotUIEvent.which() == "distanceButtonPressed":
        onroad_distance_button = msg.frogpilotUIEvent.distanceButtonPressed
        onroad_distance_button_events += create_button_events(onroad_distance_button, self.onroad_distance_button, {1: ButtonType.gapAdjustCruise})
        self.onroad_distance_button = onroad_distance_button

    if onroad_distance_button_events:
      carState.buttonEvents = [*(be.copy() for be in carState.buttonEvents), *onroad_distance_button_events]

    frogpilotCarState.distancePressed |= self.onroad_distance_button

    panda_states = [panda_state for panda_state in sm["pandaStates"] if panda_state.safetyModel == self.CP.safetyConfigs[-1].safetyModel]

    self.car_safety_mode_set = len(panda_states) > 0

    if any(be.pressed and be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in carState.buttonEvents):
      self.accel_press_count += 1

    if any(be.pressed and be.type == ButtonType.decelCruise for be in carState.buttonEvents):
      self.decel_press_count += 1

    if frogpilotCarState.distancePressed:
      self.gap_counter += 1
    elif not self.distance_pressed_previously:
      self.gap_counter = 0

    self.distance_pressed_previously = frogpilotCarState.distancePressed

    if not frogpilotCarState.distancePressed and 1 <= self.gap_counter < self.long_press_threshold:
      self.handle_button_event(frogpilot_toggles.distance_button_function, sm, frogpilot_toggles)
    elif frogpilotCarState.distancePressed and self.gap_counter == self.long_press_threshold:
      self.handle_button_event(frogpilot_toggles.distance_long_button_function, sm, frogpilot_toggles)
    elif frogpilotCarState.distancePressed and self.gap_counter == self.very_long_press_threshold:
      self.handle_button_event(frogpilot_toggles.distance_long_button_function, sm, frogpilot_toggles)
      self.handle_button_event(frogpilot_toggles.distance_very_long_button_function, sm, frogpilot_toggles)

    if any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents):
      self.lkas_button_press_count += 1
      self.handle_button_event(frogpilot_toggles.lkas_button_function, sm, frogpilot_toggles)

    frogpilotCarState.speedSensorsDisagree &= carState.canValid

    self.rx_checks_invalid = any(panda_state.safetyRxChecksInvalid for panda_state in panda_states)
    self.always_on_lateral.update(carState, frogpilotCarState, sm, frogpilot_toggles, self.rx_checks_invalid)

    frogpilotCarState.accelPressCount = self.accel_press_count
    frogpilotCarState.decelPressCount = self.decel_press_count
    frogpilotCarState.distanceLongPressed = self.very_long_press_threshold > self.gap_counter >= self.long_press_threshold
    frogpilotCarState.distanceVeryLongPressed = self.gap_counter >= self.very_long_press_threshold
    frogpilotCarState.experimentalModePressCount = self.experimental_mode_press_count
    frogpilotCarState.lkasButtonPressCount = self.lkas_button_press_count
    frogpilotCarState.parked = carState.gearShifter == frogpilot_variables.GearShifter.park or (self.manual_transmission and carState.standstill)
    frogpilotCarState.vEgo = carState.vEgo

    return frogpilotCarState
