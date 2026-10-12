import ctypes
import json
import os
import pickle
import struct
import sys
from pathlib import Path

import numpy as np
from tinygrad.dtype import dtypes
from tinygrad.tensor import Tensor

from cereal import log
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.controls.lib.drive_helpers import get_accel_from_plan, get_curvature_from_plan
from openpilot.selfdrive.modeld.constants import Meta, ModelConstants, Plan
from openpilot.selfdrive.modeld.models.commonmodel_pyx import DrivingModelFrame
from openpilot.selfdrive.modeld.parse_model_outputs import Parser
from openpilot.selfdrive.modeld.runners.tinygrad_helpers import qcom_tensor_from_opencl_address
from openpilot.system.hardware import TICI

LEGACY_META_GROUP_SIZES = {48: 7, 53: 8}
UNGATED_GAS_PRESS_PROBABILITY = 1.

MODEL_RUNNERS_PATH = Path(__file__).parent / "model_runners"


class ThneedModel:
  def __init__(self, model_path, input_shapes):
    with open(model_path / "supercombo.thneed", "rb") as model_file:
      header_size = struct.unpack("<I", model_file.read(4))[0]
      header = json.loads(model_file.read(header_size).decode("latin-1"))

    self.input_names = [item["name"] for item in header["inputs"]]
    self.input_dtypes = {}

    for item in header["inputs"]:
      if item["size"] == np.prod(input_shapes[item["name"]]):
        self.input_dtypes[item["name"]] = np.uint8
      else:
        self.input_dtypes[item["name"]] = np.float32

    self.float_pointer = ctypes.POINTER(ctypes.c_float)
    self.library = ctypes.CDLL(str(MODEL_RUNNERS_PATH / "libthneed_runner.so"))
    self.library.thneed_model_create.argtypes = [ctypes.c_char_p]
    self.library.thneed_model_create.restype = ctypes.c_void_p
    self.library.thneed_model_run.argtypes = [ctypes.c_void_p, ctypes.POINTER(self.float_pointer), self.float_pointer]
    self.library.thneed_model_run.restype = None
    self.model = self.library.thneed_model_create(os.fsencode(model_path / "supercombo.thneed"))
    self.output = np.zeros(header["outputs"][0]["size"] // np.dtype(np.float32).itemsize, dtype=np.float32)

  def run(self, inputs):
    pointers = [inputs[name].ctypes.data_as(self.float_pointer) for name in self.input_names]
    input_pointers = (self.float_pointer * len(pointers))(*pointers)
    self.library.thneed_model_run(self.model, input_pointers, self.output.ctypes.data_as(self.float_pointer))
    return self.output


class LegacyModelState:
  def __init__(self, context, model_path):
    self.combined = (model_path / "supercombo_metadata.pkl").is_file()
    self.native_model = (model_path / "supercombo.thneed").is_file()

    if self.combined:
      with open(model_path / "supercombo_metadata.pkl", "rb") as metadata_file:
        metadata = pickle.load(metadata_file)

      self.vision_input_shapes = {name: shape for name, shape in metadata["input_shapes"].items() if "img" in name}
      policy_input_shapes = {name: shape for name, shape in metadata["input_shapes"].items() if "img" not in name}
      self.policy_output_slices = metadata["output_slices"]
      self.curvature_source = metadata.get("curvature_source", "model")
      self.lateral_delay_source = "vehicle"

      if self.native_model:
        self.policy_run = ThneedModel(model_path, metadata["input_shapes"])
      else:
        with open(model_path / "supercombo_tinygrad.pkl", "rb") as model_file:
          self.policy_run = pickle.load(model_file)
    else:
      with open(model_path / "driving_vision_metadata.pkl", "rb") as metadata_file:
        vision_metadata = pickle.load(metadata_file)

      with open(model_path / "driving_policy_metadata.pkl", "rb") as metadata_file:
        policy_metadata = pickle.load(metadata_file)

      self.vision_input_shapes = vision_metadata["input_shapes"]
      self.vision_output_slices = vision_metadata["output_slices"]
      policy_input_shapes = policy_metadata["input_shapes"]
      self.policy_output_slices = policy_metadata["output_slices"]
      self.curvature_source = policy_metadata["curvature_source"]
      self.lateral_delay_source = policy_metadata["lateral_delay_source"]

      with open(model_path / "driving_vision_tinygrad.pkl", "rb") as model_file:
        self.vision_run = pickle.load(model_file)

      with open(model_path / "driving_policy_tinygrad.pkl", "rb") as model_file:
        self.policy_run = pickle.load(model_file)

    desire_slots = policy_input_shapes["desire"][1]
    if desire_slots == 25:
      self.history_stride = ModelConstants.MODEL_RUN_FREQ // ModelConstants.MODEL_CONTEXT_FREQ
    else:
      self.history_stride = 1

    history_length = desire_slots * self.history_stride

    if self.combined:
      newest_feature = -self.history_stride
      self.feeds_back_curvature = self.history_stride == 1
    else:
      newest_feature = -1
      self.feeds_back_curvature = self.curvature_source != "plan"

    self.vision_input_names = list(self.vision_input_shapes)
    self.frames = {name: DrivingModelFrame(context, self.history_stride) for name in self.vision_input_names}
    self.vision_inputs = {}
    self.numpy_inputs = {name: np.zeros(shape, dtype=np.float32) for name, shape in policy_input_shapes.items()}
    self.policy_inputs = {name: Tensor(value, device="NPY").realize() for name, value in self.numpy_inputs.items()}

    if self.history_stride == 1:
      self.parser = Parser(exp=np.exp)
    else:
      self.parser = Parser()

    self.prev_desire = np.zeros(ModelConstants.DESIRE_LEN, dtype=np.float32)
    self.desire_history = np.zeros((1, history_length, ModelConstants.DESIRE_LEN), dtype=np.float32)
    self.feature_history = np.zeros((1, history_length, ModelConstants.FEATURE_LEN), dtype=np.float32)
    self.feature_indices = np.arange(newest_feature, -history_length, -self.history_stride)[::-1]
    self.curvature_history = np.zeros((1, history_length, ModelConstants.DESIRED_CURV_WIDTH), dtype=np.float32)
    self.curvature_indices = np.arange(-1, -history_length - 1, -self.history_stride)[::-1]

  def run(self, bufs, transforms, inputs, prepare_only):
    inputs["desire_pulse"][0] = 0
    self.desire_history[:, :-1] = self.desire_history[:, 1:]
    self.desire_history[0, -1] = np.where(inputs["desire_pulse"] - self.prev_desire > .99, inputs["desire_pulse"], 0)
    self.prev_desire[:] = inputs["desire_pulse"]

    self.numpy_inputs["desire"][:] = self.desire_history.reshape((1, -1, self.history_stride, ModelConstants.DESIRE_LEN)).max(axis=2)
    self.numpy_inputs["traffic_convention"][:] = inputs["traffic_convention"]

    for name in ["lateral_control_params", "radar_tracks"]:
      if name in self.numpy_inputs:
        self.numpy_inputs[name][:] = inputs[name]

    images = {name: self.frames[name].prepare(bufs[name], transforms[name].flatten()) for name in self.vision_input_names}

    if prepare_only:
      return None

    for name, image in images.items():
      if self.native_model:
        pixels = self.frames[name].buffer_from_cl(image).reshape(self.vision_input_shapes[name])
        self.vision_inputs[name] = pixels.astype(self.policy_run.input_dtypes[name], copy=False)
      elif TICI and "USBGPU" not in os.environ:
        if name not in self.vision_inputs:
          self.vision_inputs[name] = qcom_tensor_from_opencl_address(image.mem_address, self.vision_input_shapes[name], dtype=dtypes.uint8)
      else:
        pixels = self.frames[name].buffer_from_cl(image).reshape(self.vision_input_shapes[name])
        self.vision_inputs[name] = Tensor(pixels, dtype=dtypes.uint8).realize()

    if self.native_model:
      raw_outputs = [self.policy_run.run({**self.numpy_inputs, **self.vision_inputs})]
      outputs = self.slice_outputs(raw_outputs[0], self.policy_output_slices)
    elif self.combined:
      raw_outputs = [self.policy_run(**self.policy_inputs, **self.vision_inputs).contiguous().realize().uop.base.buffer.numpy()]
      outputs = self.slice_outputs(raw_outputs[0], self.policy_output_slices)
    else:
      vision_output = self.vision_run(**self.vision_inputs).contiguous().realize().uop.base.buffer.numpy()
      outputs = self.slice_outputs(vision_output, self.vision_output_slices)
      self.update_features(outputs["hidden_state"])

      policy_output = self.policy_run(**self.policy_inputs).contiguous().realize().uop.base.buffer.numpy()
      outputs.update(self.slice_outputs(policy_output, self.policy_output_slices))
      raw_outputs = [vision_output, policy_output]

    if os.getenv("SEND_RAW_PRED"):
      outputs["raw_pred"] = np.concatenate([raw_output.copy() for raw_output in raw_outputs])

    outputs = self.parser.parse_outputs(outputs)

    if "desired_curvature" in outputs:
      self.parser.parse_mdn("desired_curvature", outputs, in_N=0, out_N=0, out_shape=(ModelConstants.DESIRED_CURV_WIDTH,))

    if outputs["meta"].shape[1] in LEGACY_META_GROUP_SIZES:
      outputs["meta"] = convert_legacy_meta(outputs["meta"])

    if self.combined:
      self.update_features(outputs["hidden_state"])

    if self.feeds_back_curvature:
      self.curvature_history[:, :-1] = self.curvature_history[:, 1:]
      self.curvature_history[0, -1] = outputs["desired_curvature"][0]
      self.numpy_inputs["prev_desired_curv"][:] = self.curvature_history[:, self.curvature_indices]

    return outputs

  def slice_outputs(self, model_outputs, output_slices):
    return {name: model_outputs[np.newaxis, output_slice] for name, output_slice in output_slices.items()}

  def update_action(self, action, model_output, prev_action, steer_actuator_delay, longitudinal_actuator_delay, v_ego, min_lat_control_speed):
    if self.curvature_source == "plan":
      return action

    plan = model_output["plan"][0]
    desired_accel = action.desiredAcceleration
    should_stop = action.shouldStop

    if self.curvature_source == "legacy_plan":
      desired_curvature = get_curvature_from_plan(plan[:, Plan.T_FROM_CURRENT_EULER][:, 2],
                                                  plan[:, Plan.ORIENTATION_RATE][:, 2],
                                                  ModelConstants.T_IDXS,
                                                  v_ego,
                                                  steer_actuator_delay + .2)
    elif self.curvature_source == "model_hold" and v_ego <= min_lat_control_speed:
      desired_curvature = prev_action.desiredCurvature
    else:
      desired_curvature = model_output["desired_curvature"][0, 0]

    if not self.combined:
      desired_accel, should_stop = get_accel_from_plan(plan[:, Plan.VELOCITY][:, 0],
                                                       plan[:, Plan.ACCELERATION][:, 0],
                                                       ModelConstants.T_IDXS,
                                                       action_t=longitudinal_actuator_delay + DT_MDL)

    return log.ModelDataV2.Action(desiredCurvature=float(desired_curvature),
                                  desiredAcceleration=float(desired_accel),
                                  shouldStop=bool(should_stop))

  def update_features(self, hidden_state):
    self.feature_history[:, :-1] = self.feature_history[:, 1:]
    self.feature_history[0, -1] = hidden_state[0]
    self.numpy_inputs["features_buffer"][:] = self.feature_history[:, self.feature_indices]

  def update_inputs(self, inputs, sm, steer_actuator_delay, lat_delay):
    if "lateral_control_params" in self.numpy_inputs:
      if self.combined:
        v_ego = sm["carState"].vEgo
      else:
        v_ego = max(sm["carState"].vEgo, 0.)

      if self.lateral_delay_source == "vehicle":
        lateral_delay = steer_actuator_delay + .2
      else:
        lateral_delay = lat_delay

      inputs["lateral_control_params"] = np.array([v_ego, lateral_delay], dtype=np.float32)

    if "radar_tracks" in self.numpy_inputs:
      radar_tracks = np.zeros_like(self.numpy_inputs["radar_tracks"]).reshape(-1, 3)

      if sm.updated["liveTracks"]:
        for radar_track, track in zip(radar_tracks, sm["liveTracks"].points):
          radar_track[:] = [track.dRel, track.yRel, track.vRel]

      inputs["radar_tracks"] = radar_tracks.reshape(self.numpy_inputs["radar_tracks"].shape)


def convert_legacy_meta(meta):
  group_size = LEGACY_META_GROUP_SIZES[meta.shape[1]]

  stock_meta = np.zeros((1, Meta.RIGHT_BLINKER.stop), dtype=meta.dtype)
  stock_meta[0, Meta.ENGAGED] = meta[0, Meta.ENGAGED]
  stock_meta[0, Meta.GAS_PRESS] = UNGATED_GAS_PRESS_PROBABILITY

  for field in [Meta.GAS_DISENGAGE, Meta.BRAKE_DISENGAGE, Meta.STEER_OVERRIDE, Meta.HARD_BRAKE_3, Meta.HARD_BRAKE_4, Meta.HARD_BRAKE_5]:
    stock_meta[0, field] = meta[0, field.start:field.start + len(ModelConstants.META_T_IDXS) * group_size:group_size]

  return stock_meta


def is_legacy_model(model_path):
  if (model_path / "supercombo_metadata.pkl").is_file():
    return True

  with open(model_path / "driving_policy_metadata.pkl", "rb") as metadata_file:
    return "desire" in pickle.load(metadata_file)["input_shapes"]


def prepare_model_process(model_path, demo):
  library_path = str(MODEL_RUNNERS_PATH / "libthneed.so")
  preloaded_libraries = os.environ.get("LD_PRELOAD", "").split()

  if (model_path / "supercombo.thneed").is_file() and library_path not in preloaded_libraries:
    environment = dict(os.environ)
    environment["LD_PRELOAD"] = " ".join([library_path, *preloaded_libraries])

    if demo:
      arguments = [sys.executable, "-m", "selfdrive.modeld.modeld", "--demo"]
    else:
      launcher = 'from openpilot.system.manager.process import launcher\nlauncher("selfdrive.modeld.modeld", "modeld")'
      arguments = [sys.executable, "-c", launcher]

    os.execve(sys.executable, arguments, environment)
