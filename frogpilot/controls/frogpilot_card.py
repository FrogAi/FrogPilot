#!/usr/bin/env python3
import cereal.messaging as messaging

from cereal import log
from opendbc.car import structs
from opendbc.safety import ALTERNATIVE_EXPERIENCE
from openpilot.selfdrive.car.cruise import ButtonType

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables

EventName = log.OnroadEvent.EventName

class FrogPilotCard:
  def __init__(self, CP):
    self.CP = CP

    self.always_on_lateral_allowed = False
    self.car_safety_mode_set = True
    self.cruise_available_previously = True
    self.lkas_press_used = False
    self.manual_transmission = CP.transmissionType == structs.CarParams.TransmissionType.manual
    self.steering_disengaged = False

    self.accel_press_count = 0
    self.decel_press_count = 0
    self.lkas_button_press_count = 0

    self.always_on_lateral_set = bool(CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL)
    self.lkas_starts_lateral = self.always_on_lateral_set and CP.brand not in frogpilot_variables.LKAS_STATUS_BRANDS

    self.ui_event_sock = messaging.sub_sock("frogpilotUIEvent")

  def update(self, carState, frogpilotCarState, sm, frogpilot_toggles):
    if self.CP.brand == "hyundai":
      safety_model = self.CP.safetyConfigs[-1].safetyModel
      self.car_safety_mode_set = any(panda_state.safetyModel == safety_model for panda_state in sm["pandaStates"])

    if frogpilot_toggles.always_on_lateral_lkas:
      if not self.car_safety_mode_set:
        self.always_on_lateral_allowed = False
      else:
        if any(be.type == ButtonType.lkas and be.pressed for be in carState.buttonEvents):
          self.always_on_lateral_allowed = not self.always_on_lateral_allowed

        self.always_on_lateral_allowed |= any(panda_state.controlsAllowed for panda_state in sm["pandaStates"])
    elif frogpilot_toggles.always_on_lateral_main and self.CP.brand == "tesla":
      self.always_on_lateral_allowed = True
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

    if carState.steeringDisengage:
      self.steering_disengaged = True
    elif not carState.steeringPressed:
      self.steering_disengaged = False

    self.always_on_lateral_enabled = self.always_on_lateral_allowed and self.always_on_lateral_set and frogpilot_toggles.always_on_lateral
    self.always_on_lateral_enabled &= carState.gearShifter not in frogpilot_variables.NON_DRIVING_GEARS
    self.always_on_lateral_enabled &= sm["liveCalibration"].calPerc >= 1
    self.always_on_lateral_enabled &= not sm["frogpilotSelfdriveState"].hasDisableEvents
    self.always_on_lateral_enabled &= not any(event.name == EventName.tooDistracted for event in sm["onroadEvents"])
    self.always_on_lateral_enabled &= not (carState.brakePressed and carState.vEgo < frogpilot_toggles.always_on_lateral_pause_speed) or carState.standstill
    self.always_on_lateral_enabled &= not self.steering_disengaged

    if any(be.pressed and be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in carState.buttonEvents):
      self.accel_press_count += 1

    if any(be.pressed and be.type == ButtonType.decelCruise for be in carState.buttonEvents):
      self.decel_press_count += 1

    if any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents):
      self.lkas_button_press_count += 1

    frogpilotCarState.accelPressCount = self.accel_press_count
    frogpilotCarState.alwaysOnLateralAllowed = self.always_on_lateral_allowed
    frogpilotCarState.alwaysOnLateralEnabled = self.always_on_lateral_enabled
    frogpilotCarState.decelPressCount = self.decel_press_count
    frogpilotCarState.lkasButtonPressCount = self.lkas_button_press_count
    frogpilotCarState.parked = carState.gearShifter == frogpilot_variables.GearShifter.park or (self.manual_transmission and carState.standstill)
    frogpilotCarState.vEgo = carState.vEgo

    return frogpilotCarState
