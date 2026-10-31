import pickle
import pytest
import types

import numpy as np

from cereal import log
from openpilot.common.params import Params
from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.selfdrive.modeld.tinygrad import tinygrad_modeld
from openpilot.frogpilot.selfdrive.modeld.tinygrad.constants import ModelConstants
from openpilot.frogpilot.selfdrive.modeld.tinygrad.parse_model_outputs import Parser
from openpilot.frogpilot.system import model_manager
from openpilot.frogpilot.system.the_pond import models as the_pond_models
from openpilot.frogpilot.tools import compile_models

CHEDDAR_INPUT_SHAPES = {
  "action_t": (1, 2),
  "desire": (8,),
  "new_img": (2, 6, 128, 256),
  "state_desire_q": (132, 1, 8),
  "state_feat_q": (128, 1, 512),
  "state_img_q": (2, 5, 6, 128, 256),
  "traffic_convention": (1, 2),
}
CHEDDAR_OUTPUT_SLICES = {
  "lane_lines": slice(0, 528),
  "lane_lines_prob": slice(528, 536),
  "road_edges": slice(536, 800),
  "meta": slice(800, 855),
  "desire_pred": slice(855, 887),
  "pose": slice(887, 899),
  "wide_from_device_euler": slice(899, 905),
  "road_transform": slice(905, 917),
  "plan": slice(917, 1907),
  "lead": slice(1907, 2051),
  "lead_prob": slice(2051, 2054),
  "desire_state": slice(2054, 2062),
  "action": slice(2062, 2066),
  "hidden_state": slice(2066, 2578),
  "pad": slice(2578, 2580),
}
CHEDDAR_OUTPUT_SHAPES = {
  "next_state_desire_q": (132, 1, 8),
  "next_state_feat_q": (128, 1, 512),
  "next_state_img_q": (2, 5, 6, 128, 256),
  "outputs": (1, 2580),
}
CHEDDAR_OUTPUT_SIZE = 2580
CHEDDAR_FILES = ["cheddar-model_driving_metadata.pkl", "cheddar-model_driving_tinygrad.pkl"]
CD210_FILES = ["cd210-model_driving_policy_metadata.pkl", "cd210-model_driving_policy_tinygrad.pkl",
               "cd210-model_driving_vision_metadata.pkl", "cd210-model_driving_vision_tinygrad.pkl"]


@pytest.fixture
def model_versions(monkeypatch):
  versions = {"cheddar-model": "v10", "dark-souls": "v9", "wd-40": "v2"}
  monkeypatch.setattr(frogpilot_utilities, "get_model_version", lambda model: versions[model])

  helpers = (frogpilot_utilities.use_action_delay, frogpilot_utilities.use_model_lead_trajectory)
  for helper in helpers:
    helper.cache_clear()

  yield

  for helper in helpers:
    helper.cache_clear()


@pytest.fixture
def models_path(tmp_path, monkeypatch):
  monkeypatch.setattr(frogpilot_paths, "MODEL_SIZES_PATH", tmp_path / "model_sizes.json")
  monkeypatch.setattr(frogpilot_paths, "MODELS_PATH", tmp_path)
  monkeypatch.setattr(frogpilot_paths, "VERSION_PATH", tmp_path / "model_version")
  return tmp_path


@pytest.fixture
def params(monkeypatch):
  params = Params()
  params.put("Model", "cheddar-model")
  monkeypatch.setattr(frogpilot_variables, "params", params)
  return params


def action_output(action):
  return {"action": np.array([action], dtype=np.float32), "plan": np.zeros((1, ModelConstants.IDX_N, ModelConstants.PLAN_WIDTH), dtype=np.float32)}


def manager_with_models():
  manager = model_manager.ModelManager()
  manager.available_models = ["cheddar-model", "cd210-model", "wd-40"]
  manager.model_versions = ["v10", "v9", "v2"]
  return manager


def raw_outputs():
  model_output = np.random.default_rng(0).standard_normal(CHEDDAR_OUTPUT_SIZE).astype(np.float32)
  return {name: model_output[np.newaxis, output_slice] for name, output_slice in CHEDDAR_OUTPUT_SLICES.items()}


