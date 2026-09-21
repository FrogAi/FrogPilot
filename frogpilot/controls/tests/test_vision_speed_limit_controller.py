from datetime import datetime, UTC
from types import SimpleNamespace

import pytest

from openpilot.common.constants import CV
from openpilot.frogpilot.common.tests.vision_helpers import MemoryParams
from openpilot.frogpilot.common.vision_speed_limit import HEARTBEAT_TIMEOUT, VISION_SPEED_LIMIT_PARAM, Detection, SpeedLimitConfirmation
from openpilot.frogpilot.controls.lib.speed_limit_controller import SpeedLimitController


@pytest.fixture
def controller(mocker):
  planner = SimpleNamespace(params=MemoryParams(), params_memory=MemoryParams(), gps_valid=True)
  planner.params.put("PreviousSpeedLimit", 0.0)
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
  vcruise.vision_cruise_cap = None
  vcruise.previous_cruise_setting = None
  toggles = controller.frogpilot_toggles
  toggles.speed_limit_controller = control_enabled
  toggles.show_speed_limits = True
  toggles.curve_speed_controller = False
  sm = SubMaster(vCruiseCluster=cruise_speed * CV.MS_TO_KPH, vEgoCluster=20.0)
  target = vcruise.update(True, datetime(2026, 1, 1, tzinfo=UTC), False, cruise_speed, 20.0, sm, toggles)
  expected = min(cruise_speed, 45 * CV.MPH_TO_MS + controller.offset) if control_enabled else cruise_speed
  assert target == pytest.approx(expected)
  assert vcruise.slc_offset == (controller.offset if control_enabled else 0)


@pytest.fixture
def cruise(controller, mocker):
  from openpilot.frogpilot.controls.lib.frogpilot_vcruise import FrogPilotVCruise

  controller.frogpilot_planner.gps_position = {}
  mocker.patch("openpilot.frogpilot.controls.lib.frogpilot_vcruise.CurveSpeedController")
  mocker.patch("openpilot.frogpilot.controls.lib.frogpilot_vcruise.SpeedLimitController", return_value=controller)
  vcruise = FrogPilotVCruise(controller.frogpilot_planner)
  vcruise.update_force_stop = mocker.Mock()
  controller.frogpilot_toggles.show_speed_limits = True
  controller.frogpilot_toggles.curve_speed_controller = False
  for i in range(1, 8):
    setattr(controller.frogpilot_toggles, f"speed_limit_offset{i}", 0.0)
  return vcruise


def cruise_update(cruise, sm, setting=70, engaged=True, speed=45):
  sm["carControl"].longActive = engaged
  sm["carState"].vCruiseCluster = setting * CV.MPH_TO_KPH
  sm["carState"].vEgoCluster = speed * CV.MPH_TO_MS
  return cruise.update(engaged, datetime(2026, 1, 1, tzinfo=UTC), False,
                       setting * CV.MPH_TO_MS, speed * CV.MPH_TO_MS, sm,
                       cruise.slc.frogpilot_toggles) * CV.MS_TO_MPH


@pytest.mark.parametrize("replacement", [35, 45, 55])
@pytest.mark.parametrize("confirmation", [False, True])
def test_vision_handover_obeys_confirmation(cruise, mocker, replacement, confirmation):
  sm = SubMaster(dashboard=0, map_limit=replacement)
  toggles = cruise.slc.frogpilot_toggles
  toggles.speed_limit_confirmation_higher = confirmation
  toggles.speed_limit_confirmation_lower = confirmation
  confirmation = confirmation and replacement != 45
  assert cruise_update(cruise, sm) == pytest.approx(45)
  mocker.patch("time.monotonic", return_value=104.0)
  for _ in range(3):
    assert cruise_update(cruise, sm) == pytest.approx(45 if confirmation else replacement)
  if confirmation:
    assert cruise.slc.source == "None"
    assert cruise.slc.vision_speed_limit == 0
    assert cruise.slc.unconfirmed_speed_limit * CV.MS_TO_MPH == pytest.approx(replacement)
    cruise.slc.frogpilot_planner.params_memory.values["SpeedLimitAccepted"] = True
    assert cruise_update(cruise, sm) == pytest.approx(replacement)
  assert cruise.slc.source == "Map Data"


def test_vision_loss_retains_control_cap_without_resurrecting_limit(cruise, mocker):
  sm = SubMaster(dashboard=0, map_limit=0)
  assert cruise_update(cruise, sm) == pytest.approx(45)
  mocker.patch("time.monotonic", return_value=104.0)
  for _ in range(3):
    assert cruise_update(cruise, sm) == pytest.approx(45)
    assert cruise.slc.target == cruise.slc.vision_speed_limit == 0
    assert cruise.slc.source == "None"
  assert cruise_update(cruise, sm, setting=40) == pytest.approx(40)
  assert cruise_update(cruise, sm, setting=42) == pytest.approx(42)
  assert cruise.vision_cruise_cap is None


