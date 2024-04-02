import numpy as np

import cereal.messaging as messaging

from openpilot.common.conversions import Conversions as CV
from openpilot.common.numpy_fast import interp
from openpilot.common.params import Params

from openpilot.selfdrive.car.interfaces import ACCEL_MIN, ACCEL_MAX
from openpilot.selfdrive.controls.lib.desire_helper import LANE_CHANGE_SPEED_MIN
from openpilot.selfdrive.controls.lib.drive_helpers import V_CRUISE_UNSET
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import A_CHANGE_COST, J_EGO_COST, COMFORT_BRAKE, STOP_DISTANCE, get_jerk_factor, \
                                                                           get_safe_obstacle_distance, get_stopped_equivalence_factor, get_T_FOLLOW
from openpilot.selfdrive.controls.lib.longitudinal_planner import A_CRUISE_MIN, get_max_accel

from openpilot.system.version import get_short_branch

from openpilot.selfdrive.frogpilot.controls.lib.conditional_experimental_mode import ConditionalExperimentalMode
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import CITY_SPEED_LIMIT, CRUISING_SPEED, calculate_lane_width, calculate_road_curvature

# Acceleration profiles - Credit goes to the DragonPilot team!
                 # MPH = [0., 18,  36,  63,  94]
A_CRUISE_MIN_BP_CUSTOM = [0., 8., 16., 28., 42.]
                 # MPH = [0., 6.71, 13.4, 17.9, 24.6, 33.6, 44.7, 55.9, 67.1, 123]
A_CRUISE_MAX_BP_CUSTOM = [0.,    3,   6.,   8.,  11.,  15.,  20.,  25.,  30., 55.]

A_CRUISE_MIN_VALS_ECO = [-0.001, -0.010, -0.28, -0.56, -0.56]
A_CRUISE_MAX_VALS_ECO = [3.5, 3.2, 2.3, 2.0, 1.15, .80, .58, .36, .30, .091]

A_CRUISE_MIN_VALS_SPORT = [-0.50, -0.52, -0.55, -0.57, -0.60]
A_CRUISE_MAX_VALS_SPORT = [3.5, 3.5, 3.3, 2.8, 1.5, 1.0, .75, .6, .38, .2]

def get_min_accel_eco(v_ego):
  return interp(v_ego, A_CRUISE_MIN_BP_CUSTOM, A_CRUISE_MIN_VALS_ECO)

def get_max_accel_eco(v_ego):
  return interp(v_ego, A_CRUISE_MAX_BP_CUSTOM, A_CRUISE_MAX_VALS_ECO)

def get_min_accel_sport(v_ego):
  return interp(v_ego, A_CRUISE_MIN_BP_CUSTOM, A_CRUISE_MIN_VALS_SPORT)

def get_max_accel_sport(v_ego):
  return interp(v_ego, A_CRUISE_MAX_BP_CUSTOM, A_CRUISE_MAX_VALS_SPORT)