def test_action_output():
  prev_action = log.ModelDataV2.Action(desiredAcceleration=-0.8)

  action = tinygrad_modeld.get_action_from_model(action_output([0.5, -0.8]), prev_action, 0.3, 0.9, 10.0, True, 0.0, True, 0.3)

  assert action.desiredAcceleration == pytest.approx(-0.8)
  assert action.desiredCurvature == pytest.approx(0.5 / 10.0**2)
  assert not action.shouldStop


def test_action_output_low_speed():
  prev_action = log.ModelDataV2.Action(desiredAcceleration=0.2)

  action = tinygrad_modeld.get_action_from_model(action_output([0.5, 0.2]), prev_action, 0.3, 0.9, 1.0, True, 0.0, True, 0.3)

  assert action.desiredCurvature == pytest.approx(0.5 / 2.5**2)
  assert not action.shouldStop


def test_action_output_stop():
  prev_action = log.ModelDataV2.Action(desiredAcceleration=-0.5, desiredCurvature=0.01)

  action = tinygrad_modeld.get_action_from_model(action_output([0.5, -0.5]), prev_action, 0.3, 0.9, 0.1, True, 0.0, True, 0.3)

  assert action.shouldStop
  assert action.desiredCurvature == pytest.approx(0.01)


def test_combined_model_behaviour(model_versions):
  assert frogpilot_utilities.is_combined_model("cheddar-model")
  assert frogpilot_utilities.use_action_delay("cheddar-model")
  assert frogpilot_utilities.use_model_lead_trajectory("cheddar-model")


def test_combined_model_compile(models_path, monkeypatch):
  monkeypatch.setattr(frogpilot_paths, "COMPILED_DIR", models_path / "compiled")
  (models_path / "compiled").mkdir()
  calls = []

  def run(command, check, env=None):
    calls.append((command, env))
    if command[-1].endswith(".onnx"):
      (models_path / f"{models_path.joinpath(command[-1]).stem}_metadata.pkl").write_bytes(b"metadata")

  monkeypatch.setattr(compile_models.subprocess, "run", run)
  for name in ("cheddar-model_driving.onnx", "cd210-model_driving_vision.onnx"):
    (models_path / name).write_bytes(b"onnx")
    compile_models.compile_model(models_path / name)

  combined_command, split_command = calls[0][0], calls[2][0]
  assert combined_command[1] == str(frogpilot_paths.COMBINED_MODEL_COMPILE_SCRIPT)
  assert combined_command[-1] == str(models_path / "compiled/cheddar-model_driving_tinygrad.pkl")
  assert split_command[1].endswith("examples/openpilot/compile3.py")
  assert (models_path / "compiled/cheddar-model_driving_metadata.pkl").is_file()


def test_combined_model_download(models_path, params, monkeypatch):
  manager = manager_with_models()
  urls = []

  def download_file(destination, session, url, offset_bytes=0, total_bytes=0):
    urls.append(url)
    destination.write_bytes(b"model")

  monkeypatch.setattr(manager.download_state, "download_file", download_file)
  monkeypatch.setattr(model_manager.frogpilot_download_utilities, "verify_download", lambda destination, url, session: True)

  assert manager.download_model("cheddar-model", "https://resources", {name: 5 for name in CHEDDAR_FILES})

  assert urls == [f"https://resources/Models/compiled/{name}" for name in CHEDDAR_FILES]
  assert manager.is_downloaded("cheddar-model")


