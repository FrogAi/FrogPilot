#!/usr/bin/env python3
# Twilsonco's Lateral Neural Network Feedforward
from collections import deque
from difflib import SequenceMatcher
from typing import NamedTuple

import json
import math
import numpy as np
import os

from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.numpy_fast import interp
from openpilot.selfdrive.controls.lib.drive_helpers import CONTROL_N
from openpilot.selfdrive.modeld.constants import ModelConstants

from openpilot.frogpilot.common.frogpilot_variables import NNFF_MODELS_PATH, get_nnff_model_files, params

# At higher speeds (25+mph) we can assume:
# Lateral acceleration achieved by a specific car correlates to
# torque applied to the steering rack. It does not correlate to
# wheel slip, or to speed.

# This controller applies torque to achieve desired lateral
# accelerations. To compensate for the low speed effects we
# use a LOW_SPEED_FACTOR in the error. Additionally, there is
# friction in the steering wheel that needs to be overcome to
# move it at all, this is compensated for too.

# dict used to rename activation functions whose names aren't valid python identifiers
ACTIVATION_FUNCTION_NAMES = {'σ': 'sigmoid'}

LOW_SPEED_X = [0, 10, 20, 30]
LOW_SPEED_Y = [15, 13, 10, 5]
LOW_SPEED_Y_NN = [12, 3, 1, 0]

LAT_PLAN_MIN_IDX = 5

class FluxModel:
  def __init__(self, params_file):
    with open(params_file, "r") as f:
      params = json.load(f)

    self.input_size = params["input_size"]
    self.output_size = params["output_size"]

    self.input_mean = np.array(params["input_mean"], dtype=np.float32).T
    self.input_std = np.array(params["input_std"], dtype=np.float32).T

    self.layers = []
    for layer_params in params["layers"]:
      bias_array = np.array(layer_params[next(key for key in layer_params.keys() if key.endswith('_b'))], dtype=np.float32).T
      weight_array = np.array(layer_params[next(key for key in layer_params.keys() if key.endswith('_W'))], dtype=np.float32).T

      activation = layer_params["activation"]
      for name, replacement in ACTIVATION_FUNCTION_NAMES.items():
        activation = activation.replace(name, replacement)

      self.layers.append((weight_array, bias_array, activation))

    self.validate_layers()
    self.check_for_friction_override()

  # Begin activation functions.
  # These are called by name using the keys in the model json file
  @staticmethod
  def identity(x):
    return x

  @staticmethod
  def sigmoid(x):
    return 1 / (1 + np.exp(-x))
  # End activation functions

  def check_for_friction_override(self):
    self.friction_override = self.evaluate([10.0, 0.0, 0.2]) < 0.1

  def evaluate(self, input_array):
    if len(input_array) != self.input_size:
      # If the input is length 2-4, then it's a simplified evaluation.
      # In that case, need to add on zeros to fill out the input array to match the correct length.
      if len(input_array) >= 2:
        input_array += [0] * (self.input_size - len(input_array))
      else:
        raise ValueError(f"Input length {len(input_array)} must be 2 or greater")

    # Rescale the input array using the input_mean and input_std
    output = self.forward((np.array(input_array, dtype=np.float32) - self.input_mean) / self.input_std)
    return float(output[0, 0])

  def forward(self, x):
    for weights, bias, activation in self.layers:
      x = getattr(self, activation)(x @ weights + bias)
    return x

  def validate_layers(self):
    for _, _, activation in self.layers:
      if not hasattr(self, activation):
        raise ValueError(f"Unknown activation function: {activation}")

def get_lookahead_value(future_values, current_value):
  if not future_values:
    return current_value

  if any(sign(value) != sign(current_value) for value in future_values):
    return 0.0

  return min(future_values + [current_value], key=abs)

def get_nn_model(car, eps_firmware) -> FluxModel | None:
  model_path = get_nn_model_path(car, eps_firmware)
  if model_path:
    params.put("NNFFModelName", car.replace("_", " "))
    return FluxModel(model_path)
  return None

def get_nn_model_path(car, eps_firmware) -> str | None:
  def best_model_path(query):
    candidates = get_nnff_model_files()
    if not candidates:
      return None, 0.0

    best = max(candidates, key=lambda model: similarity(model, query))
    return os.path.join(NNFF_MODELS_PATH, f"{best}.json"), similarity(best, query)

  def find_valid_model(*queries):
    for query in queries:
      path, score = best_model_path(query)
      if path and car in path and score >= 0.9:
        return path
    return None

  query1 = f"{car} {eps_firmware}" if len(eps_firmware) > 3 else car
  return find_valid_model(query1, car)

def get_predicted_lateral_jerk(lat_accels, t_diffs):
  # compute finite difference between subsequent model_data.acceleration.y values
  # this is just two calls of np.diff followed by an element-wise division
  return (np.diff(lat_accels) / t_diffs).tolist()

