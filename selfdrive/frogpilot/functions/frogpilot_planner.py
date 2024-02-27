import cereal.messaging as messaging

from openpilot.common.conversions import Conversions as CV
from openpilot.selfdrive.car.interfaces import ACCEL_MIN, ACCEL_MAX
from openpilot.selfdrive.controls.lib.desire_helper import LANE_CHANGE_SPEED_MIN
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import STOP_DISTANCE
from openpilot.selfdrive.controls.lib.longitudinal_planner import A_CRUISE_MIN, get_max_accel

from openpilot.selfdrive.frogpilot.functions.frogpilot_functions import CRUISING_SPEED, FrogPilotFunctions

from openpilot.selfdrive.frogpilot.functions.conditional_experimental_mode import ConditionalExperimentalMode

class FrogPilotPlanner:
  def __init__(self, CP, params, params_memory):
    self.CP = CP
    self.params_memory = params_memory

    self.fpf = FrogPilotFunctions()

    self.cem = ConditionalExperimentalMode(self.params_memory)

    self.road_curvature = 0
    self.stop_distance = 0
    self.v_cruise = 0

    self.accel_limits = [A_CRUISE_MIN, get_max_accel(0)]

    self.update_frogpilot_params(params)

  def update(self, carState, controlsState, modelData, mpc, sm, v_cruise, v_ego):
    enabled = controlsState.enabled

    # Configure the deceleration profile
    if self.deceleration_profile == 1:
      min_accel = self.fpf.get_min_accel_eco(v_ego)
    elif self.deceleration_profile == 2:
      min_accel = self.fpf.get_min_accel_sport(v_ego)
    elif mpc.mode == 'acc':
      min_accel = A_CRUISE_MIN
    else:
      min_accel = ACCEL_MIN

    # Configure the acceleration profile
    if self.acceleration_profile == 1:
      max_accel = self.fpf.get_max_accel_eco(v_ego)
    elif self.acceleration_profile in (2, 3):
      max_accel = self.fpf.get_max_accel_sport(v_ego)
    elif mpc.mode == 'acc':
      max_accel = get_max_accel(v_ego)
    else:
      max_accel = ACCEL_MAX

    self.accel_limits = [min_accel, max_accel]

    # Update Conditional Experimental Mode
    if self.conditional_experimental_mode and self.CP.openpilotLongitudinalControl or self.green_light_alert and carState.standstill:
      self.cem.update(carState, enabled, sm['frogpilotNavigation'], modelData, sm['radarState'], self.road_curvature, self.stop_distance, mpc.t_follow, v_ego)

    # Update the current lane widths
    check_lane_width = self.adjacent_lanes or self.blind_spot_path
    if check_lane_width and v_ego >= LANE_CHANGE_SPEED_MIN:
      self.lane_width_left = float(self.fpf.calculate_lane_width(modelData.laneLines[0], modelData.laneLines[1], modelData.roadEdges[0]))
      self.lane_width_right = float(self.fpf.calculate_lane_width(modelData.laneLines[3], modelData.laneLines[2], modelData.roadEdges[1]))
    else:
      self.lane_width_left = 0
      self.lane_width_right = 0

    # Update the current road curvature
    self.road_curvature = self.fpf.road_curvature(modelData, v_ego)

    # Update the desired stopping distance
    self.stop_distance = STOP_DISTANCE

    # Update the max allowed speed
    self.v_cruise = self.update_v_cruise(carState, controlsState, enabled, modelData, v_cruise, v_ego)

  def update_v_cruise(self, carState, controlsState, enabled, modelData, v_cruise, v_ego):
    # Offsets to adjust the max speed to match the cluster
    v_ego_cluster = max(carState.vEgoCluster, v_ego)
    v_ego_diff = v_ego_cluster - v_ego

    v_cruise_cluster = max(controlsState.vCruiseCluster, controlsState.vCruise) * CV.KPH_TO_MS
    v_cruise_diff = v_cruise_cluster - v_cruise

    targets = []
    filtered_targets = [target for target in targets if target > CRUISING_SPEED]

    return min(filtered_targets) if filtered_targets else v_cruise

  def publish(self, sm, pm, mpc):
    frogpilot_plan_send = messaging.new_message('frogpilotPlan')
    frogpilot_plan_send.valid = sm.all_checks(service_list=['carState', 'controlsState'])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    frogpilotPlan.conditionalExperimental = self.cem.experimental_mode

    frogpilotPlan.desiredFollowDistance = mpc.safe_obstacle_distance - mpc.stopped_equivalence_factor
    frogpilotPlan.safeObstacleDistance = mpc.safe_obstacle_distance
    frogpilotPlan.safeObstacleDistanceStock = mpc.safe_obstacle_distance_stock
    frogpilotPlan.stoppedEquivalenceFactor = mpc.stopped_equivalence_factor

    frogpilotPlan.laneWidthLeft = self.lane_width_left
    frogpilotPlan.laneWidthRight = self.lane_width_right

    frogpilotPlan.redLight = self.cem.red_light_detected

    pm.send('frogpilotPlan', frogpilot_plan_send)

  def update_frogpilot_params(self, params):
    self.is_metric = params.get_bool("IsMetric")

    self.conditional_experimental_mode = params.get_bool("ConditionalExperimental")
    if self.conditional_experimental_mode:
      self.cem.update_frogpilot_params(self.is_metric, params)
      params.put_bool("ExperimentalMode", True)

    custom_alerts = params.get_bool("CustomAlerts")
    self.green_light_alert = custom_alerts and params.get_bool("GreenLightAlert")
    if self.green_light_alert and not self.conditional_experimental_mode:
      self.cem.update_frogpilot_params(self.is_metric, params)

    self.custom_personalities = params.get_bool("CustomPersonalities")
    self.aggressive_follow = params.get_float("AggressiveFollow")
    self.standard_follow = params.get_float("StandardFollow")
    self.relaxed_follow = params.get_float("RelaxedFollow")
    self.aggressive_jerk = params.get_float("AggressiveJerk")
    self.standard_jerk = params.get_float("StandardJerk")
    self.relaxed_jerk = params.get_float("RelaxedJerk")

    custom_ui = params.get_bool("CustomUI")
    self.adjacent_lanes = custom_ui and params.get_bool("AdjacentPath")
    self.blind_spot_path = custom_ui and params.get_bool("BlindSpotPath")

    longitudinal_tune = params.get_bool("LongitudinalTune")
    self.acceleration_profile = params.get_int("AccelerationProfile") if longitudinal_tune else 0
    self.deceleration_profile = params.get_int("DecelerationProfile") if longitudinal_tune else 0
    self.aggressive_acceleration = longitudinal_tune and params.get_bool("AggressiveAcceleration")
    self.increased_stopping_distance = params.get_int("StoppingDistance") * (1 if self.is_metric else CV.FOOT_TO_METER) if longitudinal_tune else 0
