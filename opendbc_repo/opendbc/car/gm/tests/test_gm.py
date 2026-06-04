import unittest

from unittest.mock import patch

from cereal import custom
from opendbc.car import Bus, DT_CTRL, structs
from opendbc.car.car_helpers import interfaces
from opendbc.car.common.conversions import Conversions as CV
from opendbc.car.gm.fingerprints import FINGERPRINTS
from opendbc.car.gm.interface import PEDAL_MSG
from opendbc.car.gm.values import CAMERA_ACC_CAR, CAR, CC_ONLY_CAR, EV_CAR, GM_RX_OFFSET, AccState, CanBus, GMOPGMFlags, GMOPGMSafetyFlags, GMSafetyFlags
from opendbc.testing import parameterized

CAMERA_DIAGNOSTIC_ADDRESS = 0x24b


class TestGMFingerprint(unittest.TestCase):
  @parameterized("car_model, fingerprints", FINGERPRINTS.items())
  def test_can_fingerprints(self, car_model, fingerprints):
    assert len(fingerprints) > 0

    assert all(len(finger) for finger in fingerprints)

    # The camera can sometimes be communicating on startup
    # OPGM variables
    if car_model in CAMERA_ACC_CAR - CC_ONLY_CAR:
      for finger in fingerprints:
        for required_addr in (CAMERA_DIAGNOSTIC_ADDRESS, CAMERA_DIAGNOSTIC_ADDRESS + GM_RX_OFFSET):
          assert finger.get(required_addr) == 8, required_addr


# OPGM variables
class TestGMOPGMFingerprint(unittest.TestCase):
  def test_cc_only_fingerprints_cover_no_acc_safety_rx_contract(self):
    required_addrs = {0xC9, 0x184, 0x1C4, 0x1E1, 0x34A, 0x3D1}

    for car_model in CC_ONLY_CAR:
      for finger in FINGERPRINTS[car_model]:
        expected_addrs = required_addrs | ({0xBD} if car_model in EV_CAR else set())
        missing_addrs = expected_addrs - finger.keys()
        assert not missing_addrs, f"{car_model}: missing {sorted(hex(addr) for addr in missing_addrs)}"


