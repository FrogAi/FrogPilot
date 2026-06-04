#!/usr/bin/env python3
import unittest

from opendbc.car.gm.values import GMOPGMSafetyFlags, GMSafetyFlags
from opendbc.car.structs import CarParams
from opendbc.safety.tests.libsafety import libsafety_py
import opendbc.safety.tests.common as common
from opendbc.safety.tests.common import CANPackerSafety


class Buttons:
  UNPRESS = 1
  RES_ACCEL = 2
  DECEL_SET = 3
  CANCEL = 6


class GmLongitudinalBase(common.CarSafetyTest, common.LongitudinalGasBrakeSafetyTest):

  RELAY_MALFUNCTION_ADDRS = {0: (0x180, 0x2CB), 2: (0x184,)}  # ASCMLKASteeringCmd, ASCMGasRegenCmd, PSCMStatus

  MAX_POSSIBLE_BRAKE = 2 ** 12
  MAX_BRAKE = 400

  MAX_POSSIBLE_GAS = 4000  # reasonably excessive limits, not signal max
  MIN_POSSIBLE_GAS = -4000

  PCM_CRUISE = False  # openpilot can control the PCM state if longitudinal

  def _send_brake_msg(self, brake):
    values = {"FrictionBrakeCmd": -brake}
    return self.packer_chassis.make_can_msg_safety("EBCMFrictionBrakeCmd", self.BRAKE_BUS, values)

  def _send_gas_msg(self, gas):
    values = {"GasRegenCmd": gas}
    return self.packer.make_can_msg_safety("ASCMGasRegenCmd", 0, values)

  # override these tests from CarSafetyTest, GM longitudinal uses button enable
  def _pcm_status_msg(self, enable):
    raise NotImplementedError

  def test_disable_control_allowed_from_cruise(self):
    pass

  def test_enable_control_allowed_from_cruise(self):
    pass

  def test_cruise_engaged_prev(self):
    pass

  def test_set_resume_buttons(self):
    """
      SET and RESUME enter controls allowed on their falling and rising edges, respectively.
    """
    for btn_prev in range(8):
      for btn_cur in range(8):
        with self.subTest(btn_prev=btn_prev, btn_cur=btn_cur):
          self._rx(self._button_msg(btn_prev))
          self.safety.set_controls_allowed(0)
          for _ in range(10):
            self._rx(self._button_msg(btn_cur))

          should_enable = btn_cur != Buttons.DECEL_SET and btn_prev == Buttons.DECEL_SET
          should_enable = should_enable or (btn_cur == Buttons.RES_ACCEL and btn_prev != Buttons.RES_ACCEL)
          should_enable = should_enable and btn_cur != Buttons.CANCEL
          self.assertEqual(should_enable, self.safety.get_controls_allowed())

  def test_cancel_button(self):
    self.safety.set_controls_allowed(1)
    self._rx(self._button_msg(Buttons.CANCEL))
    self.assertFalse(self.safety.get_controls_allowed())


