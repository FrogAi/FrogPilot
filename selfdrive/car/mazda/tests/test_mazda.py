from cereal import car, log
from opendbc.can.packer import CANPacker
from panda import ALTERNATIVE_EXPERIENCE
from openpilot.common.realtime import DT_CTRL
from openpilot.selfdrive.car import gen_empty_fingerprint
from openpilot.selfdrive.car.car_helpers import interfaces
from openpilot.selfdrive.car.mazda.carstate import STANDSTILL_LOCKOUT_TIME
from openpilot.selfdrive.car.mazda.values import CAR, DBC, CarControllerParams
from openpilot.selfdrive.car.tests.test_car_interfaces import Toggles
from openpilot.selfdrive.controls.lib.events import EventName
from openpilot.selfdrive.pandad import can_list_to_can_capnp

ButtonType = car.CarState.ButtonEvent.Type


class Drive:
  def __init__(self, always_on_lateral=True, live_toggle=None, platform=CAR.MAZDA_CX5_2022):
    CarInterface, CarController, CarState = interfaces[platform]

    self.cancels = 0
    self.nanos = 0
    self.torque = 0

    self.toggles = Toggles(always_on_lateral=always_on_lateral if live_toggle is None else live_toggle)

    CP = CarInterface.get_params(platform, gen_empty_fingerprint(), [], False, self.toggles, False)
    if always_on_lateral:
      CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL
    FPCP = CarInterface.get_frogpilot_params(platform, gen_empty_fingerprint(), [], CP, self.toggles)

    self.packer = CANPacker(DBC[platform]["pt"])

    self.CI = CarInterface(CP, FPCP, CarController, CarState)

  def step(self, controls_ready=True, crz_active=0, crz_available=1, crz_ready=1, lane_lines=2, lat_active=False, lkas_block=0, lkas_button=0, speed=0):
    self.nanos += 10_000_000

    can_msgs = [
      self.packer.make_can_msg("CAM_LANEINFO", 2, {"LANE_LINES": lane_lines}),
      self.packer.make_can_msg("CRZ_CTRL", 0, {"CRZ_ACTIVE": crz_active, "CRZ_AVAILABLE": crz_available, "CRZ_READY": crz_ready}),
      self.packer.make_can_msg("ENGINE_DATA", 0, {"SPEED": speed}),
      self.packer.make_can_msg("MSG_05", 0, {"LKAS_BUTTON": lkas_button}),
      self.packer.make_can_msg("STEER_RATE", 0, {"LKAS_BLOCK": lkas_block}),
      self.packer.make_can_msg("WHEEL_SPEEDS", 0, {"FL": speed, "FR": speed, "RL": speed, "RR": speed}),
    ]
    CC = car.CarControl.new_message(actuators=car.CarControl.Actuators.new_message(steer=1.0), latActive=lat_active).as_reader()

    with log.Event.from_bytes(can_list_to_can_capnp(can_msgs)) as can:
      can_event = can.as_builder()
    can_event.logMonoTime = self.nanos

    carState, _ = self.CI.update(CC, [can_event.to_bytes()], self.toggles)

    if controls_ready:
      actuators, can_sends = self.CI.apply(CC, 0, self.toggles)

      self.cancels += sum(addr == 0x09D and dat[0] == 0x01 for addr, _, dat, _ in can_sends)
      self.torque = actuators.steerOutputCan

    return carState


def below_steer_speed(carState):
  return EventName.belowSteerSpeed in [event.name for event in carState.events]


def lkas_pressed(carState):
  return any(be.pressed and be.type == ButtonType.lkas for be in carState.buttonEvents)


def test_lkas_button():
  drive = Drive()
  drive.step()

  lkas_turned_off = drive.step(lane_lines=0)
  pressed = drive.step(lkas_button=1)
  held = drive.step(lkas_button=1)

  assert not lkas_pressed(lkas_turned_off)
  assert lkas_pressed(pressed)
  assert not lkas_pressed(held)


def test_main_off_at_start():
  drive = Drive()

  drive.step(controls_ready=False, crz_available=0, crz_ready=0)

  for _ in range(20):
    drive.step()

  main_on = drive.cancels

  drive.step(crz_available=0)

  for _ in range(40):
    drive.step()

  assert main_on == 2
  assert drive.cancels == main_on


def test_main_off_at_start_gives_up():
  drive = Drive()

  for _ in range(400):
    drive.step()

  assert drive.cancels == 30


def test_main_off_at_start_skipped():
  for always_on_lateral, start in ((False, {}), (True, {"controls_ready": False, "crz_available": 0}), (True, {"crz_active": 1})):
    drive = Drive(always_on_lateral)

    drive.step(**start)

    for _ in range(40):
      drive.step()

    assert drive.cancels == 0, (always_on_lateral, start)


def test_main_off_at_start_follows_safety_mode():
  for always_on_lateral, cancels in ((False, 0), (True, 2)):
    drive = Drive(always_on_lateral, live_toggle=not always_on_lateral)

    for _ in range(20):
      drive.step()

    assert drive.cancels == cancels, always_on_lateral


def test_standstill_lockout():
  drive = Drive()

  for _ in range(100):
    drive.step(crz_active=1, speed=30)

  locked_out = [drive.step(crz_active=1, lkas_block=1) for _ in range(300)]
  locked_out += [drive.step(crz_active=1, lkas_block=1, speed=10) for _ in range(310)]
  drive.step(crz_active=1, speed=10)
  hands_off = drive.step(crz_active=1, lkas_block=1, speed=10)

  assert not any(below_steer_speed(carState) for carState in locked_out)
  assert not any(carState.steerFaultTemporary for carState in locked_out)
  assert hands_off.steerFaultTemporary


def test_standstill_lockout_min_steer_speed():
  drive = Drive(platform=CAR.MAZDA_CX9_2021)

  drive.step(lkas_block=1)
  drive.step(lat_active=True, lkas_block=1, speed=50)

  assert drive.torque > 0


def test_standstill_lockout_moving_start():
  drive = Drive()

  drive.CI.update(car.CarControl.new_message().as_reader(), [], drive.toggles)
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
