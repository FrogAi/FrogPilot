from types import SimpleNamespace

from cereal import car
from openpilot.common.conversions import Conversions as CV
from openpilot.common.params import Params

from openpilot.selfdrive.frogpilot.controls.lib.model_manager import RADARLESS_MODELS

CITY_SPEED_LIMIT = 25  # 55mph is typically the minimum speed for highways
CRUISING_SPEED = 5     # Roughly the speed cars go when not touching the gas while in drive
PROBABILITY = 0.6      # 60% chance of condition being true
THRESHOLD = 5          # Time threshold (0.25s)


class FrogPilotVariables:
  def __init__(self):
    self.frogpilot_toggles = SimpleNamespace()

    self.params = Params()
    self.params_memory = Params("/dev/shm/params")

    self.update_frogpilot_params(False)

  @property
  def toggles(self):
    return self.frogpilot_toggles

  @property
  def toggles_updated(self):
    return self.params_memory.get_bool("FrogPilotTogglesUpdated")

  def update_frogpilot_params(self, started=True):
    openpilot_installed = self.params.get_bool("HasAcceptedTerms")

    toggles = self.frogpilot_toggles

    key = "CarParams" if started else "CarParamsPersistent"
    msg_bytes = self.params.get(key, block=started and openpilot_installed)

    if msg_bytes is None:
      car_name = "mock"
      openpilot_longitudinal = False
      pcm_cruise = False
    else:
      with car.CarParams.from_bytes(msg_bytes) as msg:
        CP = msg
        car_name = CP.carName
        openpilot_longitudinal = CP.openpilotLongitudinalControl
        pcm_cruise = CP.pcmCruise

    toggles.is_metric = self.params.get_bool("IsMetric")
    distance_conversion = 1 if toggles.is_metric else CV.FOOT_TO_METER
    speed_conversion = CV.KPH_TO_MS if toggles.is_metric else CV.MPH_TO_MS

    if not started:
      toggles.radarless_model = self.params.get("Model", block=openpilot_installed, encoding='utf-8') in RADARLESS_MODELS

    toggles.alert_volume_control = self.params.get_bool("AlertVolumeControl")
    toggles.disengage_volume = self.params.get_int("DisengageVolume") if toggles.alert_volume_control else 100
    toggles.engage_volume = self.params.get_int("EngageVolume") if toggles.alert_volume_control else 100
    toggles.prompt_volume = self.params.get_int("PromptVolume") if toggles.alert_volume_control else 100
    toggles.promptDistracted_volume = self.params.get_int("PromptDistractedVolume") if toggles.alert_volume_control else 100
    toggles.refuse_volume = self.params.get_int("RefuseVolume") if toggles.alert_volume_control else 100
    toggles.warningSoft_volume = self.params.get_int("WarningSoftVolume") if toggles.alert_volume_control else 100
    toggles.warningImmediate_volume = self.params.get_int("WarningImmediateVolume") if toggles.alert_volume_control else 100

    always_on_lateral = self.params.get_bool("AlwaysOnLateral")
    toggles.always_on_lateral_main = always_on_lateral and self.params.get_bool("AlwaysOnLateralMain")
    toggles.always_on_lateral_pause_speed = self.params.get_int("PauseAOLOnBrake") if always_on_lateral else 0

    toggles.automatic_updates = self.params.get_bool("AutomaticUpdates")

    toggles.cluster_offset = self.params.get_float("ClusterOffset") if car_name == "toyota" else 1

    toggles.conditional_experimental_mode = openpilot_longitudinal and self.params.get_bool("ConditionalExperimental")
    toggles.conditional_curves = toggles.conditional_experimental_mode and self.params.get_bool("CECurves")
    toggles.conditional_curves_lead = toggles.conditional_curves and self.params.get_bool("CECurvesLead")
    toggles.conditional_limit = self.params.get_int("CESpeed") * speed_conversion if toggles.conditional_experimental_mode else 0
    toggles.conditional_limit_lead = self.params.get_int("CESpeedLead") * speed_conversion if toggles.conditional_experimental_mode else 0
    toggles.conditional_navigation = toggles.conditional_experimental_mode and self.params.get_bool("CENavigation")
    toggles.conditional_navigation_intersections = toggles.conditional_navigation and self.params.get_bool("CENavigationIntersections")
    toggles.conditional_navigation_lead = toggles.conditional_navigation and self.params.get_bool("CENavigationLead")
    toggles.conditional_navigation_turns = toggles.conditional_navigation and self.params.get_bool("CENavigationTurns")
    toggles.conditional_signal = toggles.conditional_experimental_mode and self.params.get_bool("CESignal")
    toggles.conditional_slower_lead = toggles.conditional_experimental_mode and self.params.get_bool("CESlowerLead")
    toggles.conditional_stop_lights = toggles.conditional_experimental_mode and self.params.get_bool("CEStopLights")
    toggles.conditional_stop_lights_lead = toggles.conditional_stop_lights and self.params.get_bool("CEStopLightsLead")

    custom_alerts = self.params.get_bool("CustomAlerts")
    toggles.green_light_alert = custom_alerts and self.params.get_bool("GreenLightAlert")
    toggles.lead_departing_alert = custom_alerts and self.params.get_bool("LeadDepartingAlert")
    toggles.loud_blindspot_alert = custom_alerts and self.params.get_bool("LoudBlindspotAlert")

    custom_themes = self.params.get_bool("CustomTheme")
    holiday_themes = custom_themes and self.params.get_bool("HolidayThemes")
    toggles.current_holiday_theme = self.params_memory.get_int("CurrentHolidayTheme") if holiday_themes else 0
    toggles.custom_sounds = self.params.get_int("CustomSounds") if custom_themes else 0
    toggles.goat_scream = toggles.current_holiday_theme == 0 and toggles.custom_sounds == 1 and self.params.get_bool("GoatScream")
    toggles.random_events = custom_themes and self.params.get_bool("RandomEvents")

    custom_ui = self.params.get_bool("CustomUI")
    toggles.adjacent_lanes = custom_ui and self.params.get_bool("AdjacentPath")
    toggles.blind_spot_path = custom_ui and self.params.get_bool("BlindSpotPath")

    toggles.device_management = self.params.get_bool("DeviceManagement")
    device_shutdown_setting = self.params.get_int("DeviceShutdown") if toggles.device_management else 33
    toggles.device_shutdown_time = (device_shutdown_setting - 3) * 3600 if device_shutdown_setting >= 4 else device_shutdown_setting * (60 * 15)
    toggles.increase_thermal_limits = toggles.device_management and self.params.get_bool("IncreaseThermalLimits")
    toggles.low_voltage_shutdown = self.params.get_float("LowVoltageShutdown") if toggles.device_management else 11.8
    toggles.offline_mode = toggles.device_management and self.params.get_bool("OfflineMode")

    driving_personalities = openpilot_longitudinal and self.params.get_bool("DrivingPersonalities")
    toggles.custom_personalities = driving_personalities and self.params.get_bool("CustomPersonalities")
    aggressive_profile = toggles.custom_personalities and self.params.get_bool("AggressivePersonalityProfile")
    toggles.aggressive_jerk_acceleration = self.params.get_int("AggressiveJerkAcceleration") / 100 if aggressive_profile else 0.5
    toggles.aggressive_jerk_speed = self.params.get_int("AggressiveJerkSpeed") / 100 if aggressive_profile else 0.5
    toggles.aggressive_follow = self.params.get_float("AggressiveFollow") if aggressive_profile else 1.25
    standard_profile = toggles.custom_personalities and self.params.get_bool("StandardPersonalityProfile")
    toggles.standard_jerk_acceleration = self.params.get_int("StandardJerkAcceleration") / 100 if standard_profile else 1.0
    toggles.standard_jerk_speed = self.params.get_int("StandardJerkSpeed") / 100 if standard_profile else 1.0
    toggles.standard_follow = self.params.get_float("StandardFollow") if standard_profile else 1.45
    relaxed_profile = toggles.custom_personalities and self.params.get_bool("RelaxedPersonalityProfile")
    toggles.relaxed_jerk_acceleration = self.params.get_int("RelaxedJerkAcceleration") / 100 if relaxed_profile else 1.0
    toggles.relaxed_jerk_speed = self.params.get_int("RelaxedJerkSpeed") / 100 if relaxed_profile else 1.0
    toggles.relaxed_follow = self.params.get_float("RelaxedFollow") if relaxed_profile else 1.75
    traffic_profile = toggles.custom_personalities and self.params.get_bool("TrafficPersonalityProfile")
    toggles.traffic_mode_jerk_acceleration = [self.params.get_int("TrafficJerkAcceleration") / 100.0, toggles.aggressive_jerk_acceleration] if traffic_profile else [0.5, 0.5]
    toggles.traffic_mode_jerk_speed = [self.params.get_int("TrafficJerkSpeed") / 100.0, toggles.aggressive_jerk_speed] if traffic_profile else [0.5, 0.5]
    toggles.traffic_mode_t_follow = [self.params.get_float("TrafficFollow"), toggles.aggressive_follow] if traffic_profile else [0.5, 1.0]

    toggles.experimental_mode_via_press = openpilot_longitudinal and self.params.get_bool("ExperimentalModeActivation")
    toggles.experimental_mode_via_distance = toggles.experimental_mode_via_press and self.params.get_bool("ExperimentalModeViaDistance")
    toggles.experimental_mode_via_lkas = toggles.experimental_mode_via_press and self.params.get_bool("ExperimentalModeViaLKAS")

    lane_change_customizations = self.params.get_bool("LaneChangeCustomizations")
    toggles.lane_change_delay = self.params.get_int("LaneChangeTime") if lane_change_customizations else 0
    toggles.lane_detection_width = self.params.get_int("LaneDetectionWidth") * distance_conversion / 10 if lane_change_customizations else 0
    toggles.lane_detection = toggles.lane_detection_width != 0
    toggles.minimum_lane_change_speed = self.params.get_int("MinimumLaneChangeSpeed") * speed_conversion if lane_change_customizations else 20 * CV.MPH_TO_MS
    toggles.nudgeless = lane_change_customizations and self.params.get_bool("NudgelessLaneChange")
    toggles.one_lane_change = lane_change_customizations and self.params.get_bool("OneLaneChange")

    lateral_tune = self.params.get_bool("LateralTune")
    toggles.force_auto_tune = lateral_tune and self.params.get_bool("ForceAutoTune")
    stock_steer_ratio = self.params.get_float("SteerRatioStock")
    toggles.steer_ratio = self.params.get_float("SteerRatio") if lateral_tune else stock_steer_ratio
    toggles.use_custom_steer_ratio = toggles.steer_ratio != stock_steer_ratio
    toggles.taco_tune = lateral_tune and self.params.get_bool("TacoTune")
    toggles.turn_desires = lateral_tune and self.params.get_bool("TurnDesires")

    toggles.long_pitch = openpilot_longitudinal and car_name == "gm" and self.params.get_bool("LongPitch")

    longitudinal_tune = openpilot_longitudinal and self.params.get_bool("LongitudinalTune")
    toggles.acceleration_profile = self.params.get_int("AccelerationProfile") if longitudinal_tune else 0
    toggles.aggressive_acceleration = longitudinal_tune and self.params.get_bool("AggressiveAcceleration")
    toggles.aggressive_acceleration_experimental = toggles.aggressive_acceleration and self.params.get_bool("AggressiveAccelerationExperimental")
    toggles.deceleration_profile = self.params.get_int("DecelerationProfile") if longitudinal_tune else 0
    toggles.increased_stopping_distance = self.params.get_int("StoppingDistance") * distance_conversion if longitudinal_tune else 0
    toggles.lead_detection_threshold = self.params.get_int("LeadDetectionThreshold") / 100 if longitudinal_tune else 0.5
    toggles.smoother_braking = longitudinal_tune and self.params.get_bool("SmoothBraking")
    toggles.smoother_braking_far_lead = toggles.smoother_braking and self.params.get_bool("SmoothBrakingFarLead")
    toggles.smoother_braking_jerk = toggles.smoother_braking and self.params.get_bool("SmoothBrakingJerk")
    toggles.sport_plus = longitudinal_tune and toggles.acceleration_profile == 3
    toggles.traffic_mode = longitudinal_tune and self.params.get_bool("TrafficMode")

    toggles.model_selector = self.params.get_bool("ModelSelector")

    quality_of_life = self.params.get_bool("QOLControls")
    toggles.custom_cruise_increase = self.params.get_int("CustomCruise") if quality_of_life and not pcm_cruise else 1
    toggles.custom_cruise_increase_long = self.params.get_int("CustomCruiseLong") if quality_of_life and not pcm_cruise else 5
    map_gears = quality_of_life and self.params.get_bool("MapGears")
    toggles.map_acceleration = map_gears and self.params.get_bool("MapAcceleration")
    toggles.map_deceleration = map_gears and self.params.get_bool("MapDeceleration")
    toggles.pause_lateral_below_speed = self.params.get_int("PauseLateralSpeed") * speed_conversion if quality_of_life else 0
    toggles.pause_lateral_below_signal = quality_of_life and self.params.get_bool("PauseLateralOnSignal")
    toggles.reverse_cruise_increase = quality_of_life and pcm_cruise and self.params.get_bool("ReverseCruise")
    toggles.set_speed_offset = self.params.get_int("SetSpeedOffset") * (1 if toggles.is_metric else CV.MPH_TO_KPH) if quality_of_life and not pcm_cruise else 0

    toggles.map_turn_speed_controller = openpilot_longitudinal and self.params.get_bool("MTSCEnabled")
    toggles.mtsc_curvature_check = toggles.map_turn_speed_controller and self.params.get_bool("MTSCCurvatureCheck")
    self.params_memory.put_float("MapTargetLatA", 2 * (self.params.get_int("MTSCAggressiveness") / 100))

    toggles.sng_hack = openpilot_longitudinal and car_name == "toyota" and self.params.get_bool("SNGHack")

    toggles.speed_limit_controller = openpilot_longitudinal and self.params.get_bool("SpeedLimitController")
    toggles.force_mph_dashboard = toggles.speed_limit_controller and self.params.get_bool("ForceMPHDashboard")
    toggles.map_speed_lookahead_higher = self.params.get_int("SLCLookaheadHigher") if toggles.speed_limit_controller else 0
    toggles.map_speed_lookahead_lower = self.params.get_int("SLCLookaheadLower") if toggles.speed_limit_controller else 0
    toggles.offset1 = self.params.get_int("Offset1") * speed_conversion if toggles.speed_limit_controller else 0
    toggles.offset2 = self.params.get_int("Offset2") * speed_conversion if toggles.speed_limit_controller else 0
    toggles.offset3 = self.params.get_int("Offset3") * speed_conversion if toggles.speed_limit_controller else 0
    toggles.offset4 = self.params.get_int("Offset4") * speed_conversion if toggles.speed_limit_controller else 0
    toggles.set_speed_limit = toggles.speed_limit_controller and self.params.get_bool("SetSpeedLimit")
    toggles.speed_limit_alert = toggles.speed_limit_controller and self.params.get_bool("SpeedLimitChangedAlert")
    toggles.speed_limit_confirmation = toggles.speed_limit_controller and self.params.get_bool("SLCConfirmation")
    toggles.speed_limit_confirmation_lower = toggles.speed_limit_confirmation and self.params.get_bool("SLCConfirmationLower")
    toggles.speed_limit_confirmation_higher = toggles.speed_limit_confirmation and self.params.get_bool("SLCConfirmationHigher")
    toggles.speed_limit_controller_override = self.params.get_int("SLCOverride") if toggles.speed_limit_controller else 0
    toggles.use_experimental_mode = toggles.speed_limit_controller and self.params.get_int("SLCFallback") == 1
    toggles.use_previous_limit = toggles.speed_limit_controller and self.params.get_int("SLCFallback") == 2
    toggles.speed_limit_priority1 = self.params.get("SLCPriority1", encoding='utf-8') if toggles.speed_limit_controller else None
    toggles.speed_limit_priority2 = self.params.get("SLCPriority2", encoding='utf-8') if toggles.speed_limit_controller else None
    toggles.speed_limit_priority3 = self.params.get("SLCPriority3", encoding='utf-8') if toggles.speed_limit_controller else None
    toggles.speed_limit_priority_highest = toggles.speed_limit_priority1 == "Highest"
    toggles.speed_limit_priority_lowest = toggles.speed_limit_priority1 == "Lowest"

    toyota_doors = car_name == "toyota" and self.params.get_bool("ToyotaDoors")
    toggles.lock_doors = toyota_doors and self.params.get_bool("LockDoors")
    toggles.unlock_doors = toyota_doors and self.params.get_bool("UnlockDoors")

    toggles.vision_turn_controller = openpilot_longitudinal and self.params.get_bool("VisionTurnControl")
    toggles.curve_sensitivity = self.params.get_int("CurveSensitivity") / 100 if toggles.vision_turn_controller else 1
    toggles.turn_aggressiveness = self.params.get_int("TurnAggressiveness") / 100 if toggles.vision_turn_controller else 1

FrogPilotVariables = FrogPilotVariables()