def test_vision_loss_cap_ends_on_disengagement(cruise, mocker):
  sm = SubMaster(dashboard=0, map_limit=0)
  cruise_update(cruise, sm)
  mocker.patch("time.monotonic", return_value=104.0)
  assert cruise_update(cruise, sm) == pytest.approx(45)
  cruise_update(cruise, sm, engaged=False)
  assert cruise.vision_cruise_cap is None
  assert cruise.slc.previous_source == "None"
  assert cruise_update(cruise, sm) == pytest.approx(70)


@pytest.mark.parametrize("manual,set_speed,expected", [(True, False, 50), (False, True, 70), (False, False, 45)])
def test_vision_loss_respects_gas_override_mode(cruise, mocker, manual, set_speed, expected):
  sm = SubMaster(dashboard=0, map_limit=0)
  cruise_update(cruise, sm)
  mocker.patch("time.monotonic", return_value=104.0)
  cruise.slc.frogpilot_toggles.speed_limit_controller_override_manual = manual
  cruise.slc.frogpilot_toggles.speed_limit_controller_override_set_speed = set_speed
  sm["carState"].gasPressed = True
  assert cruise_update(cruise, sm, speed=50) == pytest.approx(expected)
  sm["carState"].gasPressed = False
  assert cruise_update(cruise, sm, speed=50) == pytest.approx(expected)


def test_denied_handover_cannot_release_vision_control_cap(cruise, mocker):
  sm = SubMaster(dashboard=0, map_limit=55)
  cruise.slc.frogpilot_toggles.speed_limit_confirmation_higher = True
  cruise_update(cruise, sm)
  mocker.patch("time.monotonic", return_value=104.0)
  cruise_update(cruise, sm)
  sm["frogpilotCarState"].decelPressed = True
  assert cruise_update(cruise, sm) == pytest.approx(45)
  sm["frogpilotCarState"].decelPressed = False
  assert cruise_update(cruise, sm) == pytest.approx(45)
  assert cruise.slc.denied_target * CV.MS_TO_MPH == pytest.approx(55)


def test_pending_handover_cancels_when_replacement_disappears(cruise, mocker):
  sm = SubMaster(dashboard=0, map_limit=55)
  cruise.slc.frogpilot_toggles.speed_limit_confirmation_higher = True
  cruise_update(cruise, sm)
  mocker.patch("time.monotonic", return_value=104.0)
  cruise_update(cruise, sm)
  cruise.slc.frogpilot_planner.params_memory.values["SpeedLimitAccepted"] = True
  sm["mapdOut"].speedLimit = 0
  assert cruise_update(cruise, sm) == pytest.approx(45)
  assert cruise.slc.unconfirmed_speed_limit == 0
  assert not cruise.slc.frogpilot_planner.params_memory.get_bool("SpeedLimitAccepted")


@pytest.mark.parametrize("confirmation", [False, True])
def test_new_vision_read_after_expiry_obeys_increase_setting(cruise, mocker, confirmation):
  sm = SubMaster(dashboard=0, map_limit=0)
  cruise.slc.frogpilot_toggles.speed_limit_confirmation_higher = confirmation
  cruise_update(cruise, sm)
  mocker.patch("time.monotonic", return_value=104.0)
  assert cruise_update(cruise, sm) == pytest.approx(45)
  state = SpeedLimitConfirmation()
  for now in (103.5, 103.8):
    state.update(Detection(55, 0.95), now, now)
  cruise.slc.frogpilot_planner.params_memory.put(VISION_SPEED_LIMIT_PARAM, state.snapshot(104))
  assert cruise_update(cruise, sm) == pytest.approx(45 if confirmation else 55)
  if confirmation:
    sm["frogpilotCarState"].accelPressed = True
    assert cruise_update(cruise, sm) == pytest.approx(55)
  assert cruise.slc.source == "Vision"


@pytest.mark.parametrize("action", ["accel", "disable_controller"])
def test_driver_can_release_vision_loss_cap(cruise, mocker, action):
  sm = SubMaster(dashboard=0, map_limit=0)
  cruise_update(cruise, sm)
  mocker.patch("time.monotonic", return_value=104.0)
  assert cruise_update(cruise, sm) == pytest.approx(45)
  if action == "accel":
    sm["frogpilotCarState"].accelPressed = True
  else:
    cruise.slc.frogpilot_toggles.speed_limit_controller = False
  assert cruise_update(cruise, sm) == pytest.approx(70)
  assert cruise.vision_cruise_cap is None


def test_curve_target_does_not_become_a_sticky_vision_cap(cruise, mocker):
  sm = SubMaster(dashboard=0, map_limit=0)
  cruise.slc.frogpilot_toggles.curve_speed_controller = True
  cruise.csc.update_target.return_value = 25 * CV.MPH_TO_MS
  assert cruise_update(cruise, sm) == pytest.approx(25)
  mocker.patch("time.monotonic", return_value=104.0)
  cruise.csc.update_target.return_value = 70 * CV.MPH_TO_MS
  assert cruise_update(cruise, sm) == pytest.approx(45)


