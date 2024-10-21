import json
import math
import os
import random
import time

from types import SimpleNamespace

from cereal import car
from openpilot.common.basedir import BASEDIR
from openpilot.common.conversions import Conversions as CV
from openpilot.common.numpy_fast import clip, interp
from openpilot.common.params import Params, UnknownKeyName
from openpilot.selfdrive.controls.lib.desire_helper import LANE_CHANGE_SPEED_MIN
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.system.hardware.power_monitoring import VBATT_PAUSE_CHARGING
from openpilot.system.version import get_build_metadata
from panda import ALTERNATIVE_EXPERIENCE

params = Params()
params_memory = Params("/dev/shm/params")

GearShifter = car.CarState.GearShifter
NON_DRIVING_GEARS = [GearShifter.neutral, GearShifter.park, GearShifter.reverse, GearShifter.unknown]

CITY_SPEED_LIMIT = 25                                   # 55mph is typically the minimum speed for highways
CRUISING_SPEED = 5                                      # Roughly the speed cars go when not touching the gas while in drive
MODEL_LENGTH = ModelConstants.IDX_N                     # Minimum length of the model
PLANNER_TIME = ModelConstants.T_IDXS[MODEL_LENGTH - 1]  # Length of time the model projects out for
THRESHOLD = 0.6                                         # 60% chance of condition being true
TO_RADIANS = math.pi / 180                              # Conversion factor from degrees to radians

ACTIVE_THEME_PATH = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "active_theme")
MODELS_PATH = os.path.join("/data", "models")
RANDOM_EVENTS_PATH = os.path.join(BASEDIR, "selfdrive", "frogpilot", "assets", "random_events")
THEME_SAVE_PATH = os.path.join("/data", "themes")

DEFAULT_MODEL = "dragon-rider"
DEFAULT_MODEL_NAME = "Dragon Rider"

DEFAULT_CLASSIC_MODEL = "north-dakota"
DEFAULT_CLASSIC_MODEL_NAME = "North Dakota (Default)"

def get_frogpilot_toggles():
  while True:
    toggles = params.get("FrogPilotToggles")
    if toggles is not None:
      try:
        return SimpleNamespace(**json.loads(toggles))
      except Exception as e:
        print(f"Unexpected error while retrieving toggles: {e}, value: {toggles}")
        time.sleep(0.1)
    time.sleep(0.1)

def has_prime():
  return params.get_int("PrimeType") > 0

def update_frogpilot_toggles():
  params_memory.put_bool("FrogPilotTogglesUpdated", True)

frogpilot_default_params: list[tuple[str, bool | bytes | int | float | str]] = [
  ("AccelerationPath", 1),
  ("AccelerationProfile", 2),
  ("AdjacentLeadsUI", 1),
  ("AdjacentPath", 0),
  ("AdjacentPathMetrics", 0),
  ("AdvancedCustomUI", 0),
  ("AdvancedLateralTune", 0),
  ("AggressiveFollow", 1.25),
  ("AggressiveJerkAcceleration", 50),
  ("AggressiveJerkDanger", 100),
  ("AggressiveJerkDeceleration", 50),
  ("AggressiveJerkSpeed", 50),
  ("AggressiveJerkSpeedDecrease", 50),
  ("AggressivePersonalityProfile", 1),
  ("AlertVolumeControl", 0),
  ("AlwaysOnLateral", 1),
  ("AlwaysOnLateralLKAS", 0),
  ("AlwaysOnLateralMain", 1),
  ("AMapKey1", ""),
  ("AMapKey2", ""),
  ("AutomaticallyUpdateModels", 1),
  ("AutomaticUpdates", 1),
  ("AvailableModels", ""),
  ("AvailableModelsNames", ""),
  ("BigMap", 0),
  ("BlacklistedModels", ""),
  ("BlindSpotMetrics", 1),
  ("BlindSpotPath", 1),
  ("BorderMetrics", 1),
  ("CameraView", 3),
  ("CarMake", ""),
  ("CarModel", ""),
  ("CarModelName", ""),
  ("CECurves", 1),
  ("CECurvesLead", 0),
  ("CELead", 0),
  ("CEModelStopTime", 8),
  ("CENavigation", 1),
  ("CENavigationIntersections", 0),
  ("CENavigationLead", 0),
  ("CENavigationTurns", 1),
  ("CertifiedHerbalistDrives", 0),
  ("CertifiedHerbalistScore", 0),
  ("CESignalSpeed", 55),
  ("CESignalLaneDetection", 1),
  ("CESlowerLead", 0),
  ("CESpeed", 0),
  ("CESpeedLead", 0),
  ("CEStoppedLead", 0),
  ("ClassicModels", ""),
  ("ClusterOffset", 1.015),
  ("Compass", 0),
  ("ConditionalExperimental", 1),
  ("CrosstrekTorque", 1),
  ("CurveSensitivity", 100),
  ("CurveSpeedControl", 0),
  ("CustomAlerts", 1),
  ("CustomColors", "frog"),
  ("CustomCruise", 1),
  ("CustomCruiseLong", 5),
  ("CustomDistanceIcons", "stock"),
  ("CustomIcons", "frog-animated"),
  ("CustomizationLevel", 0),
  ("CustomizationLevelConfirmed", 0),
  ("CustomPersonalities", 0),
  ("CustomSignals", "frog"),
  ("CustomSounds", "frog"),
  ("CustomUI", 1),
  ("DecelerationProfile", 1),
  ("DefaultModelName", DEFAULT_CLASSIC_MODEL_NAME),
  ("DeveloperUI", 0),
  ("DeviceManagement", 1),
  ("DeviceShutdown", 9),
  ("DisableCurveSpeedSmoothing", 0),
  ("DisableOnroadUploads", 0),
  ("DisableOpenpilotLongitudinal", 0),
  ("DisengageVolume", 101),
  ("DissolvedOxygenDrives", 0),
  ("DissolvedOxygenScore", 0),
  ("DriverCamera", 0),
  ("DuckAmigoDrives", 0),
  ("DuckAmigoScore", 0),
  ("DynamicPathWidth", 0),
  ("DynamicPedalsOnUI", 1),
  ("EngageVolume", 101),
  ("ExperimentalGMTune", 0),
  ("ExperimentalModeActivation", 1),
  ("ExperimentalModels", ""),
  ("ExperimentalModeViaDistance", 1),
  ("ExperimentalModeViaLKAS", 1),
  ("ExperimentalModeViaTap", 0),
  ("Fahrenheit", 0),
  ("ForceAutoTune", 0),
  ("ForceAutoTuneOff", 0),
  ("ForceFingerprint", 0),
  ("ForceMPHDashboard", 0),
  ("ForceStandstill", 0),
  ("ForceStops", 0),
  ("FPSCounter", 1),
  ("FrogPilotToggles", ""),
  ("FrogsGoMoosTweak", 1),
  ("FullMap", 0),
  ("GasRegenCmd", 1),
  ("GMapKey", ""),
  ("GoatScream", 0),
  ("GreenLightAlert", 0),
  ("HideAlerts", 0),
  ("HideAOLStatusBar", 0),
  ("HideCEMStatusBar", 0),
  ("HideLeadMarker", 0),
  ("HideMapIcon", 0),
  ("HideMaxSpeed", 0),
  ("HideSpeed", 0),
  ("HideSpeedLimit", 0),
  ("HolidayThemes", 1),
  ("HumanAcceleration", 1),
  ("HumanFollowing", 1),
  ("IncreasedStoppedDistance", 3),
  ("IncreaseThermalLimits", 0),
  ("JerkInfo", 1),
  ("LaneChangeCustomizations", 0),
  ("LaneChangeTime", 1.0),
  ("LaneDetectionWidth", 9),
  ("LaneLinesWidth", 4),
  ("LateralMetrics", 1),
  ("LateralTune", 1),
  ("LeadDepartingAlert", 0),
  ("LeadDetectionThreshold", 35),
  ("LeadInfo", 1),
  ("LockDoors", 1),
  ("LongitudinalMetrics", 1),
  ("LongitudinalTune", 1),
  ("LongPitch", 1),
  ("LosAngelesDrives", 0),
  ("LosAngelesScore", 0),
  ("LoudBlindspotAlert", 0),
  ("LowVoltageShutdown", str(VBATT_PAUSE_CHARGING)),
  ("MapAcceleration", 0),
  ("MapDeceleration", 0),
  ("MapGears", 0),
  ("MapboxPublicKey", ""),
  ("MapboxSecretKey", ""),
  ("MapsSelected", ""),
  ("MapStyle", 0),
  ("MaxDesiredAcceleration", 4.0),
  ("MinimumLaneChangeSpeed", str(LANE_CHANGE_SPEED_MIN / CV.MPH_TO_MS)),
  ("Model", DEFAULT_CLASSIC_MODEL),
  ("ModelName", DEFAULT_CLASSIC_MODEL_NAME),
  ("ModelRandomizer", 0),
  ("ModelUI", 1),
  ("MTSCCurvatureCheck", 1),
  ("MTSCEnabled", 1),
  ("NavigationModels", ""),
  ("NavigationUI", 1),
  ("NewLongAPI", 0),
  ("NewLongAPIGM", 1),
  ("NNFF", 1),
  ("NNFFLite", 1),
  ("NoLogging", 0),
  ("NorthDakotaDrives", 0),
  ("NorthDakotaScore", 0),
  ("NotreDameDrives", 0),
  ("NotreDameScore", 0),
  ("NoUploads", 0),
  ("NudgelessLaneChange", 0),
  ("NumericalTemp", 1),
  ("OfflineMode", 1),
  ("Offset1", 5),
  ("Offset2", 5),
  ("Offset3", 5),
  ("Offset4", 10),
  ("OneLaneChange", 1),
  ("OnroadDistanceButton", 0),
  ("openpilotMinutes", 0),
  ("PathEdgeWidth", 20),
  ("PathWidth", 6.1),
  ("PauseAOLOnBrake", 0),
  ("PauseLateralOnSignal", 0),
  ("PauseLateralSpeed", 0),
  ("PedalsOnUI", 0),
  ("PersonalizeOpenpilot", 1),
  ("PreferredSchedule", 2),
  ("PromptDistractedVolume", 101),
  ("PromptVolume", 101),
  ("QOLLateral", 1),
  ("QOLLongitudinal", 1),
  ("QOLVisuals", 1),
  ("RadarlessModels", ""),
  ("RadicalTurtleDrives", 0),
  ("RadicalTurtleScore", 0),
  ("RandomEvents", 0),
  ("RecertifiedHerbalistDrives", 0),
  ("RecertifiedHerbalistScore", 0),
  ("RefuseVolume", 101),
  ("RelaxedFollow", 1.75),
  ("RelaxedJerkAcceleration", 100),
  ("RelaxedJerkDanger", 100),
  ("RelaxedJerkDeceleration", 100),
  ("RelaxedJerkSpeed", 100),
  ("RelaxedJerkSpeedDecrease", 100),
  ("RelaxedPersonalityProfile", 1),
  ("ResetFrogTheme", 0),
  ("ReverseCruise", 0),
  ("RoadEdgesWidth", 2),
  ("RoadNameUI", 1),
  ("RotatingWheel", 1),
  ("ScreenBrightness", 101),
  ("ScreenBrightnessOnroad", 101),
  ("ScreenManagement", 1),
  ("ScreenRecorder", 1),
  ("ScreenTimeout", 30),
  ("ScreenTimeoutOnroad", 30),
  ("SearchInput", 0),
  ("SecretGoodOpenpilotDrives", 0),
  ("SecretGoodOpenpilotScore", 0),
  ("SetSpeedLimit", 0),
  ("SetSpeedOffset", 0),
  ("ShowCPU", 1),
  ("ShowGPU", 0),
  ("ShowIP", 0),
  ("ShowMemoryUsage", 1),
  ("ShowSLCOffset", 1),
  ("ShowSteering", 1),
  ("ShowStoppingPoint", 1),
  ("ShowStoppingPointMetrics", 0),
  ("ShowStorageLeft", 0),
  ("ShowStorageUsed", 0),
  ("Sidebar", 0),
  ("SidebarMetrics", 1),
  ("SignalMetrics", 0),
  ("SLCConfirmation", 0),
  ("SLCConfirmationHigher", 0),
  ("SLCConfirmationLower", 0),
  ("SLCFallback", 2),
  ("SLCLookaheadHigher", 5),
  ("SLCLookaheadLower", 5),
  ("SLCOverride", 1),
  ("SLCPriority1", "Dashboard"),
  ("SLCPriority2", "Navigation"),
  ("SLCPriority3", "Map Data"),
  ("SNGHack", 1),
  ("SpeedLimitChangedAlert", 1),
  ("SpeedLimitController", 1),
  ("StartupMessageBottom", "so I do what I want 🐸"),
  ("StartupMessageTop", "Hippity hoppity this is my property"),
  ("StandardFollow", 1.45),
  ("StandardJerkAcceleration", 100),
  ("StandardJerkDanger", 100),
  ("StandardJerkDeceleration", 100),
  ("StandardJerkSpeed", 100),
  ("StandardJerkSpeedDecrease", 100),
  ("StandardPersonalityProfile", 1),
  ("StandbyMode", 0),
  ("StaticPedalsOnUI", 0),
  ("SteerFriction", 0.1),
  ("SteerFrictionStock", 0.1),
  ("SteerLatAccel", 2.5),
  ("SteerLatAccelStock", 2.5),
  ("SteerKP", 1),
  ("SteerKPStock", 1),
  ("SteerRatio", 15),
  ("SteerRatioStock", 15),
  ("StoppedTimer", 0),
  ("TacoTune", 0),
  ("ToyotaDoors", 1),
  ("TrafficFollow", 0.5),
  ("TrafficJerkAcceleration", 50),
  ("TrafficJerkDanger", 100),
  ("TrafficJerkDeceleration", 50),
  ("TrafficJerkSpeed", 50),
  ("TrafficJerkSpeedDecrease", 50),
  ("TrafficPersonalityProfile", 1),
  ("TuningInfo", 1),
  ("TurnAggressiveness", 100),
  ("TurnDesires", 0),
  ("UnlimitedLength", 1),
  ("UnlockDoors", 1),
  ("UseSI", 1),
  ("UseVienna", 0),
  ("VisionTurnControl", 1),
  ("VoltSNG", 0),
  ("WarningImmediateVolume", 101),
  ("WarningSoftVolume", 101),
  ("WD40Drives", 0),
  ("WD40Score", 0),
  ("WheelIcon", "frog"),
  ("WheelSpeed", 0)
]

