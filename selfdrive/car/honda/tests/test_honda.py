import re

from cereal import car
from opendbc.can.packer import CANPacker
from openpilot.selfdrive.car import gen_empty_fingerprint
from openpilot.selfdrive.car.car_helpers import interfaces
from openpilot.selfdrive.car.honda import hondacan
from openpilot.selfdrive.car.honda.carcontroller import HUDData
from openpilot.selfdrive.car.honda.fingerprints import FW_VERSIONS
from openpilot.selfdrive.car.honda.values import DBC
from openpilot.selfdrive.car.tests.test_car_interfaces import TOGGLES
from openpilot.selfdrive.controls.lib.events import EventName

HONDA_FW_VERSION_RE = br"[A-Z0-9]{5}(-|,)[A-Z0-9]{3}(-|,)[A-Z0-9]{4}(\x00){2}$"


class TestHondaFingerprint:
  def test_fw_version_format(self):
    # Asserts all FW versions follow an expected format
    for fw_by_ecu in FW_VERSIONS.values():
      for fws in fw_by_ecu.values():
        for fw in fws:
          assert re.match(HONDA_FW_VERSION_RE, fw) is not None, fw


# FrogPilot variables
class TestHondaAlwaysOnLateral:
  def test_cruise_disabled(self):
    CarInterface, CarController, CarState = interfaces["HONDA_CIVIC"]
    CP = CarInterface.get_non_essential_params("HONDA_CIVIC", TOGGLES)
    FPCP = CarInterface.get_frogpilot_params("HONDA_CIVIC", gen_empty_fingerprint(), [], CP, TOGGLES)
    CI = CarInterface(CP, FPCP, CarController, CarState)

    disengaged, _ = CI.update(car.CarControl.new_message(), [], TOGGLES)
    engaged, _ = CI.update(car.CarControl.new_message(enabled=True), [], TOGGLES)

    assert EventName.speedTooLow not in [event.name for event in disengaged.events]
    assert EventName.speedTooLow in [event.name for event in engaged.events]

  def test_dashboard(self):
    CarInterface, _, _ = interfaces["HONDA_CIVIC_2022"]
    CP = CarInterface.get_non_essential_params("HONDA_CIVIC_2022", TOGGLES)
    lkas_hud = {"LKAS_PROBLEM": 0}

    always_on_lateral_packer = CANPacker(DBC["HONDA_CIVIC_2022"]["pt"])
    engaged_packer = CANPacker(DBC["HONDA_CIVIC_2022"]["pt"])

    CAN = hondacan.CanBus(CP)

    always_on_lateral_hud = HUDData(0, 0, False, False, 0, 0, 0, 0)
    engaged_hud = HUDData(0, 0, False, True, 0, 0, 0, 0)

    engaged = hondacan.create_ui_commands(engaged_packer, CAN, CP, True, 0, engaged_hud, False, None, lkas_hud, True)
    always_on_lateral = hondacan.create_ui_commands(always_on_lateral_packer, CAN, CP, False, 0, always_on_lateral_hud, False, None, lkas_hud, True)

    assert always_on_lateral == engaged