def test_mapbox_handover_cannot_bypass_confirmation(cruise, mocker):
  sm = SubMaster(dashboard=0, map_limit=0)
  cruise.slc.frogpilot_toggles.speed_limit_confirmation_higher = True
  cruise_update(cruise, sm)
  mocker.patch.object(cruise.slc, "update_mapbox_speed_limit")
  cruise.slc.mapbox_speed_limit = 55 * CV.MPH_TO_MS
  mocker.patch("time.monotonic", return_value=104.0)
  assert cruise_update(cruise, sm) == pytest.approx(45)
  assert cruise.slc.confirmation_source == "Mapbox"
  cruise.slc.frogpilot_planner.params_memory.put("SpeedLimitAccepted", True)
  # Same number from a different source requires a new prompt.
  sm["mapdOut"].speedLimit = 55 * CV.MPH_TO_MS
  assert cruise_update(cruise, sm) == pytest.approx(45)
  assert cruise.slc.confirmation_source == "Map Data"
  assert not cruise.slc.frogpilot_planner.params_memory.get_bool("SpeedLimitAccepted")


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
    messages = [messaging.new_message("frogpilotCarState", valid=True).as_reader()]
    if tick % (RUNTIME_LOOP_HZ // 2) == 0:
      messages.append(messaging.new_message("deviceState", valid=True).as_reader())
    sm.update_msgs(100 + tick / RUNTIME_LOOP_HZ, messages)
  assert sm.all_checks(["deviceState", "frogpilotCarState"])


def test_vision_worker_preserves_existing_car_state_readers(mocker):
  from cereal import messaging
  from openpilot.frogpilot.system.speed_limit_vision import SpeedLimitVisionDaemon, main

  # FrogPilot fills all 15 slots. A sixteenth reader evicts existing consumers.
  pm = messaging.PubMaster(["carState"])
  readers = [messaging.sub_sock("carState", conflate=True) for _ in range(15)]
  mocker.patch.object(SpeedLimitVisionDaemon, "run")
  main()
  for timestamp in range(1, 4):
    message = messaging.new_message("carState", valid=True)
    message.logMonoTime = timestamp
    pm.send("carState", message)
    for reader in readers:
      received = messaging.recv_one_or_none(reader)
      assert received is not None and received.logMonoTime == timestamp


@pytest.mark.parametrize("gear", ["drive", "low", "park", "reverse", "neutral", "unknown"])
@pytest.mark.parametrize("can_valid", [True, False])
def test_auxiliary_car_state_preserves_gear_and_validity(mocker, gear, can_valid):
  from cereal import car, custom
  from openpilot.selfdrive.car.card import Car

  publisher = Car.__new__(Car)
  publisher.sm = SimpleNamespace(frame=1, all_checks=lambda _: True)
  publisher.pm = mocker.Mock()
  publisher.rk = SimpleNamespace(remaining=0.0)
  publisher.last_actuators_output = car.CarControl.Actuators.new_message()
  publisher.can_rcv_cum_timeout_counter = 0
  state = car.CarState.new_message(gearShifter=gear, canValid=can_valid)
  publisher.state_publish(state, None, custom.FrogPilotCarState.new_message())
  messages = {call.args[0]: call.args[1] for call in publisher.pm.send.call_args_list}
  assert messages["frogpilotCarState"].valid == messages["carState"].valid == can_valid
  assert messages["frogpilotCarState"].frogpilotCarState.drivingGear == (gear in ("drive", "low"))


def test_real_camera_school_sign_cannot_supply_controller_limit(controller, mocker):
  from pathlib import Path

  import cv2
  import numpy as np
  from cereal import messaging
  from msgq.visionipc import VisionIpcClient, VisionIpcServer, VisionStreamType
  from openpilot.common.params import Params
  from openpilot.frogpilot.system.speed_limit_vision import RUNTIME_LOOP_HZ, SpeedLimitVisionDaemon

  params = Params(memory=True)
  controller.frogpilot_planner.params_memory = params
  services = ["deviceState", "frogpilotCarState", "mapdOut"]
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
      car = messaging.new_message("frogpilotCarState", valid=True)
      car.frogpilotCarState.drivingGear = True
      pm.send("frogpilotCarState", car)
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
        assert sm.all_checks(["deviceState", "frogpilotCarState"])
        assert daemon.connect_camera()
      server.send(stream, frames[tick % 2], frame_id=tick, timestamp_eof=int(now * 1e9))
      daemon.step()

    assert sm.all_checks(["deviceState", "frogpilotCarState"])
    assert params.get(VISION_SPEED_LIMIT_PARAM)["speedLimit"] == 0
    update(controller, SubMaster(dashboard=0, map_limit=0))
    assert controller.source == "None"
    assert controller.target == 0

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
