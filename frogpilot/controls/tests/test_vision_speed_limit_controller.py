from datetime import datetime, UTC
from types import SimpleNamespace

import pytest

from openpilot.common.constants import CV
from openpilot.frogpilot.common.vision_speed_limit import HEARTBEAT_TIMEOUT, VISION_SPEED_LIMIT_PARAM, Detection, SpeedLimitConfirmation
from openpilot.frogpilot.controls.lib.speed_limit_controller import SpeedLimitController


class TestParams:
  __test__ = False

  def __init__(self):
    self.values = {"PreviousSpeedLimit": 0.0}

  def get(self, key):
    return self.values.get(key)

  def get_bool(self, key):
    return bool(self.get(key))

  def put_nonblocking(self, key, value):
    self.values[key] = value

  def remove(self, key):
    self.values.pop(key, None)


@pytest.fixture
def controller(mocker):
  planner = SimpleNamespace(params=TestParams(), params_memory=TestParams(), gps_valid=True)
  slc = SpeedLimitController(SimpleNamespace(frogpilot_planner=planner))
  slc.frogpilot_toggles = SimpleNamespace(
    speed_limit_controller=True, vision_speed_limit_detection=True, is_metric=False,
    speed_limit_priority1="Vision", speed_limit_priority2="Map Data", speed_limit_priority3="Dashboard",
    speed_limit_priority_highest=False, speed_limit_priority_lowest=False,
    speed_limit_confirmation_higher=False, speed_limit_confirmation_lower=False,
    slc_fallback_previous_speed_limit=True, slc_mapbox_filler=False, speed_limit_filler=False,
    map_speed_lookahead_higher=0, map_speed_lookahead_lower=0,
    speed_limit_controller_override_manual=True, speed_limit_controller_override_set_speed=False,
    **{f"speed_limit_offset{i}": float(i) for i in range(1, 8)},
  )
  mocker.patch("openpilot.frogpilot.controls.lib.speed_limit_controller.time.monotonic", return_value=100.0)
  state = SpeedLimitConfirmation()
  state.update(Detection(45, 0.95), 99.0, 99.0)
  state.update(Detection(45, 0.95), 99.2, 99.2)
  planner.params_memory.values[VISION_SPEED_LIMIT_PARAM] = state.snapshot(100.0)
  yield slc
  slc.close()


class SubMaster(dict):
  def __init__(self, dashboard=35, map_limit=55, **car_state):
    super().__init__(
      frogpilotCarState=SimpleNamespace(dashboardSpeedLimit=dashboard * CV.MPH_TO_MS, accelPressed=False, decelPressed=False),
      mapdOut=SimpleNamespace(tileLoaded=True, wayId=1, speedLimit=map_limit * CV.MPH_TO_MS, nextSpeedLimit=0),
      carControl=SimpleNamespace(longActive=True), selfdriveState=SimpleNamespace(enabled=True),
      carState=SimpleNamespace(gasPressed=False, **car_state),
    )
    self.alive = {"mapdOut": True}


def update(controller, sm=None):
  controller.update_limits({}, datetime(2026, 1, 1, tzinfo=UTC), False, 20.0, sm or SubMaster())


def test_vision_uses_existing_priority_and_offset(controller):
  update(controller)
  assert controller.source == "Vision"
  assert controller.target == pytest.approx(45 * CV.MPH_TO_MS)
  assert controller.offset == 4
  assert controller.frogpilot_planner.params.get("PreviousSpeedLimit") == 0


@pytest.mark.parametrize("enabled, expected", [(False, "Map Data"), (True, "Vision")])
def test_detection_toggle_gates_controller(controller, enabled, expected):
  controller.frogpilot_toggles.vision_speed_limit_detection = enabled
  update(controller)
  assert controller.source == expected


@pytest.mark.parametrize("priority, expected", [("highest", "Map Data"), ("lowest", "Dashboard")])
def test_highest_and_lowest_consider_vision(controller, priority, expected):
  setattr(controller.frogpilot_toggles, f"speed_limit_priority_{priority}", True)
  update(controller)
  assert controller.source == expected
  update(controller, SubMaster(dashboard=0, map_limit=0))
  assert controller.source == "Vision"


@pytest.mark.parametrize("slot", [1, 2, 3])
def test_vision_can_use_any_priority_slot(controller, slot):
  for index in (1, 2, 3):
    setattr(controller.frogpilot_toggles, f"speed_limit_priority{index}", "Vision" if index == slot else "None")
  update(controller)
  assert controller.source == "Vision"


