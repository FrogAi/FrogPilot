from openpilot.common.numpy_fast import clip, interp
from cereal import car
from typing import Any
from openpilot.common.realtime import DT_CTRL
from openpilot.common.params import Params
from openpilot.selfdrive.controls.lib.longcontrol import LongControl
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import LongitudinalMpc
from openpilot.selfdrive.car.hyundai.values import HyundaiFlags, CarControllerParams


LongCtrlState = car.CarControl.Actuators.LongControlState

class JerkOutput:
  def __init__(self, jerk_upper_limit, jerk_lower_limit, cb_upper, cb_lower):
    self.jerk_upper_limit = jerk_upper_limit
    self.jerk_lower_limit = jerk_lower_limit
    self.cb_upper = cb_upper
    self.cb_lower = cb_lower

class HKGLongitudinalTuning:
  def __init__(self, CP) -> None:
    self.CP = CP
    self._setup_controllers()
    self._init_state()

  def _setup_controllers(self) -> None:
    self.mpc = LongitudinalMpc(mode='acc')

    self.long_control = LongControl(self.CP)
    self.DT_CTRL = DT_CTRL
    self.params = Params()
    self.hkg_tuning = self.params.get_bool('HKGtuning')
    self.has_radar = self.params.get_bool("HyundaiRadarTracks")

  def _init_state(self) -> None:
    self.last_accel = 0.0
    self.brake_ramp = 0.0
    self.accel_last = 0.0
    self.using_e2e = False
    self.accel_raw = 0.0
    self.accel_last_jerk = 0.0
    self.jerk = 0.0
    self.jerk_count = 0.0
    self.jerk_upper_limit = 0.0
    self.jerk_lower_limit = 0.0
    self.cb_upper = self.cb_lower = 0.0


  def make_jerk(self, CS, accel, actuators):
    state = getattr(actuators, "longControlState", LongCtrlState.pid)
    # Handle cancel state to prevent cruise fault
    if not CS.out.cruiseState.enabled:
      self.jerk_upper_limit = 0.0
      self.jerk_lower_limit = 0.0
      self.cb_upper = self.cb_lower = 0.0
      self.accel_last_jerk = 0.0
      return 0.0

    current_accel = clip(actuators.accel, CarControllerParams.ACCEL_MIN, CarControllerParams.ACCEL_MAX)
    self.jerk = (current_accel - self.accel_last_jerk) / self.DT_CTRL
    self.accel_last_jerk = current_accel

    jerk_max = 5.0
    v_error = abs(CS.out.vEgo - CS.out.cruiseState.speed)

    if self.CP.flags & HyundaiFlags.CANFD.value:
      if v_error < 3.0:
        jerk_reduction = interp(v_error,
                              [0.0, 1.0, 3.0],
                              [0.3, 0.5, 1.0])
        jerk_max *= jerk_reduction

      self.jerk_upper_limit = min(max(0.5, self.jerk * 2.0), jerk_max)
      self.jerk_lower_limit = min(max(1.0, -self.jerk * 4.0), jerk_max)
      self.cb_upper = self.cb_lower = 0.0
    else:
      self.jerk_upper_limit = min(max(0.5, self.jerk * 2.0), jerk_max)
      self.jerk_lower_limit = min(max(1.0, -self.jerk * 2.0), jerk_max)
      # Make comfort band smaller when close to set speed, bigger when far away
      error_factor = interp(v_error, [0.0, 0.5, 1.0, 5.0],
                                    [0.0, 0.1, 0.5, 1.0])

      accel_factor = interp(abs(accel), [0.0, 1.0], [0.2, 0.1])
      if accel >= 0:
          self.cb_upper = clip(0.8 * error_factor + accel * accel_factor, 0, 1.0)
          self.cb_lower = clip(0.6 * error_factor + accel * accel_factor, 0, 0.8)
      else:
          self.cb_upper = clip(1.0 * error_factor + accel * accel_factor, 0, 1.2)
          self.cb_lower = clip(0.8 * error_factor + accel * accel_factor, 0, 1.2)

    return self.jerk

  def handle_cruise_cancel(self, CS):
    """Handle cruise control cancel to prevent faults."""
    if not CS.out.cruiseState.enabled or CS.out.gasPressed or CS.out.brakePressed:
      self.accel_last = 0.0
      self.brake_ramp = 0.0
      return True
    return False

  def calculate_limited_accel(self, accel, actuators, CS):
    """acceleration limiting."""
    # Reset acceleration limits when cruise is disabled or brake is pressed
    if self.handle_cruise_cancel(CS):
      return accel

    self.make_jerk(CS, accel, actuators)

    accel_delta = accel - self.accel_last
    brake_aggressiveness = 0.0
    ramp_rate = 0.7

    if accel > 0 and self.accel_last < 0:
      self.accel_last = 0.0
      accel_delta = accel

    if accel < 0:
      # Negative accel logic
      brake_ratio = clip(abs(accel / CarControllerParams.ACCEL_MIN), 0.0, 1.0)
      brake_aggressiveness = brake_ratio ** 1.5

      if CS.out.vEgo < 4.47:  # ~10 mph
        ramp_rate = interp(
          brake_aggressiveness,
          [0.0, 0.2, 0.4, 0.6, 0.8, 1.0],
          [0.4, 0.55, 0.65, 0.85, 1.2, 1.3]
          )
      else:
        ramp_rate = interp(
          brake_aggressiveness,
          [0.0, 0.2, 0.4, 0.6, 0.8, 1.0],
          [0.2, 0.3, 0.4, 0.6, 0.9, 1.1]
          )

      if brake_ratio > 0.8:
        ramp_rate *= 0.8

      if self.accel_last >= 0:
        self.brake_ramp = 0.0
        ramp_rate *= 0.5

      self.brake_ramp = min(1.0, self.brake_ramp + (ramp_rate * self.DT_CTRL))

      # Smooth factor
      smooth_factor = interp(abs(accel), [0.0, 0.3, 1.0, 2.0], [0.95, 0.85, 0.75, 0.60])
      if brake_aggressiveness > 0.8:
        smooth_factor *= 0.8

      # Final smoothing scaled by brake ramp
      accel_delta *= (smooth_factor * self.brake_ramp)

      # Enforce jerk limits
      accel_delta = clip(
        accel_delta,
        -self.jerk_lower_limit * self.DT_CTRL,
        self.jerk_upper_limit * self.DT_CTRL
      )

      # Transition from positive accel to negative
      if self.accel_last > 0:
        transition_factor = interp(
          abs(accel), [0, 0.5, 1.0, 1.5, 2.0],
          [0.3, 0.4, 0.5, 0.6, 0.7]
        )
        accel_delta *= transition_factor
    else:
      # Positive Acceleration Logic
      ramp_rate = 0.9 # Increase ramp rate for positive acceleration
      accel_delta = min(accel - self.accel_last, ramp_rate * self.DT_CTRL)

    # Update accel
    accel = self.accel_last + accel_delta
    self.accel_last = accel
    return accel

  def calculate_accel(self, accel, actuators, CS, frogpilot_toggles):
    """Calculate acceleration with cruise control status handling."""
    if self.handle_cruise_cancel(CS):
      return 0.0
    accel = self.calculate_limited_accel(accel, actuators, CS)
    return clip(accel, CarControllerParams.ACCEL_MIN, min(frogpilot_toggles.max_desired_acceleration, CarControllerParams.ACCEL_MAX))

  def apply_tune(self, CP: Any) -> None:
    CP.vEgoStopping = 0.3
    CP.vEgoStarting = 0.1
    CP.stoppingDecelRate = 0.01
    CP.startAccel = 1.6
    CP.startingState = True

  def get_jerk(self) -> JerkOutput:
    return JerkOutput(
      self.jerk_upper_limit,
      self.jerk_lower_limit,
      self.cb_upper,
      self.cb_lower,
    )

  def calculate_and_get_jerk(self, CS, accel, actuators):
    """Calculate jerk and return JerkOutput."""
    if self.hkg_tuning:
      self.make_jerk(CS, accel, actuators)
      return self.get_jerk()
    else:
      normal_jerk = self.calculate_normal_jerk(actuators.longControlState)
      return JerkOutput(normal_jerk, normal_jerk, 0.0, 0.0)

class HKGLongitudinalController:
  def __init__(self, CP):
    self.CP = CP
    self.tuning = HKGLongitudinalTuning(CP) if Params().get_bool("HKGtuning") else None
    self.jerk = None

  def apply_tune(self, CP):
    if self.tuning:
      self.tuning.apply_tune(CP)

  def calculate_normal_jerk(self, long_control_state):
    """Calculate normal jerk based on long control state."""
    return 3.0 if long_control_state == LongCtrlState.pid else 1.0