class TestGmSafetyBase(common.AlwaysOnLateralTorqueSteeringSafetyTest, common.CarSafetyTest, common.DriverTorqueSteeringSafetyTest):
  STANDSTILL_THRESHOLD = 10 * 0.0311
  # Ensures ASCM is off on ASCM cars, and relay is not malfunctioning for camera-ACC cars
  RELAY_MALFUNCTION_ADDRS = {0: (0x180,), 2: (0x184,)}  # ASCMLKASteeringCmd, PSCMStatus
  BUTTONS_BUS = 0  # rx or tx
  BRAKE_BUS = 0  # tx only

  MAX_RATE_UP = 10
  MAX_RATE_DOWN = 15
  MAX_TORQUE_LOOKUP = [0], [300]
  MAX_RT_DELTA = 128
  DRIVER_TORQUE_ALLOWANCE = 65
  DRIVER_TORQUE_FACTOR = 4

  PCM_CRUISE = True  # openpilot is tied to the PCM state if not longitudinal

  EXTRA_SAFETY_PARAM = 0

  def setUp(self):
    self.packer = CANPackerSafety("gm_global_a_powertrain_generated")
    self.packer_chassis = CANPackerSafety("gm_global_a_chassis")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.gm, 0)
    self.safety.init_tests()

  def _pcm_status_msg(self, enable):
    if self.PCM_CRUISE:
      values = {"CruiseState": enable}
      return self.packer.make_can_msg_safety("AcceleratorPedal2", 0, values)
    else:
      raise NotImplementedError

  def _speed_msg(self, speed):
    values = {"%sWheelSpd" % s: speed for s in ["RL", "RR"]}
    return self.packer.make_can_msg_safety("EBCMWheelSpdRear", 0, values)

  def _user_brake_msg(self, brake):
    # GM safety has a brake threshold of 8
    values = {"BrakePedalPos": 8 if brake else 0}
    return self.packer.make_can_msg_safety("ECMAcceleratorPos", 0, values)

  def _user_gas_msg(self, gas):
    values = {"AcceleratorPedal2": 1 if gas else 0}
    if self.PCM_CRUISE:
      # Fill CruiseState with expected value if the safety mode reads cruise state from gas msg
      values["CruiseState"] = self.safety.get_controls_allowed()
    return self.packer.make_can_msg_safety("AcceleratorPedal2", 0, values)

  def _torque_driver_msg(self, torque):
    # Safety tests assume driver torque is an int, use DBC factor
    values = {"LKADriverAppldTrq": torque * 0.01}
    return self.packer.make_can_msg_safety("PSCMStatus", 0, values)

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"LKASteeringCmd": torque, "LKASteeringCmdActive": steer_req}
    return self.packer.make_can_msg_safety("ASCMLKASteeringCmd", 0, values)

  def _button_msg_on_bus(self, buttons, bus):
    values = {"ACCButtons": buttons}
    return self.packer.make_can_msg_safety("ASCMSteeringButton", bus, values)

  def _button_msg(self, buttons):
    return self._button_msg_on_bus(buttons, self.BUTTONS_BUS)

  # FrogPilot variables
  def _set_aol_acc_main(self, enabled: bool):
    values = {"CruiseMainOn": enabled, "BrakePressed": False, "CruiseActive": 0}
    self.assertTrue(self._rx(self.packer.make_can_msg_safety("ECMEngineStatus", 0, values)))
    self.assertEqual(enabled, self.safety.get_acc_main_on())
    self.assertFalse(self.safety.get_controls_allowed())

  # OPGM variables
  def assert_interceptor_tx_blocked(self):
    self.safety.set_controls_allowed(True)
    self.assertFalse(self._tx(interceptor_command_msg(0, 0)))
    self.assertFalse(self._tx(interceptor_command_msg(1000, 1000)))


class TestGmEVSafetyBase(TestGmSafetyBase):
  EXTRA_SAFETY_PARAM = GMSafetyFlags.EV

  # existence of _user_regen_msg adds regen tests
  def _user_regen_msg(self, regen):
    values = {"RegenPaddle": 2 if regen else 0}
    return self.packer.make_can_msg_safety("EBCMRegenPaddle", 0, values)


class TestGmAscmSafety(GmLongitudinalBase, TestGmSafetyBase):
  TX_MSGS = [[0x180, 0], [0x409, 0], [0x40A, 0], [0x2CB, 0], [0x370, 0],  # pt bus
             [0xA1, 1], [0x306, 1], [0x308, 1], [0x310, 1],  # obs bus
             [0x315, 2]]  # ch bus
  FWD_BLACKLISTED_ADDRS: dict[int, list[int]] = {}
  RELAY_MALFUNCTION_ADDRS = {0: (0x180, 0x2CB)}  # ASCMLKASteeringCmd, ASCMGasRegenCmd
  FWD_BUS_LOOKUP: dict[int, int] = {}
  BRAKE_BUS = 2

  MAX_GAS = 1018
  MIN_GAS = -650  # maximum regen
  INACTIVE_GAS = -650

  def setUp(self):
    self.packer = CANPackerSafety("gm_global_a_powertrain_generated")
    self.packer_chassis = CANPackerSafety("gm_global_a_chassis")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.gm, self.EXTRA_SAFETY_PARAM)
    self.safety.init_tests()

  # OPGM variables
  def test_interceptor_command_blocked_without_interceptor_flag(self):
    if self.safety.get_current_safety_param() & GMOPGMSafetyFlags.GAS_INTERCEPTOR:
      raise unittest.SkipTest("only applies to non-interceptor safety modes")
    self.assert_interceptor_tx_blocked()


class TestGmAscmEVSafety(TestGmAscmSafety, TestGmEVSafetyBase):
  pass


class TestGmCameraSafetyBase(TestGmSafetyBase):
  def _user_brake_msg(self, brake):
    values = {"BrakePressed": brake}
    return self.packer.make_can_msg_safety("ECMEngineStatus", 0, values)


