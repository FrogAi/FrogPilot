#!/usr/bin/env python3
from openpilot.common.constants import CV

from openpilot.frogpilot.common import frogpilot_variables

class FrogPilotVCruise:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

  def update(self, v_cruise, v_ego, sm, frogpilot_toggles):
    v_cruise_cluster = max(sm["carState"].vCruiseCluster * CV.KPH_TO_MS, v_cruise)

    v_ego_cluster = max(sm["carState"].vEgoCluster, v_ego)
    v_ego_diff = v_ego_cluster - v_ego

    targets = [v_cruise]
    v_cruise = min(targets)

    return v_cruise
