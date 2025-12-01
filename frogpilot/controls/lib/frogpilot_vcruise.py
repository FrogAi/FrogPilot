#!/usr/bin/env python3
import numpy as np

from openpilot.common.constants import CV
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import CRUISE_MIN_ACCEL

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.controls.lib.curve_speed_controller import CurveSpeedController

class FrogPilotVCruise:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.csc = CurveSpeedController(self)

    self.csc_active = False
    self.csc_controlling_speed = False
    self.taco_controlling_speed = False

  def update(self, long_control_active, v_cruise, v_ego, sm, frogpilot_toggles):
    v_cruise_cluster = max(sm["carState"].vCruiseCluster * CV.KPH_TO_MS, v_cruise)

    v_ego_cluster = max(sm["carState"].vEgoCluster, v_ego)
    v_ego_diff = v_ego_cluster - v_ego

    # FrogsGoMoo's Curve Speed Controller
    self.csc.profile_learner.update(long_control_active, sm)

    if long_control_active and frogpilot_toggles.curve_speed_controller:
      self.csc_target = self.csc.update_target(v_cruise, v_ego)
    else:
      self.csc.target = None

      self.csc_target = v_cruise

    self.csc_active = self.csc_target < v_cruise

    targets = [self.csc_target, v_cruise]

    taco_target = v_cruise
    if long_control_active and frogpilot_toggles.taco_tune and not frogpilot_toggles.curve_speed_controller:
      turn_speeds = np.maximum(frogpilot_utilities.calculate_taco_turn_speeds(sm["modelV2"].orientationRate.z, sm["modelV2"].velocity.x, v_ego), 0.0)
      turn_distances = np.maximum(np.asarray(sm["modelV2"].position.x) - v_ego * frogpilot_variables.TACO_LAG_TIME, 0.0)
      taco_target = np.sqrt(turn_speeds ** 2 - 2 * CRUISE_MIN_ACCEL * turn_distances).min()
      targets.append(taco_target)

    taco_active = taco_target < v_cruise

    v_cruise = min(targets)

    self.csc_controlling_speed = self.csc_active and self.csc_target == v_cruise
    self.taco_controlling_speed = taco_active and taco_target == v_cruise

    return v_cruise
