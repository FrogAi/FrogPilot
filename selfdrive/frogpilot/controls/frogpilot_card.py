#!/usr/bin/env python3
from cereal import car, custom
from openpilot.selfdrive.controls.lib.drive_helpers import CRUISE_LONG_PRESS
from openpilot.selfdrive.controls.lib.events import ET, EventName

from openpilot.selfdrive.frogpilot.frogpilot_variables import NON_DRIVING_GEARS, params, params_memory

ButtonType = car.CarState.ButtonEvent.Type
FrogPilotButtonType = custom.FrogPilotCarState.ButtonEvent.Type

def handle_experimental_mode(conditional_experimental_mode):
  if conditional_experimental_mode:
    conditional_status = params_memory.get_int("CEStatus")

    override_value = 0 if conditional_status in {1, 2} else 1 if conditional_status >= 3 else 2

    params_memory.put_int("CEStatus", override_value)
  else:
    params.put_bool_nonblocking("ExperimentalMode", not params.get_bool("ExperimentalMode"))

class FrogPilotCard:
  def __init__(self, Car):
    self.car = Car

    self.long_press_threshold = CRUISE_LONG_PRESS * (1.5 if self.car.CP.carName == "gm" else 1)
    self.very_long_press_threshold = CRUISE_LONG_PRESS * 5

    self.accel_pressed = False
    self.decel_pressed = False
    self.force_coast = False
    self.pause_lateral = False
    self.pause_longitudinal = False
    self.prev_distance_button = False
    self.traffic_mode = False

    self.gap_counter = 0

  def update_distance_button(self, sm):
    if self.car.frogpilot_toggles.experimental_mode_via_distance and sm["carControl"].longActive:
      handle_experimental_mode(self.car.frogpilot_toggles.conditional_experimental_mode)
    elif self.car.frogpilot_toggles.force_coast_via_distance:
      self.force_coast = not self.force_coast
    elif self.car.frogpilot_toggles.pause_lateral_via_distance:
      self.pause_lateral = not self.pause_lateral
    elif self.car.frogpilot_toggles.pause_longitudinal_via_distance:
      self.pause_longitudinal = not self.pause_longitudinal
    elif self.car.frogpilot_toggles.traffic_mode_via_distance:
      self.traffic_mode = not self.traffic_mode

  def update_distance_button_long(self, sm):
    if self.car.frogpilot_toggles.experimental_mode_via_distance_long and sm["carControl"].longActive:
      handle_experimental_mode(self.car.frogpilot_toggles.conditional_experimental_mode)
    elif self.car.frogpilot_toggles.force_coast_via_distance_long:
      self.force_coast = not self.force_coast
    elif self.car.frogpilot_toggles.pause_lateral_via_distance_long:
      self.pause_lateral = not self.pause_lateral
    elif self.car.frogpilot_toggles.pause_longitudinal_via_distance_long:
      self.pause_longitudinal = not self.pause_longitudinal
    elif self.car.frogpilot_toggles.traffic_mode_via_distance_long:
      self.traffic_mode = not self.traffic_mode

  def update_distance_button_very_long(self, sm):
    self.update_distance_button_long(sm)

    if self.car.frogpilot_toggles.experimental_mode_via_distance_very_long and sm["carControl"].longActive:
      handle_experimental_mode(self.car.frogpilot_toggles.conditional_experimental_mode)
    elif self.car.frogpilot_toggles.force_coast_via_distance_very_long:
      self.force_coast = not self.force_coast
    elif self.car.frogpilot_toggles.pause_lateral_via_distance_very_long:
      self.pause_lateral = not self.pause_lateral
    elif self.car.frogpilot_toggles.pause_longitudinal_via_distance_very_long:
      self.pause_longitudinal = not self.pause_longitudinal
    elif self.car.frogpilot_toggles.traffic_mode_via_distance_very_long:
      self.traffic_mode = not self.traffic_mode

  def update_lkas_button(self, sm):
    if self.car.frogpilot_toggles.experimental_mode_via_lkas and sm["carControl"].longActive:
      handle_experimental_mode(self.car.frogpilot_toggles.conditional_experimental_mode)
    elif self.car.frogpilot_toggles.force_coast_via_lkas:
      self.force_coast = not self.force_coast
    elif self.car.frogpilot_toggles.pause_lateral_via_lkas:
      self.pause_lateral = not self.pause_lateral
    elif self.car.frogpilot_toggles.pause_longitudinal_via_lkas:
      self.pause_longitudinal = not self.pause_longitudinal
    elif self.car.frogpilot_toggles.traffic_mode_via_lkas:
      self.traffic_mode = not self.traffic_mode

  def update(self, carState, frogpilotCarState, sm):
    self.always_on_lateral_enabled = self.car.frogpilot_toggles.always_on_lateral_set

    if self.car.frogpilot_toggles.use_lkas_for_aol:
      self.always_on_lateral_enabled |= self.car.frogpilot_toggles.always_on_lateral_lkas or carState.cruiseState.enabled
      self.always_on_lateral_enabled &= frogpilotCarState.alwaysOnLateralAllowed
    else:
      self.always_on_lateral_enabled |= self.car.frogpilot_toggles.always_on_lateral_main or carState.cruiseState.enabled
      self.always_on_lateral_enabled &= carState.cruiseState.available

    self.always_on_lateral_enabled &= carState.gearShifter not in NON_DRIVING_GEARS
    self.always_on_lateral_enabled &= sm["frogpilotPlan"].lateralCheck
    self.always_on_lateral_enabled &= sm["liveCalibration"].calPerc >= 1
    self.always_on_lateral_enabled &= not (carState.brakePressed and carState.vEgo < self.car.frogpilot_toggles.always_on_lateral_pause_speed) or carState.standstill
    self.always_on_lateral_enabled &= not any(getattr(event, ET.IMMEDIATE_DISABLE, False) for event in sm["onroadEvents"] if event.name != EventName.speedTooLow) or self.car.frogpilot_toggles.frogs_go_moo

    if sm.updated["frogpilotPlan"] or any(be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in carState.buttonEvents):
      self.accel_pressed = any(be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in carState.buttonEvents)

    if sm.updated["frogpilotPlan"] or any(be.type == ButtonType.decelCruise for be in carState.buttonEvents):
      self.decel_pressed = any(be.type == ButtonType.decelCruise for be in carState.buttonEvents)

    self.force_coast &= not (carState.brakePressed or carState.gasPressed)

    if frogpilotCarState.distancePressed:
      self.gap_counter += 1
    elif not self.prev_distance_button:
      self.gap_counter = 0

    if not frogpilotCarState.distancePressed and 1 < self.gap_counter < self.long_press_threshold:
      self.update_distance_button(sm)
    elif self.gap_counter == self.long_press_threshold:
      self.update_distance_button_long(sm)
    elif self.gap_counter == self.very_long_press_threshold:
      self.update_distance_button_very_long(sm)

    self.prev_distance_button = frogpilotCarState.distancePressed

    lkas_button = any(be.pressed and be.type == FrogPilotButtonType.lkas for be in carState.buttonEvents)

    if lkas_button:
      self.update_lkas_button(sm)

    frogpilotCarState.accelPressed = self.accel_pressed
    frogpilotCarState.alwaysOnLateralEnabled = self.always_on_lateral_enabled
    frogpilotCarState.decelPressed = self.decel_pressed
    frogpilotCarState.distanceLongPressed = self.very_long_press_threshold > self.gap_counter >= self.long_press_threshold
    frogpilotCarState.distanceVeryLongPressed = self.gap_counter >= self.very_long_press_threshold
    frogpilotCarState.forceCoast = self.force_coast
    frogpilotCarState.pauseLateral = self.pause_lateral
    frogpilotCarState.pauseLongitudinal = self.pause_longitudinal
    frogpilotCarState.trafficMode = self.traffic_mode

    return frogpilotCarState
