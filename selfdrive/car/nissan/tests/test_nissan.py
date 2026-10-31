import types

from cereal import car
from collections import defaultdict
from opendbc.can.packer import CANPacker
from openpilot.selfdrive.car import gen_empty_fingerprint
from openpilot.selfdrive.car.car_helpers import get_car_interface
from openpilot.selfdrive.car.nissan.carcontroller import CarController
from openpilot.selfdrive.car.nissan.interface import CarInterface
from openpilot.selfdrive.car.nissan.values import CAR, DBC
from openpilot.selfdrive.car.tests.test_car_interfaces import TOGGLES
from openpilot.selfdrive.pandad import can_list_to_can_capnp

ButtonType = car.CarState.ButtonEvent.Type


def test_dashboard():
  CP = car.CarParams.new_message(carFingerprint=CAR.NISSAN_ROGUE)
  CS = types.SimpleNamespace(lkas_hud_info_msg=defaultdict(int), lkas_hud_msg=defaultdict(int), out=car.CarState.new_message(vEgoRaw=20.0))

  for enabled, lat_active, icon in ((False, False, 0), (False, True, 2), (True, False, 2)):
    CC = car.CarControl.new_message(enabled=enabled, latActive=lat_active).as_reader()

    _, can_sends = CarController(DBC[CAR.NISSAN_ROGUE]["pt"], CP, None).update(CC, CS, 0, None)
    hud = next(dat for address, _, dat, _ in can_sends if address == 0x2B1)

    assert (int.from_bytes(hud, "little") >> 60) & 3 == icon


def test_lkas_switch_presses():
  CP = CarInterface.get_params(CAR.NISSAN_ROGUE, gen_empty_fingerprint(), [], False, TOGGLES, docs=False)
  FPCP = CarInterface.get_frogpilot_params(CAR.NISSAN_ROGUE, gen_empty_fingerprint(), [], CP, TOGGLES)
  CI = get_car_interface(CP, FPCP)
  packer = CANPacker(DBC[CAR.NISSAN_ROGUE]["pt"])

  presses = []
  for frame, lkas_enabled in enumerate((0, 0, 1, 1, 0, 0)):
    can = can_list_to_can_capnp([packer.make_can_msg("LKAS_SETTINGS", 2, {"LKAS_ENABLED": lkas_enabled})])
    CS, _ = CI.update(car.CarControl.new_message(), [can], TOGGLES)
    presses += [(frame, be.pressed) for be in CS.buttonEvents if be.type == ButtonType.lkas]

  assert presses == [(2, True), (3, False), (4, True), (5, False)]
