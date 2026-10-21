from collections import defaultdict
from types import SimpleNamespace

from parameterized import parameterized

from opendbc.can import CANPacker
from opendbc.car import Bus, gen_empty_fingerprint, structs
from opendbc.car.can_definitions import CanData
from opendbc.car.common.conversions import Conversions as CV
from opendbc.car.rivian.carcontroller import CarController
from opendbc.car.rivian.fingerprints import FW_VERSIONS
from opendbc.car.rivian.interface import CarInterface
from opendbc.car.rivian.values import CAR, DBC, FW_QUERY_CONFIG, WMI, ModelLine, ModelYear
from opendbc.car.tests.test_car_interfaces import TOGGLES
from opendbc.safety.tests.common import MAX_SPEED_DELTA


class TestRivian:
  def test_custom_fuzzy_fingerprinting(self, subtests):
    for platform in CAR:
      with subtests.test(platform=platform.name):
        for wmi in WMI:
          for line in ModelLine:
            for year in ModelYear:
              for bad in (True, False):
                vin = ["0"] * 17
                vin[:3] = wmi
                vin[3] = line.value
                vin[9] = year.value
                if bad:
                  vin[3] = "Z"
                vin = "".join(vin)

                matches = FW_QUERY_CONFIG.match_fw_to_car_fuzzy({}, vin, FW_VERSIONS)
                should_match = year != ModelYear.S_2025 and not bad
                assert (matches == {platform}) == should_match, "Bad match"


# FrogPilot variables
class TestRivianAlwaysOnLateral:
  @parameterized.expand([("vdm_faster", MAX_SPEED_DELTA + 0.1, True), ("vdm_slower", -MAX_SPEED_DELTA - 0.1, True),
                         ("within_delta", MAX_SPEED_DELTA - 0.1, False)])
  def test_speed_sensors_disagree(self, _, speed_delta, speed_sensors_disagree):
    CP = CarInterface.get_non_essential_params(CAR.RIVIAN_R1_GEN1)
    CI = CarInterface(CP, CarInterface.get_frogpilot_params(CAR.RIVIAN_R1_GEN1, gen_empty_fingerprint(), [], CP, TOGGLES))
    packer = CANPacker(DBC[CAR.RIVIAN_R1_GEN1][Bus.pt])

    for frame in range(2):
      frames = [packer.make_can_msg("ESP_Status", 0, {"ESP_Vehicle_Speed": 36.0}),
                packer.make_can_msg("VDM_PropStatus", 0, {"VDM_VehicleSpeed": 36.0 + speed_delta * CV.MS_TO_KPH})]
      _, fp_ret = CI.update([(int(frame * 1e7), [CanData(*f) for f in frames])], TOGGLES)

    assert fp_ret.speedSensorsDisagree == speed_sensors_disagree

  def test_wheel_touch(self):
    CP = CarInterface.get_non_essential_params(CAR.RIVIAN_R1_GEN1)
    CS = SimpleNamespace(acm_lka_hba_cmd=defaultdict(int), out=structs.CarState(), sccm_wheel_touch=defaultdict(int), vdm_adas_status=defaultdict(int))

    for enabled, lat_active, hands_on in ((False, False, 0), (False, True, 1), (True, False, 1), (True, True, 1)):
      CC = structs.CarControl(enabled=enabled, latActive=lat_active).as_reader()

      _, can_sends = CarController(DBC[CP.carFingerprint], CP).update(CC, CS, 0)
      wheel_touch = next(dat for addr, dat, _ in can_sends if addr == 0x321)

      assert (wheel_touch[2] >> 5) & 1 == hands_on, (enabled, lat_active)  # SCCM_WheelTouch_HandsOn
