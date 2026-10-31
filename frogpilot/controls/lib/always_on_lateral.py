#!/usr/bin/env python3
from cereal import car
from openpilot.selfdrive.controls.lib.events import EventName
from panda import ALTERNATIVE_EXPERIENCE

from openpilot.frogpilot.common import frogpilot_variables

ButtonType = car.CarState.ButtonEvent.Type

class AlwaysOnLateral:
  def __init__(self, CP, FPCP, frogpilot_toggles):
    self.armed = False
    self.brake_pressed_prev = False
    self.can_lost = False
    self.can_valid = False
    self.disarm_pending = False
    self.engaged_prev = False

    self.has_cruise_main = CP.carName != "tesla"
    self.lkas_button = frogpilot_toggles.always_on_lateral_lkas_button or (FPCP.alwaysOnLateralLKASButton and frogpilot_toggles.always_on_lateral)
    self.waiting = CP.carFingerprint not in frogpilot_variables.CRUISE_MAIN_OFF_AT_START_CARS
    self.waiting &= CP.carName not in frogpilot_variables.CRUISE_MAIN_OFF_AT_START_BRANDS
    self.waiting &= self.has_cruise_main
    self.waiting &= not (CP.carName == "hyundai" and CP.openpilotLongitudinalControl)

    self.cruise_main_prev = self.waiting

    if frogpilot_toggles.always_on_lateral:
      CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL

    if self.lkas_button:
      CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL_LKAS_BUTTON

    self.always_on_lateral = bool(CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL)

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

    if frogpilotCarState.steeringDisengage:
      self.disarm_pending = True

    if self.disarm_pending and not sm["carControl"].enabled:
      self.armed = False
      self.disarm_pending = False

    if self.armed:
      self.waiting = False

    self.brake_pressed_prev = carState.brakePressed
    self.cruise_main_prev = carState.cruiseState.available
    self.engaged_prev = sm["carControl"].enabled

    paused = carState.brakePressed and not carState.standstill and carState.vEgo < frogpilot_toggles.always_on_lateral_pause_on_brake_speed
    paused |= any(event.immediateDisable or event.softDisable for event in [*sm["onroadEvents"], *sm["frogpilotPlan"].frogpilotEvents])
    paused |= any(event.name == EventName.tooDistracted for event in sm["onroadEvents"])
    paused |= not sm["frogpilotPlan"].lateralCheck
    paused |= frogpilotCarState.speedSensorsDisagree
    paused |= rx_checks_invalid

    frogpilotCarState.alwaysOnLateralAllowed = self.armed and (frogpilot_toggles.always_on_lateral or sm["carControl"].enabled)
    frogpilotCarState.alwaysOnLateralEnabled = self.armed and not paused and frogpilot_toggles.always_on_lateral
    frogpilotCarState.alwaysOnLateralSteeringOff = sm["carControl"].enabled and not self.armed
    frogpilotCarState.alwaysOnLateralWaiting = self.waiting and frogpilot_toggles.always_on_lateral
