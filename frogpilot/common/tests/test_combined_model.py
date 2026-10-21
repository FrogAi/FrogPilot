import numpy as np
import pickle
import pytest

from types import SimpleNamespace

from cereal import log
from tinygrad.dtype import dtypes
from tinygrad.tensor import Tensor

from openpilot.selfdrive.controls.lib.drive_helpers import smooth_value
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.selfdrive.modeld.modeld import LONG_SMOOTH_SECONDS, get_action_from_model

from openpilot.frogpilot.common import frogpilot_model_utilities, frogpilot_utilities, frogpilot_variables

INPUT_SHAPES = {
  "new_img": (2, 6, 128, 256),
  "desire": (8,),
  "traffic_convention": (1, 2),
  "action_t": (1, 2),
  "state_img_q": (2, 5, 6, 128, 256),
  "state_desire_q": (132, 1, 8),
  "state_feat_q": (128, 1, 512),
}
OUTPUT_SHAPES = {
  "outputs": (1, 2580),
  "next_state_img_q": (2, 5, 6, 128, 256),
  "next_state_desire_q": (132, 1, 8),
  "next_state_feat_q": (128, 1, 512),
}
OUTPUT_SLICES = {
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


class FakeFrame:
  def buffer_from_cl(self, image):
    return np.full(12 * 128 * 256, image, dtype=np.uint8)

  def prepare(self, buf, transform):
    return buf


class FakeRun:
  def __init__(self):
    self.inputs = []
    self.values = []

  def __call__(self, **inputs):
    self.inputs.append(inputs)
    self.values.append({name: value.numpy().copy() for name, value in inputs.items()})

    return Tensor(np.arange(2580, dtype=np.float32)).realize()


def get_inputs(desire):
  desire_pulse = np.zeros(ModelConstants.DESIRE_LEN, dtype=np.float32)
  desire_pulse[desire] = 1

  return {"action_t": np.array([0.3, 0.6], dtype=np.float32), "desire_pulse": desire_pulse, "traffic_convention": np.array([0, 1], dtype=np.float32)}


def get_model_output(action):
  plan = np.ones((1, ModelConstants.IDX_N, ModelConstants.PLAN_WIDTH), dtype=np.float32)

  return {"action": np.array([action], dtype=np.float32), "plan": plan}


@pytest.fixture
def model(mocker, tmp_path):
  write_metadata(tmp_path)

  with open(tmp_path / "driving_tinygrad.pkl", "wb") as model_file:
    pickle.dump(None, model_file)

  mocker.patch.object(frogpilot_model_utilities, "DrivingModelFrame", lambda context, temporal_skip: FakeFrame())
  mocker.patch.object(frogpilot_model_utilities, "TICI", False)

  model = frogpilot_model_utilities.CombinedModelState(None, tmp_path)
  model.model_run = FakeRun()

  return model


def write_metadata(path):
  with open(path / "driving_metadata.pkl", "wb") as metadata_file:
    pickle.dump({"input_shapes": INPUT_SHAPES, "output_shapes": OUTPUT_SHAPES, "output_slices": OUTPUT_SLICES}, metadata_file)


def test_action_accel():
  prev_action = log.ModelDataV2.Action(desiredAcceleration=1.0)

  action = get_action_from_model(get_model_output([0.0, -0.5, 0.0, 0.0]), prev_action, 0.3, 0.6, 10.0, 0.0, False, 0.0)

  assert action.desiredAcceleration == pytest.approx(smooth_value(-0.5, 1.0, LONG_SMOOTH_SECONDS))
  assert not action.shouldStop


@pytest.mark.parametrize("current_speed_stop, min_stable_delay", [(False, 0.0), (True, frogpilot_model_utilities.MIN_STABLE_DELAY)])
@pytest.mark.parametrize("v_ego, lat_smooth_seconds, expected", [
  (10.0, 0.0, 1.5 / 10.0**2),
  (10.0, 0.1, smooth_value(1.5 / 10.0**2, 0.002, 0.1)),
  (1.0, 0.0, 1.5 / 2.5**2),
  (0.2, 0.0, 0.002),
])
def test_action_curvature(current_speed_stop, min_stable_delay, v_ego, lat_smooth_seconds, expected):
  prev_action = log.ModelDataV2.Action(desiredCurvature=0.002)

  action = get_action_from_model(get_model_output([1.5, 0.0, 0.0, 0.0]), prev_action, 0.3, 0.6, v_ego, lat_smooth_seconds, current_speed_stop, min_stable_delay)

  assert action.desiredCurvature == pytest.approx(expected)


@pytest.mark.parametrize("v_ego, accel, prev_accel, stop", [
  (0.2, 0.05, 5.0, True),
  (0.2, 0.2, -5.0, False),
  (0.5, -1.0, -1.0, False),
])
def test_action_stop(v_ego, accel, prev_accel, stop):
  prev_action = log.ModelDataV2.Action(desiredAcceleration=prev_accel)

  action = get_action_from_model(get_model_output([0.0, accel, 0.0, 0.0]), prev_action, 0.3, 0.6, v_ego, 0.0, True, 0.0)

  assert action.shouldStop == stop


def test_combined_inputs(model):
  model.run({"big_img": 2, "img": 1}, {"big_img": np.eye(3), "img": np.eye(3)}, get_inputs(log.Desire.laneChangeLeft), False)
  model.run({"big_img": 2, "img": 1}, {"big_img": np.eye(3), "img": np.eye(3)}, get_inputs(log.Desire.laneChangeLeft), False)

  first, second = model.model_run.values
  assert first["desire"][log.Desire.laneChangeLeft] == 1
  assert not np.any(second["desire"])
  np.testing.assert_allclose(second["action_t"], [[0.3, 0.6]])
  np.testing.assert_equal(second["traffic_convention"], [[0, 1]])
  assert np.all(second["big_img"] == 2)
  assert np.all(second["img"] == 1)
  assert second["img"].shape == (1, 12, 128, 256)

  first, second = model.model_run.inputs
  assert all(first[name] is second[name] for name in ("state_desire_q", "state_feat_q", "state_img_q"))
  assert second["state_img_q"].dtype == dtypes.uint8
  assert second["state_feat_q"].dtype == dtypes.float32


def test_combined_lead_trajectories(tmp_path):
  write_metadata(tmp_path)

  assert frogpilot_utilities.predicts_lead_trajectories(tmp_path)


@pytest.mark.parametrize("files, complete", [
  (("driving_metadata.pkl", "driving_tinygrad.pkl"), True),
  (("driving_metadata.pkl",), False),
])
def test_combined_model_complete(tmp_path, files, complete):
  for file_name in files:
    (tmp_path / file_name).touch()

  assert frogpilot_model_utilities.is_combined_model(tmp_path)
  assert frogpilot_variables.is_model_complete(tmp_path) == complete


def test_combined_outputs(model):
  outputs = model.run({"big_img": 2, "img": 1}, {"big_img": np.eye(3), "img": np.eye(3)}, get_inputs(0), False)

  assert not np.any(model.model_run.values[0]["desire"])
  np.testing.assert_equal(outputs["action"], [np.arange(2062, 2066)])
  assert outputs["plan"].shape == (1, ModelConstants.IDX_N, ModelConstants.PLAN_WIDTH)
  assert outputs["lead"].shape == (1, ModelConstants.LEAD_MHP_SELECTION, ModelConstants.LEAD_TRAJ_LEN, ModelConstants.LEAD_WIDTH)


@pytest.mark.parametrize("lat_active, curvature", [(False, 0.01), (True, 0.002)])
def test_combined_update_inputs(model, lat_active, curvature):
  inputs = get_inputs(0)
  prev_action = log.ModelDataV2.Action(desiredCurvature=0.002)
  sm = {"carControl": SimpleNamespace(latActive=lat_active), "controlsState": SimpleNamespace(curvature=0.01)}

  model.update_inputs(inputs, sm, prev_action, 0.25, 0.55)

  np.testing.assert_allclose(inputs["action_t"], [0.25, 0.55])
  assert prev_action.desiredCurvature == pytest.approx(curvature)
