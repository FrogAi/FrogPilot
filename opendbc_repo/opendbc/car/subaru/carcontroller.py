import numpy as np
from opendbc.can import CANPacker
from opendbc.car import Bus, make_tester_present_msg
from opendbc.car.lateral import apply_driver_steer_torque_limits, common_fault_avoidance
from opendbc.car.interfaces import CarControllerBase
from opendbc.car.subaru import subarucan
from opendbc.car.subaru.values import DBC, GLOBAL_ES_ADDR, CanBus, CarControllerParams, SubaruFlags

# FIXME: These limits aren't exact. The real limit is more than likely over a larger time period and
# involves the total steering angle change rather than rate, but these limits work well for now
MAX_STEER_RATE = 25  # deg/s
MAX_STEER_RATE_FRAMES = 7  # tx control frames needed before torque can be cut

# FrogPilot variables
_SNG_ACC_MIN_DIST = 3
_SNG_ACC_MAX_DIST = 4.5


class CarController(CarControllerBase):
  def __init__(self, dbc_names, CP):
    super().__init__(dbc_names, CP)
    self.apply_torque_last = 0

    self.cruise_button_prev = 0
    self.steer_rate_counter = 0

    self.p = CarControllerParams(CP)
    self.packer = CANPacker(DBC[CP.carFingerprint][Bus.pt])

    # FrogPilot variables
    self.manual_hold = False
    self.prev_standstill = False
    self.sng_acc_resume = False

    self.prev_close_distance = 0
    self.prev_cruise_state = 0
    self.sng_acc_resume_cnt = 0
    self.standstill_start = 0

  def update(self, CC, CS, now_nanos):
    actuators = CC.actuators
    hud_control = CC.hudControl
    pcm_cancel_cmd = CC.cruiseControl.cancel

    can_sends = []

    # *** steering ***
    if (self.frame % self.p.STEER_STEP) == 0:
      apply_torque = int(round(actuators.torque * self.p.STEER_MAX))

      # limits due to driver torque

      new_torque = int(round(apply_torque))
      apply_torque = apply_driver_steer_torque_limits(new_torque, self.apply_torque_last, CS.out.steeringTorque, self.p)

      if not CC.latActive:
        apply_torque = 0

      if self.CP.flags & SubaruFlags.PREGLOBAL:
        can_sends.append(subarucan.create_preglobal_steering_control(self.packer, self.frame // self.p.STEER_STEP, apply_torque, CC.latActive))
      else:
        apply_steer_req = CC.latActive

        if self.CP.flags & SubaruFlags.STEER_RATE_LIMITED:
          # Steering rate fault prevention
          self.steer_rate_counter, apply_steer_req = \
            common_fault_avoidance(abs(CS.out.steeringRateDeg) > MAX_STEER_RATE, apply_steer_req,
                                   self.steer_rate_counter, MAX_STEER_RATE_FRAMES)

        can_sends.append(subarucan.create_steering_control(self.packer, apply_torque, apply_steer_req))

      self.apply_torque_last = apply_torque

    # FrogPilot variables
    # *** stop and go ***
    if CS.subaru_sng:
      throttle_cmd, speed_cmd = self.stop_and_go(CC, CS)

    # *** longitudinal ***

    if CC.longActive:
      apply_throttle = int(round(np.interp(actuators.accel, CarControllerParams.THROTTLE_LOOKUP_BP, CarControllerParams.THROTTLE_LOOKUP_V)))
      apply_rpm = int(round(np.interp(actuators.accel, CarControllerParams.RPM_LOOKUP_BP, CarControllerParams.RPM_LOOKUP_V)))
      apply_brake = int(round(np.interp(actuators.accel, CarControllerParams.BRAKE_LOOKUP_BP, CarControllerParams.BRAKE_LOOKUP_V)))

      # limit min and max values
      cruise_throttle = np.clip(apply_throttle, CarControllerParams.THROTTLE_MIN, CarControllerParams.THROTTLE_MAX)
      cruise_rpm = np.clip(apply_rpm, CarControllerParams.RPM_MIN, CarControllerParams.RPM_MAX)
      cruise_brake = np.clip(apply_brake, CarControllerParams.BRAKE_MIN, CarControllerParams.BRAKE_MAX)
    else:
      cruise_throttle = CarControllerParams.THROTTLE_INACTIVE
      cruise_rpm = CarControllerParams.RPM_MIN
      cruise_brake = CarControllerParams.BRAKE_MIN

    # *** alerts and pcm cancel ***
    if self.CP.flags & SubaruFlags.PREGLOBAL:
      if self.frame % 5 == 0:
        # 1 = main, 2 = set shallow, 3 = set deep, 4 = resume shallow, 5 = resume deep
        # disengage ACC when OP is disengaged
        if pcm_cancel_cmd:
          cruise_button = 1
        # turn main on if off and past start-up state
        elif not CS.out.cruiseState.available and CS.ready:
          cruise_button = 1
        else:
          cruise_button = CS.cruise_button

        # unstick previous mocked button press
        if cruise_button == 1 and self.cruise_button_prev == 1:
          cruise_button = 0
        self.cruise_button_prev = cruise_button

        can_sends.append(subarucan.create_preglobal_es_distance(self.packer, cruise_button, CS.es_distance_msg))

      # FrogPilot variables
      if CS.subaru_sng:
        can_sends.append(subarucan.create_preglobal_throttle(self.packer, CS.throttle_msg["COUNTER"] + 1, CS.throttle_msg, throttle_cmd))
    else:
      if self.frame % 10 == 0:
        can_sends.append(subarucan.create_es_dashstatus(self.packer, self.frame // 10, CS.es_dashstatus_msg, CC.enabled,
                                                        self.CP.openpilotLongitudinalControl, CC.longActive, hud_control.leadVisible))

        can_sends.append(subarucan.create_es_lkas_state(self.packer, self.frame // 10, CS.es_lkas_state_msg, CC.enabled, hud_control.visualAlert,
                                                        hud_control.leftLaneVisible, hud_control.rightLaneVisible,
                                                        hud_control.leftLaneDepart, hud_control.rightLaneDepart))

        if self.CP.flags & SubaruFlags.SEND_INFOTAINMENT:
          can_sends.append(subarucan.create_es_infotainment(self.packer, self.frame // 10, CS.es_infotainment_msg, hud_control.visualAlert))

      # FrogPilot variables
      if CS.subaru_sng:
        can_sends.append(subarucan.create_throttle(self.packer, CS.throttle_msg["COUNTER"] + 1, CS.throttle_msg, throttle_cmd))
        if self.frame % 2 == 0:
          can_sends.append(subarucan.create_brake_pedal(self.packer, self.frame // 2, CS.brake_pedal_msg, speed_cmd, pcm_cancel_cmd))

      if self.CP.openpilotLongitudinalControl:
        if self.frame % 5 == 0:
          can_sends.append(subarucan.create_es_status(self.packer, self.frame // 5, CS.es_status_msg,
                                                      self.CP.openpilotLongitudinalControl, CC.longActive, cruise_rpm))

          can_sends.append(subarucan.create_es_brake(self.packer, self.frame // 5, CS.es_brake_msg,
                                                     self.CP.openpilotLongitudinalControl, CC.longActive, cruise_brake))

          can_sends.append(subarucan.create_es_distance(self.packer, self.frame // 5, CS.es_distance_msg, 0, pcm_cancel_cmd,
                                                        self.CP.openpilotLongitudinalControl, cruise_brake > 0, cruise_throttle))
      else:
        if pcm_cancel_cmd:
          if not (self.CP.flags & SubaruFlags.HYBRID):
            bus = CanBus.alt if self.CP.flags & SubaruFlags.GLOBAL_GEN2 else CanBus.main
            can_sends.append(subarucan.create_es_distance(self.packer, CS.es_distance_msg["COUNTER"] + 1, CS.es_distance_msg, bus, pcm_cancel_cmd))

      if self.CP.flags & SubaruFlags.DISABLE_EYESIGHT:
        # Tester present (keeps eyesight disabled)
        if self.frame % 100 == 0:
          can_sends.append(make_tester_present_msg(GLOBAL_ES_ADDR, CanBus.camera, suppress_response=True))

        # Create all of the other eyesight messages to keep the rest of the car happy when eyesight is disabled
        if self.frame % 5 == 0:
          can_sends.append(subarucan.create_es_highbeamassist(self.packer))

        if self.frame % 10 == 0:
          can_sends.append(subarucan.create_es_static_1(self.packer))

        if self.frame % 2 == 0:
          can_sends.append(subarucan.create_es_static_2(self.packer))

    new_actuators = actuators.as_builder()
    new_actuators.torque = self.apply_torque_last / self.p.STEER_MAX
    new_actuators.torqueOutputCan = self.apply_torque_last

    self.frame += 1
    return new_actuators, can_sends

  # FrogPilot variables
  def stop_and_go(self, CC, CS, speed_cmd=False, throttle_cmd=False):
    if self.CP.flags & SubaruFlags.PREGLOBAL:
      trigger_resume = CC.enabled
      trigger_resume &= CS.car_follow == 1
      trigger_resume &= CS.close_distance > self.prev_close_distance
      trigger_resume &= CS.out.standstill
      trigger_resume &= _SNG_ACC_MIN_DIST < CS.close_distance < _SNG_ACC_MAX_DIST

      if trigger_resume:
        self.sng_acc_resume = True
    else:
      if CS.car_follow == 0 and CS.cruise_state == 3 and CS.out.standstill and self.prev_cruise_state == 1:
        self.manual_hold = True

      if not CS.out.standstill:
        self.manual_hold = False

      trigger_resume = CC.enabled
      trigger_resume &= CS.car_follow == 1
      trigger_resume &= CS.close_distance > self.prev_close_distance
      trigger_resume &= CS.cruise_state == 3
      trigger_resume &= not self.manual_hold
      trigger_resume &= _SNG_ACC_MIN_DIST < CS.close_distance < _SNG_ACC_MAX_DIST

      if trigger_resume:
        self.sng_acc_resume = True

      if CC.enabled and CS.car_follow == 1 and CS.out.standstill and self.frame > self.standstill_start + 50:
        speed_cmd = True

      if CS.out.standstill and not self.prev_standstill:
        self.standstill_start = self.frame

      self.prev_standstill = CS.out.standstill
      self.prev_cruise_state = CS.cruise_state

    if self.sng_acc_resume:
      if self.sng_acc_resume_cnt < 5:
        throttle_cmd = True
        self.sng_acc_resume_cnt += 1
      else:
        self.sng_acc_resume = False
        self.sng_acc_resume_cnt = -1

    self.prev_close_distance = CS.close_distance
    return throttle_cmd, speed_cmd
