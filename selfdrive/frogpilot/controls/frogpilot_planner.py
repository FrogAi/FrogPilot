import numpy as np

import cereal.messaging as messaging

from cereal import car, log

from openpilot.common.conversions import Conversions as CV
from openpilot.common.numpy_fast import interp
from openpilot.common.params import Params

from openpilot.selfdrive.car.interfaces import ACCEL_MIN, ACCEL_MAX
from openpilot.selfdrive.controls.lib.drive_helpers import V_CRUISE_UNSET
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import A_CHANGE_COST, J_EGO_COST, COMFORT_BRAKE, STOP_DISTANCE, get_jerk_factor, \
                                                                           get_safe_obstacle_distance, get_stopped_equivalence_factor, get_T_FOLLOW
from openpilot.selfdrive.controls.lib.longitudinal_planner import A_CRUISE_MIN, Lead, get_max_accel

from openpilot.selfdrive.frogpilot.controls.lib.conditional_experimental_mode import ConditionalExperimentalMode
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import calculate_lane_width, calculate_road_curvature
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_variables import CITY_SPEED_LIMIT, CRUISING_SPEED

GearShifter = car.CarState.GearShifter

# Acceleration profiles - Credit goes to the DragonPilot team!
                 # MPH = [0., 18,  36,  63,  94]
A_CRUISE_MIN_BP_CUSTOM = [0., 8., 16., 28., 42.]
                 # MPH = [0., 6.71, 13.4, 17.9, 24.6, 33.6, 44.7, 55.9, 89.5]
A_CRUISE_MAX_BP_CUSTOM = [0.,    3,   6.,   8.,  11.,  15.,  20.,  25.,  40.]

A_CRUISE_MIN_VALS_ECO = [-0.001, -0.010, -0.28, -0.56, -0.56]
A_CRUISE_MAX_VALS_ECO = [3.5, 3.2, 2.3, 2.0, 1.15, .80, .58, .36, .30]

A_CRUISE_MIN_VALS_SPORT = [-0.50, -0.52, -0.55, -0.57, -0.60]
A_CRUISE_MAX_VALS_SPORT = [3.5, 3.5, 3.3, 2.8, 1.5, 1.0, 0.75, 0.65, 0.6]

TRAFFIC_MODE_BP = [0., CITY_SPEED_LIMIT]

def get_min_accel_eco(v_ego):
  return interp(v_ego, A_CRUISE_MIN_BP_CUSTOM, A_CRUISE_MIN_VALS_ECO)

def get_max_accel_eco(v_ego):
  return interp(v_ego, A_CRUISE_MAX_BP_CUSTOM, A_CRUISE_MAX_VALS_ECO)

def get_min_accel_sport(v_ego):
  return interp(v_ego, A_CRUISE_MIN_BP_CUSTOM, A_CRUISE_MIN_VALS_SPORT)

def get_max_accel_sport(v_ego):
  return interp(v_ego, A_CRUISE_MAX_BP_CUSTOM, A_CRUISE_MAX_VALS_SPORT)