def test_combined_model_run(monkeypatch):
  monkeypatch.setattr(tinygrad_modeld, "TICI", False)
  calls = []

  def model_run(**inputs):
    calls.append({k: v.numpy().copy() for k, v in inputs.items()})
    return tinygrad_modeld.Tensor(np.zeros(CHEDDAR_OUTPUT_SIZE, dtype=np.float32)).realize()

  frame_size = 12 * 128 * 256
  frame = types.SimpleNamespace(prepare=lambda buf, transform: buf, buffer_from_cl=lambda buf: np.full(frame_size, buf, dtype=np.uint8))
  state = tinygrad_modeld.CombinedModelState.__new__(tinygrad_modeld.CombinedModelState)
  state.frames = {"big_img": frame, "img": frame}
  state.numpy_inputs = {
    "action_t": np.zeros((1, 2), dtype=np.float32),
    "desire": np.zeros(8, dtype=np.float32),
    "traffic_convention": np.zeros((1, 2), dtype=np.float32),
  }
  state.host_inputs = {k: tinygrad_modeld.Tensor(v, device="NPY").realize() for k, v in state.numpy_inputs.items()}
  state.model_run = model_run
  state.output_slices = CHEDDAR_OUTPUT_SLICES
  state.parser = Parser(ignore_missing=True)
  state.prev_desire = np.zeros(ModelConstants.DESIRE_LEN, dtype=np.float32)
  state.states = {"state_feat_q": tinygrad_modeld.Tensor(np.full((128, 1, 512), 3, dtype=np.float32)).realize()}
  state.vision_input_names = ["img", "big_img"]
  state.vision_input_shape = (1, 12, 128, 256)
  state.vision_inputs = {}

  transforms = {"big_img": np.eye(3, dtype=np.float32), "img": np.eye(3, dtype=np.float32)}
  lane_change = np.eye(ModelConstants.DESIRE_LEN, dtype=np.float32)[log.Desire.laneChangeLeft]
  for desire in (lane_change, lane_change, np.zeros(ModelConstants.DESIRE_LEN, dtype=np.float32)):
    inputs = {"action_t": np.array([0.4, 0.9], dtype=np.float32), "desire": desire.copy(), "traffic_convention": np.array([0, 1], dtype=np.float32)}
    outputs = state.run({"big_img": 2, "img": 1}, transforms, inputs, True)

  assert len(calls) == 3
  assert [call["desire"][log.Desire.laneChangeLeft] for call in calls] == [1, 0, 0]
  assert np.allclose(calls[0]["action_t"], [[0.4, 0.9]])
  assert np.all(calls[0]["traffic_convention"] == [[0, 1]])
  assert np.all(calls[0]["img"] == 1)
  assert np.all(calls[0]["big_img"] == 2)
  assert np.all(calls[0]["state_feat_q"] == 3)
  assert outputs["plan"].shape == (1, ModelConstants.IDX_N, ModelConstants.PLAN_WIDTH)


def test_combined_model_setup(tmp_path, monkeypatch):
  monkeypatch.setattr(frogpilot_paths, "MODELS_PATH", tmp_path)
  monkeypatch.setattr(tinygrad_modeld, "DrivingModelFrame", lambda context, temporal_skip: temporal_skip)
  metadata = {"input_shapes": CHEDDAR_INPUT_SHAPES, "output_shapes": CHEDDAR_OUTPUT_SHAPES, "output_slices": CHEDDAR_OUTPUT_SLICES}
  (tmp_path / "cheddar-model_driving_metadata.pkl").write_bytes(pickle.dumps(metadata))
  (tmp_path / "cheddar-model_driving_tinygrad.pkl").write_bytes(pickle.dumps("compiled run"))

  state = tinygrad_modeld.CombinedModelState(None, "cheddar-model")

  assert state.desire_type == "desire"
  assert not state.use_lateral_control_params
  assert state.frames == {"big_img": 0, "img": 0}
  assert state.vision_input_shape == (1, 12, 128, 256)
  assert set(state.host_inputs) == {"action_t", "desire", "traffic_convention"}
  assert state.state_names == ["state_desire_q", "state_feat_q", "state_img_q"]
  assert state.states["state_img_q"].dtype == tinygrad_modeld.dtypes.uint8
  assert state.states["state_feat_q"].dtype == tinygrad_modeld.dtypes.float32
  assert state.states["state_feat_q"].shape == (128, 1, 512)
  assert state.model_run == "compiled run"


def test_combined_outputs():
  outputs = Parser(ignore_missing=True).parse_combined_outputs(raw_outputs())

  assert outputs["plan"].shape == (1, ModelConstants.IDX_N, ModelConstants.PLAN_WIDTH)
  assert outputs["plan_stds"].shape == (1, ModelConstants.IDX_N, ModelConstants.PLAN_WIDTH)
  assert outputs["lane_lines"].shape == (1, ModelConstants.NUM_LANE_LINES, ModelConstants.IDX_N, ModelConstants.LANE_LINES_WIDTH)
  assert outputs["lead"].shape == (1, ModelConstants.LEAD_MHP_SELECTION, ModelConstants.LEAD_TRAJ_LEN, ModelConstants.LEAD_WIDTH)
  assert np.isclose(outputs["desire_state"].sum(), 1)
  assert outputs["action"].shape == (1, 4)
  assert np.all((outputs["lane_lines_prob"] > 0) & (outputs["lane_lines_prob"] < 1))


