#!/usr/bin/env python3
from cereal import car
from openpilot.selfdrive.car.cruise import CRUISE_LONG_PRESS
from openpilot.selfdrive.selfdrived.events import EventName

from openpilot.frogpilot.common.frogpilot_variables import NON_DRIVING_GEARS

ButtonType = car.CarState.ButtonEvent.Type

def handle_experimental_mode(conditional_experimental_mode):
  if conditional_experimental_mode:
    conditional_status = params_memory.get("CEStatus")

    override_value = 0 if conditional_status in (1, 2) else 1 if conditional_status >= 3 else 2

    params_memory.put("CEStatus", override_value)
  else:
    params.put_bool_nonblocking("ExperimentalMode", not params.get_bool("ExperimentalMode"))

class FrogPilotCard:
  def __init__(self, CP):
    self.long_press_threshold = CRUISE_LONG_PRESS * (1.5 if CP.brand == "gm" else 1)
    self.very_long_press_threshold = CRUISE_LONG_PRESS * 5

    self.prev_distance_button = False

    self.gap_counter = 0

  def update_distance_button(self, sm, frogpilot_toggles):
    if frogpilot_toggles.experimental_mode_via_distance and sm["carControl"].longActive:
      handle_experimental_mode(frogpilot_toggles.conditional_experimental_mode)

  def update_distance_button_long(self, sm, frogpilot_toggles):
    if frogpilot_toggles.experimental_mode_via_distance_long and sm["carControl"].longActive:
      handle_experimental_mode(frogpilot_toggles.conditional_experimental_mode)

  def update_distance_button_very_long(self, sm, frogpilot_toggles):
    self.update_distance_button_long(sm)

    if frogpilot_toggles.experimental_mode_via_distance_very_long and sm["carControl"].longActive:
      handle_experimental_mode(frogpilot_toggles.conditional_experimental_mode)

  def update_lkas_button(self, sm, frogpilot_toggles):
    if frogpilot_toggles.experimental_mode_via_lkas and sm["carControl"].longActive:
      handle_experimental_mode(frogpilot_toggles.conditional_experimental_mode)

  def update(self, carState, frogpilotCarState, sm, frogpilot_toggles):
    self.always_on_lateral_enabled = frogpilot_toggles.always_on_lateral_set

    if frogpilot_toggles.use_lkas_for_aol:
      self.always_on_lateral_enabled |= frogpilot_toggles.always_on_lateral_lkas or carState.cruiseState.enabled
      self.always_on_lateral_enabled &= frogpilotCarState.alwaysOnLateralAllowed
    else:
      self.always_on_lateral_enabled |= frogpilot_toggles.always_on_lateral_main or carState.cruiseState.enabled
      self.always_on_lateral_enabled &= carState.cruiseState.available

    self.always_on_lateral_enabled &= carState.gearShifter not in NON_DRIVING_GEARS
    self.always_on_lateral_enabled &= sm["frogpilotPlan"].lateralCheck
    self.always_on_lateral_enabled &= sm["liveCalibration"].calPerc >= 1
    self.always_on_lateral_enabled &= not (carState.brakePressed and carState.vEgo < frogpilot_toggles.always_on_lateral_pause_speed or carState.standstill)
    self.always_on_lateral_enabled &= not any(event.immediateDisable for events in (sm["onroadEvents"], sm["frogpilotOnroadEvents"]) for event in events if event.name != EventName.speedTooLow) or frogpilot_toggles.frogs_go_moo

    frogpilotCarState.distancePressed |= params_memory.get_bool("OnroadDistanceButtonPressed")

    if frogpilotCarState.distancePressed:
      self.gap_counter += 1
    elif not self.prev_distance_button:
      self.gap_counter = 0

    if not frogpilotCarState.distancePressed and 1 < self.gap_counter < self.long_press_threshold:
      self.update_distance_button(sm, frogpilot_toggles)
    elif self.gap_counter == self.long_press_threshold:
      self.update_distance_button_long(sm, frogpilot_toggles)
    elif self.gap_counter == self.very_long_press_threshold:
      self.update_distance_button_very_long(sm, frogpilot_toggles)

    lkas_button = any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents)

    if lkas_button:
      self.update_lkas_button(sm, frogpilot_toggles)

    self.prev_distance_button = frogpilotCarState.distancePressed

    frogpilotCarState.alwaysOnLateralEnabled = self.always_on_lateral_enabled
    frogpilotCarState.distanceLongPressed = self.very_long_press_threshold > self.gap_counter >= self.long_press_threshold
    frogpilotCarState.distanceVeryLongPressed = self.gap_counter >= self.very_long_press_threshold

    return frogpilotCarState
