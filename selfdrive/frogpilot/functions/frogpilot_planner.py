import cereal.messaging as messaging
import numpy as np

from openpilot.common.conversions import Conversions as CV
from openpilot.common.numpy_fast import clip
from openpilot.selfdrive.car.interfaces import ACCEL_MIN, ACCEL_MAX
from openpilot.selfdrive.controls.lib.desire_helper import LANE_CHANGE_SPEED_MIN
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import STOP_DISTANCE
from openpilot.selfdrive.controls.lib.longitudinal_planner import A_CRUISE_MIN, get_max_accel

from openpilot.selfdrive.frogpilot.functions.frogpilot_functions import CRUISING_SPEED, FrogPilotFunctions

from openpilot.selfdrive.frogpilot.functions.conditional_experimental_mode import ConditionalExperimentalMode
from openpilot.selfdrive.frogpilot.functions.map_turn_speed_controller import MapTurnSpeedController
from openpilot.selfdrive.frogpilot.functions.speed_limit_controller import SpeedLimitController


class FrogPilotPlanner:
  def __init__(self, CP, params, params_memory):
    self.CP = CP
    self.params_memory = params_memory

    self.fpf = FrogPilotFunctions()

    self.cem = ConditionalExperimentalMode(self.params_memory)
    self.mtsc = MapTurnSpeedController()

    self.override_slc = False

    self.overridden_speed = 0
    self.mtsc_target = 0
    self.road_curvature = 0
    self.slc_target = 0
    self.stop_distance = 0
    self.v_cruise = 0

    self.accel_limits = [A_CRUISE_MIN, get_max_accel(0)]

    self.update_frogpilot_params(params)

  def update(self, carState, controlsState, modelData, mpc, sm, v_cruise, v_ego):
    enabled = controlsState.enabled

    # Use the stock deceleration profile to handle MTSC more precisely
    v_cruise_changed = self.mtsc_target < v_cruise

    # Configure the deceleration profile
    if v_cruise_changed:
      min_accel = A_CRUISE_MIN
    elif self.deceleration_profile == 1:
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
    check_lane_width = self.adjacent_lanes or self.blind_spot_path or self.lane_detection
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

    # Pfeiferj's Map Turn Speed Controller
    if self.map_turn_speed_controller and v_ego > CRUISING_SPEED and enabled:
      mtsc_active = self.mtsc_target < v_cruise
      self.mtsc_target = np.clip(self.mtsc.target_speed(v_ego, carState.aEgo), CRUISING_SPEED, v_cruise)

      # MTSC failsafes
      if self.mtsc_curvature_check and self.road_curvature < 1.0 and not mtsc_active:
        self.mtsc_target = v_cruise
      if v_ego - self.mtsc_limit >= self.mtsc_target:
        self.mtsc_target = v_cruise
    else:
      self.mtsc_target = v_cruise

    # Pfeiferj's Speed Limit Controller
    if self.speed_limit_controller:
      SpeedLimitController.update_speed_limits()
      unconfirmed_slc_target = SpeedLimitController.desired_speed_limit

      # Check if the new speed limit has been confirmed by the user
      if self.speed_limit_confirmation:
        if self.params_memory.get_bool("SLCConfirmed") or self.slc_target == 0:
          self.slc_target = unconfirmed_slc_target
          self.params_memory.put_bool("SLCConfirmed", False)
      else:
        self.slc_target = unconfirmed_slc_target

      # Override SLC upon gas pedal press and reset upon brake/cancel button
      self.override_slc &= self.overridden_speed > self.slc_target
      self.override_slc |= carState.gasPressed and v_ego > self.slc_target > CRUISING_SPEED
      self.override_slc &= enabled and not carState.standstill

      # Use the override speed if SLC is being overridden
      if self.override_slc:
        if self.speed_limit_controller_override == 1:
          # Set the speed limit to the manual set speed
          if carState.gasPressed:
            self.overridden_speed = v_ego + v_ego_diff
          self.overridden_speed = np.clip(self.overridden_speed, self.slc_target, v_cruise + v_cruise_diff)
        elif self.speed_limit_controller_override == 2:
          # Set the speed limit to the max set speed
          self.overridden_speed = v_cruise + v_cruise_diff
      else:
        self.overridden_speed = 0
    else:
      self.slc_target = v_cruise

    targets = [self.mtsc_target, max(self.overridden_speed, self.slc_target) - v_ego_diff]
    filtered_targets = [target for target in targets if target > CRUISING_SPEED]

    return min(filtered_targets) if filtered_targets else v_cruise

  def publish(self, sm, pm, mpc):
    frogpilot_plan_send = messaging.new_message('frogpilotPlan')
    frogpilot_plan_send.valid = sm.all_checks(service_list=['carState', 'controlsState'])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    frogpilotPlan.adjustedCruise = float(self.mtsc_target * (CV.MS_TO_KPH if self.is_metric else CV.MS_TO_MPH))
    frogpilotPlan.conditionalExperimental = self.cem.experimental_mode

    frogpilotPlan.desiredFollowDistance = mpc.safe_obstacle_distance - mpc.stopped_equivalence_factor
    frogpilotPlan.safeObstacleDistance = mpc.safe_obstacle_distance
    frogpilotPlan.safeObstacleDistanceStock = mpc.safe_obstacle_distance_stock
    frogpilotPlan.stoppedEquivalenceFactor = mpc.stopped_equivalence_factor

    frogpilotPlan.laneWidthLeft = self.lane_width_left
    frogpilotPlan.laneWidthRight = self.lane_width_right

    frogpilotPlan.redLight = self.cem.red_light_detected

    frogpilotPlan.slcOverridden = bool(self.override_slc)
    frogpilotPlan.slcOverriddenSpeed = float(self.overridden_speed)
    frogpilotPlan.slcSpeedLimit = float(self.slc_target)
    frogpilotPlan.slcSpeedLimitOffset = SpeedLimitController.offset
    frogpilotPlan.unconfirmedSlcSpeedLimit = SpeedLimitController.desired_speed_limit

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

    nudgeless_lane_change = params.get_bool("NudgelessLaneChange")
    self.lane_detection = nudgeless_lane_change and params.get_bool("LaneDetection")

    longitudinal_tune = params.get_bool("LongitudinalTune")
    self.acceleration_profile = params.get_int("AccelerationProfile") if longitudinal_tune else 0
    self.deceleration_profile = params.get_int("DecelerationProfile") if longitudinal_tune else 0
    self.aggressive_acceleration = longitudinal_tune and params.get_bool("AggressiveAcceleration")
    self.increased_stopping_distance = params.get_int("StoppingDistance") * (1 if self.is_metric else CV.FOOT_TO_METER) if longitudinal_tune else 0
    self.smoother_braking = longitudinal_tune and params.get_bool("SmoothBraking")

    self.map_turn_speed_controller = params.get_bool("MTSCEnabled")
    if self.map_turn_speed_controller:
      self.mtsc_curvature_check = params.get_bool("MTSCCurvatureCheck")
      self.mtsc_limit = params.get_float("MTSCLimit") * (CV.KPH_TO_MS if self.is_metric else CV.MPH_TO_MS)
      self.params_memory.put_float("MapTargetLatA", 2 * (params.get_int("MTSCAggressiveness") / 100))

    self.speed_limit_controller = params.get_bool("SpeedLimitController")
    if self.speed_limit_controller:
      self.speed_limit_confirmation = params.get_bool("SLCConfirmation")
      self.speed_limit_controller_override = params.get_int("SLCOverride")
