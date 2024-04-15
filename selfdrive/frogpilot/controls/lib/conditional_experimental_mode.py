from openpilot.common.conversions import Conversions as CV
from openpilot.common.numpy_fast import interp
from openpilot.common.params import Params

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import CITY_SPEED_LIMIT, CRUISING_SPEED, PROBABILITY, MovingAverageCalculator

from openpilot.selfdrive.frogpilot.controls.lib.speed_limit_controller import SpeedLimitController

SLOW_DOWN_BP = [0., 10., 20., 30., 40., 50., 55., 60.]
SLOW_DOWN_DISTANCE = [20, 30., 50., 70., 80., 90., 105., 120.]
TRAJECTORY_SIZE = 33

class ConditionalExperimentalMode:
  def __init__(self):
    self.params = Params()
    self.params_memory = Params("/dev/shm/params")

    self.curve_detected = False
    self.experimental_mode = False
    self.lead_detected = False
    self.red_light_detected = False
    self.slower_lead_detected = False

    self.previous_status_value = 0
    self.previous_v_ego = 0
    self.previous_v_lead = 0
    self.status_value = 0

    self.curvature_mac = MovingAverageCalculator()
    self.lead_detection_mac = MovingAverageCalculator()
    self.lead_slowing_down_mac = MovingAverageCalculator()
    self.slow_lead_mac = MovingAverageCalculator()
    self.slowing_down_mac = MovingAverageCalculator()
    self.stop_light_mac = MovingAverageCalculator()

    self.update_frogpilot_params()

  def update(self, carState, enabled, frogpilotNavigation, lead, modelData, road_curvature, t_follow, v_ego):
    lead_distance = lead.dRel
    standstill = carState.standstill
    v_lead = lead.vLead

    if self.experimental_mode_via_press and enabled:
      overridden = self.params_memory.get_int("CEStatus")
    else:
      overridden = 0

    self.update_conditions(lead_distance, lead.status, modelData, road_curvature, standstill, t_follow, v_ego, v_lead)

    condition_met = self.check_conditions(carState, frogpilotNavigation, lead, modelData, standstill, v_ego) and enabled
    if condition_met and overridden not in {1, 3, 5} or overridden in {2, 4, 6}:
      self.experimental_mode = True
    else:
      self.experimental_mode = False
      self.status_value = 0

    self.status_value = overridden if overridden in {1, 2, 3, 4, 5, 6} else self.status_value
    if self.status_value != self.previous_status_value:
      self.params_memory.put_int("CEStatus", self.status_value)
      self.previous_status_value = self.status_value

    if self.params_memory.get_bool("FrogPilotTogglesUpdated"):
      self.update_frogpilot_params()

  def check_conditions(self, carState, frogpilotNavigation, lead, modelData, standstill, v_ego):
    if standstill:
      self.status_value = 0
      return self.experimental_mode

    # Keep Experimental Mode active if stopping for a red light
    if self.status_value == 15 and self.slowing_down(v_ego):
      return True

    if self.navigation and modelData.navEnabled and (frogpilotNavigation.approachingIntersection or frogpilotNavigation.approachingTurn) and (self.navigation_lead or not self.lead_detected):
      self.status_value = 7 if frogpilotNavigation.approachingIntersection else 8
      return True

    if SpeedLimitController.experimental_mode:
      self.status_value = 9
      return True

    if (not self.lead_detected and v_ego <= self.limit) or (self.lead_detected and v_ego <= self.limit_lead):
      self.status_value = 10 if self.lead_detected else 11
      return True

    if self.slower_lead and self.slower_lead_detected:
      self.status_value = 12
      return True

    if self.signal and v_ego <= CITY_SPEED_LIMIT and (carState.leftBlinker or carState.rightBlinker):
      self.status_value = 13
      return True

    if self.curves and self.curve_detected:
      self.status_value = 14
      return True

    if self.stop_lights and self.red_light_detected:
      self.status_value = 15
      return True

    return False

  def update_conditions(self, lead_distance, lead_status, modelData, road_curvature, standstill, t_follow, v_ego, v_lead):
    self.lead_detection(lead_status)
    self.road_curvature(road_curvature)
    self.slow_lead(lead_distance, t_follow, v_ego)
    self.stop_sign_and_light(lead_distance, modelData, standstill, v_ego, v_lead)

  def lead_detection(self, lead_status):
    self.lead_detection_mac.add_data(lead_status)
    self.lead_detected = self.lead_detection_mac.get_moving_average() >= PROBABILITY

  def lead_slowing_down(self, lead_distance, v_ego, v_lead):
    if self.lead_detected:
      lead_close = lead_distance < CITY_SPEED_LIMIT
      lead_far = lead_distance >= CITY_SPEED_LIMIT and (v_lead >= self.previous_v_lead > 1 or v_lead > v_ego)
      lead_slowing_down = v_lead < self.previous_v_lead
      lead_stopped = v_lead < 1

      self.previous_v_lead = v_lead

      self.lead_slowing_down_mac.add_data((lead_close or lead_slowing_down or lead_stopped) and not lead_far)
      return self.lead_slowing_down_mac.get_moving_average() >= PROBABILITY
    else:
      self.lead_slowing_down_mac.reset_data()
      self.previous_v_lead = 0
      return False

  # Determine the road curvature - Credit goes to to Pfeiferj!
  def road_curvature(self, road_curvature):
    lead_check = self.curves_lead or not self.lead_detected

    if lead_check and not self.red_light_detected:
      # Setting a limit of 3.5 helps prevent it triggering for red lights
      curve_detected = 3.5 >= road_curvature > 1.6
      curve_active = 3.5 >= road_curvature > 1.1 and self.curve_detected

      self.curvature_mac.add_data(curve_detected or curve_active)
      self.curve_detected = self.curvature_mac.get_moving_average() >= PROBABILITY
    else:
      self.curvature_mac.reset_data()
      self.curve_detected = False

  def slow_lead(self, lead_distance, t_follow, v_ego):
    if self.lead_detected:
      slower_lead_ahead = lead_distance < (v_ego - 1) * t_follow

      self.slow_lead_mac.add_data(slower_lead_ahead)
      self.slower_lead_detected = self.slow_lead_mac.get_moving_average() >= PROBABILITY
    else:
      self.slow_lead_mac.reset_data()
      self.slower_lead_detected = False

  def slowing_down(self, v_ego):
    slowing_down = v_ego <= self.previous_v_ego
    speed_check = v_ego < CRUISING_SPEED

    self.previous_v_ego = v_ego

    self.slowing_down_mac.add_data(slowing_down and speed_check)
    return self.slowing_down_mac.get_moving_average() >= PROBABILITY

  # Stop sign/stop light detection - Credit goes to the DragonPilot team!
  def stop_sign_and_light(self, lead_distance, modelData, standstill, v_ego, v_lead):
    lead_check = self.stop_lights_lead or not self.lead_slowing_down(lead_distance, v_ego, v_lead) or standstill

    model_check = len(modelData.orientation.x) == len(modelData.position.x) == TRAJECTORY_SIZE
    model_stopping = modelData.position.x[TRAJECTORY_SIZE - 1] < interp(v_ego * CV.MS_TO_KPH, SLOW_DOWN_BP, SLOW_DOWN_DISTANCE)

    model_filtered = not (self.curve_detected or self.slower_lead_detected)

    self.stop_light_mac.add_data(lead_check and model_check and model_stopping and model_filtered)
    self.red_light_detected = self.stop_light_mac.get_moving_average() >= PROBABILITY

  def update_frogpilot_params(self):
    is_metric = self.params.get_bool("IsMetric")

    self.curves = self.params.get_bool("CECurves")
    self.curves_lead = self.curves and self.params.get_bool("CECurvesLead")

    self.experimental_mode_via_press = self.params.get_bool("ExperimentalModeActivation")

    self.limit = self.params.get_int("CESpeed") * (CV.KPH_TO_MS if is_metric else CV.MPH_TO_MS)
    self.limit_lead = self.params.get_int("CESpeedLead") * (CV.KPH_TO_MS if is_metric else CV.MPH_TO_MS)

    self.navigation = self.params.get_bool("CENavigation")
    self.navigation_lead = self.navigation and self.params.get_bool("CENavigationLead")

    self.signal = self.params.get_bool("CESignal")

    self.slower_lead = self.params.get_bool("CESlowerLead")

    self.stop_lights = self.params.get_bool("CEStopLights")
    self.stop_lights_lead = self.stop_lights and self.params.get_bool("CEStopLightsLead")