class TestGMOPGMInterface(unittest.TestCase):
  class ParamsStub:
    def __init__(self, *args, **kwargs):
      pass

  @staticmethod
  def get_car_interface(candidate, alpha_long=True, fingerprint=None):
    fingerprints = {bus: {} for bus in range(8)}
    fingerprints[0].update(fingerprint or {})

    CarInterface = interfaces[candidate]
    CP = CarInterface.get_params(candidate, fingerprints, [], alpha_long=alpha_long, is_release=False, docs=False, frogpilot_toggles=None)
    with patch("opendbc.car.interfaces.Params", TestGMOPGMInterface.ParamsStub):
      return CarInterface(CP, custom.FrogPilotCarParams.new_message())

  @staticmethod
  def get_long_active_control():
    CC = structs.CarControl()
    CC.enabled = True
    CC.longActive = True
    CC.actuators.accel = 0.2
    return CC.as_reader()

  @staticmethod
  def get_cancel_control():
    CC = structs.CarControl()
    CC.cruiseControl.cancel = True
    return CC.as_reader()

  @staticmethod
  def update_ready_state(car_interface, v_ego):
    CS, _ = car_interface.update([], None)
    CS.cruiseState.enabled = True
    CS.cruiseState.available = True
    CS.cruiseState.speed = max(car_interface.CP.minEnableSpeed + 2., v_ego)
    CS.vEgo = v_ego
    return CS

  @staticmethod
  def safety_param(car_interface):
    return car_interface.CP.safetyConfigs[0].safetyParam

  @staticmethod
  def update_carstate(car_interface):
    CS, _ = car_interface.CS.update(car_interface.can_parsers, None)
    return CS

  def test_cc_only_params_use_no_acc_cc_long_contract(self):
    for candidate in CC_ONLY_CAR:
      car_interface = self.get_car_interface(candidate)
      safety_param = self.safety_param(car_interface)

      assert car_interface.CP.networkLocation == structs.CarParams.NetworkLocation.fwdCamera
      assert car_interface.CP.flags & GMOPGMFlags.CC_LONG.value
      assert car_interface.CP.openpilotLongitudinalControl
      assert safety_param & GMSafetyFlags.HW_CAM.value
      assert safety_param & GMOPGMSafetyFlags.CC_LONG.value
      assert safety_param & GMOPGMSafetyFlags.NO_ACC.value
      assert not safety_param & GMSafetyFlags.HW_CAM_LONG.value

  def test_cc_only_ev_params_use_ev_safety_contract(self):
    for candidate in (CAR.CHEVROLET_BOLT_2017, CAR.CHEVROLET_BOLT_2018, CAR.CHEVROLET_BOLT_CC):
      car_interface = self.get_car_interface(candidate, alpha_long=False)
      safety_param = self.safety_param(car_interface)

      assert car_interface.CP.transmissionType == structs.CarParams.TransmissionType.direct
      assert safety_param & GMSafetyFlags.EV.value
      assert safety_param & GMOPGMSafetyFlags.NO_ACC.value

  def test_cc_only_cancel_uses_powertrain_bus(self):
    car_interface = self.get_car_interface(CAR.CHEVROLET_BOLT_2018, alpha_long=False)
    assert not car_interface.CP.openpilotLongitudinalControl
    assert self.safety_param(car_interface) & GMOPGMSafetyFlags.CC_LONG.value
    CC = self.get_cancel_control()

    cancel_sends = []
    for frame in range(20):
      self.update_ready_state(car_interface, car_interface.CP.minEnableSpeed + 1.)
      _, can_sends = car_interface.apply(CC, int(frame * DT_CTRL * 1e9))
      cancel_sends.extend(can for can in can_sends if can[0] == 0x1E1)

    assert cancel_sends
    assert {can[2] for can in cancel_sends} == {CanBus.POWERTRAIN}

  def test_cc_only_non_bolt_pedal_message_does_not_enable_interceptor(self):
    car_interface = self.get_car_interface(CAR.CHEVROLET_EQUINOX_CC, fingerprint={PEDAL_MSG: 6})
    safety_param = self.safety_param(car_interface)

    assert not car_interface.CP.enableGasInterceptorDEPRECATED
    assert car_interface.CP.flags & GMOPGMFlags.CC_LONG.value
    assert safety_param & GMOPGMSafetyFlags.CC_LONG.value
    assert safety_param & GMOPGMSafetyFlags.NO_ACC.value
    assert not safety_param & GMOPGMSafetyFlags.GAS_INTERCEPTOR.value
    assert not safety_param & GMOPGMSafetyFlags.PEDAL_LONG.value

  def test_cc_only_bolt_pedal_message_uses_pedal_long_contract(self):
    car_interface = self.get_car_interface(CAR.CHEVROLET_BOLT_2017, fingerprint={PEDAL_MSG: 6})
    safety_param = self.safety_param(car_interface)

    assert car_interface.CP.enableGasInterceptorDEPRECATED
    assert car_interface.CP.flags & GMOPGMFlags.PEDAL_LONG.value
    assert not safety_param & GMSafetyFlags.HW_CAM_LONG.value
    assert safety_param & GMOPGMSafetyFlags.NO_ACC.value
    assert safety_param & GMOPGMSafetyFlags.GAS_INTERCEPTOR.value
    assert safety_param & GMOPGMSafetyFlags.PEDAL_LONG.value

  def test_non_cc_only_pedal_message_uses_camera_long_interceptor_contract(self):
    car_interface = self.get_car_interface(CAR.CHEVROLET_VOLT, fingerprint={PEDAL_MSG: 6})
    safety_param = self.safety_param(car_interface)

    assert car_interface.CP.enableGasInterceptorDEPRECATED
    assert car_interface.CP.networkLocation == structs.CarParams.NetworkLocation.fwdCamera
    assert not car_interface.CP.flags & GMOPGMFlags.PEDAL_LONG.value
    assert safety_param & GMSafetyFlags.HW_CAM.value
    assert safety_param & GMSafetyFlags.HW_CAM_LONG.value
    assert safety_param & GMOPGMSafetyFlags.GAS_INTERCEPTOR.value
    assert not safety_param & GMOPGMSafetyFlags.PEDAL_LONG.value

  def test_camera_long_interceptor_controller_emits_expected_longitudinal_messages(self):
    car_interface = self.get_car_interface(CAR.CHEVROLET_VOLT, fingerprint={PEDAL_MSG: 6})
    CC = self.get_long_active_control()

    can_addresses = set()
    for frame in range(6):
      self.update_ready_state(car_interface, 5.)
      _, can_sends = car_interface.apply(CC, int(frame * DT_CTRL * 1e9))
      can_addresses.update(can[0] for can in can_sends)

    assert {0x200, 0x2CB, 0x315, 0x370}.issubset(can_addresses)

  def test_gas_interceptor_parser_registers_gas_sensor_when_enabled(self):
    car_interface = self.get_car_interface(CAR.CHEVROLET_BOLT_2017, fingerprint={PEDAL_MSG: 6})
    assert "GAS_SENSOR" not in car_interface.can_parsers[Bus.pt].vl

    car_interface.CS.update(car_interface.can_parsers, None)

    assert "GAS_SENSOR" in car_interface.can_parsers[Bus.pt].vl
    assert 0x201 in car_interface.can_parsers[Bus.pt].addresses
    assert {"INTERCEPTOR_GAS", "INTERCEPTOR_GAS2"}.issubset(car_interface.can_parsers[Bus.pt].vl["GAS_SENSOR"])

  def test_cc_only_parser_registers_no_acc_cruise_status(self):
    car_interface = self.get_car_interface(CAR.CHEVROLET_EQUINOX_CC, alpha_long=False)
    assert "ECMCruiseControl" not in car_interface.can_parsers[Bus.pt].vl

    car_interface.CS.update(car_interface.can_parsers, None)

    assert "ECMCruiseControl" in car_interface.can_parsers[Bus.pt].vl
    assert 0x3D1 in car_interface.can_parsers[Bus.pt].addresses
    assert {"CruiseActive", "CruiseSetSpeed"}.issubset(car_interface.can_parsers[Bus.pt].vl["ECMCruiseControl"])

  def test_gas_interceptor_carstate_uses_interceptor_threshold(self):
    car_interface = self.get_car_interface(CAR.CHEVROLET_BOLT_2017, fingerprint={PEDAL_MSG: 6})
    self.update_carstate(car_interface)

    gas_sensor = car_interface.can_parsers[Bus.pt].vl["GAS_SENSOR"]
    for gas, gas_pressed in ((20, False), (21, True)):
      gas_sensor["INTERCEPTOR_GAS"] = gas
      gas_sensor["INTERCEPTOR_GAS2"] = gas
      CS = self.update_carstate(car_interface)
      assert CS.gasPressed == gas_pressed

  def test_cc_only_carstate_uses_no_acc_cruise_status(self):
    car_interface = self.get_car_interface(CAR.CHEVROLET_EQUINOX_CC, alpha_long=False)
    self.update_carstate(car_interface)

    pt_cp = car_interface.can_parsers[Bus.pt].vl
    pt_cp["AcceleratorPedal2"]["CruiseState"] = AccState.FAULTED
    pt_cp["EBCMFrictionBrakeStatus"]["FrictionBrakeUnavailable"] = 1
    pt_cp["ECMCruiseControl"]["CruiseActive"] = 1
    pt_cp["ECMCruiseControl"]["CruiseSetSpeed"] = 88

    CS = self.update_carstate(car_interface)
    assert not CS.accFaulted
    assert CS.cruiseState.enabled
    assert abs(CS.cruiseState.speed - 88 * CV.KPH_TO_MS) < 1e-6