def test_expired_vision_cannot_be_resurrected_by_previous_limit(controller, mocker):
  update(controller, SubMaster(dashboard=0, map_limit=0))
  controller.overridden_speed = 30
  mocker.patch("openpilot.frogpilot.controls.lib.speed_limit_controller.time.monotonic", return_value=101 + HEARTBEAT_TIMEOUT)
  update(controller, SubMaster(dashboard=0, map_limit=0))
  assert controller.target == 0
  assert controller.source == "None"
  assert controller.overridden_speed == 0
  assert controller.vision_speed_limit == 0
  update(controller, SubMaster(dashboard=0, map_limit=0))
  assert controller.target == 0


def test_disabled_vision_falls_back_to_selected_source(controller):
  update(controller)
  controller.frogpilot_toggles.vision_speed_limit_detection = False
  update(controller)
  assert controller.source == "Map Data"
  assert controller.target == pytest.approx(55 * CV.MPH_TO_MS)


def test_deselected_vision_is_not_previous_limit_fallback(controller):
  update(controller)
  controller.frogpilot_toggles.speed_limit_priority1 = "None"
  update(controller, SubMaster(dashboard=0, map_limit=0))
  assert controller.target == 0


def test_existing_confirmation_accepts_vision_change(controller):
  controller.frogpilot_toggles.vision_speed_limit_detection = False
  update(controller)
  controller.frogpilot_toggles.vision_speed_limit_detection = True
  controller.frogpilot_toggles.speed_limit_confirmation_lower = True
  update(controller)
  assert controller.target == pytest.approx(55 * CV.MPH_TO_MS)
  assert controller.unconfirmed_speed_limit == pytest.approx(45 * CV.MPH_TO_MS)
  controller.frogpilot_planner.params_memory.values["SpeedLimitAccepted"] = True
  update(controller)
  assert controller.source == "Vision"
  assert controller.target == pytest.approx(45 * CV.MPH_TO_MS)
  assert controller.unconfirmed_speed_limit == 0


def test_denied_vision_change_preserves_current_limit(controller):
  controller.frogpilot_toggles.vision_speed_limit_detection = False
  update(controller)
  controller.frogpilot_toggles.vision_speed_limit_detection = True
  controller.frogpilot_toggles.speed_limit_confirmation_lower = True
  update(controller)
  sm = SubMaster()
  sm["frogpilotCarState"].decelPressed = True
  update(controller, sm)
  assert controller.denied_target == pytest.approx(45 * CV.MPH_TO_MS)
  assert controller.target == pytest.approx(55 * CV.MPH_TO_MS)


@pytest.mark.parametrize("fallback", [False, True])
@pytest.mark.parametrize("unavailable", ["expired", "disabled", "deselected"])
def test_unavailable_vision_cancels_pending_confirmation(controller, mocker, fallback, unavailable):
  controller.frogpilot_toggles.vision_speed_limit_detection = False
  update(controller)
  controller.frogpilot_toggles.vision_speed_limit_detection = True
  controller.frogpilot_toggles.speed_limit_confirmation_lower = True
  controller.frogpilot_toggles.slc_fallback_previous_speed_limit = fallback
  update(controller)
  assert controller.unconfirmed_speed_limit == pytest.approx(45 * CV.MPH_TO_MS)
  controller.frogpilot_planner.params_memory.values["SpeedLimitAccepted"] = True

  if unavailable == "expired":
    mocker.patch("time.monotonic", return_value=101 + HEARTBEAT_TIMEOUT)
  elif unavailable == "disabled":
    controller.frogpilot_toggles.vision_speed_limit_detection = False
  else:
    controller.frogpilot_toggles.speed_limit_priority1 = "None"
  update(controller, SubMaster(dashboard=0, map_limit=0))

  assert controller.unconfirmed_speed_limit == 0
  assert controller.speed_limit_changed_timer == 0
  assert not controller.frogpilot_planner.params_memory.get_bool("SpeedLimitAccepted")
  assert controller.target == pytest.approx(55 * CV.MPH_TO_MS if fallback else 0)
  assert controller.source == ("Map Data" if fallback else "None")


def test_expired_vision_clears_its_denied_limit(controller, mocker):
  controller.frogpilot_toggles.vision_speed_limit_detection = False
  update(controller)
  controller.frogpilot_toggles.vision_speed_limit_detection = True
  controller.frogpilot_toggles.speed_limit_confirmation_lower = True
  update(controller)
  sm = SubMaster()
  sm["frogpilotCarState"].decelPressed = True
  update(controller, sm)
  assert controller.denied_target > 0
  mocker.patch("time.monotonic", return_value=101 + HEARTBEAT_TIMEOUT)
  update(controller, SubMaster(dashboard=0, map_limit=0))
  assert controller.denied_target == 0
  assert controller.target == pytest.approx(55 * CV.MPH_TO_MS)