# At a given roll, if pitch magnitude increases, the
# gravitational acceleration component starts pointing
# in the longitudinal direction, decreasing the lateral
# acceleration component. Here we do the same thing
# to the roll value itself, then passed to nnff.
def roll_pitch_adjust(roll, pitch):
  return roll * math.cos(pitch)

def sign(x):
  return 1.0 if x > 0 else -1.0 if x < 0 else 0.0

def similarity(s1: str, s2: str) -> float:
  return SequenceMatcher(None, s1, s2).ratio()

class LatControlInputs(NamedTuple):
  lateral_acceleration: float
  roll_compensation: float
  vego: float
  aego: float

class NeuralNetworkFeedforward:
  def __init__(self, CP, LatControlTorque):
    self.lat_control_torque = LatControlTorque

    self.lat_torque_nn_model = get_nn_model(CP.carFingerprint, str(next((fw.fwVersion for fw in CP.carFw if fw.ecu == "eps"), "")).replace("\\", ""))

    self.torque_from_lateral_accel = self.lat_control_torque.torque_from_lateral_accel

    self.use_steering_angle = self.lat_control_torque.torque_params.useSteeringAngle

    # Instantaneous lateral jerk changes very rapidly, making it not useful on its own,
    # however, we can "look ahead" to the future planned lateral jerk in order to gauge
    # whether the current desired lateral jerk will persist into the future, i.e.
    # whether it's "deliberate" or not. This lets us simply ignore short-lived jerk.
    # Note that LAT_PLAN_MIN_IDX is defined above and is used in order to prevent
    # using a "future" value that is actually planned to occur before the "current" desired
    # value, which is offset by the steerActuatorDelay.
    self.friction_look_ahead_bp = [9.0, 30.0]  # corresponding speeds in m/s in [0, ~40] in 1.0 increments
    self.friction_look_ahead_v = [1.4, 2.0]  # how many seconds in the future to look ahead in [0, ~2.1] in 0.1 increments

    # Scaling the lateral acceleration "friction response" could be helpful for some.
    # Increase for a stronger response, decrease for a weaker response.
    self.lat_accel_friction_factor = 0.7  # in [0, 3], in 0.05 increments. 3 is arbitrary safety limit
    self.lat_jerk_friction_factor = 0.4

    # precompute time differences between ModelConstants.T_IDXS
    self.lateral_delay = CP.steerActuatorDelay
    self.t_diffs = np.diff(ModelConstants.T_IDXS)

    self.pitch = FirstOrderFilter(0.0, 0.5, 0.01)

    # NN model takes current v_ego, lateral_accel, lat accel/jerk error, roll, and past/future/planned data
    # of lat accel and roll
    # Past value is computed using previous desired lat accel and observed roll
    self.nn_friction_override = self.lat_torque_nn_model.friction_override if self.lat_torque_nn_model else False

    # setup future time offsets
    self.future_times = [0.3, 0.6, 1.0, 1.5]  # seconds in the future
    self.nn_future_times = [time + self.lateral_delay for time in self.future_times]

    # setup past time offsets
    self.past_times = [-0.3, -0.2, -0.1]
    history_check_frames = [int(abs(time) * 100) for time in self.past_times]
    self.history_frame_offsets = [history_check_frames[0] - time for time in history_check_frames]
    self.lateral_accel_desired_deque = deque(maxlen=history_check_frames[0])
    self.past_future_len = len(self.past_times) + len(self.nn_future_times)
    self.roll_deque = deque(maxlen=history_check_frames[0])

  def update_live_delay(self, lateral_delay):
    self.lateral_delay = lateral_delay

    self.nn_future_times = [time + self.lateral_delay for time in self.future_times]
    self.past_future_len = len(self.past_times) + len(self.nn_future_times)

  def compute_nnff(self, CS, VM, actual_lateral_accel, desired_lateral_accel, gravity_adjusted_lateral_accel, lateral_accel_deadzone, llk, measurement, model_data, params, pid_log, roll_compensation, setpoint, frogpilot_toggles):
    if self.use_steering_angle:
      actual_curvature_rate = -VM.calc_curvature(math.radians(CS.steeringRateDeg), CS.vEgo, 0.0)
      actual_lateral_jerk = actual_curvature_rate * CS.vEgo ** 2
    else:
      actual_lateral_jerk = 0.0

    model_good = model_data is not None and len(model_data.orientation.x) >= CONTROL_N

    if model_good:
      # prepare "look-ahead" desired lateral jerk
      lookahead = interp(CS.vEgo, self.friction_look_ahead_bp, self.friction_look_ahead_v)
      friction_upper_idx = next((idxs for idxs, value in enumerate(ModelConstants.T_IDXS) if value > lookahead), 16)

      predicted_lateral_jerk = get_predicted_lateral_jerk(model_data.acceleration.y, self.t_diffs)
      desired_lateral_jerk = (interp(self.lateral_delay, ModelConstants.T_IDXS, model_data.acceleration.y) - desired_lateral_accel) / self.lateral_delay

      lookahead_lateral_jerk = get_lookahead_value(predicted_lateral_jerk[LAT_PLAN_MIN_IDX:friction_upper_idx], desired_lateral_jerk)

      if not self.use_steering_angle or lookahead_lateral_jerk == 0.0:
        lookahead_lateral_jerk = 0.0
        actual_lateral_jerk = 0.0
        self.lat_accel_friction_factor = 1.0

      lateral_jerk_setpoint = self.lat_jerk_friction_factor * lookahead_lateral_jerk
      lateral_jerk_measurement = self.lat_jerk_friction_factor * actual_lateral_jerk
    else:
      lateral_jerk_setpoint = 0
      lateral_jerk_measurement = 0
      lookahead_lateral_jerk = 0

    if self.lat_control_torque.nnff_loaded and model_good and frogpilot_toggles.nnff:
      # update past data
      pitch = 0
      roll = params.roll
      if len(llk.calibratedOrientationNED.value) > 1:
        pitch = self.pitch.update(llk.calibratedOrientationNED.value[1])
        roll = roll_pitch_adjust(roll, pitch)

      self.roll_deque.append(roll)
      self.lateral_accel_desired_deque.append(desired_lateral_accel)

      # prepare past and future values
      # adjust future times to account for longitudinal acceleration
      adjusted_future_times = [time + 0.5 * CS.aEgo * (time / max(CS.vEgo, 1.0)) for time in self.nn_future_times]
      past_rolls = [self.roll_deque[min(len(self.roll_deque)-1, offset)] for offset in self.history_frame_offsets]
      future_rolls = [roll_pitch_adjust(interp(time, ModelConstants.T_IDXS, model_data.orientation.x) + roll, interp(time, ModelConstants.T_IDXS, model_data.orientation.y) + pitch) for time in adjusted_future_times]

      past_lateral_accels_desired = [self.lateral_accel_desired_deque[min(len(self.lateral_accel_desired_deque)-1, offset)] for offset in self.history_frame_offsets]
      future_lateral_accels = [interp(time, ModelConstants.T_IDXS[:CONTROL_N], model_data.acceleration.y) for time in adjusted_future_times]

      base_input = [CS.vEgo, roll]
      nnff_common = past_rolls + future_rolls

      # compute NNFF error response
      nnff_setpoint_input = base_input[:1] + [setpoint, lateral_jerk_setpoint] + base_input[1:] + [setpoint] * self.past_future_len + nnff_common
      nnff_measurement_input = base_input[:1] + [measurement, lateral_jerk_measurement] + base_input[1:] + [measurement] * self.past_future_len + nnff_common

      torque_from_setpoint = self.lat_torque_nn_model.evaluate(nnff_setpoint_input)
      torque_from_measurement = self.lat_torque_nn_model.evaluate(nnff_measurement_input)

      pid_log.error = torque_from_setpoint - torque_from_measurement

      error_blend = interp(abs(desired_lateral_accel), [1.0, 2.0], [0.0, 1.0])
      if error_blend > 0.0:  # blend in stronger error response when in high lat accel
        torque_from_error = self.lat_torque_nn_model.evaluate([CS.vEgo, setpoint - measurement, lateral_jerk_setpoint - lateral_jerk_measurement, 0.0])
        if sign(pid_log.error) == sign(torque_from_error) and abs(pid_log.error) < abs(torque_from_error):
          pid_log.error = pid_log.error * (1.0 - error_blend) + torque_from_error * error_blend

      # compute feedforward (same as nn setpoint output)
      error = setpoint - measurement
      friction_input = self.lat_accel_friction_factor * error + self.lat_jerk_friction_factor * lookahead_lateral_jerk
      nn_input = [CS.vEgo, desired_lateral_accel, friction_input, roll] + past_lateral_accels_desired + future_lateral_accels + nnff_common
      ff = self.lat_torque_nn_model.evaluate(nn_input)

      # apply friction override for cars with low NN friction response
      if self.nn_friction_override:
        pid_log.error += self.torque_from_lateral_accel(0.0, self.lat_control_torque.torque_params)
    else:
      torque_from_measurement = self.torque_from_lateral_accel(measurement, self.lat_control_torque.torque_params)
      torque_from_setpoint = self.torque_from_lateral_accel(setpoint, self.lat_control_torque.torque_params)

      pid_log.error = float(torque_from_setpoint - torque_from_measurement)

      error = desired_lateral_accel - actual_lateral_accel
      friction_input = self.lat_accel_friction_factor * error + self.lat_jerk_friction_factor * lookahead_lateral_jerk
      ff = self.torque_from_lateral_accel(gravity_adjusted_lateral_accel, self.lat_control_torque.torque_params)

    return pid_log, ff
