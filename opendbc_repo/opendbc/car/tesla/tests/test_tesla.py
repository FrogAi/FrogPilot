from parameterized import parameterized
from types import SimpleNamespace

from opendbc.can import CANPacker
from opendbc.car import Bus, gen_empty_fingerprint, structs
from opendbc.car.can_definitions import CanData
from opendbc.car.common.conversions import Conversions as CV
from opendbc.car.tesla.carcontroller import CarController
from opendbc.car.tesla.interface import CarInterface
from opendbc.car.tesla.values import CANBUS, CAR, DBC
from opendbc.car.tests.test_car_interfaces import TOGGLES
from opendbc.safety.tests.common import MAX_SPEED_DELTA


class TestTeslaAlwaysOnLateral:
  @parameterized.expand([("esp_faster", MAX_SPEED_DELTA + 0.1, True), ("esp_slower", -MAX_SPEED_DELTA - 0.1, True),
                         ("within_delta", MAX_SPEED_DELTA - 0.1, False)])
  def test_speed_sensors_disagree(self, _, speed_delta, speed_sensors_disagree):
    CP = CarInterface.get_non_essential_params(CAR.TESLA_MODEL_3)
    CI = CarInterface(CP, CarInterface.get_frogpilot_params(CAR.TESLA_MODEL_3, gen_empty_fingerprint(), [], CP, TOGGLES))
    packer = CANPacker(DBC[CAR.TESLA_MODEL_3][Bus.party])

    for frame in range(2):
      frames = [packer.make_can_msg("DI_speed", CANBUS.party, {"DI_vehicleSpeed": 36.0}),
                packer.make_can_msg("ESP_B", CANBUS.party, {"ESP_vehicleSpeed": 36.0 + speed_delta * CV.MS_TO_KPH})]
      _, fp_ret = CI.update([(int(frame * 1e7), [CanData(*f) for f in frames])], TOGGLES)

    assert fp_ret.speedSensorsDisagree == speed_sensors_disagree

  def test_steering_matches_engaged(self):
    CP = CarInterface.get_non_essential_params(CAR.TESLA_MODEL_3)
    CS = SimpleNamespace(hands_on_level=0, out=structs.CarState(steeringAngleDeg=-5.0, steeringTorque=2.0, vEgo=20.0, vEgoRaw=20.0))

    steering_sends = {}
    for enabled in (False, True):
      controller = CarController(DBC[CAR.TESLA_MODEL_3], CP)
      CC = structs.CarControl(enabled=enabled, latActive=True, actuators=structs.CarControl.Actuators(steeringAngleDeg=10.0)).as_reader()

      steering_sends[enabled] = []
      for _ in range(50):
        can_sends = controller.update(CC, CS, 0)[1]
        steering_sends[enabled] += [(dat, bus) for addr, dat, bus in can_sends if addr == 0x488]

    assert steering_sends[False] == steering_sends[True]
