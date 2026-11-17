from parameterized import parameterized

from opendbc.can import CANPacker
from opendbc.car import Bus, gen_empty_fingerprint, structs
from opendbc.car.can_definitions import CanData
from opendbc.car.chrysler.interface import CarInterface
from opendbc.car.chrysler.values import CAR, DBC
from opendbc.car.tests.test_car_interfaces import TOGGLES


class TestChryslerLaneSenseButton:
  @parameterized.expand([(CAR.RAM_1500_5TH_GEN, 0xDD), (CAR.RAM_HD_5TH_GEN, 0x330)])
  def test_ram_press_from_center_stack_2_only(self, car_model, center_stack_1_addr):
    CP = CarInterface.get_non_essential_params(car_model)
    CI = CarInterface(CP, CarInterface.get_frogpilot_params(car_model, gen_empty_fingerprint(), [], CP, TOGGLES))
    packer = CANPacker(DBC[car_model][Bus.pt])

    presses = []
    for frame, (center_stack_1, center_stack_2) in enumerate(((0, 0), (1, 0), (0, 0), (0, 1))):
      # Center_Stack_1.LKAS_Button is bit 53
      frames = [(center_stack_1_addr, bytes([0, 0, 0, 0, 0, 0, center_stack_1 << 5, 0]), 0),
                packer.make_can_msg("Center_Stack_2", 0, {"LKAS_Button": center_stack_2})]
      CS, _ = CI.update([(int(frame * 1e7), [CanData(*f) for f in frames])], TOGGLES)
      presses.append(any(be.pressed and be.type == structs.CarState.ButtonEvent.Type.lkas for be in CS.buttonEvents))

    assert presses == [False, False, False, True]
