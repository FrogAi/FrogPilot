from openpilot.common.conversions import Conversions as CV
from openpilot.common.numpy_fast import interp
from openpilot.common.params import Params
from openpilot.selfdrive.modeld.constants import ModelConstants

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import MovingAverageCalculator
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_variables import CITY_SPEED_LIMIT, CRUISING_SPEED, PROBABILITY

SLOW_DOWN_BP = [0., 10., 20., 30., 40., 50., 55., 60.]
SLOW_DOWN_DISTANCE = [20, 30., 50., 70., 80., 90., 105., 120.]

class ConditionalExperimentalMode:
  def __init__(self):
    self.params_memory = Params("/dev/shm/params")

    self.curve_detected = False
    self.experimental_mode = False
    self.lead_stopped = False
    self.red_light_detected = False

    self.previous_v_ego = 0
    self.previous_v_lead = 0

    self.curvature_mac = MovingAverageCalculator()
    self.lead_detection_mac = MovingAverageCalculator()
    self.lead_slowing_down_mac = MovingAverageCalculator()
    self.slow_lead_mac = MovingAverageCalculator()
    self.slowing_down_mac = MovingAverageCalculator()
    self.stop_light_mac = MovingAverageCalculator()

  def update(self, carState, enabled, frogpilotNavigation, lead_distance, lead, modelData, road_curvature, slower_lead, v_ego, v_lead, frogpilot_toggles):
    if frogpilot_toggles.experimental_mode_via_press and enabled:
      overridden = self.params_memory.get_int("CEStatus")
    else:
      overridden = 0

    self.update_conditions(lead_distance, lead.status, modelData, road_curvature, slower_lead, carState.standstill, v_ego, v_lead, frogpilot_toggles)

    condition_met = self.check_conditions(carState, frogpilotNavigation, lead, modelData, v_ego, frogpilot_toggles) and enabled
    self.experimental_mode = condition_met and overridden not in {1, 3, 5} or overridden in {2, 4, 6}

    self.params_memory.put_int("CEStatus", overridden if overridden in {1, 2, 3, 4, 5, 6} else self.status_value if condition_met else 0)

  def check_conditions(self, carState, frogpilotNavigation, lead, modelData, v_ego, frogpilot_toggles):
    if carState.standstill:
      self.status_value = 0
      return self.experimental_mode

    # Keep Experimental Mode active if stopping for a red light
    if self.red_light_detected and self.slowing_down(v_ego):
      return True

    approaching_maneuver = modelData.navEnabled and (frogpilotNavigation.approachingIntersection or frogpilotNavigation.approachingTurn)
    if frogpilot_toggles.conditional_navigation and approaching_maneuver and (frogpilot_toggles.conditional_navigation_lead or not self.lead_detected):
      self.status_value = 7 if frogpilotNavigation.approachingIntersection else 8
      return True

    if (not self.lead_detected and v_ego <= frogpilot_toggles.conditional_limit) or (self.lead_detected and v_ego <= frogpilot_toggles.conditional_limit_lead):
      self.status_value = 11 if self.lead_detected else 12
      return True

    if frogpilot_toggles.conditional_slower_lead and self.slower_lead_detected:
      self.status_value = 12 if self.lead_stopped else 13
      return True

    if frogpilot_toggles.conditional_signal and v_ego <= CITY_SPEED_LIMIT and (carState.leftBlinker or carState.rightBlinker):
      self.status_value = 14
      return True

    if frogpilot_toggles.conditional_curves and self.curve_detected:
      self.status_value = 15
      return True

    if frogpilot_toggles.conditional_stop_lights and self.red_light_detected:
      self.status_value = 16
      return True

    return False

  def update_conditions(self, lead_distance, lead_status, modelData, road_curvature, slower_lead, standstill, v_ego, v_lead, frogpilot_toggles):
    self.lead_detection(lead_status)
    self.road_curvature(road_curvature, v_ego, frogpilot_toggles)
    self.slow_lead(slower_lead, v_lead)
    self.stop_sign_and_light(lead_distance, modelData, standstill, v_ego, v_lead, frogpilot_toggles)

  def lead_detection(self, lead_status):
    self.lead_detection_mac.add_data(lead_status)
    self.lead_detected = self.lead_detection_mac.get_moving_average() >= PROBABILITY

  def lead_slowing_down(self, lead_distance, v_ego, v_lead):
    if self.lead_detected:
      lead_close = lead_distance < CITY_SPEED_LIMIT
      lead_far = lead_distance >= CITY_SPEED_LIMIT and (v_lead >= self.previous_v_lead > 1 or v_lead > v_ego or self.red_light_detected)
      lead_slowing_down = v_lead < self.previous_v_lead

      self.previous_v_lead = v_lead

      self.lead_slowing_down_mac.add_data((lead_close or lead_slowing_down or self.lead_stopped) and not lead_far)
      return self.lead_slowing_down_mac.get_moving_average() >= PROBABILITY
    else:
      self.lead_slowing_down_mac.reset_data()
      self.previous_v_lead = 0
      return False

  # Determine the road curvature - Credit goes to to Pfeiferj!
  def road_curvature(self, road_curvature, v_ego, frogpilot_toggles):
    if frogpilot_toggles.conditional_curves_lead or not self.lead_detected:
      curve_detected = (1 / road_curvature)**0.5 < v_ego
      curve_active = (0.9 / road_curvature)**0.5 < v_ego and self.curve_detected

      self.curvature_mac.add_data(curve_detected or curve_active)
      self.curve_detected = self.curvature_mac.get_moving_average() >= PROBABILITY
    else:
      self.curvature_mac.reset_data()
      self.curve_detected = False

  def slow_lead(self, slower_lead, v_lead):
    if self.lead_detected:
      self.lead_stopped = v_lead < 1
      self.slow_lead_mac.add_data(self.lead_stopped or slower_lead)
      self.slower_lead_detected = self.slow_lead_mac.get_moving_average() >= PROBABILITY
    else:
      self.slow_lead_mac.reset_data()
      self.lead_stopped = False
      self.slower_lead_detected = False

  def slowing_down(self, v_ego):
    slowing_down = v_ego <= self.previous_v_ego
    speed_check = v_ego < CRUISING_SPEED

    self.previous_v_ego = v_ego

    self.slowing_down_mac.add_data(slowing_down and speed_check)
    return self.slowing_down_mac.get_moving_average() >= PROBABILITY

  # Stop sign/stop light detection - Credit goes to the DragonPilot team!
  def stop_sign_and_light(self, lead_distance, modelData, standstill, v_ego, v_lead, frogpilot_toggles):
    lead_check = frogpilot_toggles.conditional_stop_lights_lead or not self.lead_slowing_down(lead_distance, v_ego, v_lead) or standstill
    model_stopping = modelData.position.x[ModelConstants.IDX_N - 1] < interp(v_ego * CV.MS_TO_KPH, SLOW_DOWN_BP, SLOW_DOWN_DISTANCE)
    model_filtered = not (self.curve_detected or self.slower_lead_detected)

    self.stop_light_mac.add_data(lead_check and model_stopping and model_filtered)
    self.red_light_detected = self.stop_light_mac.get_moving_average() >= PROBABILITY
