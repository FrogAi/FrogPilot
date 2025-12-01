#!/usr/bin/env python3
import numpy as np

from openpilot.selfdrive.controls.lib.longitudinal_planner import ACCEL_MIN, get_coast_accel, get_max_accel

A_CRUISE_MIN_ECO = ACCEL_MIN / 2
A_CRUISE_MIN_COAST = -0.05

                  # MPH = [0.0,  11,  22,  34,  45,  56,  89]
A_CRUISE_MAX_BP_CUSTOM =  [0.0,  5., 10., 15., 20., 25., 40.]
A_CRUISE_MAX_VALS_ECO =   [1.4, 1.3, 1.0, 0.7, 0.6, 0.5, 0.4]
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
    eco_gear = sm["frogpilotCarState"].ecoGear
    sport_gear = sm["frogpilotCarState"].sportGear

    if (eco_gear or sport_gear) and frogpilot_toggles.map_acceleration:
      if eco_gear:
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

    if self.frogpilot_planner.frogpilot_vcruise.csc_controlling_speed and self.frogpilot_planner.frogpilot_vcruise.csc.decel_rate > 0:
      self.max_accel = max(-self.frogpilot_planner.frogpilot_vcruise.csc.decel_rate, ACCEL_MIN)

    force_decel = sm["controlsState"].forceDecel

    lead_two_relevant = self.frogpilot_planner.is_lead_relevant(sm["radarState"].leadTwo, sm["carState"].standstill, v_ego)

    if (self.frogpilot_planner.lead_relevant or lead_two_relevant or self.frogpilot_planner.frogpilot_vcruise.csc_controlling_speed or
        self.frogpilot_planner.frogpilot_vcruise.taco_controlling_speed or force_decel):
      self.min_accel = ACCEL_MIN
    elif (eco_gear or sport_gear) and frogpilot_toggles.map_deceleration:
      if eco_gear:
        self.min_accel = A_CRUISE_MIN_ECO
      else:
        self.min_accel = ACCEL_MIN
    else:
      if frogpilot_toggles.deceleration_profile == DECELERATION_PROFILES["ECO"]:
        self.min_accel = A_CRUISE_MIN_ECO
      elif frogpilot_toggles.deceleration_profile == DECELERATION_PROFILES["ECO_PLUS"]:
        if len(sm["carControl"].orientationNED) == 3:
          self.min_accel = min(get_coast_accel(sm["carControl"].orientationNED[1]), A_CRUISE_MIN_COAST)
        else:
          self.min_accel = get_coast_accel(0)
      else:
        self.min_accel = ACCEL_MIN
