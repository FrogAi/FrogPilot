#!/usr/bin/env python3
import cereal.messaging as messaging

from cereal import log
from opendbc.safety import ALTERNATIVE_EXPERIENCE
from openpilot.common.params import Params
from openpilot.selfdrive.car.cruise import CRUISE_LONG_PRESS, ButtonType

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables

EventName = log.OnroadEvent.EventName

class FrogPilotCard:
  def __init__(self, CP):
    self.CP = CP

    self.params = Params(return_defaults=True)

    self.always_on_lateral_allowed = False
    self.cruise_available_previously = True
    self.distancePressed_previously = False

    self.accel_press_count = 0
    self.decel_press_count = 0
    self.experimental_mode_press_count = 0
    self.gap_counter = 0
    self.lkas_button_press_count = 0

    self.always_on_lateral_set = bool(CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL)
    self.frogs_go_moo = frogpilot_utilities.is_FrogsGoMoo()

    self.long_press_threshold = CRUISE_LONG_PRESS * (1.5 if self.CP.brand == "gm" else 1)
    self.very_long_press_threshold = CRUISE_LONG_PRESS * 5

    self.ui_event_sock = messaging.sub_sock("frogpilotUIEvent")

  def handle_button_event(self, key, sm, frogpilot_toggles):
    if sm["carControl"].longActive and getattr(frogpilot_toggles, f"experimental_mode_via_{key}"):
      self.handle_experimental_mode(sm, frogpilot_toggles)

  def handle_experimental_mode(self, sm, frogpilot_toggles):
    if frogpilot_toggles.conditional_experimental_mode:
      self.experimental_mode_press_count += 1
    else:
      self.params.put_bool_nonblocking("ExperimentalMode", not sm["selfdriveState"].experimentalMode)

  def update(self, carState, frogpilotCarState, sm, frogpilot_toggles):
    if self.CP.brand == "hyundai":
      for be in carState.buttonEvents:
        if be.type == ButtonType.lkas and be.pressed and frogpilot_toggles.always_on_lateral_lkas:
          self.always_on_lateral_allowed = not self.always_on_lateral_allowed
        elif be.type == ButtonType.mainCruise and be.pressed and frogpilot_toggles.always_on_lateral_main:
          self.always_on_lateral_allowed = not self.always_on_lateral_allowed
    elif frogpilot_toggles.always_on_lateral_main:
      self.always_on_lateral_allowed |= sm["carControl"].enabled or not self.cruise_available_previously
      self.always_on_lateral_allowed &= carState.cruiseState.available

      if carState.canValid:
        self.cruise_available_previously = carState.cruiseState.available

    if not frogpilot_toggles.always_on_lateral_lkas and not frogpilot_toggles.always_on_lateral_main:
      self.always_on_lateral_allowed = carState.cruiseState.enabled

    self.always_on_lateral_enabled = self.always_on_lateral_allowed and self.always_on_lateral_set
    self.always_on_lateral_enabled &= carState.gearShifter not in frogpilot_variables.NON_DRIVING_GEARS
    self.always_on_lateral_enabled &= sm["liveCalibration"].calPerc >= 1
    self.always_on_lateral_enabled &= not sm["frogpilotSelfdriveState"].hasDisableEvents or self.frogs_go_moo
    self.always_on_lateral_enabled &= not any(event.name == EventName.tooDistracted for event in sm["onroadEvents"])
    self.always_on_lateral_enabled &= not (carState.brakePressed and carState.vEgo < frogpilot_toggles.always_on_lateral_pause_speed) or carState.standstill

    if any(be.pressed and be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in carState.buttonEvents):
      self.accel_press_count += 1

    if any(be.pressed and be.type == ButtonType.decelCruise for be in carState.buttonEvents):
      self.decel_press_count += 1

    if frogpilotCarState.distancePressed:
      self.gap_counter += 1
    elif not self.distancePressed_previously:
      self.gap_counter = 0

    self.distancePressed_previously = frogpilotCarState.distancePressed

    if not frogpilotCarState.distancePressed and 1 <= self.gap_counter < self.long_press_threshold:
      self.handle_button_event("distance", sm, frogpilot_toggles)
    elif frogpilotCarState.distancePressed and self.gap_counter == self.long_press_threshold:
      self.handle_button_event("distance_long", sm, frogpilot_toggles)
    elif frogpilotCarState.distancePressed and self.gap_counter == self.very_long_press_threshold:
      self.handle_button_event("distance_long", sm, frogpilot_toggles)
      self.handle_button_event("distance_very_long", sm, frogpilot_toggles)

    if any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents):
      self.lkas_button_press_count += 1
      self.handle_button_event("lkas", sm, frogpilot_toggles)

    frogpilotCarState.accelPressCount = self.accel_press_count
    frogpilotCarState.alwaysOnLateralEnabled = self.always_on_lateral_enabled
    frogpilotCarState.decelPressCount = self.decel_press_count
    frogpilotCarState.distanceLongPressed = self.very_long_press_threshold > self.gap_counter >= self.long_press_threshold
    frogpilotCarState.distanceVeryLongPressed = self.gap_counter >= self.very_long_press_threshold
    frogpilotCarState.experimentalModePressCount = self.experimental_mode_press_count
    frogpilotCarState.forceCoast = self.force_coast
    frogpilotCarState.lkasButtonPressCount = self.lkas_button_press_count
    frogpilotCarState.pauseLateral = self.pause_lateral
    frogpilotCarState.pauseLongitudinal = self.pause_longitudinal
    frogpilotCarState.trafficModeEnabled = self.traffic_mode_enabled

    return frogpilotCarState
