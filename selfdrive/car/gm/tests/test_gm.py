import cereal.messaging as messaging

from cereal import car, custom, log
from collections import defaultdict
from parameterized import param, parameterized
from types import SimpleNamespace

from opendbc.can.packer import CANPacker
from panda import ALTERNATIVE_EXPERIENCE
from openpilot.common.conversions import Conversions as CV
from openpilot.selfdrive.car import gen_empty_fingerprint
from openpilot.selfdrive.car.card import Car
from openpilot.selfdrive.car.gm import gmcan
from openpilot.selfdrive.car.gm.carcontroller import CarController
from openpilot.selfdrive.car.gm.carstate import CarState
from openpilot.selfdrive.car.gm.fingerprints import FINGERPRINTS
from openpilot.selfdrive.car.gm.interface import CAM_MSG, CarInterface
from openpilot.selfdrive.car.gm.values import CAMERA_ACC_CAR, DBC, GM_RX_OFFSET, CanBus, CruiseButtons
from openpilot.selfdrive.car.tests.test_car_interfaces import Toggles

CAMERA_DIAGNOSTIC_ADDRESS = 0x24b


class TestGMFingerprint:
  @parameterized.expand(FINGERPRINTS.items())
  def test_can_fingerprints(self, car_model, fingerprints):
    assert len(fingerprints) > 0

    assert all(len(finger) for finger in fingerprints)

    # The camera can sometimes be communicating on startup
    if car_model in CAMERA_ACC_CAR:
      for finger in fingerprints:
        for required_addr in (CAMERA_DIAGNOSTIC_ADDRESS, CAMERA_DIAGNOSTIC_ADDRESS + GM_RX_OFFSET):
          assert finger.get(required_addr) == 8, required_addr


# FrogPilot variables
class TestGMMainOffAtStart:
  def _main_presses(self, car_model, steps, always_on_lateral=True, live_toggle=None):
    toggles = Toggles(always_on_lateral=always_on_lateral if live_toggle is None else live_toggle)
    CP = CarInterface.get_params(car_model, gen_empty_fingerprint(), [], False, toggles, False)
    if always_on_lateral:
      CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL
    controller = CarController(None, CP, None)

    CS = SimpleNamespace(buttons_counter=2, cam_lka_steering_cmd_counter=0, cruise_main_off_seen=False, loopback_lka_steering_cmd_ts_nanos=0,
                         loopback_lka_steering_cmd_updated=False, out=car.CarState.new_message(), pscm_status=defaultdict(int), pt_lka_steering_cmd_counter=0)

    presses = []
    for frame, (panda_ready, cruise_main, engaged, cruise_active) in enumerate(steps):
      CS.cruise_main_off_seen |= not cruise_main
      CS.loopback_lka_steering_cmd_ts_nanos = int(panda_ready)
      CS.out.cruiseState.available = cruise_main
      CS.out.cruiseState.enabled = cruise_active
      CC = car.CarControl.new_message(enabled=engaged).as_reader()

      _, can_sends = controller.update(CC, CS, 0, toggles)
      for addr, _, dat, bus in can_sends:
        if addr == 0x1E1 and (dat[5] >> 4) & 0x7 == CruiseButtons.MAIN:
          presses.append((frame, bus, dat[4] & 0x3))

    return presses

  def test_main_off_once(self):
    steps = [(False, True, False, False), (True, True, False, False), (True, True, False, False), (True, False, False, False), (True, True, False, False)]

    assert self._main_presses("CHEVROLET_BOLT_EUV", steps) == [(1, 0, 3)]
    assert self._main_presses("CHEVROLET_BOLT_EUV", steps, always_on_lateral=False) == []

  def test_main_off_follows_safety_mode(self):
    steps = [(True, True, False, False)]

    assert self._main_presses("CHEVROLET_BOLT_EUV", steps, always_on_lateral=False, live_toggle=True) == []
    assert self._main_presses("CHEVROLET_BOLT_EUV", steps, always_on_lateral=True, live_toggle=False) == [(0, 0, 3)]

  @parameterized.expand([("cruise_active", (False, True, False, True)), ("engaged", (False, True, True, False)), ("main_off", (False, False, False, False))])
  def test_no_main_off(self, _, before_panda_ready):
    steps = [before_panda_ready, (True, True, False, False)]

    assert self._main_presses("CHEVROLET_BOLT_EUV", steps) == []

  @parameterized.expand([("CADILLAC_XT4", False), ("CHEVROLET_BOLT_CC", True), ("CHEVROLET_BOLT_EUV", True), ("CHEVROLET_SUBURBAN_CC", True),
                         ("CHEVROLET_VOLT", False), ("GMC_YUKON_CC", True)])
  def test_platforms(self, car_model, sends):
    assert bool(self._main_presses(car_model, [(True, True, False, False)])) == sends


