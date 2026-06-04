#!/usr/bin/env python3
import numpy as np

from openpilot.selfdrive.controls.lib.longitudinal_planner import ACCEL_MIN, get_max_accel


A_CRUISE_MIN_ECO =      ACCEL_MIN / 2
A_CRUISE_MIN_ECO_PLUS = ACCEL_MIN / 4

                  # MPH = [0.0,  11,  22,  34,  45,  56,  89]
A_CRUISE_MAX_BP_CUSTOM =  [0.0,  5., 10., 15., 20., 25., 40.]
A_CRUISE_MAX_VALS_ECO =   [2.0, 1.5, 1.0, 0.8, 0.6, 0.4, 0.2]
A_CRUISE_MAX_VALS_SPORT = [3.0, 2.5, 2.0, 1.5, 1.0, 0.8, 0.6]

ACCELERATION_PROFILES = {
  "STANDARD": 0,
  "ECO": 1,
  "SPORT": 2,
  "SPORT_PLUS": 3
}

DECELERATION_PROFILES = {
  "STANDARD": 0,
  "ECO": 1,
  "ECO_PLUS": 2
}

def get_max_accel_eco(v_ego):
  return np.interp(v_ego, A_CRUISE_MAX_BP_CUSTOM, A_CRUISE_MAX_VALS_ECO)

def get_max_accel_sport(v_ego):
  return np.interp(v_ego, A_CRUISE_MAX_BP_CUSTOM, A_CRUISE_MAX_VALS_SPORT)

def get_max_allowed_accel(v_ego):
  return np.interp(v_ego, [0., 5., 20.], [4.0, 4.0, 2.0])  # ISO 15622:2018


class FrogPilotAcceleration:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.max_accel = 0
    self.min_accel = 0

  def update(self, v_ego, sm, frogpilot_toggles):
    if (sm["frogpilotCarState"].ecoGear or sm["frogpilotCarState"].sportGear) and frogpilot_toggles.map_acceleration:
      if sm["frogpilotCarState"].ecoGear:
        self.max_accel = get_max_accel_eco(v_ego)
      else:
        if frogpilot_toggles.acceleration_profile == ACCELERATION_PROFILES["SPORT"]:
          self.max_accel = get_max_accel_sport(v_ego)
        else:
          self.max_accel = get_max_allowed_accel(v_ego)
    else:
      if frogpilot_toggles.acceleration_profile == ACCELERATION_PROFILES["ECO"]:
        self.max_accel = get_max_accel_eco(v_ego)
      elif frogpilot_toggles.acceleration_profile == ACCELERATION_PROFILES["SPORT"]:
        self.max_accel = get_max_accel_sport(v_ego)
      elif frogpilot_toggles.acceleration_profile == ACCELERATION_PROFILES["SPORT_PLUS"]:
        self.max_accel = get_max_allowed_accel(v_ego)
      else:
        self.max_accel = get_max_accel(v_ego)

    if self.frogpilot_planner.tracking_lead:
      self.min_accel = ACCEL_MIN
    elif (sm["frogpilotCarState"].ecoGear or sm["frogpilotCarState"].sportGear) and frogpilot_toggles.map_deceleration:
      if sm["frogpilotCarState"].ecoGear:
        self.min_accel = A_CRUISE_MIN_ECO
      else:
        self.min_accel = A_CRUISE_MIN_ECO_PLUS
    else:
      if frogpilot_toggles.deceleration_profile == DECELERATION_PROFILES["ECO"]:
        self.min_accel = A_CRUISE_MIN_ECO
      elif frogpilot_toggles.deceleration_profile == DECELERATION_PROFILES["ECO_PLUS"]:
        self.min_accel = A_CRUISE_MIN_ECO_PLUS
      else:
        self.min_accel = ACCEL_MIN
