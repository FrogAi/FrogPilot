#!/usr/bin/env python3
import cereal.messaging as messaging

from opendbc.car import structs
from openpilot.selfdrive.car.cruise import ButtonType

from openpilot.frogpilot.common import frogpilot_utilities

class FrogPilotCard:
  def __init__(self, CP):
    self.CP = CP

    self.manual_transmission = CP.transmissionType == structs.CarParams.TransmissionType.manual

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
    frogpilotCarState.decelPressCount = self.decel_press_count
    frogpilotCarState.lkasButtonPressCount = self.lkas_button_press_count
    frogpilotCarState.parked = carState.gearShifter == frogpilot_variables.GearShifter.park or (self.manual_transmission and carState.standstill)
    frogpilotCarState.vEgo = carState.vEgo

    return frogpilotCarState
