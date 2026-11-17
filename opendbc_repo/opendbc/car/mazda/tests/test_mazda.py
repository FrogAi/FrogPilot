from opendbc.can import CANPacker
from opendbc.car import DT_CTRL, Bus, gen_empty_fingerprint, structs
from opendbc.car.mazda.carstate import STANDSTILL_LOCKOUT_TIME
from opendbc.car.mazda.interface import CarInterface
from opendbc.car.mazda.values import CAR, DBC, CarControllerParams
from opendbc.car.tests.test_car_interfaces import Toggles
from opendbc.safety import ALTERNATIVE_EXPERIENCE


class Drive:
  def __init__(self, always_on_lateral=True, live_toggle=None, platform=CAR.MAZDA_CX5_2022):
    self.cancels = 0
    self.nanos = 0
    self.torque = 0

    if live_toggle is None:
      live_toggle = always_on_lateral

    self.toggles = Toggles(always_on_lateral=live_toggle)

    CP = CarInterface.get_non_essential_params(platform)
    if always_on_lateral:
      CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL
    FPCP = CarInterface.get_frogpilot_params(platform, gen_empty_fingerprint(), [], CP, self.toggles)

    self.CI = CarInterface(CP, FPCP)
    self.CI.update([], self.toggles)
    self.packer = CANPacker(DBC[platform][Bus.pt])

  def step(self, crz_active=0, crz_available=1, crz_ready=1, lane_lines=2, lat_active=False, lkas_block=0, lkas_button=0, speed=0):
    self.nanos += 10_000_000

    frames = [
      self.packer.make_can_msg("CAM_LANEINFO", 2, {"LANE_LINES": lane_lines}),
      self.packer.make_can_msg("CRZ_CTRL", 0, {"CRZ_ACTIVE": crz_active, "CRZ_AVAILABLE": crz_available, "CRZ_READY": crz_ready}),
      self.packer.make_can_msg("ENGINE_DATA", 0, {"SPEED": speed}),
      self.packer.make_can_msg("MSG_05", 0, {"LKAS_BUTTON": lkas_button}),
      self.packer.make_can_msg("STEER_RATE", 0, {"LKAS_BLOCK": lkas_block}),
      self.packer.make_can_msg("WHEEL_SPEEDS", 0, {"FL": speed, "FR": speed, "RL": speed, "RR": speed}),
    ]
    CC = structs.CarControl(actuators=structs.CarControl.Actuators(torque=1.0), latActive=lat_active).as_reader()

    carState, _ = self.CI.update([(self.nanos, frames)], self.toggles)
    actuators, can_sends = self.CI.apply(CC, self.nanos, self.toggles)

    self.cancels += sum(addr == 0x09D and dat[0] == 0x01 for addr, dat, _ in can_sends)
    self.torque = actuators.torqueOutputCan

    return carState


def lkas_events(carState):
  return [be.pressed for be in carState.buttonEvents if be.type == structs.CarState.ButtonEvent.Type.lkas]


def test_lkas_button():
  drive = Drive()
  drive.step()

  assert lkas_events(drive.step(lane_lines=0)) == []
  assert lkas_events(drive.step(lane_lines=0, lkas_button=1)) == [True]
  assert lkas_events(drive.step(lkas_button=1)) == []
  assert lkas_events(drive.step()) == [False]


def test_main_off_at_start():
  drive = Drive()

  for _ in range(20):
    drive.step()

  main_on = drive.cancels

  for _ in range(40):
    drive.step(crz_available=0)

  assert main_on == 2
  assert drive.cancels == main_on


def test_main_off_at_start_follows_safety_mode():
  for always_on_lateral, cancels in ((False, 0), (True, 2)):
    drive = Drive(always_on_lateral, live_toggle=not always_on_lateral)

    for _ in range(20):
      drive.step()

    assert drive.cancels == cancels, always_on_lateral


def test_main_off_at_start_gives_up():
  drive = Drive()

  for _ in range(400):
    drive.step()

  assert drive.cancels == 30


def test_main_off_at_start_skipped():
  for always_on_lateral, start in ((False, {}), (True, {"crz_available": 0}), (True, {"crz_active": 1})):
    drive = Drive(always_on_lateral)

    drive.step(**start)

    for _ in range(40):
      drive.step()

    assert drive.cancels == 0, (always_on_lateral, start)


def test_main_off_at_start_waits_for_ready():
  drive = Drive()

  for _ in range(5):
    drive.step(crz_available=0, crz_ready=0)

  for _ in range(20):
    drive.step()

  assert drive.cancels == 2


def test_standstill_lockout():
  drive = Drive()

  for _ in range(100):
    drive.step(crz_active=1, speed=30)

  locked_out = [drive.step(crz_active=1, lkas_block=1) for _ in range(300)]
  locked_out += [drive.step(crz_active=1, lkas_block=1, speed=10) for _ in range(310)]
  drive.step(crz_active=1, speed=10)
  hands_off = drive.step(crz_active=1, lkas_block=1, speed=10)

  assert not any(carState.lowSpeedAlert for carState in locked_out)
  assert not any(carState.steerFaultTemporary for carState in locked_out)
  assert hands_off.steerFaultTemporary


def test_standstill_lockout_min_steer_speed():
  drive = Drive(platform=CAR.MAZDA_CX9_2021)

  drive.step(lkas_block=1)
  drive.step(lat_active=True, lkas_block=1, speed=50)

  assert drive.torque > 0


def test_standstill_lockout_moving_start():
  drive = Drive()

  moving = drive.step(lkas_block=1, speed=10)

  assert moving.steerFaultTemporary


def test_standstill_lockout_timeout():
  drive = Drive()

  drive.step(lkas_block=1)

  locked_out = [drive.step(lkas_block=1, speed=10) for _ in range(round(STANDSTILL_LOCKOUT_TIME / DT_CTRL))]
  timed_out = drive.step(lkas_block=1, speed=10)

  assert not any(carState.steerFaultTemporary for carState in locked_out)
  assert timed_out.steerFaultTemporary


def test_standstill_lockout_torque():
  drive = Drive()

  drive.step(lkas_block=1)

  locked_out_torque = []
  for _ in range(310):
    drive.step(lat_active=True, lkas_block=1, speed=10)
    locked_out_torque.append(drive.torque)

  drive.step(lat_active=True, speed=10)

  assert not any(locked_out_torque)
  assert drive.torque == CarControllerParams.STEER_DELTA_UP