class TestGMButtons:
  # The cars' own ASCMSteeringButton frames by (button, counter): a 2017 Yukon XL without ACC (commaCarSegments
  # b513af7447934108/00000007--fc8cb7ced6/20) and a Yukon with ACC (36c62b5da6f08154|00000052--983c17c5b2/0)
  K2_CC_FRAMES = {
    (1, 0): "000000010018df", (1, 1): "00000001011dce", (1, 2): "000000010212fd", (1, 3): "000000010317ec",
    (2, 0): "000000010028cf", (2, 1): "00000001012dbe", (2, 2): "000000010222ed", (2, 3): "000000010327dc",
    (3, 0): "000000010038bf", (3, 1): "00000001013dae", (3, 2): "000000010232dd", (3, 3): "000000010337cc",
  }
  YUKON_FRAMES = {(1, 0): "000000010010ff", (1, 1): "000000010115ee", (1, 2): "00000001021add", (1, 3): "00000001031fcc"}

  @parameterized.expand([("CHEVROLET_SUBURBAN_CC", K2_CC_FRAMES), ("GMC_YUKON_CC", K2_CC_FRAMES), ("CHEVROLET_SUBURBAN", YUKON_FRAMES)])
  def test_car_frames(self, car_model, frames):
    CP = car.CarParams.new_message(carFingerprint=car_model)
    packer = CANPacker(DBC[car_model]["pt"])

    for (button, counter), frame in frames.items():
      _, _, dat, _ = gmcan.create_buttons(packer, CP, CanBus.POWERTRAIN, float(counter), button)
      assert dat.hex() == frame, (button, counter)


class TestGMCSLC:
  @parameterized.expand([
    param("experimental_mode", [CruiseButtons.RES_ACCEL], cruise_speed=60, experimental_mode=True, v_ego=70),
    param("experimental_mode_above_set_speed", [CruiseButtons.DECEL_SET], accel=1.0, cruise_speed=70, experimental_mode=True),
    param("experimental_mode_accelerating", [], accel=1.0, experimental_mode=True),
    param("experimental_mode_planned_speed", [CruiseButtons.RES_ACCEL], accel=1.0, cruise_speed=55, experimental_mode=True, planned_speed=55, v_ego=50),
    param("gas", [CruiseButtons.DECEL_SET], cruise_speed=60, gas_pressed=True, long_active=False, v_ego=62),
    param("paused_experimental_mode", [], cruise_speed=60, experimental_mode=True, long_active=False, paused=True, v_ego=70),
    param("paused_forced_deceleration", [], accel=-1.0, cruise_speed=55, paused=True, v_ego=55),
    param("paused_forced_deceleration_experimental_mode", [CruiseButtons.DECEL_SET], accel=-1.0, experimental_mode=True, paused=True),
    param("paused_gas", [], cruise_speed=60, gas_pressed=True, long_active=False, paused=True, v_ego=62),
    param("paused_planned_speed", [], long_active=False, paused=True, planned_speed=55),
    param("planned_speed", [CruiseButtons.DECEL_SET], planned_speed=55),
  ])
  def test_buttons(self, _, buttons, accel=0.0, cruise_speed=65, experimental_mode=False, gas_pressed=False, long_active=True, paused=False,
                   planned_speed=65, v_ego=65):
    toggles = Toggles(cslc=True)
    fingerprint = gen_empty_fingerprint()
    fingerprint[CanBus.CAMERA][CAM_MSG] = 8
    CP = CarInterface.get_params("CHEVROLET_SILVERADO", fingerprint, [], False, toggles, False)

    card = Car(CI=CarInterface(CP, custom.FrogPilotCarParams.new_message(), CarController, CarState))
    card.CI.CS = SimpleNamespace(buttons_counter=0, cam_lka_steering_cmd_counter=0, CP=CP, cruise_buttons=CruiseButtons.UNPRESS, cruise_main_off_seen=False,
                                 distance_button=0, loopback_lka_steering_cmd_ts_nanos=0, loopback_lka_steering_cmd_updated=False,
                                 out=car.CarState.new_message(gasPressed=gas_pressed, vEgo=v_ego * CV.MPH_TO_MS), pscm_status=defaultdict(int),
                                 pt_lka_steering_cmd_counter=0)
    card.CI.CS.out.cruiseState.speed = cruise_speed * CV.MPH_TO_MS
    card.frogpilot_card.pause_longitudinal = paused
    card.frogpilot_toggles = toggles
    card.initialized_prev = True
    card.sm.alive["carControl"] = True
    card.sm.data["controlsState"] = log.ControlsState.new_message(experimentalMode=experimental_mode)
    card.sm.data["frogpilotPlan"] = custom.FrogPilotPlan.new_message(vCruise=planned_speed * CV.MPH_TO_MS)

    CC = car.CarControl.new_message(enabled=True, longActive=long_active)
    CC.actuators.accel = accel
    CC.hudControl.setSpeed = 65 * CV.MPH_TO_MS

    sendcan_sock = messaging.sub_sock("sendcan")

    card.controls_update(car.CarState.new_message(canValid=True), CC.as_reader())

    sendcan = messaging.recv_one_or_none(sendcan_sock).sendcan
    assert sorted({(msg.dat[5] >> 4) & 0x7 for msg in sendcan if msg.address == 0x1E1}) == buttons
