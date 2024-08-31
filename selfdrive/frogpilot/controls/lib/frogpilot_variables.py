import os
import random

from types import SimpleNamespace

from cereal import car
from openpilot.common.conversions import Conversions as CV
from openpilot.common.numpy_fast import interp
from openpilot.common.params import Params, UnknownKeyName
from openpilot.selfdrive.controls.lib.desire_helper import LANE_CHANGE_SPEED_MIN
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.system.hardware.power_monitoring import VBATT_PAUSE_CHARGING

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import MODELS_PATH
from openpilot.selfdrive.frogpilot.controls.lib.model_manager import DEFAULT_MODEL, DEFAULT_MODEL_NAME, process_model_name

CITY_SPEED_LIMIT = 25                                   # 55mph is typically the minimum speed for highways
CRUISING_SPEED = 5                                      # Roughly the speed cars go when not touching the gas while in drive
MODEL_LENGTH = ModelConstants.IDX_N                     # Minimum length of the model
PLANNER_TIME = ModelConstants.T_IDXS[MODEL_LENGTH - 1]  # Length of time the model projects out for
THRESHOLD = 0.6                                         # 60% chance of condition being true

def get_max_allowed_accel(v_ego):
  return interp(v_ego, [0., 5., 20.], [4.0, 4.0, 2.0])  # ISO 15622:2018

