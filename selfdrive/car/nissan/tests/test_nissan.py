import types

from cereal import car
from collections import defaultdict
from openpilot.selfdrive.car.nissan.carcontroller import CarController
from openpilot.selfdrive.car.nissan.values import CAR, DBC


def test_dashboard():
  CP = car.CarParams.new_message(carFingerprint=CAR.NISSAN_ROGUE)
  CS = types.SimpleNamespace(lkas_hud_info_msg=defaultdict(int), lkas_hud_msg=defaultdict(int), out=car.CarState.new_message(vEgoRaw=20.0))

  for enabled, lat_active, icon in ((False, False, 0), (False, True, 2), (True, False, 2)):
    CC = car.CarControl.new_message(enabled=enabled, latActive=lat_active).as_reader()

    _, can_sends = CarController(DBC[CAR.NISSAN_ROGUE]["pt"], CP, None).update(CC, CS, 0, None)
    hud = next(dat for address, _, dat, _ in can_sends if address == 0x2B1)

    assert (int.from_bytes(hud, "little") >> 60) & 3 == icon
