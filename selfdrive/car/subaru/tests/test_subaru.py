import types

from cereal import car
from collections import defaultdict
from openpilot.selfdrive.car.subaru.carcontroller import CarController
from openpilot.selfdrive.car.subaru.fingerprints import FW_VERSIONS
from openpilot.selfdrive.car.subaru.values import DBC, SubaruFlags

Ecu = car.CarParams.Ecu

ECU_NAME = {v: k for k, v in Ecu.schema.enumerants.items()}


class TestSubaruFingerprint:
  def test_fw_version_format(self):
    for platform, fws_per_ecu in FW_VERSIONS.items():
      for (ecu, _, _), fws in fws_per_ecu.items():
        fw_size = len(fws[0])
        for fw in fws:
          assert len(fw) == fw_size, f"{platform} {ecu}: {len(fw)} {fw_size}"


# FrogPilot variables
class TestSubaruAlwaysOnLateral:
  def test_dashboard(self):
    CP = car.CarParams.new_message(carFingerprint="SUBARU_IMPREZA")
    CS = types.SimpleNamespace(es_dashstatus_msg=defaultdict(int), es_lkas_state_msg=defaultdict(int), out=car.CarState.new_message())

    for enabled, lat_active, dash_state in ((False, False, 0), (False, True, 2), (True, False, 2), (True, True, 2)):
      controller = CarController(DBC["SUBARU_IMPREZA"]["pt"], CP, None)
      CC = car.CarControl.new_message(enabled=enabled, latActive=lat_active).as_reader()

      _, can_sends = controller.update(CC, CS, 0, types.SimpleNamespace(always_on_lateral=True, subaru_sng=False))

      lkas_state = next(dat for addr, _, dat, _ in can_sends if addr == 0x322)
      assert (int.from_bytes(lkas_state, "little") >> 18) & 3 == dash_state, (enabled, lat_active)

  def test_preglobal_cruise_main(self):
    CP = car.CarParams.new_message(carFingerprint="SUBARU_OUTBACK_PREGLOBAL", flags=SubaruFlags.PREGLOBAL.value)
    CS = types.SimpleNamespace(cruise_button=0, es_distance_msg=defaultdict(int), out=car.CarState.new_message(), ready=True)

    for always_on_lateral in (False, True):
      controller = CarController(DBC["SUBARU_OUTBACK_PREGLOBAL"]["pt"], CP, None)

      toggles = types.SimpleNamespace(always_on_lateral=always_on_lateral, subaru_sng=False)

      _, can_sends = controller.update(car.CarControl.new_message().as_reader(), CS, 0, toggles)

      es_distance = next(dat for addr, _, dat, _ in can_sends if addr == 0x161)
      assert (int.from_bytes(es_distance, "little") >> 48) & 7 == int(not always_on_lateral)
