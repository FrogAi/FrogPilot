#!/usr/bin/env python3
from openpilot.common.constants import CV

from openpilot.frogpilot.common.frogpilot_variables import CRUISING_SPEED

class FrogPilotVCruise:
  def __init__(self, FrogPilotPlanner, params):
  self.frogpilot_planner = FrogPilotPlanner

  def update(self, gps_position, long_control_active, now, time_validated, v_cruise, v_ego, params, params_memory, sm, frogpilot_toggles):
    v_cruise_cluster = max(sm["carState"].vCruiseCluster * CV.KPH_TO_MS, v_cruise)
    v_cruise_diff = v_cruise_cluster - v_cruise

    v_ego_cluster = max(sm["carState"].vEgoCluster, v_ego)
    v_ego_diff = v_ego_cluster - v_ego

    targets = [v_cruise]
    v_cruise = min([target if target > CRUISING_SPEED else v_cruise for target in targets])

    return v_cruise