class TestGmCameraSafety(TestGmCameraSafetyBase):
  TX_MSGS = [[0x180, 0],  # pt bus
             [0x184, 2]]  # camera bus
  FWD_BLACKLISTED_ADDRS = {2: [0x180], 0: [0x184]}  # block LKAS message and PSCMStatus
  BUTTONS_BUS = 2  # tx only

  def setUp(self):
    self.packer = CANPackerSafety("gm_global_a_powertrain_generated")
    self.packer_chassis = CANPackerSafety("gm_global_a_chassis")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.gm, GMSafetyFlags.HW_CAM | self.EXTRA_SAFETY_PARAM)
    self.safety.init_tests()

  def test_buttons(self):
    # Only CANCEL button is allowed while cruise is enabled
    self.safety.set_controls_allowed(0)
    for btn in range(8):
      self.assertFalse(self._tx(self._button_msg(btn)))

    self.safety.set_controls_allowed(1)
    for btn in range(8):
      self.assertFalse(self._tx(self._button_msg(btn)))

    for enabled in (True, False):
      self._rx(self._pcm_status_msg(enabled))
      self.assertEqual(enabled, self._tx(self._button_msg(Buttons.CANCEL)))

  # OPGM variables
  def test_interceptor_command_blocked_without_interceptor_flag(self):
    if self.safety.get_current_safety_param() & GMOPGMSafetyFlags.GAS_INTERCEPTOR:
      raise unittest.SkipTest("only applies to non-interceptor safety modes")
    self.assert_interceptor_tx_blocked()

  def test_interceptor_only_param_does_not_enable_partial_mode(self):
    self.safety.set_safety_hooks(CarParams.SafetyModel.gm, GMSafetyFlags.HW_CAM | GMOPGMSafetyFlags.GAS_INTERCEPTOR)
    self.safety.init_tests()

    for addr, bus, length in ((0x184, 0, 8), (0x34A, 0, 5), (0x1E1, 0, 7), (0x1C4, 0, 8), (0xC9, 0, 8), (0xBE, 0, 6)):
      self.assertTrue(self._rx(common.make_msg(bus, addr, length)))
    self.assertTrue(self.safety.safety_config_valid())

    self.assert_interceptor_tx_blocked()


class TestGmCameraEVSafety(TestGmCameraSafety, TestGmEVSafetyBase):
  pass


class TestGmCameraLongitudinalSafety(GmLongitudinalBase, TestGmCameraSafetyBase):
  TX_MSGS = [[0x180, 0], [0x315, 0], [0x2CB, 0], [0x370, 0],  # pt bus
             [0x184, 2]]  # camera bus
  FWD_BLACKLISTED_ADDRS = {2: [0x180, 0x2CB, 0x370, 0x315], 0: [0x184]}  # block LKAS, ACC messages and PSCMStatus
  RELAY_MALFUNCTION_ADDRS = {0: (0x180, 0x2CB, 0x370, 0x315), 2: (0x184,)}
  BUTTONS_BUS = 0  # rx only

  MAX_GAS = 1346
  MIN_GAS = -540  # maximum regen
  INACTIVE_GAS = -500

  def setUp(self):
    self.packer = CANPackerSafety("gm_global_a_powertrain_generated")
    self.packer_chassis = CANPackerSafety("gm_global_a_chassis")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.gm, GMSafetyFlags.HW_CAM | GMSafetyFlags.HW_CAM_LONG | self.EXTRA_SAFETY_PARAM)
    self.safety.init_tests()

  # OPGM variables
  def test_interceptor_command_blocked_without_interceptor_flag(self):
    if self.safety.get_current_safety_param() & GMOPGMSafetyFlags.GAS_INTERCEPTOR:
      raise unittest.SkipTest("only applies to non-interceptor safety modes")
    self.assert_interceptor_tx_blocked()

  def test_button_command_blocked_without_button_longitudinal_flag(self):
    self.safety.set_cruise_engaged_prev(True)
    for btn in range(8):
      self.assertFalse(self._tx(self._button_msg(btn)))


class TestGmCameraLongitudinalEVSafety(TestGmCameraLongitudinalSafety, TestGmEVSafetyBase):
  pass


class TestGmIgnition(unittest.TestCase):
  TX_MSGS: list = []

  def setUp(self):
    self.safety = libsafety_py.libsafety
    self.safety.init_tests()
    self.packer = CANPackerSafety("gm_global_a_powertrain_generated")

  def _msg(self, mode):
    return self.packer.make_can_msg_safety("BCMGeneralPlatformStatus", 0, {"SystemPowerMode": mode})

  # SystemPowerMode 2=Run, 3=Crank Request
  def test_ignition_on(self):
    self.safety.ignition_can_hook(self._msg(2))
    self.assertTrue(self.safety.get_ignition_can())

  def test_ignition_off(self):
    self.safety.ignition_can_hook(self._msg(2))
    self.assertTrue(self.safety.get_ignition_can())
    self.safety.ignition_can_hook(self._msg(0))
    self.assertFalse(self.safety.get_ignition_can())
