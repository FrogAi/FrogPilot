from cereal import car
from opendbc.can.packer import CANPacker
from openpilot.selfdrive.car import gen_empty_fingerprint
from openpilot.selfdrive.car.car_helpers import interfaces
from openpilot.selfdrive.car.mazda.values import CAR, DBC
from openpilot.selfdrive.car.tests.test_car_interfaces import Toggles
from openpilot.selfdrive.pandad import can_list_to_can_capnp

ButtonType = car.CarState.ButtonEvent.Type


class Drive:
  def __init__(self, always_on_lateral=True):
    CarInterface, CarController, CarState = interfaces[CAR.MAZDA_CX5_2022]

    self.cancels = 0

    self.toggles = Toggles(always_on_lateral=always_on_lateral)

    CP = CarInterface.get_params(CAR.MAZDA_CX5_2022, gen_empty_fingerprint(), [], False, self.toggles, False)
    FPCP = CarInterface.get_frogpilot_params(CAR.MAZDA_CX5_2022, gen_empty_fingerprint(), [], CP, self.toggles)

    self.packer = CANPacker(DBC[CAR.MAZDA_CX5_2022]["pt"])

    self.CI = CarInterface(CP, FPCP, CarController, CarState)

  def step(self, controls_ready=True, crz_active=0, crz_available=1, crz_ready=1, lane_lines=2, lkas_button=0):
    can_msgs = [
      self.packer.make_can_msg("CAM_LANEINFO", 2, {"LANE_LINES": lane_lines}),
      self.packer.make_can_msg("CRZ_CTRL", 0, {"CRZ_ACTIVE": crz_active, "CRZ_AVAILABLE": crz_available, "CRZ_READY": crz_ready}),
      self.packer.make_can_msg("MSG_05", 0, {"LKAS_BUTTON": lkas_button}),
    ]
    CC = car.CarControl.new_message().as_reader()

    carState, _ = self.CI.update(CC, [can_list_to_can_capnp(can_msgs)], self.toggles)

    if controls_ready:
      _, can_sends = self.CI.apply(CC, 0, self.toggles)

      self.cancels += sum(addr == 0x09D and dat[0] == 0x01 for addr, _, dat, _ in can_sends)

    return carState


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
