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

from openpilot.selfdrive.modeld.constants import Meta, ModelConstants, Plan
from openpilot.selfdrive.modeld.parse_model_outputs import Parser
from openpilot.selfdrive.modeld.runners.tinygrad_helpers import qcom_tensor_from_opencl_address
from openpilot.system.hardware import TICI


class LegacyMeta(Meta):
  GAS_DISENGAGE = slice(1, 36, 7)
  BRAKE_DISENGAGE = slice(2, 36, 7)
  STEER_OVERRIDE = slice(3, 36, 7)
  HARD_BRAKE_3 = slice(4, 36, 7)
  HARD_BRAKE_4 = slice(5, 36, 7)
  HARD_BRAKE_5 = slice(6, 36, 7)


class PressMeta(Meta):
  GAS_DISENGAGE = slice(1, 41, 8)
  BRAKE_DISENGAGE = slice(2, 41, 8)
  STEER_OVERRIDE = slice(3, 41, 8)
  HARD_BRAKE_3 = slice(4, 41, 8)
  HARD_BRAKE_4 = slice(5, 41, 8)
  HARD_BRAKE_5 = slice(6, 41, 8)
  GAS_PRESS = slice(7, 41, 8)
  BRAKE_PRESS = slice(8, 41, 8)


class ThneedModel:
  def __init__(self, model_path, input_shapes):
    with open(model_path / "supercombo.thneed", "rb") as model_file:
      header_size = struct.unpack("<I", model_file.read(4))[0]
      header = json.loads(model_file.read(header_size).decode("latin-1"))

    self.input_names = [item["name"] for item in header["inputs"]]
    self.input_dtypes = {}

    for item in header["inputs"]:
      elements = np.prod(input_shapes[item["name"]])
      if item["size"] == elements:
        self.input_dtypes[item["name"]] = np.uint8
      else:
        self.input_dtypes[item["name"]] = np.float32

    runner_path = Path(__file__).parent / "model_runners/libthneed_runner.so"
    self.library = ctypes.CDLL(str(runner_path))
    float_pointer = ctypes.POINTER(ctypes.c_float)
    self.library.thneed_model_create.argtypes = [ctypes.c_char_p]
    self.library.thneed_model_create.restype = ctypes.c_void_p
    self.library.thneed_model_run.argtypes = [ctypes.c_void_p, ctypes.POINTER(float_pointer), float_pointer]
    self.library.thneed_model_run.restype = None
    self.model = self.library.thneed_model_create(os.fsencode(model_path / "supercombo.thneed"))
    self.output = np.zeros(header["outputs"][0]["size"] // np.dtype(np.float32).itemsize, dtype=np.float32)

  def run(self, inputs):
    float_pointer = ctypes.POINTER(ctypes.c_float)
    pointers = [inputs[name].ctypes.data_as(float_pointer) for name in self.input_names]
    input_pointers = (float_pointer * len(pointers))(*pointers)
    self.library.thneed_model_run(self.model, input_pointers, self.output.ctypes.data_as(float_pointer))
    return self.output


class CombinedModelState:
  def __init__(self, context, model_path):
    from openpilot.selfdrive.modeld.models.commonmodel_pyx import DrivingModelFrame

    with open(model_path / "supercombo_metadata.pkl", "rb") as metadata_file:
      metadata = pickle.load(metadata_file)

    self.curvature_source = metadata.get("curvature_source", "model")
    self.lateral_delay_source = "vehicle"
    self.input_shapes = metadata["input_shapes"]
    self.output_slices = metadata["output_slices"]
    self.vision_input_names = [name for name in self.input_shapes if "img" in name]
    self.numpy_inputs = {name: np.zeros(shape, dtype=np.float32) for name, shape in self.input_shapes.items() if name not in self.vision_input_names}
    self.prev_desire = np.zeros(ModelConstants.DESIRE_LEN, dtype=np.float32)
    if self.input_shapes["desire"][1] == 25:
      self.history_stride = ModelConstants.MODEL_RUN_FREQ // ModelConstants.MODEL_CONTEXT_FREQ
    else:
      self.history_stride = 1
    self.frames = {name: DrivingModelFrame(context, self.history_stride) for name in self.vision_input_names}
    self.desire_history = np.zeros((1, self.input_shapes["desire"][1] * self.history_stride, ModelConstants.DESIRE_LEN), dtype=np.float32)
    self.feature_history = np.zeros((1, self.desire_history.shape[1] - 1, ModelConstants.FEATURE_LEN), dtype=np.float32)
    self.feature_indices = np.arange(-self.history_stride, -self.feature_history.shape[1] - 1, -self.history_stride)[::-1]

    if self.history_stride > 1:
      self.parser = Parser()
    else:
      self.parser = Parser(exp=np.exp)
    self.native_model = (model_path / "supercombo.thneed").is_file()

    if self.native_model:
      self.model_run = ThneedModel(model_path, self.input_shapes)
    else:
      self.model_inputs = {name: Tensor(value, device="NPY").realize() for name, value in self.numpy_inputs.items()}

      with open(model_path / "supercombo_tinygrad.pkl", "rb") as model_file:
        self.model_run = pickle.load(model_file)

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
        pixels = self.frames[name].buffer_from_cl(image).reshape(self.input_shapes[name])
        self.numpy_inputs[name] = pixels.astype(self.model_run.input_dtypes[name], copy=False)
      elif TICI and "USBGPU" not in os.environ:
        if name not in self.model_inputs:
          self.model_inputs[name] = qcom_tensor_from_opencl_address(image.mem_address, self.input_shapes[name], dtype=dtypes.uint8)
      else:
        pixels = self.frames[name].buffer_from_cl(image).reshape(self.input_shapes[name])
        self.model_inputs[name] = Tensor(pixels, dtype=dtypes.uint8).realize()

    if self.native_model:
      self.output = self.model_run.run(self.numpy_inputs)
    else:
      self.output = self.model_run(**self.model_inputs).contiguous().realize().uop.base.buffer.numpy()

    outputs = {name: self.output[np.newaxis, output_slice] for name, output_slice in self.output_slices.items()}

    if os.getenv("SEND_RAW_PRED"):
      outputs["raw_pred"] = self.output.copy()

    outputs = self.parser.parse_outputs(outputs)
    self.feature_history[:, :-1] = self.feature_history[:, 1:]
    self.feature_history[0, -1] = outputs["hidden_state"][0]
    self.numpy_inputs["features_buffer"][:] = self.feature_history[:, self.feature_indices]

    if "prev_desired_curv" in self.numpy_inputs and self.history_stride == 1:
      self.numpy_inputs["prev_desired_curv"][:, :-1] = self.numpy_inputs["prev_desired_curv"][:, 1:]
      self.numpy_inputs["prev_desired_curv"][0, -1] = outputs["desired_curvature"][0]

    return outputs


def prepare_model_process(model_path, demo):
  library_path = str(Path(__file__).parent / "model_runners/libthneed.so")
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


def get_plan_t_idxs(model_output):
  plan_t_idxs = [np.nan] * ModelConstants.IDX_N
  plan_t_idxs[0] = 0.0
  plan_x = model_output["plan"][0, :, Plan.POSITION][:, 0].tolist()

  for xidx in range(1, ModelConstants.IDX_N):
    tidx = 0

    while tidx < ModelConstants.IDX_N - 1 and plan_x[tidx + 1] < ModelConstants.X_IDXS[xidx]:
      tidx += 1

    if tidx == ModelConstants.IDX_N - 1:
      plan_t_idxs[xidx] = ModelConstants.T_IDXS[-1]
      break

    current_x = plan_x[tidx]
    next_x = plan_x[tidx + 1]
    if abs(next_x - current_x) > 1e-9:
      fraction = (ModelConstants.X_IDXS[xidx] - current_x) / (next_x - current_x)
    else:
      fraction = float("nan")

    plan_t_idxs[xidx] = fraction * ModelConstants.T_IDXS[tidx + 1] + (1 - fraction) * ModelConstants.T_IDXS[tidx]

  return plan_t_idxs
