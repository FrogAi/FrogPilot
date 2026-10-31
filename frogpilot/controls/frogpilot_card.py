#!/usr/bin/env python3
import cereal.messaging as messaging

from cereal import car
from openpilot.selfdrive.car import create_button_events
from openpilot.selfdrive.controls.lib.drive_helpers import CRUISE_LONG_PRESS

from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.controls.lib.always_on_lateral import AlwaysOnLateral

ButtonType = car.CarState.ButtonEvent.Type

class FrogPilotCard:
  def __init__(self, Car):
    self.car = Car

    self.long_press_threshold = CRUISE_LONG_PRESS * (1.5 if self.car.CP.carName == "gm" else 1)
    self.very_long_press_threshold = CRUISE_LONG_PRESS * 5

    self.force_coast = False
    self.manual_transmission = self.car.CP.transmissionType == car.CarParams.TransmissionType.manual
    self.onroad_distance_button = False
    self.pause_lateral = False
    self.pause_longitudinal = False
    self.prev_distance_button = False
    self.traffic_mode_enabled = False

    self.accel_press_count = 0
    self.decel_press_count = 0
    self.experimental_mode_press_count = 0
    self.gap_counter = 0

    self.always_on_lateral = AlwaysOnLateral(self.car.CP, self.car.FPCP, self.car.frogpilot_toggles)

    self.ui_event_sock = messaging.sub_sock("frogpilotUIEvent")

  def handle_experimental_mode(self):
    frogpilot_toggles = self.car.frogpilot_toggles

    if frogpilot_toggles.conditional_experimental_mode or frogpilot_toggles.slc_fallback_experimental_mode:
      self.experimental_mode_press_count += 1
    else:
      frogpilot_variables.params.put_bool_nonblocking("ExperimentalMode", not frogpilot_variables.params.get_bool("ExperimentalMode"))

  def handle_button_event(self, function, sm):
    if sm["carControl"].longActive and function == frogpilot_variables.BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]:
      self.handle_experimental_mode()
    elif sm["carControl"].longActive and function == frogpilot_variables.BUTTON_FUNCTIONS["FORCE_COAST"]:
      self.force_coast = not self.force_coast
    elif function == frogpilot_variables.BUTTON_FUNCTIONS["PAUSE_LATERAL"]:
      self.pause_lateral = not self.pause_lateral
    elif (sm["carControl"].longActive or self.pause_longitudinal) and function == frogpilot_variables.BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]:
      self.pause_longitudinal = not self.pause_longitudinal
    elif sm["carControl"].longActive and function == frogpilot_variables.BUTTON_FUNCTIONS["TRAFFIC_MODE"]:
      self.traffic_mode_enabled = not self.traffic_mode_enabled

  def update(self, carState, frogpilotCarState, sm):
    frogpilot_toggles = self.car.frogpilot_toggles

    panda_states = [panda_state for panda_state in sm["pandaStates"] if panda_state.safetyModel == self.car.CP.safetyConfigs[-1].safetyModel]
    rx_checks_invalid = any(panda_state.safetyRxChecksInvalid for panda_state in panda_states)

    self.car.CI.CS.car_safety_mode_set = len(panda_states) > 0

    if self.car.CP.carName == "hyundai":
      self.car.CI.CS.rx_checks_invalid = rx_checks_invalid

    onroad_distance_button_events = []
    for msg in messaging.drain_sock(self.ui_event_sock):
      if msg.frogpilotUIEvent.which() == "distanceButtonPressed":
        onroad_distance_button = msg.frogpilotUIEvent.distanceButtonPressed
        onroad_distance_button_events += create_button_events(onroad_distance_button, self.onroad_distance_button, {1: ButtonType.gapAdjustCruise})
        self.onroad_distance_button = onroad_distance_button

    if onroad_distance_button_events:
      carState.buttonEvents = [*(be.copy() for be in carState.buttonEvents), *onroad_distance_button_events]

    frogpilotCarState.distancePressed |= self.onroad_distance_button

    if any(be.pressed and be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in carState.buttonEvents):
      self.accel_press_count += 1

    if any(be.pressed and be.type == ButtonType.decelCruise for be in carState.buttonEvents):
      self.decel_press_count += 1

    self.force_coast &= not (carState.brakePressed or carState.gasPressed)

    if frogpilotCarState.distancePressed:
      self.gap_counter += 1
    elif not self.prev_distance_button:
      self.gap_counter = 0

    if not frogpilotCarState.distancePressed and 1 <= self.gap_counter < self.long_press_threshold:
      self.handle_button_event(frogpilot_toggles.distance_button_function, sm)
    elif frogpilotCarState.distancePressed and self.gap_counter == self.long_press_threshold:
      self.handle_button_event(frogpilot_toggles.distance_long_button_function, sm)
    elif frogpilotCarState.distancePressed and self.gap_counter == self.very_long_press_threshold:
      self.handle_button_event(frogpilot_toggles.distance_long_button_function, sm)
      self.handle_button_event(frogpilot_toggles.distance_very_long_button_function, sm)

    if any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents):
      self.handle_button_event(frogpilot_toggles.lkas_button_function, sm)

    self.always_on_lateral.update(carState, frogpilotCarState, sm, frogpilot_toggles, rx_checks_invalid)

    self.prev_distance_button = frogpilotCarState.distancePressed

    frogpilotCarState.accelPressCount = self.accel_press_count
    frogpilotCarState.decelPressCount = self.decel_press_count
    frogpilotCarState.distanceLongPressed = self.very_long_press_threshold > self.gap_counter >= self.long_press_threshold
    frogpilotCarState.distanceVeryLongPressed = self.gap_counter >= self.very_long_press_threshold
    frogpilotCarState.experimentalModePressCount = self.experimental_mode_press_count
    frogpilotCarState.forceCoast = self.force_coast
    frogpilotCarState.parked = carState.gearShifter == frogpilot_variables.GearShifter.park or (self.manual_transmission and carState.standstill)
    frogpilotCarState.pauseLateral = self.pause_lateral
    frogpilotCarState.pauseLongitudinal = self.pause_longitudinal
    frogpilotCarState.trafficModeEnabled = self.traffic_mode_enabled
    frogpilotCarState.vEgo = carState.vEgo

    return frogpilotCarState
