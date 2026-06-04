#!/usr/bin/env python3
import numpy as np

from openpilot.selfdrive.controls.lib.longitudinal_planner import ACCEL_MIN, get_max_accel


class FrogPilotAcceleration:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.max_accel = 0
    self.min_accel = 0

  def update(self, v_ego, sm):
    self.max_accel = get_max_accel(v_ego)

    if self.frogpilot_planner.tracking_lead:
      self.min_accel = ACCEL_MIN
    else:
      self.min_accel = ACCEL_MIN
