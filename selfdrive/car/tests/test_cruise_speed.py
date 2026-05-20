import pytest
import itertools
import numpy as np

from types import SimpleNamespace

from openpilot.common.parameterized import parameterized_class
from cereal import log
from openpilot.selfdrive.car.cruise import VCruiseHelper, V_CRUISE_MIN, V_CRUISE_MAX, V_CRUISE_INITIAL, V_CRUISE_UNSET, IMPERIAL_INCREMENT
from cereal import car
from opendbc.car.gm.values import CAR, GMOPGMFlags
from openpilot.common.constants import CV
from openpilot.selfdrive.test.longitudinal_maneuvers.maneuver import Maneuver

ButtonEvent = car.CarState.ButtonEvent
ButtonType = car.CarState.ButtonEvent.Type


def run_cruise_simulation(cruise, e2e, personality, t_end=20.):
  man = Maneuver(
    '',
    duration=t_end,
    initial_speed=max(cruise - 1., 0.0),
    lead_relevancy=True,
    initial_distance_lead=100,
    cruise_values=[cruise],
    prob_lead_values=[0.0],
    breakpoints=[0.],
    e2e=e2e,
    personality=personality,
  )
  valid, output = man.evaluate()
  assert valid
  return output[-1, 3]


@parameterized_class(("e2e", "personality", "speed"), itertools.product(
                      [True, False], # e2e
                      log.LongitudinalPersonality.schema.enumerants, # personality
                      [5,35])) # speed
class TestCruiseSpeed:
  def test_cruise_speed(self):
    print(f'Testing {self.speed} m/s')
    cruise_speed = float(self.speed)

    simulation_steady_state = run_cruise_simulation(cruise_speed, self.e2e, self.personality)
    assert simulation_steady_state == pytest.approx(cruise_speed, abs=.01), f'Did not reach {self.speed} m/s'


