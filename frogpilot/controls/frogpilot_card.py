#!/usr/bin/env python3
import cereal.messaging as messaging

from opendbc.car import structs
from openpilot.selfdrive.car.cruise import ButtonType

from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.controls.lib.always_on_lateral import AlwaysOnLateral

class FrogPilotCard:
  def __init__(self, CP, FPCP, frogpilot_toggles):
    self.CP = CP

    self.car_safety_mode_set = False
    self.manual_transmission = CP.transmissionType == structs.CarParams.TransmissionType.manual
    self.rx_checks_invalid = False

    self.accel_press_count = 0
    self.decel_press_count = 0
    self.lkas_button_press_count = 0

    self.always_on_lateral = AlwaysOnLateral(CP, FPCP, frogpilot_toggles)

    self.ui_event_sock = messaging.sub_sock("frogpilotUIEvent")

  def update(self, carState, frogpilotCarState, sm, frogpilot_toggles):
    panda_states = [panda_state for panda_state in sm["pandaStates"] if panda_state.safetyModel == self.CP.safetyConfigs[-1].safetyModel]

    self.car_safety_mode_set = len(panda_states) > 0

    if any(be.pressed and be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in carState.buttonEvents):
      self.accel_press_count += 1

    if any(be.pressed and be.type == ButtonType.decelCruise for be in carState.buttonEvents):
      self.decel_press_count += 1

    if any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents):
      self.lkas_button_press_count += 1

    frogpilotCarState.speedSensorsDisagree &= carState.canValid

    self.rx_checks_invalid = any(panda_state.safetyRxChecksInvalid for panda_state in panda_states)
    self.always_on_lateral.update(carState, frogpilotCarState, sm, frogpilot_toggles, self.rx_checks_invalid)

    frogpilotCarState.accelPressCount = self.accel_press_count
    frogpilotCarState.decelPressCount = self.decel_press_count
    frogpilotCarState.lkasButtonPressCount = self.lkas_button_press_count
    frogpilotCarState.parked = carState.gearShifter == frogpilot_variables.GearShifter.park or (self.manual_transmission and carState.standstill)
    frogpilotCarState.vEgo = carState.vEgo

    return frogpilotCarState
