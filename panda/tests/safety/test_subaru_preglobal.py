#!/usr/bin/env python3
import unittest
from panda import Panda
from panda.tests.libpanda import libpanda_py
import panda.tests.safety.common as common
from panda.tests.safety.common import CANPackerPanda


class TestSubaruPreglobalSafety(common.PandaCarSafetyTest, common.DriverTorqueSteeringSafetyTest):
  FLAGS = 0
  DBC = "subaru_outback_2015_generated"
  TX_MSGS = [[0x161, 0], [0x164, 0]]
  STANDSTILL_THRESHOLD = 0  # kph
  RELAY_MALFUNCTION_ADDRS = {0: (0x164,)}
  FWD_BLACKLISTED_ADDRS = {2: [0x161, 0x164]}
  FWD_BUS_LOOKUP = {0: 2, 2: 0}

  MAX_RATE_UP = 50
  MAX_RATE_DOWN = 70
  MAX_TORQUE = 2047

  MAX_RT_DELTA = 940
  RT_INTERVAL = 250000

  DRIVER_TORQUE_ALLOWANCE = 75
  DRIVER_TORQUE_FACTOR = 10

  def setUp(self):
    self.packer = CANPackerPanda(self.DBC)
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_SUBARU_PREGLOBAL, self.FLAGS)
    self.safety.init_tests()

  def _set_prev_torque(self, t):
    self.safety.set_desired_torque_last(t)
    self.safety.set_rt_torque_last(t)

  def _torque_driver_msg(self, torque):
    values = {"Steer_Torque_Sensor": torque}
    return self.packer.make_can_msg_panda("Steering_Torque", 0, values)

  def _speed_msg(self, speed):
    # subaru safety doesn't use the scaled value, so undo the scaling
    values = {s: speed*0.0592 for s in ["FR", "FL", "RR", "RL"]}
    return self.packer.make_can_msg_panda("Wheel_Speeds", 0, values)

  def _user_brake_msg(self, brake):
    values = {"Brake_Pedal": brake}
    return self.packer.make_can_msg_panda("Brake_Pedal", 0, values)

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"LKAS_Command": torque, "LKAS_Active": steer_req}
    return self.packer.make_can_msg_panda("ES_LKAS", 0, values)

  def _user_gas_msg(self, gas):
    values = {"Throttle_Pedal": gas}
    return self.packer.make_can_msg_panda("Throttle", 0, values)

  def _pcm_status_msg(self, enable):
    values = {"Cruise_Activated": enable}
    return self.packer.make_can_msg_panda("CruiseControl", 0, values)

  # FrogPilot tests
  def _toggle_aol(self, toggle_on):
    # CruiseControl, Cruise_On is the main on button
    values = {"Cruise_On": 1 if toggle_on else 0}
    return self.packer.make_can_msg_panda("CruiseControl", 0, values)


class TestSubaruPreglobalReversedDriverTorqueSafety(TestSubaruPreglobalSafety):
  FLAGS = Panda.FLAG_SUBARU_PREGLOBAL_REVERSED_DRIVER_TORQUE
  DBC = "subaru_outback_2019_generated"


class TestSubaruPreglobalSngForwarding(unittest.TestCase):
  TX_MSGS = None
  SNG_TX_TIMEOUT = 100000
  THROTTLE = 0x140

  def setUp(self):
    self.safety = libpanda_py.libpanda
    self.safety.set_safety_hooks(Panda.SAFETY_SUBARU_PREGLOBAL, Panda.FLAG_SUBARU_SNG)
    self.safety.init_tests()

  def _tx_sng(self, bus=2):
    return self.safety.safety_tx_hook(libpanda_py.make_CANPacket(self.THROTTLE, bus, b"\x00" * 8))

  def _forwarded(self):
    return self.safety.safety_fwd_hook(0, self.THROTTLE) == 2

  def test_flag_off(self):
    self.safety.set_safety_hooks(Panda.SAFETY_SUBARU_PREGLOBAL, 0)
    self.assertFalse(self._tx_sng())
    self.assertTrue(self._forwarded())

  def test_forwarded_without_sng_tx(self):
    self.safety.set_timer(10000000)
    self.assertTrue(self._forwarded())

  def test_blocked_until_sng_tx_times_out(self):
    for ts in (0, 2**32 - self.SNG_TX_TIMEOUT // 2):
      self.safety.set_safety_hooks(Panda.SAFETY_SUBARU_PREGLOBAL, Panda.FLAG_SUBARU_SNG)
      self.safety.set_timer(ts)
      self.assertTrue(self._tx_sng())
      self.assertFalse(self._forwarded())
      self.safety.set_timer((ts + self.SNG_TX_TIMEOUT) % 2**32)
      self.assertFalse(self._forwarded())
      self.safety.set_timer((ts + self.SNG_TX_TIMEOUT + 1) % 2**32)
      self.assertTrue(self._forwarded())
      self.safety.set_timer(ts)
      self.assertTrue(self._forwarded())

  def test_rejected_tx_does_not_block(self):
    self.safety.set_timer(0)
    self.assertFalse(self._tx_sng(0))
    self.assertTrue(self._forwarded())


if __name__ == "__main__":
  unittest.main()
