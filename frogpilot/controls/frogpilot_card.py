#!/usr/bin/env python3
from openpilot.common.params import Params
from openpilot.selfdrive.car.cruise import ButtonType


class FrogPilotCard:
  def __init__(self, CP, FPCP):
    self.CP = CP

    self.params = Params(return_defaults=True)

    self.accel_pressed = False
    self.decel_pressed = False

  def update(self, carState, frogpilotCarState, sm):
    accel_pressed = any(be.pressed and be.type in (ButtonType.accelCruise, ButtonType.resumeCruise) for be in carState.buttonEvents)
    if sm.updated["frogpilotPlan"] or accel_pressed:
      self.accel_pressed = accel_pressed

    decel_pressed = any(be.pressed and be.type == ButtonType.decelCruise for be in carState.buttonEvents)
    if sm.updated["frogpilotPlan"] or decel_pressed:
      self.decel_pressed = decel_pressed

    frogpilotCarState.accelPressed = self.accel_pressed
    frogpilotCarState.decelPressed = self.decel_pressed

    return frogpilotCarState