def test_vision_expiry_preserves_pending_map_confirmation(controller, mocker):
  controller.frogpilot_toggles.speed_limit_priority1 = "Map Data"
  update(controller)
  controller.frogpilot_toggles.speed_limit_confirmation_lower = True
  update(controller, SubMaster(map_limit=45))
  mocker.patch("time.monotonic", return_value=101 + HEARTBEAT_TIMEOUT)
  update(controller, SubMaster(map_limit=45))
  assert controller.unconfirmed_speed_limit == pytest.approx(45 * CV.MPH_TO_MS)
  controller.frogpilot_planner.params_memory.values["SpeedLimitAccepted"] = True
  update(controller, SubMaster(map_limit=45))
  assert controller.source == "Map Data"
  assert controller.target == pytest.approx(45 * CV.MPH_TO_MS)


def test_display_only_does_not_apply_offset_or_override(controller):
  controller.frogpilot_toggles.speed_limit_controller = False
  controller.overridden_speed = 30
  update(controller)
  assert controller.source == "Vision"
  assert controller.overridden_speed == 0


def test_gas_override_and_disengagement_remain_supported(controller):
  update(controller)
  sm = SubMaster()
  sm["carState"].gasPressed = True
  controller.update_override(35, 30, sm)
  assert controller.overridden_speed == 30
  sm["selfdriveState"].enabled = False
  controller.update_override(35, 30, sm)
  assert controller.overridden_speed == 0


def test_metric_display_does_not_reinterpret_sign(controller):
  controller.frogpilot_toggles.is_metric = True
  update(controller)
  assert controller.target == pytest.approx(45 * CV.MPH_TO_MS)
  assert controller.target * CV.MS_TO_KPH == pytest.approx(72.42048)


@pytest.mark.parametrize("control_enabled, cruise_speed", [(False, 30.0), (True, 30.0), (True, 10.0)])
def test_vcruise_display_only_and_cruise_cap(controller, mocker, control_enabled, cruise_speed):
  from openpilot.frogpilot.controls.lib.frogpilot_vcruise import FrogPilotVCruise

  vcruise = FrogPilotVCruise.__new__(FrogPilotVCruise)
  vcruise.frogpilot_planner = controller.frogpilot_planner
  vcruise.frogpilot_planner.gps_position = {}
  vcruise.slc = controller
  vcruise.csc = mocker.Mock()
  vcruise.update_force_stop = mocker.Mock()
  toggles = controller.frogpilot_toggles
  toggles.speed_limit_controller = control_enabled
  toggles.show_speed_limits = True
  toggles.curve_speed_controller = False
  sm = SubMaster(vCruiseCluster=cruise_speed * CV.MS_TO_KPH, vEgoCluster=20.0)
  target = vcruise.update(True, datetime(2026, 1, 1, tzinfo=UTC), False, cruise_speed, 20.0, sm, toggles)
  expected = min(cruise_speed, 45 * CV.MPH_TO_MS + controller.offset) if control_enabled else cruise_speed
  assert target == pytest.approx(expected)
  assert vcruise.slc_offset == (controller.offset if control_enabled else 0)


def test_process_only_runs_onroad_when_enabled():
  from openpilot.system.manager.process_config import managed_processes, run_speed_limit_vision

  toggles = SimpleNamespace(vision_speed_limit_detection=False)
  assert not run_speed_limit_vision(True, None, None, toggles)
  toggles.vision_speed_limit_detection = True
  assert not run_speed_limit_vision(False, None, None, toggles)
  assert run_speed_limit_vision(True, None, None, toggles)
  assert managed_processes["speed_limit_vision"].should_run is run_speed_limit_vision