class FrogPilotPlanner:
  def __init__(self, CP):
    self.CP = CP

    self.params = Params()
    self.params_memory = Params("/dev/shm/params")

    self.cem = ConditionalExperimentalMode()

    self.jerk = 0
    self.t_follow = 0

  def update(self, carState, controlsState, frogpilotCarControl, frogpilotNavigation, liveLocationKalman, modelData, radarState, frogpilot_toggles):
    v_cruise_kph = min(controlsState.vCruise, V_CRUISE_UNSET)
    v_cruise = v_cruise_kph * CV.KPH_TO_MS
    v_ego = max(carState.vEgo, 0)
    v_lead = radarState.leadOne.vLead

    if frogpilot_toggles.acceleration_profile == 1:
      self.max_accel = get_max_accel_eco(v_ego)
    elif frogpilot_toggles.acceleration_profile in (2, 3):
      self.max_accel = get_max_accel_sport(v_ego)
    elif not controlsState.experimentalMode:
      self.max_accel = get_max_accel(v_ego)
    else:
      self.max_accel = ACCEL_MAX

    if frogpilot_toggles.deceleration_profile == 1:
      self.min_accel = get_min_accel_eco(v_ego)
    elif frogpilot_toggles.deceleration_profile == 2:
      self.min_accel = get_min_accel_sport(v_ego)
    elif not controlsState.experimentalMode:
      self.min_accel = A_CRUISE_MIN
    else:
      self.min_accel = ACCEL_MIN

    check_lane_width = frogpilot_toggles.adjacent_lanes or frogpilot_toggles.blind_spot_path
    if check_lane_width and v_ego >= LANE_CHANGE_SPEED_MIN:
      self.lane_width_left = float(calculate_lane_width(modelData.laneLines[0], modelData.laneLines[1], modelData.roadEdges[0]))
      self.lane_width_right = float(calculate_lane_width(modelData.laneLines[3], modelData.laneLines[2], modelData.roadEdges[1]))
    else:
      self.lane_width_left = 0
      self.lane_width_right = 0

    road_curvature = calculate_road_curvature(modelData, v_ego)

    if radarState.leadOne.status and self.CP.openpilotLongitudinalControl:
      base_jerk = get_jerk_factor(frogpilot_toggles.custom_personalities, frogpilot_toggles.aggressive_jerk, frogpilot_toggles.standard_jerk, frogpilot_toggles.relaxed_jerk, controlsState.personality)
      base_t_follow = get_T_FOLLOW(frogpilot_toggles.custom_personalities, frogpilot_toggles.aggressive_follow, frogpilot_toggles.standard_follow, frogpilot_toggles.relaxed_follow, controlsState.personality)
      self.safe_obstacle_distance = int(np.mean(get_safe_obstacle_distance(v_ego, self.t_follow)))
      self.safe_obstacle_distance_stock = int(np.mean(get_safe_obstacle_distance(v_ego, base_t_follow)))
      self.stopped_equivalence_factor = int(np.mean(get_stopped_equivalence_factor(v_lead)))
      self.jerk, self.t_follow = self.update_follow_values(base_jerk, radarState, base_t_follow, v_ego, v_lead, frogpilot_toggles)
    else:
      self.safe_obstacle_distance = 0
      self.safe_obstacle_distance_stock = 0
      self.stopped_equivalence_factor = 0
      self.t_follow = 1.45

    self.v_cruise = self.update_v_cruise(carState, controlsState, controlsState.enabled, liveLocationKalman, modelData, road_curvature, v_cruise, v_ego, frogpilot_toggles)

    if frogpilot_toggles.conditional_experimental_mode:
      self.cem.update(carState, controlsState.enabled, frogpilotNavigation, modelData, radarState, road_curvature, self.t_follow, v_ego, frogpilot_toggles)

  def update_follow_values(self, jerk, radarState, t_follow, v_ego, v_lead, frogpilot_toggles):
    lead_distance = radarState.leadOne.dRel

    # Offset by FrogAi for FrogPilot for a more natural approach to a faster lead
    if frogpilot_toggles.aggressive_acceleration_experimental and v_lead > v_ego:
      distance_factor = np.maximum(lead_distance - (v_ego * t_follow), 1)
      standstill_offset = max(stopping_distance - v_ego, 0)
      acceleration_offset = np.clip((v_lead - v_ego) + (standstill_offset * max(v_lead - v_ego, 0)) - COMFORT_BRAKE, 1, distance_factor)
      jerk /= acceleration_offset
      t_follow /= acceleration_offset
    elif frogpilot_toggles.aggressive_acceleration and v_lead > v_ego:
      distance_factor = np.maximum(lead_distance - (v_lead * t_follow), 1)
      standstill_offset = max(STOP_DISTANCE - (v_ego**COMFORT_BRAKE), 0)
      acceleration_offset = np.clip((v_lead - v_ego) + standstill_offset - COMFORT_BRAKE, 1, distance_factor)
      t_follow /= acceleration_offset

    return jerk, t_follow

  def update_v_cruise(self, carState, controlsState, enabled, liveLocationKalman, modelData, road_curvature, v_cruise, v_ego, frogpilot_toggles):
    gps_check = liveLocationKalman.gpsOK and liveLocationKalman.inputsOK

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

    frogpilotPlan.accelerationJerk = A_CHANGE_COST * (float(self.jerk) if self.lead_one.status else 1)
    frogpilotPlan.accelerationJerkStock = A_CHANGE_COST
    frogpilotPlan.conditionalExperimental = self.cem.experimental_mode
    frogpilotPlan.desiredFollowDistance = self.safe_obstacle_distance - self.stopped_equivalence_factor
    frogpilotPlan.egoJerk = J_EGO_COST * (float(self.jerk) if self.lead_one.status else 1)
    frogpilotPlan.egoJerkStock = J_EGO_COST
    frogpilotPlan.jerk = float(self.jerk)
    frogpilotPlan.safeObstacleDistance = self.safe_obstacle_distance
    frogpilotPlan.safeObstacleDistanceStock = self.safe_obstacle_distance_stock
    frogpilotPlan.stoppedEquivalenceFactor = self.stopped_equivalence_factor
    frogpilotPlan.laneWidthLeft = self.lane_width_left
    frogpilotPlan.laneWidthRight = self.lane_width_right
    frogpilotPlan.minAcceleration = self.min_accel
    frogpilotPlan.maxAcceleration = self.max_accel
    frogpilotPlan.tFollow = float(self.t_follow)
    frogpilotPlan.vCruise = float(self.v_cruise)

    pm.send('frogpilotPlan', frogpilot_plan_send)
