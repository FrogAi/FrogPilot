#!/usr/bin/env python3
import cereal.messaging as messaging

from cereal import car
from openpilot.selfdrive.car import create_button_events
from openpilot.selfdrive.controls.lib.drive_helpers import CRUISE_LONG_PRESS

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables

ButtonType = car.CarState.ButtonEvent.Type
EventName = car.CarEvent.EventName

class FrogPilotCard:
  def __init__(self, Car):
    self.car = Car

    self.long_press_threshold = CRUISE_LONG_PRESS * (1.5 if self.car.CP.carName == "gm" else 1)
    self.very_long_press_threshold = CRUISE_LONG_PRESS * 5

    self.always_on_lateral_allowed = False
    self.cruise_available_previously = True
    self.force_coast = False
    self.lkas_press_used = False
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

    self.error_log = frogpilot_paths.ERROR_FILE_PATH
    self.lkas_starts_lateral = self.car.CP.carName not in frogpilot_variables.LKAS_STATUS_BRANDS

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

    car_safety_mode_set = True
    if self.car.CP.carName == "hyundai":
      safety_model = self.car.CP.safetyConfigs[-1].safetyModel
      car_safety_mode_set = any(panda_state.safetyModel == safety_model for panda_state in sm["pandaStates"])
      self.car.CI.CS.car_safety_mode_set = car_safety_mode_set

    onroad_distance_button_events = []
    for msg in messaging.drain_sock(self.ui_event_sock):
      if msg.frogpilotUIEvent.which() == "distanceButtonPressed":
        onroad_distance_button = msg.frogpilotUIEvent.distanceButtonPressed
        onroad_distance_button_events += create_button_events(onroad_distance_button, self.onroad_distance_button, {1: ButtonType.gapAdjustCruise})
        self.onroad_distance_button = onroad_distance_button

    if onroad_distance_button_events:
      carState.buttonEvents = [*(be.copy() for be in carState.buttonEvents), *onroad_distance_button_events]

    frogpilotCarState.distancePressed |= self.onroad_distance_button

    if frogpilot_toggles.always_on_lateral_lkas:
      if not car_safety_mode_set:
        self.always_on_lateral_allowed = False
      else:
        if any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents):
          self.always_on_lateral_allowed = not self.always_on_lateral_allowed

        self.always_on_lateral_allowed |= any(panda_state.controlsAllowed for panda_state in sm["pandaStates"])
    elif frogpilot_toggles.always_on_lateral_main:
      self.always_on_lateral_allowed |= sm["carControl"].enabled or not self.cruise_available_previously
      self.always_on_lateral_allowed &= carState.cruiseState.available

      if carState.cruiseState.available and self.lkas_starts_lateral and not self.always_on_lateral_allowed:
        if any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents):
          self.always_on_lateral_allowed = True
          self.lkas_press_used = True

      if carState.canValid:
        self.cruise_available_previously = carState.cruiseState.available

    if self.lkas_press_used:
      self.lkas_press_used = not any(not be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents)
      carState.buttonEvents = [be.copy() for be in carState.buttonEvents if be.type != ButtonType.lkas]

    self.always_on_lateral_enabled = self.always_on_lateral_allowed and frogpilot_toggles.always_on_lateral_set
    self.always_on_lateral_enabled &= carState.gearShifter not in frogpilot_variables.NON_DRIVING_GEARS
    self.always_on_lateral_enabled &= sm["frogpilotPlan"].lateralCheck
    self.always_on_lateral_enabled &= sm["liveCalibration"].calPerc >= 1
    self.always_on_lateral_enabled &= not sm["frogpilotControlsState"].hasDisableEvents or frogpilot_utilities.is_FrogsGoMoo()
    self.always_on_lateral_enabled &= not any(event.name == EventName.tooDistracted for event in sm["onroadEvents"])
    self.always_on_lateral_enabled &= not (carState.brakePressed and carState.vEgo < frogpilot_toggles.always_on_lateral_pause_speed) or carState.standstill
    self.always_on_lateral_enabled &= not self.error_log.is_file() or frogpilot_utilities.is_FrogsGoMoo()

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

    self.prev_distance_button = frogpilotCarState.distancePressed

    frogpilotCarState.accelPressCount = self.accel_press_count
    frogpilotCarState.alwaysOnLateralAllowed = self.always_on_lateral_allowed
    frogpilotCarState.alwaysOnLateralEnabled = self.always_on_lateral_enabled
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