def test_runtime_subscription_accepts_messages_at_its_loop_rate(mocker):
  from cereal import messaging
  from openpilot.frogpilot.system.speed_limit_vision import RUNTIME_LOOP_HZ, SpeedLimitVisionDaemon, main

  run = mocker.patch.object(SpeedLimitVisionDaemon, "run", autospec=True)
  main()
  sm = run.call_args.args[0].sm
  for tick in range(RUNTIME_LOOP_HZ + 1):
    messages = [messaging.new_message("carState", valid=True).as_reader()]
    if tick % (RUNTIME_LOOP_HZ // 2) == 0:
      messages.append(messaging.new_message("deviceState", valid=True).as_reader())
    sm.update_msgs(100 + tick / RUNTIME_LOOP_HZ, messages)
  assert sm.all_checks(["deviceState", "carState"])


def test_real_camera_models_and_params_reach_controller(controller, mocker):
  from pathlib import Path

  import cv2
  import numpy as np
  from cereal import messaging
  from msgq.visionipc import VisionIpcClient, VisionIpcServer, VisionStreamType
  from openpilot.common.params import Params
  from openpilot.frogpilot.system.speed_limit_vision import RUNTIME_LOOP_HZ, SpeedLimitVisionDaemon

  params = Params(memory=True)
  controller.frogpilot_planner.params_memory = params
  services = ["deviceState", "carState", "mapdOut"]
  pm = messaging.PubMaster(services)
  sm = messaging.SubMaster(services, frequency=RUNTIME_LOOP_HZ)
  daemon = SpeedLimitVisionDaemon(params, sm, VisionIpcClient, VisionStreamType, mocker.Mock())
  clock = mocker.patch("time.monotonic", return_value=100.0)

  folder = Path(__file__).parents[2] / "system" / "tests" / "fixtures" / "vision_speed_limit"
  frames = []
  for number in (140, 146):
    frame = cv2.imread(str(folder / f"sign_20_frame_{number}.png"))
    height, width = frame.shape[:2]
    # Exercise the actual padded NV12 layout delivered through VisionIPC.
    stride = width + 16
    uv_offset = stride * (height + 8)
    buffer = np.zeros(uv_offset + height // 2 * stride, dtype=np.uint8)
    planar = cv2.cvtColor(frame, cv2.COLOR_BGR2YUV_I420).ravel()
    y_size = height * width
    buffer[:height * stride].reshape(height, stride)[:, :width] = planar[:y_size].reshape(height, width)
    chroma = buffer[uv_offset:].reshape(height // 2, stride)[:, :width]
    chroma[:, 0::2] = planar[y_size:y_size * 5 // 4].reshape(height // 2, width // 2)
    chroma[:, 1::2] = planar[y_size * 5 // 4:].reshape(height // 2, width // 2)
    frames.append(buffer)

  stream = VisionStreamType.VISION_STREAM_ROAD
  server = VisionIpcServer("camerad")
  server.create_buffers_with_sizes(stream, 4, width, height, len(frames[0]), stride, uv_offset)
  server.start_listener()
  try:
    assert stream in VisionIpcClient.available_streams("camerad", block=True)
    for tick in range(RUNTIME_LOOP_HZ + 1):
      now = 100 + tick / RUNTIME_LOOP_HZ
      clock.return_value = now
      car = messaging.new_message("carState", valid=True)
      car.carState.gearShifter = "drive"
      pm.send("carState", car)
      if tick % (RUNTIME_LOOP_HZ // 2) == 0:
        device = messaging.new_message("deviceState", valid=True)
        device.deviceState.started = True
        device.deviceState.memoryUsagePercent = 30
        device.deviceState.cpuUsagePercent = [10] * 8
        pm.send("deviceState", device)
      if tick <= RUNTIME_LOOP_HZ // 2:
        sm.update(0)
        if tick < RUNTIME_LOOP_HZ // 2:
          continue
        assert sm.all_checks(["deviceState", "carState"])
        assert daemon.connect_camera()
      server.send(stream, frames[tick % 2], frame_id=tick, timestamp_eof=int(now * 1e9))
      daemon.step(now)

    assert sm.all_checks(["deviceState", "carState"])
    assert params.get(VISION_SPEED_LIMIT_PARAM)["speedLimit"] == pytest.approx(20 * CV.MPH_TO_MS)
    update(controller)
    assert controller.source == "Vision"
    assert controller.target == pytest.approx(20 * CV.MPH_TO_MS)

    clock.return_value += HEARTBEAT_TIMEOUT + 1
    update(controller, SubMaster(dashboard=0, map_limit=0))
    assert controller.target == 0
  finally:
    daemon.clear("Stopped", disconnect=True)
    del server


def test_typed_param_roundtrip_and_drive_transition(tmp_path):
  from openpilot.common.params import ParamKeyFlag, Params

  params = Params(str(tmp_path), return_defaults=True)
  assert not params.get_bool("VisionSpeedLimitDetection")
  snapshot = {"speedLimit": 20.0, "confidence": 0.95, "detectedAt": 99.0, "timestamp": 100.0}
  params.put(VISION_SPEED_LIMIT_PARAM, snapshot)
  assert params.get(VISION_SPEED_LIMIT_PARAM) == snapshot
  params.clear_all(ParamKeyFlag.CLEAR_ON_OFFROAD_TRANSITION)
  assert params.get(VISION_SPEED_LIMIT_PARAM) == {}