def test_downloaded_model_checks(models_path, params):
  for name in [*CHEDDAR_FILES, *CD210_FILES[:3], "wd-40.thneed"]:
    (models_path / name).write_bytes(b"model")
  manager = manager_with_models()

  assert [manager.is_downloaded(model) for model in manager.available_models] == [True, False, True]
  assert the_pond_models.get_model_files({"id": "cheddar-model", "version": "v10"}) == [models_path / name for name in CHEDDAR_FILES]
  assert the_pond_models.get_model_files({"id": "wd-40", "version": "v2"}) == [models_path / "wd-40.thneed"]


def test_downloaded_models(models_path):
  for name in [*CHEDDAR_FILES, *CD210_FILES[:3], "wd-40.thneed"]:
    (models_path / name).write_bytes(b"model")

  downloaded = frogpilot_utilities.get_downloaded_models({"cd210-model": "v9", "cheddar-model": "v10", "wd-40": "v2"})

  assert downloaded == ["cheddar-model", "wd-40"]


def test_model_files():
  assert [name for name, _ in frogpilot_utilities.model_files("cheddar-model", "v10")] == CHEDDAR_FILES
  assert [name for name, _ in frogpilot_utilities.model_files("cd210-model", "v9")] == CD210_FILES
  assert [name for name, _ in frogpilot_utilities.model_files("wd-40", "v2")] == ["wd-40.thneed"]


def test_model_list_version(monkeypatch):
  manager = model_manager.ModelManager()
  urls = []
  monkeypatch.setattr(model_manager.frogpilot_download_utilities, "get_repository_url", lambda session: "https://resources")
  monkeypatch.setattr(manager, "fetch_models", lambda url: urls.append(url))

  manager.update_models(False, True)

  assert urls[0] == "https://resources/Versions/model_names_v18.json"


def test_old_tinygrad_models_removed(models_path):
  for name in [*CHEDDAR_FILES, *CD210_FILES, "wd-40.thneed"]:
    (models_path / name).write_bytes(b"model")

  model_manager.ModelManager().update_tinygrad_models()

  assert [path.name for path in models_path.iterdir()] == ["wd-40.thneed"]


def test_outdated_combined_model_removed(models_path, params):
  for name in [*CHEDDAR_FILES, *CD210_FILES]:
    (models_path / name).write_bytes(b"model")
  manager = manager_with_models()
  manager.model_sizes = {name: 5 for name in [*CHEDDAR_FILES, *CD210_FILES]}

  manager.remove_outdated_models({**manager.model_sizes, CHEDDAR_FILES[1]: 6})

  assert sorted(path.name for path in models_path.iterdir()) == CD210_FILES


def test_release_update_keeps_models(models_path, params):
  for name in [*CHEDDAR_FILES, *CD210_FILES]:
    (models_path / name).write_bytes(b"model")
  frogpilot_paths.VERSION_PATH.write_text("v17")
  manager = manager_with_models()
  manager.available_model_names = ["Cheddar Model", "CD210 model", "WD-40"]

  manager.validate_models()

  assert sorted(path.name for path in models_path.iterdir() if path.suffix == ".pkl") == sorted([*CD210_FILES, *CHEDDAR_FILES])


def test_split_model_behaviour(model_versions):
  assert not frogpilot_utilities.is_combined_model("dark-souls")
  assert not frogpilot_utilities.use_action_delay("dark-souls")
  assert frogpilot_utilities.use_model_lead_trajectory("dark-souls")
  assert not frogpilot_utilities.use_model_lead_trajectory("wd-40")


def test_unavailable_models_removed(models_path, params):
  for name in [*CHEDDAR_FILES, *CD210_FILES, "retired-model_driving_metadata.pkl", "retired-model_driving_tinygrad.pkl"]:
    (models_path / name).write_bytes(b"model")
  manager = model_manager.ModelManager()
  manager.available_models = ["cheddar-model"]

  manager.remove_unavailable_models()

  assert sorted(path.name for path in models_path.iterdir()) == CHEDDAR_FILES
