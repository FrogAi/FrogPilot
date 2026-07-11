#!/usr/bin/env python3
from opendbc.safety import ALTERNATIVE_EXPERIENCE
from openpilot.common.params import Params
from openpilot.selfdrive.car.cruise import ButtonType
from openpilot.selfdrive.selfdrived.events import EventName

from openpilot.frogpilot.common import frogpilot_variables


class FrogPilotCard:
  def __init__(self, CP, FPCP):
    self.CP = CP

    self.params = Params(return_defaults=True)

    self.accel_pressed = False
    self.always_on_lateral_allowed = False
    self.decel_pressed = False

    self.always_on_lateral_set = bool(FPCP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL)

  def update(self, carState, frogpilotCarState, sm, frogpilot_toggles):
    if self.CP.brand == "hyundai":
      for be in carState.buttonEvents:
        if be.type == ButtonType.lkas and be.pressed and frogpilot_toggles.always_on_lateral_lkas:
          self.always_on_lateral_allowed = not self.always_on_lateral_allowed
        elif be.type == ButtonType.mainCruise and be.pressed and frogpilot_toggles.always_on_lateral_main:
          self.always_on_lateral_allowed = not self.always_on_lateral_allowed
    elif frogpilot_toggles.always_on_lateral_main:
      self.always_on_lateral_allowed = carState.cruiseState.available

    immediate_disable = any(event.immediateDisable and event.name != EventName.speedTooLow for event in sm["onroadEvents"])
    immediate_disable |= any(event.immediateDisable for event in sm["frogpilotOnroadEvents"].events)

    self.always_on_lateral_enabled = self.always_on_lateral_allowed and self.always_on_lateral_set
    self.always_on_lateral_enabled &= carState.gearShifter not in frogpilot_variables.NON_DRIVING_GEARS
    self.always_on_lateral_enabled &= sm["frogpilotPlan"].lateralCheck
    self.always_on_lateral_enabled &= sm["liveCalibration"].calPerc >= 1
    self.always_on_lateral_enabled &= not immediate_disable
    self.always_on_lateral_enabled &= not (carState.brakePressed and carState.vEgo < frogpilot_toggles.always_on_lateral_pause_speed) or carState.standstill

    accel_pressed = any(be.pressed and be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in carState.buttonEvents)
    if sm.updated["frogpilotPlan"] or accel_pressed:
      self.accel_pressed = accel_pressed

    decel_pressed = any(be.pressed and be.type == ButtonType.decelCruise for be in carState.buttonEvents)
    if sm.updated["frogpilotPlan"] or decel_pressed:
      self.decel_pressed = decel_pressed

    frogpilotCarState.accelPressed = self.accel_pressed
    frogpilotCarState.alwaysOnLateralEnabled = self.always_on_lateral_enabled
    frogpilotCarState.decelPressed = self.decel_pressed

    return frogpilotCarState
