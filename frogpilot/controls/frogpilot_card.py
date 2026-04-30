#!/usr/bin/env python3
import cereal.messaging as messaging

from openpilot.common.params import Params
from openpilot.selfdrive.car.cruise import ButtonType

class FrogPilotCard:
  def __init__(self, CP):
    self.CP = CP

    self.params = Params(return_defaults=True)

    self.accel_press_count = 0
    self.decel_press_count = 0
    self.lkas_button_press_count = 0

    self.ui_event_sock = messaging.sub_sock("frogpilotUIEvent")

  def update(self, carState, frogpilotCarState, sm):
    if any(be.pressed and be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in carState.buttonEvents):
      self.accel_press_count += 1

    if any(be.pressed and be.type == ButtonType.decelCruise for be in carState.buttonEvents):
      self.decel_press_count += 1

    if any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents):
      self.lkas_button_press_count += 1
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