# TODO: test pcmCruise
@parameterized_class(('pcm_cruise',), [(False,)])
class TestVCruiseHelper:
  def setup_method(self):
    self.CP = car.CarParams(pcmCruise=self.pcm_cruise)
    self.v_cruise_helper = VCruiseHelper(self.CP)
    self.frogpilot_toggles = SimpleNamespace(
      conditional_experimental_mode=False,
      cruise_increase=1,
      cruise_increase_long=5,
      set_speed_offset=0,
    )
    self.reset_cruise_speed_state()

  def reset_cruise_speed_state(self):
    # Two resets previous cruise speed
    for _ in range(2):
      self.v_cruise_helper.update_v_cruise(car.CarState(cruiseState={"available": False}), enabled=False, is_metric=False,
                                           frogpilot_toggles=self.frogpilot_toggles)

  def enable(self, v_ego, experimental_mode):
    # Simulates user pressing set with a current speed
    self.v_cruise_helper.initialize_v_cruise(car.CarState(vEgo=v_ego), experimental_mode, self.frogpilot_toggles)

  def test_adjust_speed(self):
    """
    Asserts speed changes on falling edges of buttons.
    """

    self.enable(V_CRUISE_INITIAL * CV.KPH_TO_MS, False)

    for btn in (ButtonType.accelCruise, ButtonType.decelCruise):
      for pressed in (True, False):
        CS = car.CarState(cruiseState={"available": True})
        CS.buttonEvents = [ButtonEvent(type=btn, pressed=pressed)]

        self.v_cruise_helper.update_v_cruise(CS, enabled=True, is_metric=False, frogpilot_toggles=self.frogpilot_toggles)
        assert pressed == (self.v_cruise_helper.v_cruise_kph == self.v_cruise_helper.v_cruise_kph_last)

  def test_rising_edge_enable(self):
    """
    Some car interfaces may enable on rising edge of a button,
    ensure we don't adjust speed if enabled changes mid-press.
    """

    # NOTE: enabled is always one frame behind the result from button press in controlsd
    for enabled, pressed in ((False, False),
                             (False, True),
                             (True, False)):
      CS = car.CarState(cruiseState={"available": True})
      CS.buttonEvents = [ButtonEvent(type=ButtonType.decelCruise, pressed=pressed)]
      self.v_cruise_helper.update_v_cruise(CS, enabled=enabled, is_metric=False, frogpilot_toggles=self.frogpilot_toggles)
      if pressed:
        self.enable(V_CRUISE_INITIAL * CV.KPH_TO_MS, False)

      # Expected diff on enabling. Speed should not change on falling edge of pressed
      assert (not pressed) == (self.v_cruise_helper.v_cruise_kph == self.v_cruise_helper.v_cruise_kph_last)

  def test_resume_in_standstill(self):
    """
    Asserts we don't increment set speed if user presses resume/accel to exit cruise standstill.
    """

    self.enable(0, False)

    for standstill in (True, False):
      for pressed in (True, False):
        CS = car.CarState(cruiseState={"available": True, "standstill": standstill})
        CS.buttonEvents = [ButtonEvent(type=ButtonType.accelCruise, pressed=pressed)]
        self.v_cruise_helper.update_v_cruise(CS, enabled=True, is_metric=False, frogpilot_toggles=self.frogpilot_toggles)

        # speed should only update if not at standstill and button falling edge
        should_equal = standstill or pressed
        assert should_equal == (self.v_cruise_helper.v_cruise_kph == self.v_cruise_helper.v_cruise_kph_last)

  def test_set_gas_pressed(self):
    """
    Asserts pressing set while enabled with gas pressed sets
    the speed to the maximum of vEgo and current cruise speed.
    """

    for v_ego in np.linspace(0, 100, 101):
      self.reset_cruise_speed_state()
      self.enable(V_CRUISE_INITIAL * CV.KPH_TO_MS, False)

      # first decrement speed, then perform gas pressed logic
      expected_v_cruise_kph = self.v_cruise_helper.v_cruise_kph - IMPERIAL_INCREMENT
      expected_v_cruise_kph = max(expected_v_cruise_kph, v_ego * CV.MS_TO_KPH)  # clip to min of vEgo
      expected_v_cruise_kph = float(np.clip(round(expected_v_cruise_kph, 1), V_CRUISE_MIN, V_CRUISE_MAX))

      CS = car.CarState(vEgo=float(v_ego), gasPressed=True, cruiseState={"available": True})
      CS.buttonEvents = [ButtonEvent(type=ButtonType.decelCruise, pressed=False)]
      self.v_cruise_helper.update_v_cruise(CS, enabled=True, is_metric=False, frogpilot_toggles=self.frogpilot_toggles)

      # TODO: fix skipping first run due to enabled on rising edge exception
      if v_ego == 0.0:
        continue
      assert expected_v_cruise_kph == self.v_cruise_helper.v_cruise_kph

  def test_initialize_v_cruise(self):
    """
    Asserts allowed cruise speeds on enabling with SET.
    """

    for experimental_mode in (True, False):
      for v_ego in np.linspace(0, 100, 101):
        self.reset_cruise_speed_state()
        assert not self.v_cruise_helper.v_cruise_initialized

        self.enable(float(v_ego), experimental_mode)
        assert V_CRUISE_INITIAL <= self.v_cruise_helper.v_cruise_kph <= V_CRUISE_MAX
        assert self.v_cruise_helper.v_cruise_initialized

  # OPGM variables
  @staticmethod
  def get_gm_cc_only_v_cruise_helper():
    CP = car.CarParams(pcmCruise=True, carFingerprint=CAR.CHEVROLET_BOLT_2018, flags=GMOPGMFlags.CC_LONG.value)
    return VCruiseHelper(CP)

  def test_gm_cc_only_first_resume_initializes_from_ego_speed(self):
    v_cruise_helper = self.get_gm_cc_only_v_cruise_helper()
    assert v_cruise_helper.v_cruise_kph == V_CRUISE_UNSET

    v_cruise_helper.update_button_intent(car.CarState(buttonEvents=[ButtonEvent(type=ButtonType.resumeCruise, pressed=True)]))
    v_cruise_helper.initialize_v_cruise(car.CarState(vEgo=30 * CV.MPH_TO_MS), experimental_mode=False, frogpilot_toggles=self.frogpilot_toggles)

    assert V_CRUISE_INITIAL <= v_cruise_helper.v_cruise_kph <= V_CRUISE_MAX
    assert v_cruise_helper.v_cruise_kph != V_CRUISE_UNSET

  def test_gm_cc_only_resume_after_initialized_restores_previous_cruise(self):
    v_cruise_helper = self.get_gm_cc_only_v_cruise_helper()
    v_cruise_helper.v_cruise_kph = 72
    v_cruise_helper.v_cruise_kph_last = 72

    v_cruise_helper.update_button_intent(car.CarState(buttonEvents=[ButtonEvent(type=ButtonType.resumeCruise, pressed=True)]))
    v_cruise_helper.initialize_v_cruise(car.CarState(vEgo=20 * CV.MPH_TO_MS), experimental_mode=False, frogpilot_toggles=self.frogpilot_toggles)

    assert v_cruise_helper.v_cruise_kph == 72

  def test_gm_cc_only_button_intent_tracks_resume_and_set_buttons(self):
    v_cruise_helper = self.get_gm_cc_only_v_cruise_helper()

    v_cruise_helper.update_button_intent(car.CarState(buttonEvents=[ButtonEvent(type=ButtonType.resumeCruise, pressed=True)]))
    assert v_cruise_helper.resume_previous_cruise

    for button in (ButtonType.decelCruise, ButtonType.setCruise):
      v_cruise_helper.update_button_intent(car.CarState(buttonEvents=[ButtonEvent(type=button, pressed=True)]))
      assert not v_cruise_helper.resume_previous_cruise