class FrogPilotVariables:
  def __init__(self):
    self.default_frogpilot_toggles = SimpleNamespace(**dict(frogpilot_default_params))
    self.frogpilot_toggles = SimpleNamespace()

    self.development_branch = get_build_metadata().channel == "FrogPilot-Development"

    self.frogpilot_toggles.frogs_go_moo = os.path.isfile("/persist/frogsgomoo.py")
    self.frogpilot_toggles.block_user = self.development_branch and not self.frogpilot_toggles.frogs_go_moo

  def update(self, started):
    openpilot_installed = params.get_bool("HasAcceptedTerms")

    key = "CarParams" if started else "CarParamsPersistent"
    msg_bytes = params.get(key, block=openpilot_installed and started)

    if msg_bytes:
      with car.CarParams.from_bytes(msg_bytes) as CP:
        always_on_lateral_set = key == "CarParams" and CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL
        car_make = CP.carName
        car_model = CP.carFingerprint
        has_auto_tune = (car_model == "hyundai" or car_model == "toyota") and CP.lateralTuning.which == "torque"
        has_bsm = CP.enableBsm
        has_radar = not CP.radarUnavailable
        is_pid_car = CP.lateralTuning.which == "pid"
        max_acceleration_enabled = key == "CarParams" and CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.RAISE_LONGITUDINAL_LIMITS_TO_ISO_MAX
        openpilot_longitudinal = CP.openpilotLongitudinalControl
        pcm_cruise = CP.pcmCruise
    else:
      always_on_lateral_set = False
      car_make = "mock"
      car_model = "mock"
      has_auto_tune = False
      has_bsm = False
      has_radar = False
      is_pid_car = False
      max_acceleration_enabled = False
      openpilot_longitudinal = False
      pcm_cruise = False

    toggle = self.frogpilot_toggles

    toggle.is_metric = params.get_bool("IsMetric")
    distance_conversion = 1 if toggle.is_metric else CV.FOOT_TO_METER
    small_distance_conversion = 1 if toggle.is_metric else CV.INCH_TO_CM
    speed_conversion = CV.KPH_TO_MS if toggle.is_metric else CV.MPH_TO_MS

    toggle.advanced_custom_onroad_ui = params.get_bool("AdvancedCustomUI")
    toggle.hide_alerts = toggle.advanced_custom_onroad_ui and params.get_bool("HideAlerts")
    toggle.hide_lead_marker = toggle.advanced_custom_onroad_ui and params.get_bool("HideLeadMarker")
    toggle.hide_map_icon = toggle.advanced_custom_onroad_ui and params.get_bool("HideMapIcon")
    toggle.hide_max_speed = toggle.advanced_custom_onroad_ui and params.get_bool("HideMaxSpeed")
    toggle.hide_speed = toggle.advanced_custom_onroad_ui and params.get_bool("HideSpeed")
    toggle.hide_speed_limit = toggle.advanced_custom_onroad_ui and params.get_bool("HideSpeedLimit")
    toggle.use_wheel_speed = toggle.advanced_custom_onroad_ui and params.get_bool("WheelSpeed")

    toggle.advanced_lateral_tuning = params.get_bool("AdvancedLateralTune")
    stock_steer_friction = params.get_float("SteerFrictionStock")
    toggle.steer_friction = params.get_float("SteerFriction") if toggle.advanced_lateral_tuning else stock_steer_friction
    toggle.use_custom_steer_friction = toggle.steer_friction != stock_steer_friction and not is_pid_car
    stock_steer_kp = params.get_float("SteerKPStock")
    toggle.steer_kp = params.get_float("SteerKP") if toggle.advanced_lateral_tuning else stock_steer_kp
    toggle.use_custom_kp = toggle.steer_kp != stock_steer_kp and not is_pid_car
    stock_steer_lat_accel_factor = params.get_float("SteerLatAccelStock")
    toggle.steer_lat_accel_factor = params.get_float("SteerLatAccel") if toggle.advanced_lateral_tuning else stock_steer_lat_accel_factor
    toggle.use_custom_lat_accel_factor = toggle.steer_lat_accel_factor != stock_steer_lat_accel_factor and not is_pid_car
    stock_steer_ratio = params.get_float("SteerRatioStock")
    toggle.steer_ratio = params.get_float("SteerRatio") if toggle.advanced_lateral_tuning else stock_steer_ratio
    toggle.use_custom_steer_ratio = toggle.steer_ratio != stock_steer_ratio
    toggle.force_auto_tune = toggle.advanced_lateral_tuning and not has_auto_tune and not is_pid_car and params.get_bool("ForceAutoTune")
    toggle.force_auto_tune_off = toggle.advanced_lateral_tuning and has_auto_tune and not is_pid_car and params.get_bool("ForceAutoTuneOff")

    toggle.alert_volume_control = params.get_bool("AlertVolumeControl")
    toggle.disengage_volume = params.get_int("DisengageVolume") if toggle.alert_volume_control else 101
    toggle.engage_volume = params.get_int("EngageVolume") if toggle.alert_volume_control else 101
    toggle.prompt_volume = params.get_int("PromptVolume") if toggle.alert_volume_control else 101
    toggle.promptDistracted_volume = params.get_int("PromptDistractedVolume") if toggle.alert_volume_control else 101
    toggle.refuse_volume = params.get_int("RefuseVolume") if toggle.alert_volume_control else 101
    toggle.warningSoft_volume = params.get_int("WarningSoftVolume") if toggle.alert_volume_control else 101
    toggle.warningImmediate_volume = max(params.get_int("WarningImmediateVolume"), 25) if toggle.alert_volume_control else 101

    toggle.always_on_lateral = params.get_bool("AlwaysOnLateral")
    toggle.always_on_lateral_set = toggle.always_on_lateral and always_on_lateral_set
    toggle.always_on_lateral_lkas = toggle.always_on_lateral_set and car_make != "subaru" and params.get_bool("AlwaysOnLateralLKAS")
    toggle.always_on_lateral_main = toggle.always_on_lateral_set and params.get_bool("AlwaysOnLateralMain")
    toggle.always_on_lateral_pause_speed = params.get_int("PauseAOLOnBrake") if toggle.always_on_lateral_set else 0
    toggle.always_on_lateral_status_bar = toggle.always_on_lateral_set and not params.get_bool("HideAOLStatusBar")

    toggle.automatic_updates = params.get_bool("AutomaticUpdates")

    toggle.cluster_offset = params.get_float("ClusterOffset") if car_make == "toyota" else 1

    toggle.conditional_experimental_mode = openpilot_longitudinal and params.get_bool("ConditionalExperimental")
    toggle.conditional_curves = toggle.conditional_experimental_mode and params.get_bool("CECurves")
    toggle.conditional_curves_lead = toggle.conditional_curves and params.get_bool("CECurvesLead")
    toggle.conditional_lead = toggle.conditional_experimental_mode and params.get_bool("CELead")
    toggle.conditional_slower_lead = toggle.conditional_lead and params.get_bool("CESlowerLead")
    toggle.conditional_stopped_lead = toggle.conditional_lead and params.get_bool("CEStoppedLead")
    toggle.conditional_limit = params.get_int("CESpeed") * speed_conversion if toggle.conditional_experimental_mode else 0
    toggle.conditional_limit_lead = params.get_int("CESpeedLead") * speed_conversion if toggle.conditional_experimental_mode else 0
    toggle.conditional_navigation = toggle.conditional_experimental_mode and params.get_bool("CENavigation")
    toggle.conditional_navigation_intersections = toggle.conditional_navigation and params.get_bool("CENavigationIntersections")
    toggle.conditional_navigation_lead = toggle.conditional_navigation and params.get_bool("CENavigationLead")
    toggle.conditional_navigation_turns = toggle.conditional_navigation and params.get_bool("CENavigationTurns")
    toggle.conditional_model_stop_time = params.get_int("CEModelStopTime") if toggle.conditional_experimental_mode else 0
    toggle.conditional_signal = params.get_int("CESignalSpeed") if toggle.conditional_experimental_mode else 0
    toggle.conditional_signal_lane_detection = toggle.conditional_signal != 0 and params.get_bool("CESignalLaneDetection")
    toggle.conditional_status_bar = toggle.conditional_experimental_mode and not params.get_bool("HideCEMStatusBar")
    if toggle.conditional_experimental_mode:
      params.put_bool("ExperimentalMode", True)

    toggle.crosstrek_torque = car_model == "SUBARU_IMPREZA" and params.get_bool("CrosstrekTorque")

    toggle.current_holiday_theme = params.get("CurrentHolidayTheme", encoding='utf-8') if params.get_bool("HolidayThemes") else None

    toggle.curve_speed_controller = openpilot_longitudinal and params.get_bool("CurveSpeedControl")
    toggle.curve_sensitivity = params.get_int("CurveSensitivity") / 100 if toggle.curve_speed_controller else 1
    toggle.turn_aggressiveness = params.get_int("TurnAggressiveness") / 100 if toggle.curve_speed_controller else 1
    toggle.map_turn_speed_controller = toggle.curve_speed_controller and params.get_bool("MTSCEnabled")
    toggle.mtsc_curvature_check = toggle.map_turn_speed_controller and params.get_bool("MTSCCurvatureCheck")
    toggle.vision_turn_controller = toggle.curve_speed_controller and params.get_bool("VisionTurnControl")

    toggle.custom_alerts = params.get_bool("CustomAlerts")
    toggle.goat_scream_alert = toggle.current_holiday_theme is None and toggle.custom_alerts and params.get_bool("GoatScream")
    toggle.green_light_alert = toggle.custom_alerts and params.get_bool("GreenLightAlert")
    toggle.lead_departing_alert = toggle.custom_alerts and params.get_bool("LeadDepartingAlert")
    toggle.loud_blindspot_alert = has_bsm and toggle.custom_alerts and params.get_bool("LoudBlindspotAlert")
    toggle.speed_limit_changed_alert = toggle.custom_alerts and params.get_bool("SpeedLimitChangedAlert")

    toggle.custom_personalities = openpilot_longitudinal and params.get_bool("CustomPersonalities")
    toggle.aggressive_profile = toggle.custom_personalities and params.get_bool("AggressivePersonalityProfile")
    toggle.aggressive_jerk_acceleration = clip(params.get_int("AggressiveJerkAcceleration") / 100, 0.01, 5) if toggle.aggressive_profile else 0.5
    toggle.aggressive_jerk_deceleration = clip(params.get_int("AggressiveJerkDeceleration") / 100, 0.01, 5) if toggle.aggressive_profile else 0.5
    toggle.aggressive_jerk_danger = clip(params.get_int("AggressiveJerkDanger") / 100, 0.01, 5) if toggle.aggressive_profile else 0.5
    toggle.aggressive_jerk_speed = clip(params.get_int("AggressiveJerkSpeed") / 100, 0.01, 5) if toggle.aggressive_profile else 0.5
    toggle.aggressive_jerk_speed_decrease = clip(params.get_int("AggressiveJerkSpeedDecrease") / 100, 0.01, 5) if toggle.aggressive_profile else 0.5
    toggle.aggressive_follow = clip(params.get_float("AggressiveFollow"), 1, 5) if toggle.aggressive_profile else 1.25
    toggle.standard_profile = toggle.custom_personalities and params.get_bool("StandardPersonalityProfile")
    toggle.standard_jerk_acceleration = clip(params.get_int("StandardJerkAcceleration") / 100, 0.01, 5) if toggle.standard_profile else 1.0
    toggle.standard_jerk_deceleration = clip(params.get_int("StandardJerkDeceleration") / 100, 0.01, 5) if toggle.standard_profile else 1.0
    toggle.standard_jerk_danger = clip(params.get_int("StandardJerkDanger") / 100, 0.01, 5) if toggle.standard_profile else 0.5
    toggle.standard_jerk_speed = clip(params.get_int("StandardJerkSpeed") / 100, 0.01, 5) if toggle.standard_profile else 1.0
    toggle.standard_jerk_speed_decrease = clip(params.get_int("StandardJerkSpeedDecrease") / 100, 0.01, 5) if toggle.standard_profile else 1.0
    toggle.standard_follow = clip(params.get_float("StandardFollow"), 1, 5) if toggle.standard_profile else 1.45
    toggle.relaxed_profile = toggle.custom_personalities and params.get_bool("RelaxedPersonalityProfile")
    toggle.relaxed_jerk_acceleration = clip(params.get_int("RelaxedJerkAcceleration") / 100, 0.01, 5) if toggle.relaxed_profile else 1.0
    toggle.relaxed_jerk_deceleration = clip(params.get_int("RelaxedJerkDeceleration") / 100, 0.01, 5) if toggle.relaxed_profile else 1.0
    toggle.relaxed_jerk_danger = clip(params.get_int("RelaxedJerkDanger") / 100, 0.01, 5) if toggle.relaxed_profile else 0.5
    toggle.relaxed_jerk_speed = clip(params.get_int("RelaxedJerkSpeed") / 100, 0.01, 5) if toggle.relaxed_profile else 1.0
    toggle.relaxed_jerk_speed_decrease = clip(params.get_int("RelaxedJerkSpeedDecrease") / 100, 0.01, 5) if toggle.relaxed_profile else 1.0
    toggle.relaxed_follow = clip(params.get_float("RelaxedFollow"), 1, 5) if toggle.relaxed_profile else 1.75
    toggle.traffic_profile = toggle.custom_personalities and params.get_bool("TrafficPersonalityProfile")
    toggle.traffic_mode_jerk_acceleration = [clip(params.get_int("TrafficJerkAcceleration") / 100, 0.01, 5), toggle.aggressive_jerk_acceleration] if toggle.traffic_profile else [0.5, 0.5]
    toggle.traffic_mode_jerk_deceleration = [clip(params.get_int("TrafficJerkDeceleration") / 100, 0.01, 5), toggle.aggressive_jerk_deceleration] if toggle.traffic_profile else [0.5, 0.5]
    toggle.traffic_mode_jerk_danger = [clip(params.get_int("TrafficJerkDanger") / 100, 0.01, 5), toggle.aggressive_jerk_danger] if toggle.traffic_profile else [1.0, 1.0]
    toggle.traffic_mode_jerk_speed = [clip(params.get_int("TrafficJerkSpeed") / 100, 0.01, 5), toggle.aggressive_jerk_speed] if toggle.traffic_profile else [0.5, 0.5]
    toggle.traffic_mode_jerk_speed_decrease = [clip(params.get_int("TrafficJerkSpeedDecrease") / 100, 0.01, 5), toggle.aggressive_jerk_speed_decrease] if toggle.traffic_profile else [0.5, 0.5]
    toggle.traffic_mode_t_follow = [clip(params.get_float("TrafficFollow"), 0.5, 5), toggle.aggressive_follow] if toggle.traffic_profile else [0.5, 1.0]

    toggle.custom_ui = params.get_bool("CustomUI")
    toggle.acceleration_path = toggle.custom_ui and params.get_bool("AccelerationPath")
    toggle.adjacent_paths = toggle.custom_ui and params.get_bool("AdjacentPath")
    toggle.blind_spot_path = has_bsm and toggle.custom_ui and params.get_bool("BlindSpotPath")
    toggle.compass = toggle.custom_ui and params.get_bool("Compass")
    toggle.pedals_on_ui = toggle.custom_ui and params.get_bool("PedalsOnUI")
    toggle.dynamic_pedals_on_ui = toggle.pedals_on_ui and params.get_bool("DynamicPedalsOnUI")
    toggle.static_pedals_on_ui = toggle.pedals_on_ui and params.get_bool("StaticPedalsOnUI")
    toggle.rotating_wheel = toggle.custom_ui and params.get_bool("RotatingWheel")

    toggle.developer_ui = params.get_bool("DeveloperUI")
    toggle.border_metrics = toggle.developer_ui and params.get_bool("BorderMetrics")
    toggle.blind_spot_metrics = has_bsm and toggle.border_metrics and params.get_bool("BlindSpotMetrics")
    toggle.signal_metrics = toggle.border_metrics and params.get_bool("SignalMetrics")
    toggle.steering_metrics = toggle.border_metrics and params.get_bool("ShowSteering")
    toggle.show_fps = toggle.developer_ui and params.get_bool("FPSCounter")
    toggle.lateral_metrics = toggle.developer_ui and params.get_bool("LateralMetrics")
    toggle.adjacent_path_metrics = toggle.lateral_metrics and params.get_bool("AdjacentPathMetrics")
    toggle.lateral_tuning_metrics = toggle.lateral_metrics and params.get_bool("TuningInfo")
    toggle.longitudinal_metrics = toggle.developer_ui and params.get_bool("LongitudinalMetrics")
    toggle.adjacent_lead_tracking = has_radar and toggle.longitudinal_metrics and params.get_bool("AdjacentLeadsUI")
    toggle.lead_metrics = toggle.longitudinal_metrics and params.get_bool("LeadInfo")
    toggle.jerk_metrics = toggle.longitudinal_metrics and params.get_bool("JerkInfo")
    toggle.numerical_temp = toggle.developer_ui and params.get_bool("NumericalTemp")
    toggle.fahrenheit = toggle.numerical_temp and params.get_bool("Fahrenheit")
    toggle.sidebar_metrics = toggle.developer_ui and params.get_bool("SidebarMetrics")
    toggle.cpu_metrics = toggle.sidebar_metrics and params.get_bool("ShowCPU")
    toggle.gpu_metrics = toggle.sidebar_metrics and params.get_bool("ShowGPU")
    toggle.ip_metrics = toggle.sidebar_metrics and params.get_bool("ShowIP")
    toggle.memory_metrics = toggle.sidebar_metrics and params.get_bool("ShowMemoryUsage")
    toggle.storage_left_metrics = toggle.sidebar_metrics and params.get_bool("ShowStorageLeft")
    toggle.storage_used_metrics = toggle.sidebar_metrics and params.get_bool("ShowStorageUsed")
    toggle.use_si_metrics = toggle.developer_ui and params.get_bool("UseSI")

    toggle.device_management = params.get_bool("DeviceManagement")
    device_shutdown_setting = params.get_int("DeviceShutdown") if toggle.device_management else 33
    toggle.device_shutdown_time = (device_shutdown_setting - 3) * 3600 if device_shutdown_setting >= 4 else device_shutdown_setting * (60 * 15)
    toggle.increase_thermal_limits = toggle.device_management and params.get_bool("IncreaseThermalLimits")
    toggle.low_voltage_shutdown = clip(params.get_float("LowVoltageShutdown"), VBATT_PAUSE_CHARGING, 12.5) if toggle.device_management else VBATT_PAUSE_CHARGING
    toggle.no_logging = toggle.device_management and params.get_bool("NoLogging") or self.development_branch
    toggle.no_uploads = toggle.device_management and params.get_bool("NoUploads") or self.development_branch
    toggle.no_onroad_uploads = toggle.no_uploads and params.get_bool("DisableOnroadUploads")
    toggle.offline_mode = toggle.device_management and params.get_bool("OfflineMode")

    toggle.experimental_gm_tune = openpilot_longitudinal and car_make == "gm" and params.get_bool("ExperimentalGMTune")

    toggle.experimental_mode_via_press = openpilot_longitudinal and params.get_bool("ExperimentalModeActivation")
    toggle.experimental_mode_via_distance = toggle.experimental_mode_via_press and params.get_bool("ExperimentalModeViaDistance")
    toggle.experimental_mode_via_lkas = not toggle.always_on_lateral_lkas and toggle.experimental_mode_via_press and car_make != "subaru" and params.get_bool("ExperimentalModeViaLKAS")
    toggle.experimental_mode_via_tap = toggle.experimental_mode_via_press and params.get_bool("ExperimentalModeViaTap")

    toggle.frogsgomoo_tweak = openpilot_longitudinal and car_make == "toyota" and params.get_bool("FrogsGoMoosTweak")

    toggle.lane_change_customizations = params.get_bool("LaneChangeCustomizations")
    toggle.lane_change_delay = params.get_float("LaneChangeTime") if toggle.lane_change_customizations else 0
    toggle.lane_detection_width = params.get_float("LaneDetectionWidth") * distance_conversion if toggle.lane_change_customizations else 0
    toggle.lane_detection = toggle.lane_detection_width != 0
    toggle.minimum_lane_change_speed = params.get_float("MinimumLaneChangeSpeed") * speed_conversion if toggle.lane_change_customizations else LANE_CHANGE_SPEED_MIN
    toggle.nudgeless = toggle.lane_change_customizations and params.get_bool("NudgelessLaneChange")
    toggle.one_lane_change = toggle.lane_change_customizations and params.get_bool("OneLaneChange")

    toggle.lateral_tuning = params.get_bool("LateralTune")
    toggle.nnff = toggle.lateral_tuning and params.get_bool("NNFF")
    toggle.nnff_lite = toggle.lateral_tuning and params.get_bool("NNFFLite")
    toggle.use_turn_desires = toggle.lateral_tuning and params.get_bool("TurnDesires")

    toggle.long_pitch = openpilot_longitudinal and car_make == "gm" and params.get_bool("LongPitch")

    toggle.longitudinal_tuning = openpilot_longitudinal and params.get_bool("LongitudinalTune")
    toggle.acceleration_profile = params.get_int("AccelerationProfile") if toggle.longitudinal_tuning else 0
    toggle.sport_plus = max_acceleration_enabled and toggle.acceleration_profile == 3
    toggle.deceleration_profile = params.get_int("DecelerationProfile") if toggle.longitudinal_tuning else 0
    toggle.human_acceleration = toggle.longitudinal_tuning and params.get_bool("HumanAcceleration")
    toggle.human_following = toggle.longitudinal_tuning and params.get_bool("HumanFollowing")
    toggle.lead_detection_probability = clip(params.get_int("LeadDetectionThreshold") / 100, 0.01, 0.99) if toggle.longitudinal_tuning else 0.5
    toggle.max_desired_acceleration = clip(params.get_float("MaxDesiredAcceleration"), 0.1, 4.0) if toggle.longitudinal_tuning else 4.0
    toggle.taco_tune = toggle.longitudinal_tuning and params.get_bool("TacoTune")

    available_models = params.get("AvailableModels", encoding='utf-8') or ""
    available_model_names = params.get("AvailableModelsNames", encoding='utf-8') or ""
    toggle.model_randomizer = params.get_bool("ModelRandomizer")
    if available_models:
      if toggle.model_randomizer:
        blacklisted_models = (params.get("BlacklistedModels", encoding='utf-8') or "").split(',')
        existing_models = [model for model in available_models.split(',') if model not in blacklisted_models and os.path.exists(os.path.join(MODELS_PATH, f"{model}.thneed"))]
        toggle.model = random.choice(existing_models) if existing_models else DEFAULT_CLASSIC_MODEL
      else:
        toggle.model = params.get("Model", encoding='utf-8')
    else:
      toggle.model = DEFAULT_CLASSIC_MODEL
    if toggle.model in available_models.split(',') and os.path.exists(os.path.join(MODELS_PATH, f"{toggle.model}.thneed")):
      current_model_name = available_model_names.split(',')[available_models.split(',').index(toggle.model)]
      params_memory.put("CurrentModelName", current_model_name)
    else:
      toggle.model = DEFAULT_CLASSIC_MODEL
    classic_models = params.get("ClassicModels", encoding='utf-8') or ""
    toggle.classic_model = classic_models and toggle.model in classic_models.split(',')
    navigation_models = params.get("NavigationModels", encoding='utf-8') or ""
    toggle.navigationless_model = navigation_models and toggle.model not in navigation_models.split(',')
    radarless_models = params.get("RadarlessModels", encoding='utf-8') or ""
    toggle.radarless_model = radarless_models and toggle.model in radarless_models.split(',')

    toggle.model_ui = params.get_bool("ModelUI")
    toggle.dynamic_path_width = toggle.model_ui and params.get_bool("DynamicPathWidth")
    toggle.lane_line_width = params.get_int("LaneLinesWidth") * small_distance_conversion / 200 if toggle.model_ui else 0.025
    toggle.path_edge_width = params.get_int("PathEdgeWidth") if toggle.model_ui else 20
    toggle.path_width = params.get_float("PathWidth") * distance_conversion / 2 if toggle.model_ui else 0.9
    toggle.road_edge_width = params.get_int("RoadEdgesWidth") * small_distance_conversion / 200 if toggle.model_ui else 0.025
    toggle.show_stopping_point = toggle.model_ui and params.get_bool("ShowStoppingPoint")
    toggle.show_stopping_point_metrics = toggle.show_stopping_point and params.get_bool("ShowStoppingPointMetrics")
    toggle.unlimited_road_ui_length = toggle.model_ui and params.get_bool("UnlimitedLength")

    toggle.navigation_ui = params.get_bool("NavigationUI")
    toggle.big_map = toggle.navigation_ui and params.get_bool("BigMap")
    toggle.full_map = toggle.big_map and params.get_bool("FullMap")
    toggle.map_style = params.get_int("MapStyle") if toggle.navigation_ui else 0
    toggle.road_name_ui = toggle.navigation_ui and params.get_bool("RoadNameUI")
    toggle.show_speed_limit_offset = toggle.navigation_ui and params.get_bool("ShowSLCOffset")
    toggle.speed_limit_vienna = toggle.navigation_ui and params.get_bool("UseVienna")

    toggle.old_long_api = openpilot_longitudinal and car_make == "gm" and not params.get_bool("NewLongAPIGM")
    toggle.old_long_api |= openpilot_longitudinal and car_make == "hyundai" and not params.get_bool("NewLongAPI")

    toggle.personalize_openpilot = params.get_bool("PersonalizeOpenpilot")
    toggle.color_scheme = params.get("CustomColors", encoding='utf-8') if toggle.personalize_openpilot else "stock"
    toggle.distance_icons = params.get("CustomDistanceIcons", encoding='utf-8') if toggle.personalize_openpilot else "stock"
    toggle.icon_pack = params.get("CustomIcons", encoding='utf-8') if toggle.personalize_openpilot else "stock"
    toggle.signal_icons = params.get("CustomSignals", encoding='utf-8') if toggle.personalize_openpilot else "stock"
    toggle.sound_pack = params.get("CustomSounds", encoding='utf-8') if toggle.personalize_openpilot else "stock"
    toggle.wheel_image = params.get("WheelIcon", encoding='utf-8') if toggle.personalize_openpilot else "stock"

    toggle.quality_of_life_lateral = params.get_bool("QOLLateral")
    toggle.pause_lateral_below_speed = params.get_int("PauseLateralSpeed") * speed_conversion if toggle.quality_of_life_lateral else 0
    toggle.pause_lateral_below_signal = toggle.pause_lateral_below_speed != 0 and params.get_bool("PauseLateralOnSignal")

    toggle.quality_of_life_longitudinal = params.get_bool("QOLLongitudinal")
    toggle.custom_cruise_increase = params.get_int("CustomCruise") if toggle.quality_of_life_longitudinal and not pcm_cruise else 1
    toggle.custom_cruise_increase_long = params.get_int("CustomCruiseLong") if toggle.quality_of_life_longitudinal and not pcm_cruise else 5
    toggle.force_standstill = toggle.quality_of_life_longitudinal and params.get_bool("ForceStandstill")
    toggle.force_stops = toggle.quality_of_life_longitudinal and params.get_bool("ForceStops")
    toggle.increased_stopped_distance = params.get_int("IncreasedStoppedDistance") * distance_conversion if toggle.quality_of_life_longitudinal else 0
    toggle.map_gears = toggle.quality_of_life_longitudinal and params.get_bool("MapGears")
    toggle.map_acceleration = toggle.map_gears and params.get_bool("MapAcceleration")
    toggle.map_deceleration = toggle.map_gears and params.get_bool("MapDeceleration")
    toggle.reverse_cruise_increase = toggle.quality_of_life_longitudinal and pcm_cruise and params.get_bool("ReverseCruise")
    toggle.set_speed_offset = params.get_int("SetSpeedOffset") * (1 if toggle.is_metric else CV.MPH_TO_KPH) if toggle.quality_of_life_longitudinal and not pcm_cruise else 0

    toggle.quality_of_life_visuals = params.get_bool("QOLVisuals")
    toggle.camera_view = params.get_int("CameraView") if toggle.quality_of_life_visuals else 0
    toggle.driver_camera_in_reverse = toggle.quality_of_life_visuals and params.get_bool("DriverCamera")
    toggle.onroad_distance_button = toggle.quality_of_life_visuals and params.get_bool("OnroadDistanceButton")
    toggle.standby_mode = toggle.quality_of_life_visuals and params.get_bool("StandbyMode")
    toggle.stopped_timer = toggle.quality_of_life_visuals and params.get_bool("StoppedTimer")

    toggle.random_events = params.get_bool("RandomEvents")

    toggle.screen_management = params.get_bool("ScreenManagement")
    toggle.screen_brightness = params.get_int("ScreenBrightness") if toggle.screen_management else 101
    toggle.screen_brightness_onroad = params.get_int("ScreenBrightnessOnroad") if toggle.screen_management else 101
    toggle.screen_recorder = toggle.screen_management and params.get_bool("ScreenRecorder")
    toggle.screen_timeout = params.get_int("ScreenTimeout") if toggle.screen_management else 30
    toggle.screen_timeout_onroad = params.get_int("ScreenTimeoutOnroad") if toggle.screen_management else 10

    toggle.sng_hack = openpilot_longitudinal and car_make == "toyota" and params.get_bool("SNGHack")

    toggle.speed_limit_controller = openpilot_longitudinal and params.get_bool("SpeedLimitController")
    toggle.force_mph_dashboard = toggle.speed_limit_controller and params.get_bool("ForceMPHDashboard")
    toggle.map_speed_lookahead_higher = params.get_int("SLCLookaheadHigher") if toggle.speed_limit_controller else 0
    toggle.map_speed_lookahead_lower = params.get_int("SLCLookaheadLower") if toggle.speed_limit_controller else 0
    toggle.set_speed_limit = toggle.speed_limit_controller and params.get_bool("SetSpeedLimit")
    slc_fallback_method = params.get_int("SLCFallback") if toggle.speed_limit_controller else 0
    toggle.slc_fallback_experimental_mode = toggle.speed_limit_controller and slc_fallback_method == 1
    toggle.slc_fallback_previous_speed_limit = toggle.speed_limit_controller and slc_fallback_method == 2
    toggle.slc_fallback_set_speed = toggle.speed_limit_controller and slc_fallback_method == 0
    toggle.speed_limit_confirmation = toggle.speed_limit_controller and params.get_bool("SLCConfirmation")
    toggle.speed_limit_confirmation_higher = toggle.speed_limit_confirmation and params.get_bool("SLCConfirmationHigher")
    toggle.speed_limit_confirmation_lower = toggle.speed_limit_confirmation and params.get_bool("SLCConfirmationLower")
    toggle.speed_limit_controller_override = params.get_int("SLCOverride") if toggle.speed_limit_controller else 0
    toggle.speed_limit_controller_override_manual = toggle.speed_limit_controller_override == 1
    toggle.speed_limit_controller_override_set_speed = toggle.speed_limit_controller_override == 2
    toggle.speed_limit_offset1 = params.get_int("Offset1") * speed_conversion if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset2 = params.get_int("Offset2") * speed_conversion if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset3 = params.get_int("Offset3") * speed_conversion if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset4 = params.get_int("Offset4") * speed_conversion if toggle.speed_limit_controller else 0
    toggle.speed_limit_priority1 = params.get("SLCPriority1", encoding='utf-8') if toggle.speed_limit_controller else None
    toggle.speed_limit_priority2 = params.get("SLCPriority2", encoding='utf-8') if toggle.speed_limit_controller else None
    toggle.speed_limit_priority3 = params.get("SLCPriority3", encoding='utf-8') if toggle.speed_limit_controller else None
    toggle.speed_limit_priority_highest = toggle.speed_limit_priority1 == "Highest"
    toggle.speed_limit_priority_lowest = toggle.speed_limit_priority1 == "Lowest"

    toggle.startup_alert_top = params.get("StartupMessageTop", encoding='utf-8') or ""
    toggle.startup_alert_bottom = params.get("StartupMessageBottom", encoding='utf-8') or ""

    toggle.tethering_config = params.get_int("TetheringEnabled")

    toggle.toyota_doors = car_make == "toyota" and params.get_bool("ToyotaDoors")
    toggle.lock_doors = toggle.toyota_doors and params.get_bool("LockDoors")
    toggle.unlock_doors = toggle.toyota_doors and params.get_bool("UnlockDoors")

    toggle.volt_sng = car_model == "CHEVROLET_VOLT" and params.get_bool("VoltSNG")

    customization_level = params.get_int("CustomizationLevel") if params.get_bool("CustomizationLevelConfirmed") else 2

    if customization_level == 0:
      toggle.advanced_custom_onroad_ui = bool(self.default_frogpilot_toggles.AdvancedCustomUI)
      toggle.hide_alerts = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideAlerts)
      toggle.hide_lead_marker = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideLeadMarker)
      toggle.hide_map_icon = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideMapIcon)
      toggle.hide_max_speed = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideMaxSpeed)
      toggle.hide_speed = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideSpeed)
      toggle.hide_speed_limit = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideSpeedLimit)
      toggle.use_wheel_speed = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.WheelSpeed)

      toggle.advanced_lateral_tuning = bool(self.default_frogpilot_toggles.AdvancedLateralTune)
      toggle.use_custom_steer_friction = False
      toggle.use_custom_kp = False
      toggle.use_custom_lat_accel_factor = False
      toggle.use_custom_steer_ratio = False
      toggle.force_auto_tune = bool(toggle.advanced_lateral_tuning and not has_auto_tune and not is_pid_car and self.default_frogpilot_toggles.ForceAutoTune)
      toggle.force_auto_tune_off = bool(toggle.advanced_lateral_tuning and has_auto_tune and not is_pid_car and self.default_frogpilot_toggles.ForceAutoTuneOff)

      toggle.alert_volume_control = bool(self.default_frogpilot_toggles.AlertVolumeControl)
      toggle.disengage_volume = int(self.default_frogpilot_toggles.DisengageVolume if toggle.alert_volume_control else 101)
      toggle.engage_volume = int(self.default_frogpilot_toggles.EngageVolume if toggle.alert_volume_control else 101)
      toggle.prompt_volume = int(self.default_frogpilot_toggles.PromptVolume if toggle.alert_volume_control else 101)
      toggle.promptDistracted_volume = int(self.default_frogpilot_toggles.PromptDistractedVolume if toggle.alert_volume_control else 101)
      toggle.refuse_volume = int(self.default_frogpilot_toggles.RefuseVolume if toggle.alert_volume_control else 101)
      toggle.warningSoft_volume = int(self.default_frogpilot_toggles.WarningSoftVolume if toggle.alert_volume_control else 101)
      toggle.warningImmediate_volume = int(self.default_frogpilot_toggles.WarningImmediateVolume if toggle.alert_volume_control else 101)

      toggle.always_on_lateral = bool(always_on_lateral_set and self.default_frogpilot_toggles.AlwaysOnLateral)
      toggle.always_on_lateral_lkas = bool(toggle.always_on_lateral and car_make != "subaru" and self.default_frogpilot_toggles.AlwaysOnLateralLKAS)
      toggle.always_on_lateral_main = bool(toggle.always_on_lateral and self.default_frogpilot_toggles.AlwaysOnLateralMain)
      toggle.always_on_lateral_pause_speed = int(self.default_frogpilot_toggles.PauseAOLOnBrake if toggle.always_on_lateral else 0)
      toggle.always_on_lateral_status_bar = bool(toggle.always_on_lateral and not self.default_frogpilot_toggles.HideAOLStatusBar)

      toggle.cluster_offset = float(self.default_frogpilot_toggles.ClusterOffset if car_make == "toyota" else 1)

      toggle.conditional_experimental_mode = bool(openpilot_longitudinal and self.default_frogpilot_toggles.ConditionalExperimental)
      toggle.conditional_curves = bool(toggle.conditional_experimental_mode and self.default_frogpilot_toggles.CECurves)
      toggle.conditional_curves_lead = bool(toggle.conditional_curves and self.default_frogpilot_toggles.CECurvesLead)
      toggle.conditional_lead = bool(toggle.conditional_experimental_mode and self.default_frogpilot_toggles.CELead)
      toggle.conditional_slower_lead = bool(toggle.conditional_lead and self.default_frogpilot_toggles.CESlowerLead)
      toggle.conditional_stopped_lead = bool(toggle.conditional_lead and self.default_frogpilot_toggles.CEStoppedLead)
      toggle.conditional_limit = int(self.default_frogpilot_toggles.CESpeed) * speed_conversion if toggle.conditional_experimental_mode else 0
      toggle.conditional_limit_lead = int(self.default_frogpilot_toggles.CESpeedLead) * speed_conversion if toggle.conditional_experimental_mode else 0
      toggle.conditional_navigation = bool(toggle.conditional_experimental_mode and self.default_frogpilot_toggles.CENavigation)
      toggle.conditional_navigation_intersections = bool(toggle.conditional_navigation and self.default_frogpilot_toggles.CENavigationIntersections)
      toggle.conditional_navigation_lead = bool(toggle.conditional_navigation and self.default_frogpilot_toggles.CENavigationLead)
      toggle.conditional_navigation_turns = bool(toggle.conditional_navigation and self.default_frogpilot_toggles.CENavigationTurns)
      toggle.conditional_model_stop_time = int(self.default_frogpilot_toggles.CEModelStopTime if toggle.conditional_experimental_mode else 0)
      toggle.conditional_signal = int(self.default_frogpilot_toggles.CESignalSpeed if toggle.conditional_experimental_mode else 0)
      toggle.conditional_signal_lane_detection = bool(toggle.conditional_signal != 0 and self.default_frogpilot_toggles.CESignalLaneDetection)
      toggle.conditional_status_bar = bool(toggle.conditional_experimental_mode and not self.default_frogpilot_toggles.HideCEMStatusBar)
      if toggle.conditional_experimental_mode:
        params.put_bool("ExperimentalMode", True)

      toggle.crosstrek_torque = bool(car_model == "SUBARU_IMPREZA" and self.default_frogpilot_toggles.CrosstrekTorque)

      toggle.curve_speed_controller = bool(openpilot_longitudinal and self.default_frogpilot_toggles.CurveSpeedControl)
      toggle.curve_sensitivity = int(self.default_frogpilot_toggles.CurveSensitivity) / 100 if toggle.curve_speed_controller else 1
      toggle.turn_aggressiveness = int(self.default_frogpilot_toggles.TurnAggressiveness) / 100 if toggle.curve_speed_controller else 1
      toggle.map_turn_speed_controller = bool(toggle.curve_speed_controller and self.default_frogpilot_toggles.MTSCEnabled)
      toggle.mtsc_curvature_check = bool(toggle.map_turn_speed_controller and self.default_frogpilot_toggles.MTSCCurvatureCheck)
      toggle.vision_turn_controller = bool(toggle.curve_speed_controller and self.default_frogpilot_toggles.VisionTurnControl)

      toggle.custom_personalities = bool(openpilot_longitudinal and self.default_frogpilot_toggles.CustomPersonalities)
      toggle.aggressive_profile = bool(toggle.custom_personalities and self.default_frogpilot_toggles.AggressivePersonalityProfile)
      toggle.aggressive_jerk_deceleration = int(self.default_frogpilot_toggles.AggressiveJerkDeceleration) / 100 if toggle.aggressive_profile else 0.5
      toggle.aggressive_jerk_danger = int(self.default_frogpilot_toggles.AggressiveJerkDanger) / 100 if toggle.aggressive_profile else 1.0
      toggle.aggressive_jerk_speed = int(self.default_frogpilot_toggles.AggressiveJerkSpeed) / 100 if toggle.aggressive_profile else 0.5
      toggle.aggressive_jerk_speed_decrease = int(self.default_frogpilot_toggles.AggressiveJerkSpeedDecrease) / 100 if toggle.aggressive_profile else 0.5
      toggle.aggressive_follow = float(self.default_frogpilot_toggles.AggressiveFollow) if toggle.aggressive_profile else 1.25
      toggle.standard_profile = bool(toggle.custom_personalities and self.default_frogpilot_toggles.StandardPersonalityProfile)
      toggle.standard_jerk_acceleration = int(self.default_frogpilot_toggles.StandardJerkAcceleration) / 100 if toggle.standard_profile else 1.0
      toggle.standard_jerk_deceleration = int(self.default_frogpilot_toggles.StandardJerkDeceleration) / 100 if toggle.standard_profile else 1.0
      toggle.standard_jerk_danger = int(self.default_frogpilot_toggles.StandardJerkDanger) / 100 if toggle.standard_profile else 1.0
      toggle.standard_jerk_speed = int(self.default_frogpilot_toggles.StandardJerkSpeed) / 100 if toggle.standard_profile else 1.0
      toggle.standard_jerk_speed_decrease = int(self.default_frogpilot_toggles.StandardJerkSpeedDecrease) / 100 if toggle.standard_profile else 1.0
      toggle.standard_follow = float(self.default_frogpilot_toggles.StandardFollow) if toggle.standard_profile else 1.45
      toggle.relaxed_profile = bool(toggle.custom_personalities and self.default_frogpilot_toggles.RelaxedPersonalityProfile)
      toggle.relaxed_jerk_acceleration = int(self.default_frogpilot_toggles.RelaxedJerkAcceleration) / 100 if toggle.relaxed_profile else 1.0
      toggle.relaxed_jerk_deceleration = int(self.default_frogpilot_toggles.RelaxedJerkDeceleration) / 100 if toggle.relaxed_profile else 1.0
      toggle.relaxed_jerk_danger = int(self.default_frogpilot_toggles.RelaxedJerkDanger) / 100 if toggle.relaxed_profile else 1.0
      toggle.relaxed_jerk_speed = int(self.default_frogpilot_toggles.RelaxedJerkSpeed) / 100 if toggle.relaxed_profile else 1.0
      toggle.relaxed_jerk_speed_decrease = int(self.default_frogpilot_toggles.RelaxedJerkSpeedDecrease) / 100 if toggle.relaxed_profile else 1.0
      toggle.relaxed_follow = float(self.default_frogpilot_toggles.RelaxedFollow) if toggle.relaxed_profile else 1.75
      toggle.traffic_profile = bool(toggle.custom_personalities and self.default_frogpilot_toggles.TrafficPersonalityProfile)
      toggle.traffic_mode_jerk_acceleration = [int(self.default_frogpilot_toggles.TrafficJerkAcceleration) / 100, toggle.aggressive_jerk_acceleration] if toggle.traffic_profile else [0.5, 0.5]
      toggle.traffic_mode_jerk_deceleration = [int(self.default_frogpilot_toggles.TrafficJerkDeceleration) / 100, toggle.aggressive_jerk_deceleration] if toggle.traffic_profile else [0.5, 0.5]
      toggle.traffic_mode_jerk_danger = [int(self.default_frogpilot_toggles.TrafficJerkDanger) / 100, toggle.aggressive_jerk_danger] if toggle.traffic_profile else [1.0, 1.0]
      toggle.traffic_mode_jerk_speed = [int(self.default_frogpilot_toggles.TrafficJerkSpeed) / 100, toggle.aggressive_jerk_speed] if toggle.traffic_profile else [0.5, 0.5]
      toggle.traffic_mode_jerk_speed_decrease = [int(self.default_frogpilot_toggles.TrafficJerkSpeedDecrease) / 100, toggle.aggressive_jerk_speed_decrease] if toggle.traffic_profile else [0.5, 0.5]
      toggle.traffic_mode_t_follow = [float(self.default_frogpilot_toggles.TrafficFollow), toggle.aggressive_follow] if toggle.traffic_profile else [0.5, 1.0]

      toggle.custom_ui = bool(self.default_frogpilot_toggles.CustomUI)
      toggle.acceleration_path = bool(toggle.custom_ui and self.default_frogpilot_toggles.AccelerationPath)
      toggle.adjacent_paths = bool(toggle.custom_ui and self.default_frogpilot_toggles.AdjacentPath)
      toggle.blind_spot_path = bool(has_bsm and toggle.custom_ui and self.default_frogpilot_toggles.BlindSpotPath)
      toggle.compass = bool(toggle.custom_ui and self.default_frogpilot_toggles.Compass)
      toggle.pedals_on_ui = bool(toggle.custom_ui and self.default_frogpilot_toggles.PedalsOnUI)
      toggle.dynamic_pedals_on_ui = bool(toggle.pedals_on_ui and self.default_frogpilot_toggles.DynamicPedalsOnUI)
      toggle.static_pedals_on_ui = bool(toggle.pedals_on_ui and self.default_frogpilot_toggles.StaticPedalsOnUI)
      toggle.rotating_wheel = bool(toggle.custom_ui and self.default_frogpilot_toggles.RotatingWheel)

      toggle.developer_ui = bool(self.default_frogpilot_toggles.DeveloperUI)
      toggle.border_metrics = bool(toggle.developer_ui and self.default_frogpilot_toggles.BorderMetrics)
      toggle.blind_spot_metrics = bool(has_bsm and toggle.border_metrics and self.default_frogpilot_toggles.BlindSpotMetrics)
      toggle.signal_metrics = bool(toggle.border_metrics and self.default_frogpilot_toggles.SignalMetrics)
      toggle.steering_metrics = bool(toggle.border_metrics and self.default_frogpilot_toggles.ShowSteering)
      toggle.show_fps = bool(toggle.developer_ui and self.default_frogpilot_toggles.FPSCounter)
      toggle.lateral_metrics = bool(toggle.developer_ui and self.default_frogpilot_toggles.LateralMetrics)
      toggle.adjacent_path_metrics = bool(toggle.lateral_metrics and self.default_frogpilot_toggles.AdjacentPathMetrics)
      toggle.lateral_tuning_metrics = bool(toggle.lateral_metrics and self.default_frogpilot_toggles.TuningInfo)
      toggle.longitudinal_metrics = bool(toggle.developer_ui and self.default_frogpilot_toggles.LongitudinalMetrics)
      toggle.adjacent_lead_tracking = bool(has_radar and toggle.longitudinal_metrics and self.default_frogpilot_toggles.AdjacentLeadsUI)
      toggle.lead_metrics = bool(toggle.longitudinal_metrics and self.default_frogpilot_toggles.LeadInfo)
      toggle.jerk_metrics = bool(toggle.longitudinal_metrics and self.default_frogpilot_toggles.JerkInfo)
      toggle.numerical_temp = bool(toggle.developer_ui and self.default_frogpilot_toggles.NumericalTemp)
      toggle.fahrenheit = bool(toggle.numerical_temp and self.default_frogpilot_toggles.Fahrenheit)
      toggle.sidebar_metrics = bool(toggle.developer_ui and self.default_frogpilot_toggles.SidebarMetrics)
      toggle.cpu_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowCPU)
      toggle.gpu_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowGPU)
      toggle.ip_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowIP)
      toggle.memory_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowMemoryUsage)
      toggle.storage_left_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowStorageLeft)
      toggle.storage_used_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowStorageUsed)
      toggle.use_si_metrics = bool(toggle.developer_ui and self.default_frogpilot_toggles.UseSI)

      toggle.device_management = bool(self.default_frogpilot_toggles.DeviceManagement)
      device_shutdown_setting = int(self.default_frogpilot_toggles.DeviceShutdown) if toggle.device_management else 33
      toggle.device_shutdown_time = (device_shutdown_setting - 3) * 3600 if device_shutdown_setting >= 4 else device_shutdown_setting * (60 * 15)
      toggle.increase_thermal_limits = bool(toggle.device_management and self.default_frogpilot_toggles.IncreaseThermalLimits)
      toggle.low_voltage_shutdown = clip(float(self.default_frogpilot_toggles.LowVoltageShutdown), VBATT_PAUSE_CHARGING, 12.5) if toggle.device_management else VBATT_PAUSE_CHARGING
      toggle.no_logging = bool(toggle.device_management and self.default_frogpilot_toggles.NoLogging or self.development_branch)
      toggle.no_uploads = bool(toggle.device_management and self.default_frogpilot_toggles.NoUploads or self.development_branch)
      toggle.no_onroad_uploads = bool(toggle.no_uploads and self.default_frogpilot_toggles.DisableOnroadUploads)
      toggle.offline_mode = bool(toggle.device_management and self.default_frogpilot_toggles.OfflineMode)

      toggle.experimental_gm_tune = bool(openpilot_longitudinal and car_make == "gm" and self.default_frogpilot_toggles.ExperimentalGMTune)

      toggle.experimental_mode_via_press = bool(openpilot_longitudinal and self.default_frogpilot_toggles.ExperimentalModeActivation)
      toggle.experimental_mode_via_distance = bool(toggle.experimental_mode_via_press and self.default_frogpilot_toggles.ExperimentalModeViaDistance)
      toggle.experimental_mode_via_lkas = bool(not toggle.always_on_lateral_lkas and toggle.experimental_mode_via_press and car_make != "subaru" and self.default_frogpilot_toggles.ExperimentalModeViaLKAS)
      toggle.experimental_mode_via_tap = bool(toggle.experimental_mode_via_press and self.default_frogpilot_toggles.ExperimentalModeViaTap)

      toggle.frogsgomoo_tweak = bool(openpilot_longitudinal and car_make == "toyota" and self.default_frogpilot_toggles.FrogsGoMoosTweak)

      toggle.lane_change_customizations = bool(self.default_frogpilot_toggles.LaneChangeCustomizations)
      toggle.lane_change_delay = float(self.default_frogpilot_toggles.LaneChangeTime) if toggle.lane_change_customizations else 0
      toggle.lane_detection_width = float(self.default_frogpilot_toggles.LaneDetectionWidth) * distance_conversion if toggle.lane_change_customizations else 0
      toggle.lane_detection = bool(toggle.lane_detection_width != 0)
      toggle.minimum_lane_change_speed = float(self.default_frogpilot_toggles.MinimumLaneChangeSpeed) * speed_conversion if toggle.lane_change_customizations else LANE_CHANGE_SPEED_MIN
      toggle.nudgeless = bool(toggle.lane_change_customizations and self.default_frogpilot_toggles.NudgelessLaneChange)
      toggle.one_lane_change = bool(toggle.lane_change_customizations and self.default_frogpilot_toggles.OneLaneChange)

      toggle.lateral_tuning = bool(self.default_frogpilot_toggles.LateralTune)
      toggle.nnff = bool(toggle.lateral_tuning and self.default_frogpilot_toggles.NNFF)
      toggle.nnff_lite = bool(toggle.lateral_tuning and self.default_frogpilot_toggles.NNFFLite)
      toggle.use_turn_desires = bool(toggle.lateral_tuning and self.default_frogpilot_toggles.TurnDesires)

      toggle.long_pitch = bool(openpilot_longitudinal and car_make == "gm" and self.default_frogpilot_toggles.LongPitch)

      toggle.human_acceleration = bool(toggle.longitudinal_tuning and self.default_frogpilot_toggles.HumanAcceleration)
      toggle.human_following = bool(toggle.longitudinal_tuning and self.default_frogpilot_toggles.HumanFollowing)
      toggle.lead_detection_probability = clip(float(self.default_frogpilot_toggles.LeadDetectionThreshold) / 100, 0.01, 0.99) if toggle.longitudinal_tuning else 0.5
      toggle.max_desired_acceleration = clip(float(self.default_frogpilot_toggles.MaxDesiredAcceleration), 0.1, 4.0) if toggle.longitudinal_tuning else 4.0
      toggle.taco_tune = bool(toggle.longitudinal_tuning and self.default_frogpilot_toggles.TacoTune)

      toggle.model = DEFAULT_CLASSIC_MODEL
      toggle.model_randomizer = self.default_frogpilot_toggles.ModelRandomizer
      toggle.part_model_param = ""
      toggle.classic_model = bool(classic_models and toggle.model in classic_models.split(','))
      toggle.navigationless_model = bool(navigation_models and toggle.model not in navigation_models.split(','))
      toggle.radarless_model = bool(radarless_models and toggle.model in radarless_models.split(','))

      toggle.model_ui = bool(self.default_frogpilot_toggles.ModelUI)
      toggle.dynamic_path_width = bool(toggle.model_ui and self.default_frogpilot_toggles.DynamicPathWidth)
      toggle.lane_line_width = int(self.default_frogpilot_toggles.LaneLinesWidth) * small_distance_conversion / 200 if toggle.model_ui else 0.025
      toggle.path_edge_width = int(self.default_frogpilot_toggles.PathEdgeWidth) if toggle.model_ui else 20
      toggle.path_width = float(self.default_frogpilot_toggles.PathWidth) * distance_conversion / 2 if toggle.model_ui else 0.9
      toggle.road_edge_width = int(self.default_frogpilot_toggles.RoadEdgesWidth) * small_distance_conversion / 200 if toggle.model_ui else 0.025
      toggle.show_stopping_point = bool(toggle.model_ui and self.default_frogpilot_toggles.ShowStoppingPoint)
      toggle.show_stopping_point_metrics = bool(toggle.show_stopping_point and self.default_frogpilot_toggles.ShowStoppingPointMetrics)
      toggle.unlimited_road_ui_length = bool(toggle.model_ui and self.default_frogpilot_toggles.UnlimitedLength)

      toggle.navigation_ui = bool(self.default_frogpilot_toggles.NavigationUI)
      toggle.big_map = bool(toggle.navigation_ui and params.get_bool("BigMap"))
      toggle.full_map = bool(toggle.big_map and self.default_frogpilot_toggles.FullMap)
      toggle.map_style = int(self.default_frogpilot_toggles.MapStyle) if toggle.navigation_ui else 0
      toggle.road_name_ui = bool(toggle.navigation_ui and self.default_frogpilot_toggles.RoadNameUI)
      toggle.show_speed_limit_offset = bool(toggle.navigation_ui and self.default_frogpilot_toggles.ShowSLCOffset)
      toggle.speed_limit_vienna = bool(toggle.navigation_ui and self.default_frogpilot_toggles.UseVienna)

      toggle.old_long_api = bool(openpilot_longitudinal and car_make == "gm" and not self.default_frogpilot_toggles.NewLongAPIGM)
      toggle.old_long_api |= bool(openpilot_longitudinal and car_make == "hyundai" and not self.default_frogpilot_toggles.NewLongAPI)

      toggle.quality_of_life_lateral = bool(self.default_frogpilot_toggles.QOLLateral)
      toggle.pause_lateral_below_speed = int(self.default_frogpilot_toggles.PauseLateralSpeed) * speed_conversion if toggle.quality_of_life_lateral else 0
      toggle.pause_lateral_below_signal = bool(toggle.pause_lateral_below_speed != 0 and self.default_frogpilot_toggles.PauseLateralOnSignal)

      toggle.quality_of_life_longitudinal = bool(self.default_frogpilot_toggles.QOLLongitudinal)
      toggle.custom_cruise_increase = int(self.default_frogpilot_toggles.CustomCruise) if toggle.quality_of_life_longitudinal and not pcm_cruise else 1
      toggle.custom_cruise_increase_long = int(self.default_frogpilot_toggles.CustomCruiseLong) if toggle.quality_of_life_longitudinal and not pcm_cruise else 5
      toggle.force_standstill = bool(toggle.quality_of_life_longitudinal and self.default_frogpilot_toggles.ForceStandstill)
      toggle.force_stops = bool(toggle.quality_of_life_longitudinal and self.default_frogpilot_toggles.ForceStops)
      toggle.increased_stopped_distance = int(self.default_frogpilot_toggles.IncreasedStoppedDistance) * distance_conversion if toggle.quality_of_life_longitudinal else 0
      toggle.map_gears = bool(toggle.quality_of_life_longitudinal and self.default_frogpilot_toggles.MapGears)
      toggle.map_acceleration = bool(toggle.map_gears and self.default_frogpilot_toggles.MapAcceleration)
      toggle.map_deceleration = bool(toggle.map_gears and self.default_frogpilot_toggles.MapDeceleration)
      toggle.reverse_cruise_increase = bool(toggle.quality_of_life_longitudinal and pcm_cruise and self.default_frogpilot_toggles.ReverseCruise)
      toggle.set_speed_offset = int(self.default_frogpilot_toggles.SetSpeedOffset) * (1 if toggle.is_metric else CV.MPH_TO_KPH) if toggle.quality_of_life_longitudinal and not pcm_cruise else 0

      toggle.camera_view = int(self.default_frogpilot_toggles.CameraView) if toggle.quality_of_life_visuals else 0
      toggle.driver_camera_in_reverse = bool(toggle.quality_of_life_visuals and self.default_frogpilot_toggles.DriverCamera)
      toggle.standby_mode = bool(toggle.quality_of_life_visuals and self.default_frogpilot_toggles.StandbyMode)
      toggle.stopped_timer = bool(toggle.quality_of_life_visuals and self.default_frogpilot_toggles.StoppedTimer)

      toggle.random_events = bool(self.default_frogpilot_toggles.RandomEvents)

      toggle.screen_management = bool(self.default_frogpilot_toggles.ScreenManagement)
      toggle.screen_brightness = int(self.default_frogpilot_toggles.ScreenBrightness) if toggle.screen_management else 101
      toggle.screen_brightness_onroad = int(self.default_frogpilot_toggles.ScreenBrightnessOnroad) if toggle.screen_management else 101
      toggle.screen_recorder = bool(toggle.screen_management and self.default_frogpilot_toggles.ScreenRecorder)
      toggle.screen_timeout = int(self.default_frogpilot_toggles.ScreenTimeout) if toggle.screen_management else 30
      toggle.screen_timeout_onroad = int(self.default_frogpilot_toggles.ScreenTimeoutOnroad) if toggle.screen_management else 10

      toggle.sng_hack = bool(openpilot_longitudinal and car_make == "toyota" and self.default_frogpilot_toggles.SNGHack)

      toggle.force_mph_dashboard = bool(toggle.speed_limit_controller and self.default_frogpilot_toggles.ForceMPHDashboard)
      toggle.map_speed_lookahead_higher = int(self.default_frogpilot_toggles.SLCLookaheadHigher) if toggle.speed_limit_controller else 0
      toggle.map_speed_lookahead_lower = int(self.default_frogpilot_toggles.SLCLookaheadLower) if toggle.speed_limit_controller else 0
      toggle.set_speed_limit = bool(toggle.speed_limit_controller and self.default_frogpilot_toggles.SetSpeedLimit)
      slc_fallback_method = int(self.default_frogpilot_toggles.SLCFallback) if toggle.speed_limit_controller else 0
      toggle.slc_fallback_experimental_mode = bool(toggle.speed_limit_controller and slc_fallback_method == 1)
      toggle.slc_fallback_previous_speed_limit = bool(toggle.speed_limit_controller and slc_fallback_method == 2)
      toggle.slc_fallback_set_speed = bool(toggle.speed_limit_controller and slc_fallback_method == 0)
      toggle.speed_limit_controller_override = int(self.default_frogpilot_toggles.SLCOverride) if toggle.speed_limit_controller else 0
      toggle.speed_limit_controller_override_manual = bool(toggle.speed_limit_controller_override == 1)
      toggle.speed_limit_controller_override_set_speed = bool(toggle.speed_limit_controller_override == 2)
      toggle.speed_limit_priority1 = self.default_frogpilot_toggles.SLCPriority1 if toggle.speed_limit_controller else None
      toggle.speed_limit_priority2 = self.default_frogpilot_toggles.SLCPriority2 if toggle.speed_limit_controller else None
      toggle.speed_limit_priority3 = self.default_frogpilot_toggles.SLCPriority3 if toggle.speed_limit_controller else None
      toggle.speed_limit_priority_highest = bool(toggle.speed_limit_priority1 == "Highest")
      toggle.speed_limit_priority_lowest = bool(toggle.speed_limit_priority1 == "Lowest")

      toggle.startup_alert_top = str(self.default_frogpilot_toggles.StartupMessageTop)
      toggle.startup_alert_bottom = str(self.default_frogpilot_toggles.StartupMessageBottom)

      toggle.volt_sng = bool(car_model == "CHEVROLET_VOLT" and self.default_frogpilot_toggles.VoltSNG)

    elif customization_level != 2:
      toggle.advanced_custom_onroad_ui = bool(self.default_frogpilot_toggles.AdvancedCustomUI)
      toggle.hide_alerts = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideAlerts)
      toggle.hide_lead_marker = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideLeadMarker)
      toggle.hide_map_icon = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideMapIcon)
      toggle.hide_max_speed = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideMaxSpeed)
      toggle.hide_speed = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideSpeed)
      toggle.hide_speed_limit = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.HideSpeedLimit)
      toggle.use_wheel_speed = bool(toggle.advanced_custom_onroad_ui and self.default_frogpilot_toggles.WheelSpeed)

      toggle.advanced_lateral_tuning = bool(self.default_frogpilot_toggles.AdvancedLateralTune)
      toggle.use_custom_steer_friction = False
      toggle.use_custom_kp = False
      toggle.use_custom_lat_accel_factor = False
      toggle.use_custom_steer_ratio = False
      toggle.force_auto_tune = bool(toggle.advanced_lateral_tuning and not has_auto_tune and not is_pid_car and self.default_frogpilot_toggles.ForceAutoTune)
      toggle.force_auto_tune_off = bool(toggle.advanced_lateral_tuning and has_auto_tune and not is_pid_car and self.default_frogpilot_toggles.ForceAutoTuneOff)

      toggle.alert_volume_control = bool(self.default_frogpilot_toggles.AlertVolumeControl)
      toggle.disengage_volume = int(self.default_frogpilot_toggles.DisengageVolume if toggle.alert_volume_control else 101)
      toggle.engage_volume = int(self.default_frogpilot_toggles.EngageVolume if toggle.alert_volume_control else 101)
      toggle.prompt_volume = int(self.default_frogpilot_toggles.PromptVolume if toggle.alert_volume_control else 101)
      toggle.promptDistracted_volume = int(self.default_frogpilot_toggles.PromptDistractedVolume if toggle.alert_volume_control else 101)
      toggle.refuse_volume = int(self.default_frogpilot_toggles.RefuseVolume if toggle.alert_volume_control else 101)
      toggle.warningSoft_volume = int(self.default_frogpilot_toggles.WarningSoftVolume if toggle.alert_volume_control else 101)
      toggle.warningImmediate_volume = int(self.default_frogpilot_toggles.WarningImmediateVolume if toggle.alert_volume_control else 101)

      toggle.always_on_lateral_status_bar = bool(toggle.always_on_lateral and not self.default_frogpilot_toggles.HideAOLStatusBar)

      toggle.cluster_offset = float(self.default_frogpilot_toggles.ClusterOffset if car_make == "toyota" else 1)

      toggle.conditional_navigation = bool(toggle.conditional_experimental_mode and self.default_frogpilot_toggles.CENavigation)
      toggle.conditional_navigation_intersections = bool(toggle.conditional_navigation and self.default_frogpilot_toggles.CENavigationIntersections)
      toggle.conditional_navigation_lead = bool(toggle.conditional_navigation and self.default_frogpilot_toggles.CENavigationLead)
      toggle.conditional_navigation_turns = bool(toggle.conditional_navigation and self.default_frogpilot_toggles.CENavigationTurns)
      toggle.conditional_signal = int(self.default_frogpilot_toggles.CESignalSpeed if toggle.conditional_experimental_mode else 0)
      toggle.conditional_signal_lane_detection = bool(toggle.conditional_signal != 0 and self.default_frogpilot_toggles.CESignalLaneDetection)
      toggle.conditional_status_bar = bool(toggle.conditional_experimental_mode and not self.default_frogpilot_toggles.HideCEMStatusBar)
      if toggle.conditional_experimental_mode:
        params.put_bool("ExperimentalMode", True)

      toggle.crosstrek_torque = bool(car_model == "SUBARU_IMPREZA" and self.default_frogpilot_toggles.CrosstrekTorque)

      toggle.curve_sensitivity = int(self.default_frogpilot_toggles.CurveSensitivity) / 100 if toggle.curve_speed_controller else 1
      toggle.turn_aggressiveness = int(self.default_frogpilot_toggles.TurnAggressiveness) / 100 if toggle.curve_speed_controller else 1
      toggle.mtsc_curvature_check = bool(toggle.map_turn_speed_controller and self.default_frogpilot_toggles.MTSCCurvatureCheck)

      toggle.custom_personalities = bool(openpilot_longitudinal and self.default_frogpilot_toggles.CustomPersonalities)
      toggle.aggressive_profile = bool(toggle.custom_personalities and self.default_frogpilot_toggles.AggressivePersonalityProfile)
      toggle.aggressive_jerk_deceleration = int(self.default_frogpilot_toggles.AggressiveJerkDeceleration) / 100 if toggle.aggressive_profile else 0.5
      toggle.aggressive_jerk_danger = int(self.default_frogpilot_toggles.AggressiveJerkDanger) / 100 if toggle.aggressive_profile else 1.0
      toggle.aggressive_jerk_speed = int(self.default_frogpilot_toggles.AggressiveJerkSpeed) / 100 if toggle.aggressive_profile else 0.5
      toggle.aggressive_jerk_speed_decrease = int(self.default_frogpilot_toggles.AggressiveJerkSpeedDecrease) / 100 if toggle.aggressive_profile else 0.5
      toggle.aggressive_follow = float(self.default_frogpilot_toggles.AggressiveFollow) if toggle.aggressive_profile else 1.25
      toggle.standard_profile = bool(toggle.custom_personalities and self.default_frogpilot_toggles.StandardPersonalityProfile)
      toggle.standard_jerk_acceleration = int(self.default_frogpilot_toggles.StandardJerkAcceleration) / 100 if toggle.standard_profile else 1.0
      toggle.standard_jerk_deceleration = int(self.default_frogpilot_toggles.StandardJerkDeceleration) / 100 if toggle.standard_profile else 1.0
      toggle.standard_jerk_danger = int(self.default_frogpilot_toggles.StandardJerkDanger) / 100 if toggle.standard_profile else 1.0
      toggle.standard_jerk_speed = int(self.default_frogpilot_toggles.StandardJerkSpeed) / 100 if toggle.standard_profile else 1.0
      toggle.standard_jerk_speed_decrease = int(self.default_frogpilot_toggles.StandardJerkSpeedDecrease) / 100 if toggle.standard_profile else 1.0
      toggle.standard_follow = float(self.default_frogpilot_toggles.StandardFollow) if toggle.standard_profile else 1.45
      toggle.relaxed_profile = bool(toggle.custom_personalities and self.default_frogpilot_toggles.RelaxedPersonalityProfile)
      toggle.relaxed_jerk_acceleration = int(self.default_frogpilot_toggles.RelaxedJerkAcceleration) / 100 if toggle.relaxed_profile else 1.0
      toggle.relaxed_jerk_deceleration = int(self.default_frogpilot_toggles.RelaxedJerkDeceleration) / 100 if toggle.relaxed_profile else 1.0
      toggle.relaxed_jerk_danger = int(self.default_frogpilot_toggles.RelaxedJerkDanger) / 100 if toggle.relaxed_profile else 1.0
      toggle.relaxed_jerk_speed = int(self.default_frogpilot_toggles.RelaxedJerkSpeed) / 100 if toggle.relaxed_profile else 1.0
      toggle.relaxed_jerk_speed_decrease = int(self.default_frogpilot_toggles.RelaxedJerkSpeedDecrease) / 100 if toggle.relaxed_profile else 1.0
      toggle.relaxed_follow = float(self.default_frogpilot_toggles.RelaxedFollow) if toggle.relaxed_profile else 1.75
      toggle.traffic_profile = bool(toggle.custom_personalities and self.default_frogpilot_toggles.TrafficPersonalityProfile)
      toggle.traffic_mode_jerk_acceleration = [int(self.default_frogpilot_toggles.TrafficJerkAcceleration) / 100, toggle.aggressive_jerk_acceleration] if toggle.traffic_profile else [0.5, 0.5]
      toggle.traffic_mode_jerk_deceleration = [int(self.default_frogpilot_toggles.TrafficJerkDeceleration) / 100, toggle.aggressive_jerk_deceleration] if toggle.traffic_profile else [0.5, 0.5]
      toggle.traffic_mode_jerk_danger = [int(self.default_frogpilot_toggles.TrafficJerkDanger) / 100, toggle.aggressive_jerk_danger] if toggle.traffic_profile else [1.0, 1.0]
      toggle.traffic_mode_jerk_speed = [int(self.default_frogpilot_toggles.TrafficJerkSpeed) / 100, toggle.aggressive_jerk_speed] if toggle.traffic_profile else [0.5, 0.5]
      toggle.traffic_mode_jerk_speed_decrease = [int(self.default_frogpilot_toggles.TrafficJerkSpeedDecrease) / 100, toggle.aggressive_jerk_speed_decrease] if toggle.traffic_profile else [0.5, 0.5]
      toggle.traffic_mode_t_follow = [float(self.default_frogpilot_toggles.TrafficFollow), toggle.aggressive_follow] if toggle.traffic_profile else [0.5, 1.0]

      toggle.adjacent_paths = bool(toggle.custom_ui and self.default_frogpilot_toggles.AdjacentPath)

      toggle.developer_ui = bool(self.default_frogpilot_toggles.DeveloperUI)
      toggle.border_metrics = bool(toggle.developer_ui and self.default_frogpilot_toggles.BorderMetrics)
      toggle.blind_spot_metrics = bool(has_bsm and toggle.border_metrics and self.default_frogpilot_toggles.BlindSpotMetrics)
      toggle.signal_metrics = bool(toggle.border_metrics and self.default_frogpilot_toggles.SignalMetrics)
      toggle.steering_metrics = bool(toggle.border_metrics and self.default_frogpilot_toggles.ShowSteering)
      toggle.show_fps = bool(toggle.developer_ui and self.default_frogpilot_toggles.FPSCounter)
      toggle.lateral_metrics = bool(toggle.developer_ui and self.default_frogpilot_toggles.LateralMetrics)
      toggle.adjacent_path_metrics = bool(toggle.lateral_metrics and self.default_frogpilot_toggles.AdjacentPathMetrics)
      toggle.lateral_tuning_metrics = bool(toggle.lateral_metrics and self.default_frogpilot_toggles.TuningInfo)
      toggle.longitudinal_metrics = bool(toggle.developer_ui and self.default_frogpilot_toggles.LongitudinalMetrics)
      toggle.adjacent_lead_tracking = bool(has_radar and toggle.longitudinal_metrics and self.default_frogpilot_toggles.AdjacentLeadsUI)
      toggle.lead_metrics = bool(toggle.longitudinal_metrics and self.default_frogpilot_toggles.LeadInfo)
      toggle.jerk_metrics = bool(toggle.longitudinal_metrics and self.default_frogpilot_toggles.JerkInfo)
      toggle.numerical_temp = bool(toggle.developer_ui and self.default_frogpilot_toggles.NumericalTemp)
      toggle.fahrenheit = bool(toggle.numerical_temp and self.default_frogpilot_toggles.Fahrenheit)
      toggle.sidebar_metrics = bool(toggle.developer_ui and self.default_frogpilot_toggles.SidebarMetrics)
      toggle.cpu_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowCPU)
      toggle.gpu_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowGPU)
      toggle.ip_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowIP)
      toggle.memory_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowMemoryUsage)
      toggle.storage_left_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowStorageLeft)
      toggle.storage_used_metrics = bool(toggle.sidebar_metrics and self.default_frogpilot_toggles.ShowStorageUsed)
      toggle.use_si_metrics = bool(toggle.developer_ui and self.default_frogpilot_toggles.UseSI)

      toggle.device_management = bool(self.default_frogpilot_toggles.DeviceManagement)
      toggle.increase_thermal_limits = bool(toggle.device_management and self.default_frogpilot_toggles.IncreaseThermalLimits)
      toggle.low_voltage_shutdown = clip(float(self.default_frogpilot_toggles.LowVoltageShutdown), VBATT_PAUSE_CHARGING, 12.5) if toggle.device_management else VBATT_PAUSE_CHARGING
      toggle.no_logging = bool(toggle.device_management and self.default_frogpilot_toggles.NoLogging or self.development_branch)
      toggle.no_uploads = bool(toggle.device_management and self.default_frogpilot_toggles.NoUploads or self.development_branch)
      toggle.offline_mode = bool(toggle.device_management and self.default_frogpilot_toggles.OfflineMode)

      toggle.experimental_gm_tune = bool(openpilot_longitudinal and car_make == "gm" and self.default_frogpilot_toggles.ExperimentalGMTune)

      toggle.frogsgomoo_tweak = bool(openpilot_longitudinal and car_make == "toyota" and self.default_frogpilot_toggles.FrogsGoMoosTweak)

      toggle.lane_detection_width = float(self.default_frogpilot_toggles.LaneDetectionWidth) * distance_conversion if toggle.lane_change_customizations else 0
      toggle.lane_detection = bool(toggle.lane_detection_width != 0)
      toggle.minimum_lane_change_speed = float(self.default_frogpilot_toggles.MinimumLaneChangeSpeed) * speed_conversion if toggle.lane_change_customizations else LANE_CHANGE_SPEED_MIN

      toggle.lateral_tuning = bool(self.default_frogpilot_toggles.LateralTune)
      toggle.nnff_lite = bool(toggle.lateral_tuning and self.default_frogpilot_toggles.NNFFLite)
      toggle.use_turn_desires = bool(toggle.lateral_tuning and self.default_frogpilot_toggles.TurnDesires)

      toggle.long_pitch = bool(openpilot_longitudinal and car_make == "gm" and self.default_frogpilot_toggles.LongPitch)

      toggle.lead_detection_probability = clip(float(self.default_frogpilot_toggles.LeadDetectionThreshold) / 100, 0.01, 0.99) if toggle.longitudinal_tuning else 0.5
      toggle.max_desired_acceleration = clip(float(self.default_frogpilot_toggles.MaxDesiredAcceleration), 0.1, 4.0) if toggle.longitudinal_tuning else 4.0
      toggle.taco_tune = bool(toggle.longitudinal_tuning and self.default_frogpilot_toggles.TacoTune)

      toggle.model = DEFAULT_CLASSIC_MODEL
      toggle.model_randomizer = self.default_frogpilot_toggles.ModelRandomizer
      toggle.part_model_param = ""
      toggle.classic_model = bool(classic_models and toggle.model in classic_models.split(','))
      toggle.navigationless_model = bool(navigation_models and toggle.model not in navigation_models.split(','))
      toggle.radarless_model = bool(radarless_models and toggle.model in radarless_models.split(','))

      toggle.model_ui = bool(self.default_frogpilot_toggles.ModelUI)
      toggle.dynamic_path_width = bool(toggle.model_ui and self.default_frogpilot_toggles.DynamicPathWidth)
      toggle.lane_line_width = int(self.default_frogpilot_toggles.LaneLinesWidth) * small_distance_conversion / 200 if toggle.model_ui else 0.025
      toggle.path_edge_width = int(self.default_frogpilot_toggles.PathEdgeWidth) if toggle.model_ui else 20
      toggle.path_width = float(self.default_frogpilot_toggles.PathWidth) * distance_conversion / 2 if toggle.model_ui else 0.9
      toggle.road_edge_width = int(self.default_frogpilot_toggles.RoadEdgesWidth) * small_distance_conversion / 200 if toggle.model_ui else 0.025
      toggle.show_stopping_point = bool(toggle.model_ui and self.default_frogpilot_toggles.ShowStoppingPoint)
      toggle.show_stopping_point_metrics = bool(toggle.show_stopping_point and self.default_frogpilot_toggles.ShowStoppingPointMetrics)
      toggle.unlimited_road_ui_length = bool(toggle.model_ui and self.default_frogpilot_toggles.UnlimitedLength)

      toggle.map_style = int(self.default_frogpilot_toggles.MapStyle) if toggle.navigation_ui else 0
      toggle.road_name_ui = bool(toggle.navigation_ui and self.default_frogpilot_toggles.RoadNameUI)
      toggle.show_speed_limit_offset = bool(toggle.navigation_ui and self.default_frogpilot_toggles.ShowSLCOffset)
      toggle.speed_limit_vienna = bool(toggle.navigation_ui and self.default_frogpilot_toggles.UseVienna)

      toggle.old_long_api = bool(openpilot_longitudinal and car_make == "gm" and not self.default_frogpilot_toggles.NewLongAPIGM)
      toggle.old_long_api |= bool(openpilot_longitudinal and car_make == "hyundai" and not self.default_frogpilot_toggles.NewLongAPI)

      toggle.quality_of_life_lateral = bool(self.default_frogpilot_toggles.QOLLateral)
      toggle.pause_lateral_below_speed = int(self.default_frogpilot_toggles.PauseLateralSpeed) * speed_conversion if toggle.quality_of_life_lateral else 0
      toggle.pause_lateral_below_signal = bool(toggle.pause_lateral_below_speed != 0 and self.default_frogpilot_toggles.PauseLateralOnSignal)

      toggle.custom_cruise_increase = int(self.default_frogpilot_toggles.CustomCruise) if toggle.quality_of_life_longitudinal and not pcm_cruise else 1
      toggle.custom_cruise_increase_long = int(self.default_frogpilot_toggles.CustomCruiseLong) if toggle.quality_of_life_longitudinal and not pcm_cruise else 5
      toggle.force_standstill = bool(toggle.quality_of_life_longitudinal and self.default_frogpilot_toggles.ForceStandstill)
      toggle.force_stops = bool(toggle.quality_of_life_longitudinal and self.default_frogpilot_toggles.ForceStops)
      toggle.reverse_cruise_increase = bool(toggle.quality_of_life_longitudinal and pcm_cruise and self.default_frogpilot_toggles.ReverseCruise)
      toggle.set_speed_offset = int(self.default_frogpilot_toggles.SetSpeedOffset) * (1 if toggle.is_metric else CV.MPH_TO_KPH) if toggle.quality_of_life_longitudinal and not pcm_cruise else 0

      toggle.camera_view = int(self.default_frogpilot_toggles.CameraView) if toggle.quality_of_life_visuals else 0
      toggle.standby_mode = bool(toggle.quality_of_life_visuals and self.default_frogpilot_toggles.StandbyMode)

      toggle.screen_management = bool(self.default_frogpilot_toggles.ScreenManagement)
      toggle.screen_brightness = int(self.default_frogpilot_toggles.ScreenBrightness) if toggle.screen_management else 101
      toggle.screen_brightness_onroad = int(self.default_frogpilot_toggles.ScreenBrightnessOnroad) if toggle.screen_management else 101
      toggle.screen_recorder = bool(toggle.screen_management and self.default_frogpilot_toggles.ScreenRecorder)
      toggle.screen_timeout = int(self.default_frogpilot_toggles.ScreenTimeout) if toggle.screen_management else 30
      toggle.screen_timeout_onroad = int(self.default_frogpilot_toggles.ScreenTimeoutOnroad) if toggle.screen_management else 10

      toggle.sng_hack = bool(openpilot_longitudinal and car_make == "toyota" and self.default_frogpilot_toggles.SNGHack)

      toggle.force_mph_dashboard = bool(toggle.speed_limit_controller and self.default_frogpilot_toggles.ForceMPHDashboard)
      toggle.map_speed_lookahead_higher = int(self.default_frogpilot_toggles.SLCLookaheadHigher) if toggle.speed_limit_controller else 0
      toggle.map_speed_lookahead_lower = int(self.default_frogpilot_toggles.SLCLookaheadLower) if toggle.speed_limit_controller else 0
      toggle.set_speed_limit = bool(toggle.speed_limit_controller and self.default_frogpilot_toggles.SetSpeedLimit)
      toggle.speed_limit_priority1 = self.default_frogpilot_toggles.SLCPriority1 if toggle.speed_limit_controller else None
      toggle.speed_limit_priority2 = self.default_frogpilot_toggles.SLCPriority2 if toggle.speed_limit_controller else None
      toggle.speed_limit_priority3 = self.default_frogpilot_toggles.SLCPriority3 if toggle.speed_limit_controller else None
      toggle.speed_limit_priority_highest = bool(toggle.speed_limit_priority1 == "Highest")
      toggle.speed_limit_priority_lowest = bool(toggle.speed_limit_priority1 == "Lowest")

      toggle.startup_alert_top = str(self.default_frogpilot_toggles.StartupMessageTop)
      toggle.startup_alert_bottom = str(self.default_frogpilot_toggles.StartupMessageBottom)

      toggle.volt_sng = bool(car_model == "CHEVROLET_VOLT" and self.default_frogpilot_toggles.VoltSNG)

    params.put("FrogPilotToggles", json.dumps(toggle.__dict__))
    params_memory.remove("FrogPilotTogglesUpdated")
