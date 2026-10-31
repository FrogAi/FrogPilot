#!/usr/bin/env python3
import unittest
from panda import ALTERNATIVE_EXPERIENCE, Panda
from panda.tests.libpanda import libpanda_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda


class TestChryslerSafety(common.PandaCarSafetyTest, common.MotorTorqueSteeringSafetyTest, common.AlwaysOnLateralSafetyTest):
  TX_MSGS = [[0x23B, 0], [0x292, 0], [0x2A6, 0]]
  STANDSTILL_THRESHOLD = 0
  RELAY_MALFUNCTION_ADDRS = {0: (0x292,)}
  FWD_BLACKLISTED_ADDRS = {2: [0x292, 0x2A6]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  MAX_RATE_UP = 3
  MAX_RATE_DOWN = 3
  MAX_TORQUE = 261
  MAX_RT_DELTA = 112
  RT_INTERVAL = 250000
  MAX_TORQUE_ERROR = 80

  LKAS_ACTIVE_VALUE = 1

  DAS_BUS = 0

  # FrogPilot tests
  LKAS_BUTTONS = (("TRACTION_BUTTON", "TOGGLE_LKAS"),)

  def setUp(self):
    self.packer = CANPackerPanda("chrysler_pacifica_2017_hybrid_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, 0)
    self.safety.init_tests()

  def _button_msg(self, cancel=False, resume=False):
    values = {"ACC_Cancel": cancel, "ACC_Resume": resume}
    return self.packer.make_can_msg_panda("CRUISE_BUTTONS", self.DAS_BUS, values)

  def _pcm_status_msg(self, enable):
    values = {"ACC_ACTIVE": enable}
    return self.packer.make_can_msg_panda("DAS_3", self.DAS_BUS, values)

  def _speed_msg(self, speed):
    values = {"SPEED_LEFT": speed, "SPEED_RIGHT": speed}
    return self.packer.make_can_msg_panda("SPEED_1", 0, values)

  def _user_gas_msg(self, gas):
    values = {"Accelerator_Position": gas}
    return self.packer.make_can_msg_panda("ECM_5", 0, values)

  def _user_brake_msg(self, brake):
    values = {"Brake_Pedal_State": 1 if brake else 0}
    return self.packer.make_can_msg_panda("ESP_1", 0, values)

  def _torque_meas_msg(self, torque):
    values = {"EPS_TORQUE_MOTOR": torque}
    return self.packer.make_can_msg_panda("EPS_2", 0, values)

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"STEERING_TORQUE": torque, "LKAS_CONTROL_BIT": self.LKAS_ACTIVE_VALUE if steer_req else 0}
    return self.packer.make_can_msg_panda("LKAS_COMMAND", 0, values)

  def test_buttons(self):
    for controls_allowed in (True, False):
      self.safety.set_controls_allowed(controls_allowed)

      # resume only while controls allowed
      self.assertEqual(controls_allowed, self._tx(self._button_msg(resume=True)))

      # can always cancel
      self.assertTrue(self._tx(self._button_msg(cancel=True)))

      # only one button at a time
      self.assertFalse(self._tx(self._button_msg(cancel=True, resume=True)))
      self.assertFalse(self._tx(self._button_msg(cancel=False, resume=False)))

  # FrogPilot tests
  def _acc_state_msg(self, on):
    values = {"ACC_AVAILABLE": on}

    return self.packer.make_can_msg_panda("DAS_3", self.DAS_BUS, values)

  def _lkas_press_msg(self, pressed, button=0):
    msg, signal = self.LKAS_BUTTONS[button]
    values = {signal: pressed}

    return self.packer.make_can_msg_panda(msg, 0, values)

  def test_lkas_button_msgs_checked(self):
    for button in range(len(self.LKAS_BUTTONS)):
      self._reset_safety_hooks()
      self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL)
      self.safety.set_timer(int(11e6))

      self._rx(self._acc_state_msg(True))
      self._rx(self._speed_msg(0))
      self._rx(self._torque_meas_msg(0))
      self._rx(self._user_brake_msg(False))
      self._rx(self._user_gas_msg(0))

      for other in range(len(self.LKAS_BUTTONS)):
        if other != button:
          self._rx(self._lkas_press_msg(False, other))

      self.safety.safety_tick_current_safety_config()

      self.assertFalse(self.safety.get_lateral_allowed(), self.LKAS_BUTTONS[button])

      self._rx(self._lkas_press_msg(False, button))
      self.safety.safety_tick_current_safety_config()

      self.assertTrue(self.safety.get_lateral_allowed(), self.LKAS_BUTTONS[button])

  def test_lkas_button_other_messages(self):
    for button in range(1, len(self.LKAS_BUTTONS)):
      self._reset_safety_hooks()
      self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL | ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL_LKAS_BUTTON)

      self._rx(self._acc_state_msg(False))
      self._rx(self._lkas_press_msg(False, button))
      self.assertFalse(self.safety.get_lateral_allowed(), self.LKAS_BUTTONS[button])

      self._rx(self._lkas_press_msg(True, button))
      self.assertTrue(self.safety.get_lateral_allowed(), self.LKAS_BUTTONS[button])

      self._rx(self._lkas_press_msg(False, button))
      self.assertTrue(self.safety.get_lateral_allowed(), self.LKAS_BUTTONS[button])


