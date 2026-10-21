from collections import defaultdict
from types import SimpleNamespace

from opendbc.can import CANPacker
from opendbc.car import Bus, gen_empty_fingerprint, structs
from opendbc.car.nissan.carcontroller import CarController
from opendbc.car.nissan.interface import CarInterface
from opendbc.car.nissan.values import CAR, DBC
from opendbc.car.tests.test_car_interfaces import TOGGLES


def test_dashboard():
  CP = structs.CarParams(carFingerprint=CAR.NISSAN_ROGUE)
  CS = SimpleNamespace(lkas_hud_info_msg=defaultdict(int), lkas_hud_msg=defaultdict(int), out=structs.CarState(vEgoRaw=20.0))

  for enabled, lat_active, icon in ((False, False, 0), (False, True, 2), (True, False, 2)):
    CC = structs.CarControl(enabled=enabled, latActive=lat_active).as_reader()

    _, can_sends = CarController(DBC[CAR.NISSAN_ROGUE], CP).update(CC, CS, 0)
    hud = next(dat for address, dat, _ in can_sends if address == 0x2B1)

    assert (int.from_bytes(hud, "little") >> 60) & 3 == icon


def test_lkas_button():
  CP = CarInterface.get_non_essential_params(CAR.NISSAN_ROGUE)
  FPCP = CarInterface.get_frogpilot_params(CAR.NISSAN_ROGUE, gen_empty_fingerprint(), [], CP, TOGGLES)
  CI = CarInterface(CP, FPCP)
  CI.update([], TOGGLES)
  packer = CANPacker(DBC[CAR.NISSAN_ROGUE][Bus.pt])

  def lkas_events(step, lkas_enabled=None):
    if lkas_enabled is None:
      frames = []
    else:
      frames = [packer.make_can_msg("LKAS_SETTINGS", 2, {"LKAS_ENABLED": lkas_enabled})]

    CS, _ = CI.update([(step * 10_000_000, frames)], TOGGLES)

    return [be.pressed for be in CS.buttonEvents if be.type == structs.CarState.ButtonEvent.Type.lkas]

  assert lkas_events(1) == []
  assert lkas_events(2, lkas_enabled=1) == []
  assert lkas_events(3, lkas_enabled=1) == []
  assert lkas_events(4, lkas_enabled=0) == [True, False]
  assert lkas_events(5, lkas_enabled=0) == []
  assert lkas_events(6, lkas_enabled=1) == [True, False]
