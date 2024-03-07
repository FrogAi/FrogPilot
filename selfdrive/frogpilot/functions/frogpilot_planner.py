import cereal.messaging as messaging

from openpilot.common.conversions import Conversions as CV
from openpilot.selfdrive.car.interfaces import ACCEL_MIN, ACCEL_MAX
from openpilot.selfdrive.controls.lib.longitudinal_planner import A_CRUISE_MIN, get_max_accel

from openpilot.selfdrive.frogpilot.functions.frogpilot_functions import CRUISING_SPEED, FrogPilotFunctions

class FrogPilotPlanner:
  def __init__(self, CP, params, params_memory):
    self.CP = CP
    self.params_memory = params_memory

    self.fpf = FrogPilotFunctions()

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

    return min(filtered_targets + [v_cruise]) if filtered_targets else v_cruise

  def publish(self, sm, pm, mpc):
    frogpilot_plan_send = messaging.new_message('frogpilotPlan')
    frogpilot_plan_send.valid = sm.all_checks(service_list=['carState', 'controlsState'])
    frogpilotPlan = frogpilot_plan_send.frogpilotPlan

    pm.send('frogpilotPlan', frogpilot_plan_send)

  def update_frogpilot_params(self, params):
    self.is_metric = params.get_bool("IsMetric")

    custom_ui = params.get_bool("CustomUI")

    longitudinal_tune = params.get_bool("LongitudinalTune")
    self.acceleration_profile = params.get_int("AccelerationProfile") if longitudinal_tune else 0
    self.deceleration_profile = params.get_int("DecelerationProfile") if longitudinal_tune else 0
