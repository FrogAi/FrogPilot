from parameterized import parameterized

from opendbc.can import CANPacker
from opendbc.car import Bus, gen_empty_fingerprint
from opendbc.car.psa.interface import CarInterface
from opendbc.car.psa.values import CAR, DBC
from opendbc.car.tests.test_car_interfaces import TOGGLES
from opendbc.safety import ALTERNATIVE_EXPERIENCE


class TestPsaCruiseMain:
  # LONGITUDINAL_REGULATION_TYPE 3 is ACC selected, and ACC_STATUS 2 is Inhibited, 4 is Active
  @parameterized.expand([(False, 3, 2, False), (False, 0, 4, True), (True, 3, 2, True), (True, 0, 4, False), (True, 1, 4, False), (True, 2, 4, False)])
  def test_cruise_main(self, always_on_lateral, regulation_type, acc_status, available):
    CP = CarInterface.get_non_essential_params(CAR.PSA_PEUGEOT_208)
    if always_on_lateral:
      CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL

    CI = CarInterface(CP, CarInterface.get_frogpilot_params(CAR.PSA_PEUGEOT_208, gen_empty_fingerprint(), [], CP, TOGGLES))
    packer = CANPacker(DBC[CAR.PSA_PEUGEOT_208][Bus.pt])

    # The parsers only take these frames from the second update on
    for step in (1, 2):
      frames = [packer.make_can_msg("HS2_DAT_MDD_CMD_452", 1, {"LONGITUDINAL_REGULATION_TYPE": regulation_type}),
                packer.make_can_msg("HS2_DYN1_MDD_ETAT_2B6", 1, {"ACC_STATUS": acc_status})]
      CS, _ = CI.update([(step * 10_000_000, frames)], TOGGLES)

    assert CS.cruiseState.available == available