# OPGM variables
def interceptor_sensor_msg(gas, gas2=None):
  return interceptor_msg(0x201, gas, gas if gas2 is None else gas2)


def interceptor_command_msg(gas, gas2=None):
  return interceptor_msg(0x200, gas, gas if gas2 is None else gas2)


def interceptor_msg(addr, gas, gas2):
  gas = int(round(gas))
  gas2 = int(round(gas2))
  to_send = common.make_msg(0, addr, 6)
  to_send[0].data[0] = (gas & 0xFF00) >> 8
  to_send[0].data[1] = gas & 0xFF
  to_send[0].data[2] = (gas2 & 0xFF00) >> 8
  to_send[0].data[3] = gas2 & 0xFF
  return to_send


class GmInterceptorMixin:
  GAS_PRESSED_THRESHOLD = 550

  def _user_gas_msg(self, gas):
    return interceptor_sensor_msg(gas)

  def _interceptor_gas_cmd(self, gas, gas2=None):
    return interceptor_command_msg(gas, gas if gas2 is None else gas2)

  def test_interceptor_user_gas_threshold(self):
    for gas, pressed in ((550, False), (551, True)):
      self.assertTrue(self._rx(self._user_gas_msg(gas)))
      self.assertEqual(pressed, self.safety.get_gas_pressed_prev())

  def test_interceptor_command_limits(self):
    for controls_allowed, gas, gas2, should_tx in (
      (False, 0, 0, True),
      (False, 1, 1, False),
      (True, 2633, 1316, True),
      (True, 2634, 1316, False),
      (True, 2633, 1317, False),
    ):
      self.safety.set_controls_allowed(controls_allowed)
      self.assertEqual(should_tx, self._tx(self._interceptor_gas_cmd(gas, gas2)), (controls_allowed, gas, gas2))


class TestGmPedalLongSafety(GmInterceptorMixin, TestGmCameraSafety, TestGmEVSafetyBase):
  TX_MSGS = [[0x180, 0], [0x200, 0], [0x1E1, 0], [0x184, 2]]
  BUTTONS_BUS = 0

  def setUp(self):
    self.packer = CANPackerSafety("gm_global_a_powertrain_generated")
    self.packer_chassis = CANPackerSafety("gm_global_a_chassis")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(
      CarParams.SafetyModel.gm,
      GMSafetyFlags.HW_CAM | GMOPGMSafetyFlags.NO_ACC | GMOPGMSafetyFlags.PEDAL_LONG | GMOPGMSafetyFlags.GAS_INTERCEPTOR | self.EXTRA_SAFETY_PARAM)
    self.safety.init_tests()

  def _pcm_status_msg(self, enable):
    values = {"CruiseActive": enable}
    return self.packer.make_can_msg_safety("ECMCruiseControl", 0, values)

  def test_cruise_engaged_prev(self):
    for enabled in (True, False):
      self.assertTrue(self._rx(self._pcm_status_msg(enabled)))
      self.assertEqual(enabled, self.safety.get_cruise_engaged_prev())

  def test_enable_control_allowed_from_cruise(self):
    """No-ACC pedal-long mode tracks cruise state without enabling controls."""
    self._rx(self._pcm_status_msg(False))
    self.assertFalse(self.safety.get_controls_allowed())
    self._rx(self._pcm_status_msg(True))
    self.assertFalse(self.safety.get_controls_allowed())
    self.assertTrue(self.safety.get_cruise_engaged_prev())

  def test_disable_control_allowed_from_cruise(self):
    """No-ACC pedal-long mode tracks cruise state without disabling controls."""
    self.safety.set_controls_allowed(True)
    self._rx(self._pcm_status_msg(False))
    self.assertTrue(self.safety.get_controls_allowed())
    self.assertFalse(self.safety.get_cruise_engaged_prev())

  def test_acc_pcm_status_does_not_change_controls_allowed(self):
    for controls_allowed in (True, False):
      self.safety.set_controls_allowed(controls_allowed)
      self.assertTrue(self._rx(self.packer.make_can_msg_safety("AcceleratorPedal2", 0, {"CruiseState": not controls_allowed})))
      self.assertEqual(controls_allowed, self.safety.get_controls_allowed())

  def test_buttons(self):
    for btn in range(8):
      self.assertFalse(self._tx(self._button_msg(btn)))

    self._rx(self._pcm_status_msg(True))
    self.assertTrue(self._tx(self._button_msg(Buttons.CANCEL)))
    for btn in (Buttons.UNPRESS, Buttons.RES_ACCEL, Buttons.DECEL_SET):
      self.assertFalse(self._tx(self._button_msg(btn)))

  def test_camera_bus_button_command_blocked(self):
    self._rx(self._pcm_status_msg(True))
    self.assertFalse(self._tx(self._button_msg_on_bus(Buttons.CANCEL, 2)))