class TestChryslerRamDTSafety(TestChryslerSafety):
  TX_MSGS = [[0xB1, 2], [0xA6, 0], [0xFA, 0]]
  RELAY_MALFUNCTION_ADDRS = {0: (0xA6,)}
  FWD_BLACKLISTED_ADDRS = {2: [0xA6, 0xFA]}

  MAX_RATE_UP = 6
  MAX_RATE_DOWN = 6
  MAX_TORQUE = 350

  DAS_BUS = 2

  LKAS_ACTIVE_VALUE = 2

  # FrogPilot tests
  LKAS_BUTTONS = (("Center_Stack_1", "LKAS_Button"), ("Center_Stack_2", "LKAS_Button"))

  def setUp(self):
    self.packer = CANPackerPanda("chrysler_ram_dt_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, Panda.FLAG_CHRYSLER_RAM_DT)
    self.safety.init_tests()

  def _speed_msg(self, speed):
    values = {"Vehicle_Speed": speed}
    return self.packer.make_can_msg_panda("ESP_8", 0, values)

class TestChryslerRamHDSafety(TestChryslerSafety):
  TX_MSGS = [[0x275, 0], [0x276, 0], [0x23A, 2], [0x23B, 2]]
  RELAY_MALFUNCTION_ADDRS = {0: (0x276,)}
  FWD_BLACKLISTED_ADDRS = {2: [0x275, 0x276]}

  MAX_TORQUE = 361
  MAX_RATE_UP = 14
  MAX_RATE_DOWN = 14
  MAX_RT_DELTA = 182

  DAS_BUS = 2

  LKAS_ACTIVE_VALUE = 2

  # FrogPilot tests
  LKAS_BUTTONS = (("Center_Stack_1", "LKAS_Button"), ("Center_Stack_2", "LKAS_Button"))

  def setUp(self):
    self.packer = CANPackerPanda("chrysler_ram_hd_generated")
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_CHRYSLER, Panda.FLAG_CHRYSLER_RAM_HD)
    self.safety.init_tests()

  def _speed_msg(self, speed):
    values = {"Vehicle_Speed": speed}
    return self.packer.make_can_msg_panda("ESP_8", 0, values)

  # FrogPilot tests
  def test_buttons_alt(self):
    def button_alt_msg(cancel=False, resume=False):
      values = {"ACC_Cancel": cancel, "ACC_Resume": resume}
      return self.packer.make_can_msg_panda("CRUISE_BUTTONS_ALT", self.DAS_BUS, values)

    self._button_msg = button_alt_msg
    self.test_buttons()


if __name__ == "__main__":
  unittest.main()
