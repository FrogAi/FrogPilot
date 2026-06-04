#!/usr/bin/env python3
from openpilot.common.constants import CV

from openpilot.frogpilot.common import frogpilot_variables


class FrogPilotVCruise:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

  def update(self, long_control_active, now, time_validated, v_cruise, v_ego, sm, frogpilot_toggles):
    v_cruise_display = max(sm["carState"].vCruiseCluster * CV.KPH_TO_MS, v_cruise)
    v_ego_display = max(sm["carState"].vEgoCluster, v_ego)
    v_ego_display_offset = v_ego_display - v_ego

    targets = [v_cruise]
    v_cruise = min(target if target >= frogpilot_variables.CRUISING_SPEED else v_cruise for target in targets)

    return v_cruise
