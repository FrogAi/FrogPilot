#!/usr/bin/env python3
from cereal import log
from opendbc.safety import ALTERNATIVE_EXPERIENCE
from openpilot.selfdrive.car.cruise import ButtonType

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables

class AlwaysOnLateral:
  def __init__(self, CP, FPCP, frogpilot_toggles):
    self.armed = False
    self.brake_pressed_prev = False
    self.can_lost = False
    self.can_valid = False
    self.disarm_pending = False
    self.engaged_prev = False
    self.lkas_steering_off = False

    self.frogs_go_moo = frogpilot_utilities.is_FrogsGoMoo()
    self.has_cruise_main = CP.brand not in frogpilot_variables.NO_CRUISE_SWITCH_BRANDS
    self.waiting = CP.carFingerprint not in frogpilot_variables.CRUISE_MAIN_OFF_AT_START_CAR
    self.waiting &= CP.brand not in frogpilot_variables.CRUISE_MAIN_OFF_AT_START_BRANDS
    self.waiting &= self.has_cruise_main
    self.waiting &= not (CP.brand == "hyundai" and CP.openpilotLongitudinalControl)

    self.cruise_main_prev = self.waiting

    if frogpilot_toggles.always_on_lateral and not CP.passive:
      CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL

      if frogpilot_toggles.always_on_lateral_lkas_button or FPCP.alwaysOnLateralLKASButton:
        CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL_LKAS_BUTTON

    self.always_on_lateral = bool(CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL)
    self.lkas_button = bool(CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL_LKAS_BUTTON)

  def update(self, carState, frogpilotCarState, sm, frogpilot_toggles, rx_checks_invalid):
    if not (self.always_on_lateral and carState.canValid):
      if not carState.canValid:
        self.armed = False

      self.can_lost = self.can_valid

      return

    if carState.cruiseState.available and self.can_lost and self.has_cruise_main:
      self.cruise_main_prev = True
      self.waiting = True

    self.can_lost = False
    self.can_valid = True

    if any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents) and self.lkas_button and frogpilot_toggles.always_on_lateral_lkas_button:
      self.armed = not self.armed
      self.lkas_steering_off = sm["carControl"].enabled

    if self.has_cruise_main:
      if carState.cruiseState.available and not self.cruise_main_prev:
        self.armed = True

      if not carState.cruiseState.available and self.cruise_main_prev:
        self.armed = False
        self.waiting = False

    if sm["carControl"].enabled and not self.engaged_prev:
      self.armed = True

    if carState.brakePressed and not self.brake_pressed_prev and not self.has_cruise_main:
      self.armed = False

    if carState.steeringDisengage:
      self.disarm_pending = True

    if self.disarm_pending and not sm["carControl"].enabled:
      self.armed = False
      self.disarm_pending = False

    if self.armed:
      self.waiting = False

    self.lkas_steering_off &= not self.armed and sm["carControl"].enabled

    self.brake_pressed_prev = carState.brakePressed
    self.cruise_main_prev = carState.cruiseState.available
    self.engaged_prev = sm["carControl"].enabled

    paused = carState.brakePressed and not carState.standstill and carState.vEgo < frogpilot_toggles.always_on_lateral_pause_on_brake_speed
    paused |= carState.gearShifter in frogpilot_variables.NON_DRIVING_GEARS
    paused |= sm["frogpilotSelfdriveState"].hasDisableEvents and not self.frogs_go_moo
    paused |= any(event.name == log.OnroadEvent.EventName.tooDistracted for event in sm["onroadEvents"])
    paused |= not sm.alive["frogpilotSelfdriveState"]
    paused |= frogpilotCarState.speedSensorsDisagree
    paused |= rx_checks_invalid

    frogpilotCarState.alwaysOnLateralAllowed = self.armed and (sm["carControl"].enabled or frogpilot_toggles.always_on_lateral)
    frogpilotCarState.alwaysOnLateralEnabled = not paused and self.armed and frogpilot_toggles.always_on_lateral
    frogpilotCarState.alwaysOnLateralSteeringOff = self.lkas_steering_off
    frogpilotCarState.alwaysOnLateralWaiting = self.waiting and frogpilot_toggles.always_on_lateral
