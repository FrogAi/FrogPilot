#!/usr/bin/env python3
from openpilot.selfdrive.controls.lib.longitudinal_planner import ACCEL_MIN, get_max_accel

from openpilot.frogpilot.common import frogpilot_variables

class FrogPilotAcceleration:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.max_accel = 0
    self.min_accel = 0

  def update(self, v_ego, sm, frogpilot_toggles):
    eco_gear = sm["frogpilotCarState"].ecoGear
    sport_gear = sm["frogpilotCarState"].sportGear

    self.max_accel = get_max_accel(v_ego)

    force_decel = sm["controlsState"].forceDecel

    self.min_accel = ACCEL_MIN
