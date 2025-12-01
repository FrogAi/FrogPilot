#!/usr/bin/env python3
import numpy as np

from cereal import log
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import COMFORT_BRAKE, STOP_DISTANCE

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables

class FrogPilotFollowing:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.following_lead = False

    self.acceleration_jerk = 0
    self.base_t_follow = 0
    self.danger_jerk = 0
    self.speed_jerk = 0
    self.t_follow = 0

  def update(self, v_ego, sm, frogpilot_toggles):
    personality = sm["selfdriveState"].personality

    if personality == log.LongitudinalPersonality.aggressive:
      if sm["carState"].aEgo >= 0:
        self.acceleration_jerk = frogpilot_toggles.aggressive_jerk_acceleration
        self.speed_jerk = frogpilot_toggles.aggressive_jerk_speed
      else:
        self.acceleration_jerk = frogpilot_toggles.aggressive_jerk_deceleration
        self.speed_jerk = frogpilot_toggles.aggressive_jerk_speed_decrease

      self.danger_jerk = frogpilot_toggles.aggressive_jerk_danger
      self.t_follow = frogpilot_toggles.aggressive_follow
    elif personality == log.LongitudinalPersonality.standard:
      if sm["carState"].aEgo >= 0:
        self.acceleration_jerk = frogpilot_toggles.standard_jerk_acceleration
        self.speed_jerk = frogpilot_toggles.standard_jerk_speed
      else:
        self.acceleration_jerk = frogpilot_toggles.standard_jerk_deceleration
        self.speed_jerk = frogpilot_toggles.standard_jerk_speed_decrease

      self.danger_jerk = frogpilot_toggles.standard_jerk_danger
      self.t_follow = frogpilot_toggles.standard_follow
    elif personality == log.LongitudinalPersonality.relaxed:
      if sm["carState"].aEgo >= 0:
        self.acceleration_jerk = frogpilot_toggles.relaxed_jerk_acceleration
        self.speed_jerk = frogpilot_toggles.relaxed_jerk_speed
      else:
        self.acceleration_jerk = frogpilot_toggles.relaxed_jerk_deceleration
        self.speed_jerk = frogpilot_toggles.relaxed_jerk_speed_decrease

      self.danger_jerk = frogpilot_toggles.relaxed_jerk_danger
      self.t_follow = frogpilot_toggles.relaxed_follow

    self.following_lead = self.frogpilot_planner.lead_one.status
    self.following_lead &= self.frogpilot_planner.lead_one.dRel < (self.t_follow * 2) * v_ego

    self.base_t_follow = self.t_follow

    older_model = not frogpilot_utilities.predicts_lead_trajectories(frogpilot_toggles.model_path)
    if older_model and frogpilot_toggles.has_radar and frogpilot_toggles.human_following:
      self.update_follow_values(self.frogpilot_planner.lead_one.dRel, v_ego, self.frogpilot_planner.lead_one.vLead)

  def update_follow_values(self, lead_distance, v_ego, v_lead):
    # Offset by FrogAi for FrogPilot for a more natural approach to a faster lead
    if v_lead > v_ego:
      distance_factor = max(lead_distance - (v_ego * self.t_follow), 1)
      accelerating_offset = float(np.clip(STOP_DISTANCE - v_ego, 1, distance_factor))

      self.acceleration_jerk /= accelerating_offset
      self.speed_jerk /= accelerating_offset
      self.t_follow /= accelerating_offset

    # Offset by FrogAi for FrogPilot for a more natural approach to a slower lead
    if v_lead < v_ego:
      distance_factor = max(lead_distance - (v_lead * self.t_follow), 1)
      braking_offset = float(np.clip(min(v_ego - v_lead, v_lead) - COMFORT_BRAKE, 1, distance_factor))

      if lead_distance >= 100:
        far_lead_offset = max(lead_distance - (v_ego * self.t_follow) - STOP_DISTANCE, 0)
        braking_offset += far_lead_offset

      self.t_follow /= braking_offset
