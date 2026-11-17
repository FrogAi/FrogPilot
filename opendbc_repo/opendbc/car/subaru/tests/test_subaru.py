from collections import defaultdict
from types import SimpleNamespace

from opendbc.car import structs
from opendbc.car.subaru.carcontroller import CarController
from opendbc.car.subaru.fingerprints import FW_VERSIONS
from opendbc.car.subaru.values import CAR, DBC
from opendbc.car.tests.test_car_interfaces import Toggles
from opendbc.safety import ALTERNATIVE_EXPERIENCE


class TestSubaruFingerprint:
  def test_fw_version_format(self):
    for platform, fws_per_ecu in FW_VERSIONS.items():
      for (ecu, _, _), fws in fws_per_ecu.items():
        fw_size = len(fws[0])
        for fw in fws:
          assert len(fw) == fw_size, f"{platform} {ecu}: {len(fw)} {fw_size}"


# FrogPilot variables
class TestSubaruAlwaysOnLateral:
  def _controller(self, car_model, always_on_lateral=True, live_toggle=None):
    CP = structs.CarParams(carFingerprint=car_model, flags=int(car_model.config.flags))
    if always_on_lateral:
      CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL

    controller = CarController(DBC[car_model], CP)
    if live_toggle is None:
      live_toggle = always_on_lateral

    controller.frogpilot_toggles = Toggles(always_on_lateral=live_toggle)

    return controller

  def test_dashboard(self):
    CS = SimpleNamespace(es_dashstatus_msg=defaultdict(int), es_lkas_state_msg=defaultdict(int), out=structs.CarState(), subaru_sng=False)

    for enabled, lat_active, dash_state in ((False, False, 0), (False, True, 2), (True, False, 2), (True, True, 2)):
      CC = structs.CarControl(enabled=enabled, latActive=lat_active).as_reader()

      _, can_sends = self._controller(CAR.SUBARU_IMPREZA).update(CC, CS, 0)
      lkas_state = next(dat for addr, dat, _ in can_sends if addr == 0x322)

      assert (int.from_bytes(lkas_state, "little") >> 18) & 3 == dash_state, (enabled, lat_active)

  def test_preglobal_cruise_main_follows_safety_mode(self):
    CS = SimpleNamespace(cruise_button=0, es_distance_msg=defaultdict(int), out=structs.CarState(), ready=True, subaru_sng=False)

    for always_on_lateral in (False, True):
      controller = self._controller(CAR.SUBARU_OUTBACK_PREGLOBAL, always_on_lateral, live_toggle=not always_on_lateral)

      _, can_sends = controller.update(structs.CarControl().as_reader(), CS, 0)
      es_distance = next(dat for addr, dat, _ in can_sends if addr == 0x161)

      assert (int.from_bytes(es_distance, "little") >> 48) & 7 == int(not always_on_lateral)