class FrogPilotVariables:
  def __init__(self):
    self.frogpilot_toggles = SimpleNamespace()

    self.params = Params()
    self.params_memory = Params("/dev/shm/params")

    self.has_prime = self.params.get_int("PrimeType") > 0

    self.update_frogpilot_params(False)

  @property
  def toggles(self):
    return self.frogpilot_toggles

  @property
  def toggles_updated(self):
    return self.params_memory.get_bool("FrogPilotTogglesUpdated")

  def update_frogpilot_params(self, started=True, frogpilot_process=False):
    toggle = self.frogpilot_toggles

    openpilot_installed = self.params.get_bool("HasAcceptedTerms")

    key = "CarParams" if started else "CarParamsPersistent"
    msg_bytes = self.params.get(key, block=openpilot_installed and started)

    if msg_bytes:
      with car.CarParams.from_bytes(msg_bytes) as CP:
        car_make = CP.carName
        car_model = CP.carFingerprint
        toggle.openpilot_longitudinal = CP.openpilotLongitudinalControl
        pcm_cruise = CP.pcmCruise
    else:
      car_make = "mock"
      car_model = "mock"
      toggle.openpilot_longitudinal = False
      pcm_cruise = False

    toggle.is_metric = self.params.get_bool("IsMetric")
    distance_conversion = 1. if toggle.is_metric else CV.FOOT_TO_METER
    speed_conversion = CV.KPH_TO_MS if toggle.is_metric else CV.MPH_TO_MS

    toggle.alert_volume_control = self.params.get_bool("AlertVolumeControl")
    toggle.disengage_volume = self.params.get_int("DisengageVolume") if toggle.alert_volume_control else 100
    toggle.engage_volume = self.params.get_int("EngageVolume") if toggle.alert_volume_control else 100
    toggle.prompt_volume = self.params.get_int("PromptVolume") if toggle.alert_volume_control else 100
    toggle.promptDistracted_volume = self.params.get_int("PromptDistractedVolume") if toggle.alert_volume_control else 100
    toggle.refuse_volume = self.params.get_int("RefuseVolume") if toggle.alert_volume_control else 100
    toggle.warningSoft_volume = self.params.get_int("WarningSoftVolume") if toggle.alert_volume_control else 100
    toggle.warningImmediate_volume = max(self.params.get_int("WarningImmediateVolume"), 25) if toggle.alert_volume_control else 100

    toggle.always_on_lateral = self.params.get_bool("AlwaysOnLateral") and self.params.get_bool("AlwaysOnLateralSet")
    toggle.always_on_lateral_lkas = toggle.always_on_lateral and self.params.get_bool("AlwaysOnLateralLKAS")
    toggle.always_on_lateral_main = toggle.always_on_lateral and self.params.get_bool("AlwaysOnLateralMain")
    toggle.always_on_lateral_pause_speed = self.params.get_int("PauseAOLOnBrake") if toggle.always_on_lateral else 0

    toggle.automatic_updates = self.params.get_bool("AutomaticUpdates")

    bonus_content = self.params.get_bool("BonusContent")
    toggle.goat_scream = bonus_content and self.params.get_bool("GoatScream")
    holiday_themes = bonus_content and self.params.get_bool("HolidayThemes")
    toggle.current_holiday_theme = self.params.get("CurrentHolidayTheme", encoding='utf-8') if holiday_themes else None
    personalize_openpilot = bonus_content and self.params.get_bool("PersonalizeOpenpilot")
    toggle.sound_pack = self.params.get("CustomSignals", encoding='utf-8') if personalize_openpilot else "stock"
    toggle.wheel_image = self.params.get("WheelIcon", encoding='utf-8') if personalize_openpilot else "stock"
    toggle.random_events = bonus_content and self.params.get_bool("RandomEvents")

    toggle.cluster_offset = self.params.get_float("ClusterOffset") if car_make == "toyota" else 1

    toggle.conditional_experimental_mode = toggle.openpilot_longitudinal and self.params.get_bool("ConditionalExperimental")
    toggle.conditional_curves = toggle.conditional_experimental_mode and self.params.get_bool("CECurves")
    toggle.conditional_curves_lead = toggle.conditional_curves and self.params.get_bool("CECurvesLead")
    toggle.conditional_lead = toggle.conditional_experimental_mode and self.params.get_bool("CELead")
    toggle.conditional_slower_lead = toggle.conditional_lead and self.params.get_bool("CESlowerLead")
    toggle.conditional_stopped_lead = toggle.conditional_lead and self.params.get_bool("CEStoppedLead")
    toggle.conditional_limit = self.params.get_int("CESpeed") * speed_conversion if toggle.conditional_experimental_mode else 0
    toggle.conditional_limit_lead = self.params.get_int("CESpeedLead") * speed_conversion if toggle.conditional_experimental_mode else 0
    toggle.conditional_model_stop_time = self.params.get_int("CEModelStopTime") if toggle.conditional_experimental_mode else 0
    toggle.conditional_navigation = toggle.conditional_experimental_mode and self.params.get_bool("CENavigation")
    toggle.conditional_navigation_intersections = toggle.conditional_navigation and self.params.get_bool("CENavigationIntersections")
    toggle.conditional_navigation_lead = toggle.conditional_navigation and self.params.get_bool("CENavigationLead")
    toggle.conditional_navigation_turns = toggle.conditional_navigation and self.params.get_bool("CENavigationTurns")
    toggle.conditional_signal = toggle.conditional_experimental_mode and self.params.get_bool("CESignal")
    if toggle.conditional_experimental_mode:
      self.params.put_bool("ExperimentalMode", True)

    custom_alerts = self.params.get_bool("CustomAlerts")
    toggle.green_light_alert = custom_alerts and self.params.get_bool("GreenLightAlert")
    toggle.lead_departing_alert = custom_alerts and self.params.get_bool("LeadDepartingAlert")
    toggle.loud_blindspot_alert = custom_alerts and self.params.get_bool("LoudBlindspotAlert")

    custom_ui = self.params.get_bool("CustomUI")
    custom_paths = custom_ui and self.params.get_bool("CustomPaths")
    toggle.adjacent_lanes = custom_paths and self.params.get_bool("AdjacentPath")
    toggle.blind_spot_path = custom_paths and self.params.get_bool("BlindSpotPath")
    toggle.show_stopping_point = custom_ui and self.params.get_bool("ShowStoppingPoint")

    toggle.device_management = self.params.get_bool("DeviceManagement")
    device_shutdown_setting = self.params.get_int("DeviceShutdown") if toggle.device_management else 33
    toggle.device_shutdown_time = (device_shutdown_setting - 3) * 3600 if device_shutdown_setting >= 4 else device_shutdown_setting * (60 * 15)
    toggle.increase_thermal_limits = toggle.device_management and self.params.get_bool("IncreaseThermalLimits")
    toggle.low_voltage_shutdown = self.params.get_float("LowVoltageShutdown") if toggle.device_management else VBATT_PAUSE_CHARGING
    toggle.offline_mode = toggle.device_management and self.params.get_bool("OfflineMode")

    driving_personalities = toggle.openpilot_longitudinal and self.params.get_bool("DrivingPersonalities")
    toggle.custom_personalities = driving_personalities and self.params.get_bool("CustomPersonalities")
    aggressive_profile = toggle.custom_personalities and self.params.get_bool("AggressivePersonalityProfile")
    toggle.aggressive_jerk_acceleration = self.params.get_int("AggressiveJerkAcceleration") / 100. if aggressive_profile else 0.5
    toggle.aggressive_jerk_speed = self.params.get_int("AggressiveJerkSpeed") / 100. if aggressive_profile else 0.5
    toggle.aggressive_jerk_danger = self.params.get_int("AggressiveJerkDanger") / 100. if aggressive_profile else 0.5
    toggle.aggressive_follow = self.params.get_float("AggressiveFollow") if aggressive_profile else 1.25
    standard_profile = toggle.custom_personalities and self.params.get_bool("StandardPersonalityProfile")
    toggle.standard_jerk_acceleration = self.params.get_int("StandardJerkAcceleration") / 100. if standard_profile else 1.0
    toggle.standard_jerk_danger = self.params.get_int("StandardJerkDanger") / 100. if standard_profile else 0.5
    toggle.standard_jerk_speed = self.params.get_int("StandardJerkSpeed") / 100. if standard_profile else 1.0
    toggle.standard_follow = self.params.get_float("StandardFollow") if standard_profile else 1.45
    relaxed_profile = toggle.custom_personalities and self.params.get_bool("RelaxedPersonalityProfile")
    toggle.relaxed_jerk_acceleration = self.params.get_int("RelaxedJerkAcceleration") / 100. if relaxed_profile else 1.0
    toggle.relaxed_jerk_danger = self.params.get_int("RelaxedJerkDanger") / 100. if relaxed_profile else 0.5
    toggle.relaxed_jerk_speed = self.params.get_int("RelaxedJerkSpeed") / 100. if relaxed_profile else 1.0
    toggle.relaxed_follow = self.params.get_float("RelaxedFollow") if relaxed_profile else 1.75
    traffic_profile = toggle.custom_personalities and self.params.get_bool("TrafficPersonalityProfile")
    toggle.traffic_mode_jerk_acceleration = [self.params.get_int("TrafficJerkAcceleration") / 100., toggle.aggressive_jerk_acceleration] if traffic_profile else [0.5, 0.5]
    toggle.traffic_mode_jerk_danger = [self.params.get_int("TrafficJerkDanger") / 100., toggle.aggressive_jerk_danger] if traffic_profile else [1.0, 1.0]
    toggle.traffic_mode_jerk_speed = [self.params.get_int("TrafficJerkSpeed") / 100., toggle.aggressive_jerk_speed] if traffic_profile else [0.5, 0.5]
    toggle.traffic_mode_t_follow = [self.params.get_float("TrafficFollow"), toggle.aggressive_follow] if traffic_profile else [0.5, 1.0]
    onroad_distance_button = toggle.custom_personalities and self.params.get_bool("OnroadDistanceButton")
    toggle.distance_icons = self.params.get("CustomDistanceIcons", encoding='utf-8') if onroad_distance_button else "stock"

    toggle.experimental_mode_via_press = toggle.openpilot_longitudinal and self.params.get_bool("ExperimentalModeActivation")
    toggle.experimental_mode_via_distance = toggle.experimental_mode_via_press and self.params.get_bool("ExperimentalModeViaDistance")
    toggle.experimental_mode_via_lkas = not toggle.always_on_lateral_lkas and toggle.experimental_mode_via_press and self.params.get_bool("ExperimentalModeViaLKAS")

    lane_change_customizations = self.params.get_bool("LaneChangeCustomizations")
    toggle.lane_change_delay = self.params.get_int("LaneChangeTime") if lane_change_customizations else 0
    toggle.lane_detection_width = self.params.get_int("LaneDetectionWidth") * distance_conversion / 10. if lane_change_customizations else 0
    toggle.lane_detection = toggle.lane_detection_width != 0
    toggle.minimum_lane_change_speed = self.params.get_int("MinimumLaneChangeSpeed") * speed_conversion if lane_change_customizations else LANE_CHANGE_SPEED_MIN
    toggle.nudgeless = lane_change_customizations and self.params.get_bool("NudgelessLaneChange")
    toggle.one_lane_change = lane_change_customizations and self.params.get_bool("OneLaneChange")

    lateral_tune = self.params.get_bool("LateralTune")
    toggle.force_auto_tune = lateral_tune and self.params.get_bool("ForceAutoTune")
    stock_steer_ratio = self.params.get_float("SteerRatioStock")
    toggle.steer_ratio = self.params.get_float("SteerRatio") if lateral_tune else stock_steer_ratio
    toggle.use_custom_steer_ratio = toggle.steer_ratio != stock_steer_ratio
    toggle.taco_tune = lateral_tune and self.params.get_bool("TacoTune")
    toggle.turn_desires = lateral_tune and self.params.get_bool("TurnDesires")

    toggle.long_pitch = toggle.openpilot_longitudinal and car_make == "gm" and self.params.get_bool("LongPitch")
    toggle.volt_sng = car_model == "CHEVROLET_VOLT" and self.params.get_bool("VoltSNG")

    longitudinal_tune = toggle.openpilot_longitudinal and self.params.get_bool("LongitudinalTune")
    toggle.acceleration_profile = self.params.get_int("AccelerationProfile") if longitudinal_tune else 0
    toggle.deceleration_profile = self.params.get_int("DecelerationProfile") if longitudinal_tune else 0
    toggle.human_acceleration = longitudinal_tune and self.params.get_bool("HumanAcceleration")
    toggle.human_following = longitudinal_tune and self.params.get_bool("HumanFollowing")
    toggle.increased_stopping_distance = self.params.get_int("StoppingDistance") * distance_conversion if longitudinal_tune else 0
    toggle.lead_detection_threshold = self.params.get_int("LeadDetectionThreshold") / 100. if longitudinal_tune else 0.5
    toggle.sport_plus = longitudinal_tune and toggle.acceleration_profile == 3
    toggle.traffic_mode = longitudinal_tune and self.params.get_bool("TrafficMode")

    toggle.map_turn_speed_controller = toggle.openpilot_longitudinal and self.params.get_bool("MTSCEnabled")
    toggle.mtsc_curvature_check = toggle.map_turn_speed_controller and self.params.get_bool("MTSCCurvatureCheck")
    self.params_memory.put_float("MapTargetLatA", 2 * (self.params.get_int("MTSCAggressiveness") / 100.))

    toggle.model_manager = self.params.get_bool("ModelManagement", block=openpilot_installed)
    available_models = self.params.get("AvailableModels", block=toggle.model_manager, encoding='utf-8') or ''
    available_model_names = self.params.get("AvailableModelsNames", block=toggle.model_manager, encoding='utf-8') or ''
    current_model = self.params_memory.get("CurrentModel", encoding='utf-8')
    current_model_name = self.params_memory.get("CurrentModelName", encoding='utf-8')
    if toggle.model_manager and available_models and (current_model is None or not started):
      toggle.model_randomizer = self.params.get_bool("ModelRandomizer")
      if toggle.model_randomizer:
        blacklisted_models = (self.params.get("BlacklistedModels", encoding='utf-8') or '').split(',')
        existing_models = [model for model in available_models.split(',') if model not in blacklisted_models and os.path.exists(os.path.join(MODELS_PATH, f"{model}.thneed"))]
        toggle.model = random.choice(existing_models) if existing_models else DEFAULT_MODEL
      else:
        toggle.model = self.params.get("Model", block=True, encoding='utf-8')
    else:
      toggle.model = current_model
    if toggle.model in available_models.split(',') and os.path.exists(os.path.join(MODELS_PATH, f"{toggle.model}.thneed")):
      current_model_name = available_model_names.split(',')[available_models.split(',').index(toggle.model)]
      toggle.part_model_param = process_model_name(current_model_name)
      try:
        self.params.check_key(toggle.part_model_param + "CalibrationParams")
      except UnknownKeyName:
        toggle.part_model_param = ""
    else:
      toggle.model = DEFAULT_MODEL
      current_model_name = DEFAULT_MODEL_NAME
      toggle.part_model_param = ""
    navigation_models = self.params.get("NavigationModels", encoding='utf-8') or ''
    toggle.navigationless_model = navigation_models and toggle.model not in navigation_models.split(',')
    radarless_models = self.params.get("RadarlessModels", encoding='utf-8') or ''
    toggle.radarless_model = radarless_models and toggle.model in radarless_models.split(',')
    toggle.clairvoyant_model = toggle.model == "clairvoyant-driver"
    toggle.secretgoodopenpilot_model = toggle.model == "secret-good-openpilot"
    if frogpilot_process:
      self.params_memory.put("CurrentModel", toggle.model)
      self.params_memory.put("CurrentModelName", current_model_name)

    quality_of_life_controls = self.params.get_bool("QOLControls")
    toggle.custom_cruise_increase = self.params.get_int("CustomCruise") if quality_of_life_controls and not pcm_cruise else 1
    toggle.custom_cruise_increase_long = self.params.get_int("CustomCruiseLong") if quality_of_life_controls and not pcm_cruise else 5
    toggle.force_standstill = quality_of_life_controls and self.params.get_bool("ForceStandstill")
    toggle.force_stops = toggle.force_standstill and self.params.get_bool("ForceStops")
    map_gears = quality_of_life_controls and self.params.get_bool("MapGears")
    toggle.map_acceleration = map_gears and self.params.get_bool("MapAcceleration")
    toggle.map_deceleration = map_gears and self.params.get_bool("MapDeceleration")
    toggle.pause_lateral_below_speed = self.params.get_int("PauseLateralSpeed") * speed_conversion if quality_of_life_controls else 0
    toggle.pause_lateral_below_signal = toggle.pause_lateral_below_speed != 0 and self.params.get_bool("PauseLateralOnSignal")
    toggle.reverse_cruise_increase = quality_of_life_controls and pcm_cruise and self.params.get_bool("ReverseCruise")
    toggle.set_speed_offset = self.params.get_int("SetSpeedOffset") * (1. if toggle.is_metric else CV.MPH_TO_KPH) if quality_of_life_controls and not pcm_cruise else 0

    toggle.sng_hack = toggle.openpilot_longitudinal and car_make == "toyota" and self.params.get_bool("SNGHack")

    toggle.speed_limit_controller = toggle.openpilot_longitudinal and self.params.get_bool("SpeedLimitController")
    toggle.force_mph_dashboard = toggle.speed_limit_controller and self.params.get_bool("ForceMPHDashboard")
    toggle.map_speed_lookahead_higher = self.params.get_int("SLCLookaheadHigher") if toggle.speed_limit_controller else 0
    toggle.map_speed_lookahead_lower = self.params.get_int("SLCLookaheadLower") if toggle.speed_limit_controller else 0
    toggle.offset1 = self.params.get_int("Offset1") * speed_conversion if toggle.speed_limit_controller else 0
    toggle.offset2 = self.params.get_int("Offset2") * speed_conversion if toggle.speed_limit_controller else 0
    toggle.offset3 = self.params.get_int("Offset3") * speed_conversion if toggle.speed_limit_controller else 0
    toggle.offset4 = self.params.get_int("Offset4") * speed_conversion if toggle.speed_limit_controller else 0
    toggle.set_speed_limit = toggle.speed_limit_controller and self.params.get_bool("SetSpeedLimit")
    toggle.speed_limit_alert = toggle.speed_limit_controller and self.params.get_bool("SpeedLimitChangedAlert")
    toggle.speed_limit_confirmation = toggle.speed_limit_controller and self.params.get_bool("SLCConfirmation")
    toggle.speed_limit_confirmation_higher = toggle.speed_limit_confirmation and self.params.get_bool("SLCConfirmationHigher")
    toggle.speed_limit_confirmation_lower = toggle.speed_limit_confirmation and self.params.get_bool("SLCConfirmationLower")
    speed_limit_controller_override = self.params.get_int("SLCOverride") if toggle.speed_limit_controller else 0
    toggle.speed_limit_controller_override_manual = speed_limit_controller_override == 1
    toggle.speed_limit_controller_override_set_speed = speed_limit_controller_override == 2
    toggle.use_set_speed = toggle.speed_limit_controller and self.params.get_int("SLCFallback") == 0
    toggle.use_experimental_mode = toggle.speed_limit_controller and self.params.get_int("SLCFallback") == 1
    toggle.use_previous_limit = toggle.speed_limit_controller and self.params.get_int("SLCFallback") == 2
    toggle.speed_limit_priority1 = self.params.get("SLCPriority1", encoding='utf-8') if toggle.speed_limit_controller else None
    toggle.speed_limit_priority2 = self.params.get("SLCPriority2", encoding='utf-8') if toggle.speed_limit_controller else None
    toggle.speed_limit_priority3 = self.params.get("SLCPriority3", encoding='utf-8') if toggle.speed_limit_controller else None
    toggle.speed_limit_priority_highest = toggle.speed_limit_priority1 == "Highest"
    toggle.speed_limit_priority_lowest = toggle.speed_limit_priority1 == "Lowest"

    toyota_doors = car_make == "toyota" and self.params.get_bool("ToyotaDoors")
    toggle.lock_doors = toyota_doors and self.params.get_bool("LockDoors")
    toggle.unlock_doors = toyota_doors and self.params.get_bool("UnlockDoors")

    toggle.vision_turn_controller = toggle.openpilot_longitudinal and self.params.get_bool("VisionTurnControl")
    toggle.curve_sensitivity = self.params.get_int("CurveSensitivity") / 100. if toggle.vision_turn_controller else 1
    toggle.turn_aggressiveness = self.params.get_int("TurnAggressiveness") / 100. if toggle.vision_turn_controller else 1

FrogPilotVariables = FrogPilotVariables()
