#!/usr/bin/env python3
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import get_jerk_factor, get_T_FOLLOW

class FrogPilotFollowing:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.acceleration_jerk = 0
    self.danger_jerk = 0
    self.speed_jerk = 0
    self.t_follow = 0

  def update(self, sm):
    self.acceleration_jerk, self.danger_jerk, self.speed_jerk = get_jerk_factor(sm["selfdriveState"].personality)

    self.t_follow = get_T_FOLLOW(sm["selfdriveState"].personality)
