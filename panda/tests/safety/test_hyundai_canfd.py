#!/usr/bin/env python3
from parameterized import parameterized_class
import unittest
from panda import Panda
from panda.tests.libpanda import libpanda_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda
from panda.tests.safety.hyundai_common import HyundaiButtonBase, HyundaiLongitudinalBase


class TestHyundaiCanfdBase(HyundaiButtonBase, common.PandaCarSafetyTest, common.DriverTorqueSteeringSafetyTest, common.SteerRequestCutSafetyTest):

  TX_MSGS = [[0x50, 0], [0x1CF, 1], [0x2A4, 0]]
  STANDSTILL_THRESHOLD = 12  # 0.375 kph
  FWD_BLACKLISTED_ADDRS = {2: [0x50, 0x2a4]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  MAX_RATE_UP = 2
  MAX_RATE_DOWN = 3
  MAX_TORQUE = 330

  MAX_RT_DELTA = 112
  RT_INTERVAL = 250000

  DRIVER_TORQUE_ALLOWANCE = 250
  DRIVER_TORQUE_FACTOR = 2

  # Safety around steering req bit
  MIN_VALID_STEERING_FRAMES = 89
  MAX_INVALID_STEERING_FRAMES = 2
  MIN_VALID_STEERING_RT_INTERVAL = 810000  # a ~10% buffer, can send steer up to 110Hz

  PT_BUS = 0
  SCC_BUS = 2
  STEER_BUS = 0
  STEER_MSG = ""
  GAS_MSG = ("", "")
  BUTTONS_TX_BUS = 1

  @classmethod
  def setUpClass(cls):
    super().setUpClass()
    if cls.__name__ == "TestHyundaiCanfdBase":
      cls.packer = None
      cls.safety = None
      raise unittest.SkipTest

  def _torque_driver_msg(self, torque):
    values = {"STEERING_COL_TORQUE": torque}
    return self.packer.make_can_msg_panda("MDPS", self.PT_BUS, values)

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"TORQUE_REQUEST": torque, "STEER_REQ": steer_req}
    return self.packer.make_can_msg_panda(self.STEER_MSG, self.STEER_BUS, values)

  def _speed_msg(self, speed):
    values = {f"WHEEL_SPEED_{i}": speed * 0.03125 for i in range(1, 5)}
    return self.packer.make_can_msg_panda("WHEEL_SPEEDS", self.PT_BUS, values)

  def _user_brake_msg(self, brake):
    values = {"DriverBraking": brake}
    return self.packer.make_can_msg_panda("TCS", self.PT_BUS, values)

  def _user_gas_msg(self, gas):
    values = {self.GAS_MSG[1]: gas}
    return self.packer.make_can_msg_panda(self.GAS_MSG[0], self.PT_BUS, values)

  def _pcm_status_msg(self, enable):
    values = {"ACCMode": 1 if enable else 0}
    return self.packer.make_can_msg_panda("SCC_CONTROL", self.SCC_BUS, values)

  def _button_msg(self, buttons, main_button=0, bus=None):
    if bus is None:
      bus = self.PT_BUS
    values = {
      "CRUISE_BUTTONS": buttons,
      "ADAPTIVE_CRUISE_MAIN_BTN": main_button,
    }
    return self.packer.make_can_msg_panda("CRUISE_BUTTONS", bus, values)

  # FrogPilot tests
  def _toggle_aol(self, toggle_on):
    if not hasattr(self, "_aol_state"):
      self._aol_state = False

    # Already in the requested state
    if toggle_on == self._aol_state:
      return None

    # Simulate button press + release
    values = {
      "CRUISE_BUTTONS": 0,
      "ADAPTIVE_CRUISE_MAIN_BTN": 0,
      "LFA_BTN": 1,
      "COUNTER": 0,
    }
    self._rx(self.packer.make_can_msg_panda("CRUISE_BUTTONS", self.PT_BUS, values))
    self._rx(self.packer.make_can_msg_panda("CRUISE_BUTTONS", self.PT_BUS, {**values, "LFA_BTN": 0}))

    self._aol_state = toggle_on
    return None  # avoid duplicate message in harness