class TestGmCameraLongitudinalInterceptorSafety(GmInterceptorMixin, TestGmCameraLongitudinalSafety, TestGmEVSafetyBase):
  TX_MSGS = [[0x180, 0], [0x315, 0], [0x2CB, 0], [0x370, 0], [0x200, 0],  # pt bus
             [0x184, 2]]  # camera bus

  def setUp(self):
    self.packer = CANPackerSafety("gm_global_a_powertrain_generated")
    self.packer_chassis = CANPackerSafety("gm_global_a_chassis")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(
      CarParams.SafetyModel.gm,
      GMSafetyFlags.HW_CAM | GMSafetyFlags.HW_CAM_LONG | GMOPGMSafetyFlags.GAS_INTERCEPTOR | self.EXTRA_SAFETY_PARAM)
    self.safety.init_tests()

  def test_button_command_blocked_without_button_longitudinal_flag(self):
    self.safety.set_cruise_engaged_prev(True)
    for btn in range(8):
      self.assertFalse(self._tx(self._button_msg(btn)))


class TestGmCcLongitudinalSafety(TestGmCameraSafety):
  TX_MSGS = [[0x180, 0], [0x1E1, 0], [0x184, 2]]
  FWD_BLACKLISTED_ADDRS = {2: [0x180], 0: [0x184]}  # block LKAS message and PSCMStatus
  BUTTONS_BUS = 0  # tx only

  def setUp(self):
    self.packer = CANPackerSafety("gm_global_a_powertrain_generated")
    self.packer_chassis = CANPackerSafety("gm_global_a_chassis")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(
      CarParams.SafetyModel.gm,
      GMSafetyFlags.HW_CAM | GMOPGMSafetyFlags.NO_ACC | GMOPGMSafetyFlags.CC_LONG | self.EXTRA_SAFETY_PARAM)
    self.safety.init_tests()

  def _pcm_status_msg(self, enable):
    values = {"CruiseActive": enable}
    return self.packer.make_can_msg_safety("ECMCruiseControl", 0, values)

  def test_fwd_hook(self):
    for bus in range(3):
      for addr in self.SCANNED_ADDRS:
        self.assertEqual(-1, self.safety.safety_fwd_hook(bus, addr), f"{addr=:#x} from {bus=}")

  def test_no_acc_cruise_status_controls_allowed(self):
    self._rx(self._pcm_status_msg(False))
    self.assertFalse(self.safety.get_controls_allowed())
    self._rx(self._pcm_status_msg(True))
    self.assertTrue(self.safety.get_controls_allowed())
    self._rx(self._pcm_status_msg(False))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_cancel_button_not_reenabled_by_stale_pcm_cruise_status(self):
    self._rx(self._pcm_status_msg(True))
    self.assertTrue(self.safety.get_controls_allowed())

    self._rx(self._button_msg(Buttons.CANCEL))
    self.assertFalse(self.safety.get_controls_allowed())

    self._rx(self._pcm_status_msg(True))
    self.assertFalse(self.safety.get_controls_allowed())

  def test_buttons(self):
    for btn in range(8):
      self.assertFalse(self._tx(self._button_msg(btn)))

    self._rx(self._pcm_status_msg(True))
    for btn in (Buttons.UNPRESS, Buttons.RES_ACCEL, Buttons.DECEL_SET, Buttons.CANCEL):
      self.assertTrue(self._tx(self._button_msg(btn)))

    self.safety.set_controls_allowed(False)
    self.assertTrue(self._tx(self._button_msg(Buttons.CANCEL)))
    for btn in (Buttons.UNPRESS, Buttons.RES_ACCEL, Buttons.DECEL_SET):
      self.assertFalse(self._tx(self._button_msg(btn)))


class TestGmCcLongitudinalEVSafety(TestGmCcLongitudinalSafety, TestGmEVSafetyBase):
  pass


if __name__ == "__main__":
  unittest.main()