class FrogPilotPlanner:
  def __init__(self):
    self.params_memory = Params("/dev/shm/params")

    self.cem = ConditionalExperimentalMode()

    self.slower_lead = False

    self.acceleration_jerk = 0
    self.frame = 0
    self.road_curvature = 0
    self.speed_jerk = 0

  def update(self, carState, controlsState, frogpilotCarControl, frogpilotCarState, frogpilotNavigation, liveLocationKalman, modelData, radarState, frogpilot_toggles):
    self.lead_one = radarState.leadOne

    v_cruise_kph = min(controlsState.vCruise, V_CRUISE_UNSET)
    v_cruise = v_cruise_kph * CV.KPH_TO_MS

    v_ego = max(carState.vEgo, 0)
    v_lead = self.lead_one.vLead

    distance_offset = max(frogpilot_toggles.increased_stopping_distance + min(CITY_SPEED_LIMIT - v_ego, 0), 0) if not frogpilotCarControl.trafficModeActive else 0
    lead_distance = self.lead_one.dRel - distance_offset
    stopping_distance = STOP_DISTANCE + distance_offset

    if frogpilot_toggles.acceleration_profile == 1:
      self.max_accel = get_max_accel_eco(v_ego)
    elif frogpilot_toggles.acceleration_profile in (2, 3):
      self.max_accel = get_max_accel_sport(v_ego)
    elif controlsState.experimentalMode:
      self.max_accel = ACCEL_MAX
    else:
      self.max_accel = get_max_accel(v_ego)

    if controlsState.experimentalMode:
      self.min_accel = ACCEL_MIN
    elif frogpilot_toggles.deceleration_profile == 1:
      self.min_accel = get_min_accel_eco(v_ego)
    elif frogpilot_toggles.deceleration_profile == 2:
      self.min_accel = get_min_accel_sport(v_ego)
    else:
      self.min_accel = A_CRUISE_MIN

    check_lane_width = frogpilot_toggles.lane_detection
    if check_lane_width and v_ego >= frogpilot_toggles.minimum_lane_change_speed:
      self.lane_width_left = float(calculate_lane_width(modelData.laneLines[0], modelData.laneLines[1], modelData.roadEdges[0]))
      self.lane_width_right = float(calculate_lane_width(modelData.laneLines[3], modelData.laneLines[2], modelData.roadEdges[1]))
    else:
      self.lane_width_left = 0
      self.lane_width_right = 0

    if frogpilotCarControl.trafficModeActive:
      self.base_acceleration_jerk = interp(v_ego, TRAFFIC_MODE_BP, frogpilot_toggles.traffic_mode_jerk_acceleration)
      self.base_speed_jerk = interp(v_ego, TRAFFIC_MODE_BP, frogpilot_toggles.traffic_mode_jerk_speed)
      self.t_follow = interp(v_ego, TRAFFIC_MODE_BP, frogpilot_toggles.traffic_mode_t_follow)
    else:
      self.base_acceleration_jerk, self.base_speed_jerk = get_jerk_factor(frogpilot_toggles.custom_personalities,
                                                                          frogpilot_toggles.aggressive_jerk_acceleration, frogpilot_toggles.aggressive_jerk_speed,
                                                                          frogpilot_toggles.standard_jerk_acceleration, frogpilot_toggles.standard_jerk_speed,
                                                                          frogpilot_toggles.relaxed_jerk_acceleration, frogpilot_toggles.relaxed_jerk_speed,
                                                                          controlsState.personality)
      self.t_follow = get_T_FOLLOW(frogpilot_toggles.custom_personalities, frogpilot_toggles.aggressive_follow,
                                   frogpilot_toggles.standard_follow, frogpilot_toggles.relaxed_follow, controlsState.personality)

    if self.lead_one.status:
      self.update_follow_values(lead_distance, stopping_distance, v_ego, v_lead, frogpilot_toggles)
    else:
      self.acceleration_jerk = self.base_acceleration_jerk
      self.speed_jerk = self.base_speed_jerk

    self.road_curvature = calculate_road_curvature(modelData, v_ego)
    self.v_cruise = self.update_v_cruise(carState, controlsState, frogpilotCarState, frogpilotNavigation, liveLocationKalman, modelData, v_cruise, v_ego, frogpilot_toggles)

    if frogpilot_toggles.conditional_experimental_mode:
      self.cem.update(carState, controlsState.enabled, frogpilotNavigation, lead_distance, self.lead_one, modelData, self.road_curvature, self.slower_lead, v_ego, v_lead, frogpilot_toggles)

    self.frame += 1

  def update_follow_values(self, lead_distance, stopping_distance, v_ego, v_lead, frogpilot_toggles):
    # Offset by FrogAi for FrogPilot for a more natural approach to a faster lead
    if frogpilot_toggles.aggressive_acceleration_experimental and v_lead > v_ego:
      distance_factor = max(lead_distance - (v_ego * self.t_follow), 1)
      standstill_offset = max(stopping_distance - v_ego, 0) * max(v_lead - v_ego, 0)
      acceleration_offset = np.clip((v_lead - v_ego) + standstill_offset - COMFORT_BRAKE, 1, distance_factor)
      self.acceleration_jerk = self.base_acceleration_jerk / acceleration_offset
      self.speed_jerk = self.base_speed_jerk / acceleration_offset
      self.t_follow /= acceleration_offset
    elif frogpilot_toggles.aggressive_acceleration and v_lead > v_ego:
      distance_factor = np.maximum(lead_distance - (v_lead * self.t_follow), 1)
      standstill_offset = max(STOP_DISTANCE - (v_ego**COMFORT_BRAKE), 0)
      acceleration_offset = np.clip((v_lead - v_ego) + standstill_offset - COMFORT_BRAKE, 1, distance_factor)
      self.t_follow /= acceleration_offset

    # Offset by FrogAi for FrogPilot for a more natural approach to a slower lead
    if (frogpilot_toggles.conditional_experimental_mode or frogpilot_toggles.smoother_braking) and v_lead < v_ego:
      distance_factor = max(lead_distance - (v_lead * self.t_follow), 1)
      far_lead_offset = max(lead_distance - (v_ego * self.t_follow) - stopping_distance + (v_lead - CITY_SPEED_LIMIT), 0) if frogpilot_toggles.smoother_braking_far_lead else 0
      braking_offset = np.clip((v_ego - v_lead) + far_lead_offset - COMFORT_BRAKE, 1, distance_factor)
      if frogpilot_toggles.smoother_braking:
        if frogpilot_toggles.smoother_braking_jerk:
          self.acceleration_jerk = self.base_acceleration_jerk * min(braking_offset, COMFORT_BRAKE / 2)
          self.speed_jerk = self.base_speed_jerk * min(braking_offset, COMFORT_BRAKE * 2)
        self.t_follow /= braking_offset
      self.slower_lead = max(braking_offset - far_lead_offset, 1) > 1

  def update_v_cruise(self, carState, controlsState, frogpilotCarState, frogpilotNavigation, liveLocationKalman, modelData, v_cruise, v_ego, frogpilot_toggles):
    v_cruise_cluster = max(controlsState.vCruiseCluster, controlsState.vCruise) * CV.KPH_TO_MS
    v_cruise_diff = v_cruise_cluster - v_cruise

    v_ego_cluster = max(carState.vEgoCluster, v_ego)
    v_ego_diff = v_ego_cluster - v_ego

    targets = []
    filtered_targets = [target if target > CRUISING_SPEED else v_cruise for target in targets]

    return min(filtered_targets)

  def publish(self, sm, pm, frogpilot_toggles):
    frogpilot_plan_send = messaging.new_message('frogpilotPlan')
    frogpilot_plan_send.valid = sm.all_checks(service_list=['carState', 'controlsState'])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    frogpilotPlan.accelerationJerk = A_CHANGE_COST * float(self.acceleration_jerk)
    frogpilotPlan.accelerationJerkStock = A_CHANGE_COST * float(self.base_acceleration_jerk)
    frogpilotPlan.speedJerk = J_EGO_COST * float(self.speed_jerk)
    frogpilotPlan.speedJerkStock = J_EGO_COST * float(self.base_speed_jerk)
    frogpilotPlan.tFollow = float(self.t_follow)

    frogpilotPlan.conditionalExperimental = self.cem.experimental_mode

    frogpilotPlan.maxAcceleration = self.max_accel
    frogpilotPlan.minAcceleration = self.min_accel

    frogpilotPlan.vCruise = float(self.v_cruise)

    pm.send('frogpilotPlan', frogpilot_plan_send)