class TestHyundaiCanfdHDA1Base(TestHyundaiCanfdBase):

  TX_MSGS = [[0x12A, 0], [0x1A0, 1], [0x1CF, 0], [0x1E0, 0]]
  RELAY_MALFUNCTION_ADDRS = {0: (0x12A,)}  # LFA
  FWD_BLACKLISTED_ADDRS = {2: [0x12A, 0x1E0]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  STEER_MSG = "LFA"
  BUTTONS_TX_BUS = 2
  SAFETY_PARAM: int

  @classmethod
  def setUpClass(cls):
    super().setUpClass()
    if cls.__name__ in ("TestHyundaiCanfdHDA1", "TestHyundaiCanfdHDA1AltButtons"):
      cls.packer = None
      cls.safety = None
      raise unittest.SkipTest

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_canfd")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_CANFD, self.SAFETY_PARAM)
    self.safety.init_tests()


@parameterized_class([
  # Radar SCC, test with long flag to ensure flag is not respected until it is supported
  {"GAS_MSG": ("ACCELERATOR_BRAKE_ALT", "ACCELERATOR_PEDAL_PRESSED"), "SCC_BUS": 0, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_LONG},
  {"GAS_MSG": ("ACCELERATOR", "ACCELERATOR_PEDAL"), "SCC_BUS": 0, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_EV_GAS | Panda.FLAG_HYUNDAI_LONG},
  {"GAS_MSG": ("ACCELERATOR_ALT", "ACCELERATOR_PEDAL"), "SCC_BUS": 0, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_HYBRID_GAS | Panda.FLAG_HYUNDAI_LONG},
  # Camera SCC
  {"GAS_MSG": ("ACCELERATOR_BRAKE_ALT", "ACCELERATOR_PEDAL_PRESSED"), "SCC_BUS": 2, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_CAMERA_SCC},
  {"GAS_MSG": ("ACCELERATOR", "ACCELERATOR_PEDAL"), "SCC_BUS": 2, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_EV_GAS | Panda.FLAG_HYUNDAI_CAMERA_SCC},
  {"GAS_MSG": ("ACCELERATOR_ALT", "ACCELERATOR_PEDAL"), "SCC_BUS": 2, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_HYBRID_GAS | Panda.FLAG_HYUNDAI_CAMERA_SCC},
])
class TestHyundaiCanfdHDA1(TestHyundaiCanfdHDA1Base):
  pass


@parameterized_class([
  # Radar SCC, test with long flag to ensure flag is not respected until it is supported
  {"GAS_MSG": ("ACCELERATOR_BRAKE_ALT", "ACCELERATOR_PEDAL_PRESSED"), "SCC_BUS": 0, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_LONG},
  {"GAS_MSG": ("ACCELERATOR", "ACCELERATOR_PEDAL"), "SCC_BUS": 0, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_EV_GAS | Panda.FLAG_HYUNDAI_LONG},
  {"GAS_MSG": ("ACCELERATOR_ALT", "ACCELERATOR_PEDAL"), "SCC_BUS": 0, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_HYBRID_GAS | Panda.FLAG_HYUNDAI_LONG},
  # Camera SCC
  {"GAS_MSG": ("ACCELERATOR_BRAKE_ALT", "ACCELERATOR_PEDAL_PRESSED"), "SCC_BUS": 2, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_CAMERA_SCC},
  {"GAS_MSG": ("ACCELERATOR", "ACCELERATOR_PEDAL"), "SCC_BUS": 2, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_EV_GAS | Panda.FLAG_HYUNDAI_CAMERA_SCC},
  {"GAS_MSG": ("ACCELERATOR_ALT", "ACCELERATOR_PEDAL"), "SCC_BUS": 2, "SAFETY_PARAM": Panda.FLAG_HYUNDAI_HYBRID_GAS | Panda.FLAG_HYUNDAI_CAMERA_SCC},
])
class TestHyundaiCanfdHDA1AltButtons(TestHyundaiCanfdHDA1Base):

  SAFETY_PARAM: int

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_canfd")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_CANFD, Panda.FLAG_HYUNDAI_CANFD_ALT_BUTTONS | self.SAFETY_PARAM)
    self.safety.init_tests()

  def _button_msg(self, buttons, main_button=0, bus=1):
    values = {
      "CRUISE_BUTTONS": buttons,
      "ADAPTIVE_CRUISE_MAIN_BTN": main_button,
    }
    return self.packer.make_can_msg_panda("CRUISE_BUTTONS_ALT", self.PT_BUS, values)

  def test_button_sends(self):
    """
      No button send allowed with alt buttons.
    """
    for enabled in (True, False):
      for btn in range(8):
        self.safety.set_controls_allowed(enabled)
        self.assertFalse(self._tx(self._button_msg(btn)))

  # FrogPilot tests
  def _toggle_aol(self, toggle_on):
    if not hasattr(self, "_aol_state"):
      self._aol_state = False

    # Already in the requested state
    if toggle_on == self._aol_state:
      return None

    # Simulate button press + release
    values = {
      "CRUISE_BUTTONS_ALT": 0,
      "ADAPTIVE_CRUISE_MAIN_BTN": 0,
      "LFA_BTN": 1,
      "COUNTER": 0,
    }
    self._rx(self.packer.make_can_msg_panda("CRUISE_BUTTONS_ALT", self.PT_BUS, values))
    self._rx(self.packer.make_can_msg_panda("CRUISE_BUTTONS_ALT", self.PT_BUS, {**values, "LFA_BTN": 0}))

    self._aol_state = toggle_on
    return None  # avoid duplicate message in harness


class TestHyundaiCanfdHDA2EV(TestHyundaiCanfdBase):

  TX_MSGS = [[0x50, 0], [0x1CF, 1], [0x2A4, 0]]
  RELAY_MALFUNCTION_ADDRS = {0: (0x50,)}  # LKAS
  FWD_BLACKLISTED_ADDRS = {2: [0x50, 0x2a4]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  PT_BUS = 1
  SCC_BUS = 1
  STEER_MSG = "LKAS"
  GAS_MSG = ("ACCELERATOR", "ACCELERATOR_PEDAL")

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_canfd")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_CANFD, Panda.FLAG_HYUNDAI_CANFD_HDA2 | Panda.FLAG_HYUNDAI_EV_GAS)
    self.safety.init_tests()


# TODO: Handle ICE and HEV configurations once we see cars that use the new messages
class TestHyundaiCanfdHDA2EVAltSteering(TestHyundaiCanfdBase):

  TX_MSGS = [[0x110, 0], [0x1CF, 1], [0x362, 0]]
  RELAY_MALFUNCTION_ADDRS = {0: (0x110,)}  # LKAS_ALT
  FWD_BLACKLISTED_ADDRS = {2: [0x110, 0x362]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  PT_BUS = 1
  SCC_BUS = 1
  STEER_MSG = "LKAS_ALT"
  GAS_MSG = ("ACCELERATOR", "ACCELERATOR_PEDAL")

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_canfd")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_CANFD, Panda.FLAG_HYUNDAI_CANFD_HDA2 | Panda.FLAG_HYUNDAI_EV_GAS |
                                 Panda.FLAG_HYUNDAI_CANFD_HDA2_ALT_STEERING)
    self.safety.init_tests()


class TestHyundaiCanfdHDA2LongEV(HyundaiLongitudinalBase, TestHyundaiCanfdHDA2EV):

  TX_MSGS = [[0x50, 0], [0x1CF, 1], [0x2A4, 0], [0x51, 0], [0x730, 1], [0x12a, 1], [0x160, 1],
             [0x1e0, 1], [0x1a0, 1], [0x1ea, 1], [0x200, 1], [0x345, 1], [0x1da, 1]]

  RELAY_MALFUNCTION_ADDRS = {0: (0x50,), 1: (0x1a0,)}  # LKAS, SCC_CONTROL

  DISABLED_ECU_UDS_MSG = (0x730, 1)
  DISABLED_ECU_ACTUATION_MSG = (0x1a0, 1)

  STEER_MSG = "LFA"
  GAS_MSG = ("ACCELERATOR", "ACCELERATOR_PEDAL")
  STEER_BUS = 1

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_canfd")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_CANFD, Panda.FLAG_HYUNDAI_CANFD_HDA2 | Panda.FLAG_HYUNDAI_LONG | Panda.FLAG_HYUNDAI_EV_GAS)
    self.safety.init_tests()

  def _accel_msg(self, accel, aeb_req=False, aeb_decel=0):
    values = {
      "aReqRaw": accel,
      "aReqValue": accel,
    }
    return self.packer.make_can_msg_panda("SCC_CONTROL", 1, values)


# Tests HDA1 longitudinal for ICE, hybrid, EV
@parameterized_class([
  # Camera SCC is the only supported configuration for HDA1 longitudinal, TODO: allow radar SCC
  {"GAS_MSG": ("ACCELERATOR_BRAKE_ALT", "ACCELERATOR_PEDAL_PRESSED"), "SAFETY_PARAM": Panda.FLAG_HYUNDAI_LONG},
  {"GAS_MSG": ("ACCELERATOR", "ACCELERATOR_PEDAL"), "SAFETY_PARAM": Panda.FLAG_HYUNDAI_LONG | Panda.FLAG_HYUNDAI_EV_GAS},
  {"GAS_MSG": ("ACCELERATOR_ALT", "ACCELERATOR_PEDAL"), "SAFETY_PARAM": Panda.FLAG_HYUNDAI_LONG | Panda.FLAG_HYUNDAI_HYBRID_GAS},
])
class TestHyundaiCanfdHDA1Long(HyundaiLongitudinalBase, TestHyundaiCanfdHDA1Base):

  FWD_BLACKLISTED_ADDRS = {2: [0x12a, 0x1e0, 0x1a0]}

  RELAY_MALFUNCTION_ADDRS = {0: (0x12A, 0x1a0)}  # LFA, SCC_CONTROL

  DISABLED_ECU_UDS_MSG = (0x730, 1)
  DISABLED_ECU_ACTUATION_MSG = (0x1a0, 0)

  STEER_MSG = "LFA"
  STEER_BUS = 0
  SCC_BUS = 2

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "TestHyundaiCanfdHDA1Long":
      cls.safety = None
      raise unittest.SkipTest

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_canfd")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_CANFD, Panda.FLAG_HYUNDAI_CAMERA_SCC | self.SAFETY_PARAM)
    self.safety.init_tests()

  def _accel_msg(self, accel, aeb_req=False, aeb_decel=0):
    values = {
      "aReqRaw": accel,
      "aReqValue": accel,
    }
    return self.packer.make_can_msg_panda("SCC_CONTROL", 0, values)

  # no knockout
  def test_tester_present_allowed(self):
    pass


# FrogPilot tests
class TestTacoTuneHack(TestHyundaiCanfdHDA2EV):

  # Vego = raw_speed * 0.00868. Low speed is < 13 m/s.
  # 13 / 0.00868 = 1497.7. So raw speed 1497 is low, 1498 is high.
  SPEED_LOW = 1497
  SPEED_HIGH = 1498

  def setUp(self):
    self.packer = CANPackerPanda("hyundai_canfd")
    self.safety = libpanda_py.libpanda
    # HDA2 EV with Taco Tune Hack flag
    param = Panda.FLAG_HYUNDAI_CANFD_HDA2 | Panda.FLAG_HYUNDAI_EV_GAS | Panda.FLAG_HYUNDAI_TACO_TUNE_HACK
    self.safety.set_safety_hooks(Panda.SAFETY_HYUNDAI_CANFD, param)
    self.safety.init_tests()

    self.MAX_TORQUE = super().MAX_TORQUE

  def test_taco_tune_hack(self):
    # Override MAX_TORQUE to the hacked value
    self.MAX_TORQUE = 384

    # Test at low speed with controls allowed
    self.safety.init_tests()
    self.safety.set_controls_allowed(True)
    self._rx(self._speed_msg(self.SPEED_LOW))

    # Rate limits should be bypassed, and torque limit raised to MAX_TORQUE
    self._set_prev_torque(0)
    self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_TORQUE)))
    self.assertFalse(self._tx(self._torque_cmd_msg(self.MAX_TORQUE + 1)))

    # Test at low speed with controls not allowed
    self.safety.set_controls_allowed(False)
    self.assertTrue(self._tx(self._torque_cmd_msg(0)))
    self.assertFalse(self._tx(self._torque_cmd_msg(1)))

    # Test at high speed with controls allowed
    self.safety.init_tests()
    self.safety.set_controls_allowed(True)
    self._rx(self._speed_msg(self.SPEED_HIGH))

    # Normal rate limits should apply
    self._set_prev_torque(0)
    self.assertTrue(self._tx(self._torque_cmd_msg(self.MAX_RATE_UP)))
    self._set_prev_torque(0)  # Reset prev_torque to test the limit from 0
    self.assertFalse(self._tx(self._torque_cmd_msg(self.MAX_RATE_UP + 1)))

    # Normal max torque should apply
    self._set_prev_torque(super().MAX_TORQUE - 1)
    self.assertTrue(self._tx(self._torque_cmd_msg(super().MAX_TORQUE)))
    self.assertFalse(self._tx(self._torque_cmd_msg(super().MAX_TORQUE + 1)))

  def test_against_torque_driver(self):
    self._rx(self._speed_msg(self.SPEED_HIGH))
    super().test_against_torque_driver()

  def test_steer_req_bit_realtime(self):
    self._rx(self._speed_msg(self.SPEED_HIGH))
    super().test_steer_req_bit_realtime()

  def test_steer_req_bit_frames(self):
    self._rx(self._speed_msg(self.SPEED_HIGH))
    super().test_steer_req_bit_frames()

  def test_steer_req_bit_multi_invalid(self):
    self._rx(self._speed_msg(self.SPEED_HIGH))
    super().test_steer_req_bit_multi_invalid()

  def test_steer_safety_check(self):
    self._rx(self._speed_msg(self.SPEED_HIGH))
    super().test_steer_safety_check()

  def test_steer_req_bit(self):
    self._rx(self._speed_msg(self.SPEED_HIGH))
    super().test_steer_req_bit()

  def test_non_realtime_limit_up(self):
    self._rx(self._speed_msg(self.SPEED_HIGH))
    super().test_non_realtime_limit_up()

  def test_realtime_limits(self):
    self._rx(self._speed_msg(self.SPEED_HIGH))
    super().test_realtime_limits()


if __name__ == "__main__":
  unittest.main()
