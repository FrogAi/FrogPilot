import re

from parameterized import parameterized

from opendbc.can import CANPacker
from opendbc.car import Bus, gen_empty_fingerprint, structs
from opendbc.car.honda import hondacan
from opendbc.car.honda.fingerprints import FW_VERSIONS
from opendbc.car.honda.interface import CarInterface
from opendbc.car.honda.values import CAR, DBC, HONDA_BOSCH, HONDA_BOSCH_TJA_CONTROL
from opendbc.car.tests.test_car_interfaces import TOGGLES

HONDA_FW_VERSION_RE = br"[A-Z0-9]{5}(-|,)[A-Z0-9]{3}(-|,)[A-Z0-9]{4}(\x00){2}$"


class TestHondaFingerprint:
  def test_fw_version_format(self):
    # Asserts all FW versions follow an expected format
    for fw_by_ecu in FW_VERSIONS.values():
      for fws in fw_by_ecu.values():
        for fw in fws:
          assert re.match(HONDA_FW_VERSION_RE, fw) is not None, fw

  def test_tja_bosch_only(self):
    assert set(HONDA_BOSCH_TJA_CONTROL).issubset(set(HONDA_BOSCH)), "Nidec car found in TJA control list"


# FrogPilot variables
class TestHondaAlwaysOnLateral:
  @parameterized.expand([(CAR.HONDA_ACCORD,), (CAR.HONDA_CIVIC,), (CAR.HONDA_CIVIC_2022,)])
  def test_dashboard_lanes(self, car_model):
    CP = CarInterface.get_non_essential_params(car_model)
    CAN = hondacan.CanBus(CP)

    def lkas_hud(lanes_visible, lat_active):
      hud_control = structs.CarControl.HUDControl(lanesVisible=lanes_visible)
      packer = CANPacker(DBC[car_model][Bus.pt])

      return hondacan.create_lkas_hud(packer, CAN.lkas, CP, hud_control, lat_active, True, False, False, {"LKAS_PROBLEM": 0})

    engaged = lkas_hud(True, True)

    assert lkas_hud(True, False) == engaged
    assert lkas_hud(False, True) == engaged
    assert lkas_hud(False, False) != engaged

  def test_lkas_button_forced(self):
    CP = CarInterface.get_non_essential_params(CAR.HONDA_CIVIC)
    FPCP = CarInterface.get_frogpilot_params(CAR.HONDA_CIVIC, gen_empty_fingerprint(), [], CP, TOGGLES)

    assert FPCP.alwaysOnLateralLKASButton
