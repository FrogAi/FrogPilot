import json
import math
import random

from pathlib import Path
from types import SimpleNamespace

from cereal import car
from openpilot.common.basedir import BASEDIR
from openpilot.common.conversions import Conversions as CV
from openpilot.common.numpy_fast import clip, interp
from openpilot.common.params import Params, UnknownKeyName
from openpilot.selfdrive.controls.lib.desire_helper import LANE_CHANGE_SPEED_MIN
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.system.hardware.power_monitoring import VBATT_PAUSE_CHARGING
from panda import ALTERNATIVE_EXPERIENCE

params = Params()
params_default = Params("/data/params_default")
params_memory = Params("/dev/shm/params")

GearShifter = car.CarState.GearShifter
NON_DRIVING_GEARS = [GearShifter.neutral, GearShifter.park, GearShifter.reverse, GearShifter.unknown]

CITY_SPEED_LIMIT = 25                                   # 55mph is typically the minimum speed for highways
CRUISING_SPEED = 5                                      # Roughly the speed cars go when not touching the gas while in drive
MODEL_LENGTH = ModelConstants.IDX_N                     # Minimum length of the model
PLANNER_TIME = ModelConstants.T_IDXS[MODEL_LENGTH - 1]  # Length of time the model projects out for
THRESHOLD = 0.6                                         # 60% chance of condition being true
TO_RADIANS = math.pi / 180                              # Conversion factor from degrees to radians

ACTIVE_THEME_PATH = Path(BASEDIR) / "selfdrive" / "frogpilot" / "assets" / "active_theme"
MODELS_PATH = Path("/data") / "models"
RANDOM_EVENTS_PATH = Path(BASEDIR) / "selfdrive" / "frogpilot" / "assets" / "random_events"
THEME_SAVE_PATH = Path("/data") / "themes"

DEFAULT_MODEL = "national-public-radio"
DEFAULT_MODEL_NAME = "National Public Radio 👀📡"

DEFAULT_CLASSIC_MODEL = "wd-40"
DEFAULT_CLASSIC_MODEL_NAME = "WD-40 (Default) 👀📡"

def get_frogpilot_toggles():
  return SimpleNamespace(**json.loads(params_memory.get("FrogPilotToggles", block=True)))

def has_prime():
  return params.get_int("PrimeType") > 0

def update_frogpilot_toggles():
  params_memory.put_bool("FrogPilotTogglesUpdated", True)

frogpilot_default_params: list[tuple[str, str | bytes, int]] = [
  ("AccelerationPath", "1", 2),
  ("AccelerationProfile", "2", 0),
  ("AdjacentLeadsUI", "0", 3),
  ("AdjacentPath", "0", 3),
  ("AdjacentPathMetrics", "0", 3),
  ("AdvancedCustomUI", "0", 2),
  ("AdvancedLateralTune", "0", 3),
  ("AggressiveFollow", "1.25", 2),
  ("AggressiveJerkAcceleration", "50", 3),
  ("AggressiveJerkDanger", "100", 3),
  ("AggressiveJerkDeceleration", "50", 3),
  ("AggressiveJerkSpeed", "50", 3),
  ("AggressiveJerkSpeedDecrease", "50", 3),
  ("AggressivePersonalityProfile", "1", 2),
  ("AlertVolumeControl", "0", 2),
  ("AlwaysOnLateral", "1", 0),
  ("AlwaysOnLateralLKAS", "0", 0),
  ("AlwaysOnLateralMain", "1", 1),
  ("AMapKey1", "", 0),
  ("AMapKey2", "", 0),
  ("AutomaticallyUpdateModels", "1", 1),
  ("AutomaticUpdates", "1", 0),
  ("AvailableModels", "", 1),
  ("AvailableModelNames", "", 1),
  ("BigMap", "0", 2),
  ("BlacklistedModels", "", 2),
  ("BlindSpotMetrics", "1", 3),
  ("BlindSpotPath", "1", 0),
  ("BorderMetrics", "0", 3),
  ("CameraView", "3", 2),
  ("CarMake", "", 0),
  ("CarModel", "", 0),
  ("CarModelName", "", 0),
  ("CECurves", "1", 1),
  ("CECurvesLead", "0", 1),
  ("CELead", "0", 1),
  ("CEModelStopTime", "8", 2),
  ("CENavigation", "1", 2),
  ("CENavigationIntersections", "0", 2),
  ("CENavigationLead", "1", 2),
  ("CENavigationTurns", "1", 2),
  ("CESignalSpeed", "55", 2),
  ("CESignalLaneDetection", "1", 2),
  ("CESlowerLead", "0", 1),
  ("CESpeed", "0", 1),
  ("CESpeedLead", "0", 1),
  ("CEStoppedLead", "0", 1),
  ("ClassicModels", "", 1),
  ("ClippedCurvatureModels", "", 1),
  ("ClusterOffset", "1.015", 2),
  ("Compass", "0", 1),
  ("ConditionalExperimental", "1", 0),
  ("CrosstrekTorque", "1", 3),
  ("CurveSensitivity", "100", 2),
  ("CurveSpeedControl", "0", 1),
  ("CustomAlerts", "1", 0),
  ("CustomColors", "frog", 0),
  ("CustomCruise", "1", 2),
  ("CustomCruiseLong", "5", 2),
  ("CustomDistanceIcons", "stock", 0),
  ("CustomIcons", "frog-animated", 0),
  ("CustomPersonalities", "0", 2),
  ("CustomSignals", "frog", 0),
  ("CustomSounds", "frog", 0),
  ("CustomUI", "1", 0),
  ("DecelerationProfile", "1", 2),
  ("DesiredCurvatureModels", "", 1),
  ("DeveloperUI", "0", 3),
  ("DeviceManagement", "1", 1),
  ("DeviceShutdown", "9", 1),
  ("DisableOnroadUploads", "0", 2),
  ("DisableOpenpilotLongitudinal", "0", 2),
  ("DisengageVolume", "101", 2),
  ("DriverCamera", "0", 1),
  ("DynamicPathWidth", "0", 2),
  ("DynamicPedalsOnUI", "1", 2),
  ("EngageVolume", "101", 2),
  ("ExperimentalGMTune", "0", 2),
  ("ExperimentalModeActivation", "1", 1),
  ("ExperimentalModels", "", 1),
  ("ExperimentalModeViaDistance", "1", 1),
  ("ExperimentalModeViaLKAS", "1", 1),
  ("ExperimentalModeViaTap", "0", 1),
  ("Fahrenheit", "0", 3),
  ("ForceAutoTune", "0", 2),
  ("ForceAutoTuneOff", "0", 2),
  ("ForceFingerprint", "0", 2),
  ("ForceMPHDashboard", "0", 2),
  ("ForceStandstill", "0", 2),
  ("ForceStops", "0", 2),
  ("FPSCounter", "1", 3),
  ("FrogsGoMoosTweak", "1", 2),
  ("FullMap", "0", 2),
  ("GasRegenCmd", "1", 2),
  ("GMapKey", "", 0),
  ("GoatScream", "0", 2),
  ("GreenLightAlert", "0", 0),
  ("HideAlerts", "0", 2),
  ("HideAOLStatusBar", "0", 2),
  ("HideCEMStatusBar", "0", 2),
  ("HideCSCUI", "0", 2),
  ("HideLeadMarker", "0", 2),
  ("HideMapIcon", "0", 2),
  ("HideMaxSpeed", "0", 2),
  ("HideSpeed", "0", 2),
  ("HideSpeedLimit", "0", 2),
  ("HolidayThemes", "1", 0),
  ("HumanAcceleration", "1", 2),
  ("HumanFollowing", "1", 2),
  ("IncreasedStoppedDistance", "0", 2),
  ("IncreaseThermalLimits", "0", 3),
  ("JerkInfo", "0", 3),
  ("LaneChangeCustomizations", "0", 0),
  ("LaneChangeTime", "2.0", 0),
  ("LaneDetectionWidth", "0", 2),
  ("LaneLinesWidth", "4", 2),
  ("LateralMetrics", "1", 3),
  ("LateralTune", "1", 2),
  ("LeadDepartingAlert", "0", 0),
  ("LeadDetectionThreshold", "35", 3),
  ("LeadInfo", "1", 3),
  ("LockDoors", "1", 0),
  ("LongitudinalMetrics", "1", 3),
  ("LongitudinalTune", "1", 0),
  ("LongPitch", "1", 2),
  ("LoudBlindspotAlert", "0", 0),
  ("LowVoltageShutdown", str(VBATT_PAUSE_CHARGING), 3),
  ("MapAcceleration", "0", 2),
  ("MapDeceleration", "0", 2),
  ("MapGears", "0", 2),
  ("MapboxPublicKey", "", 0),
  ("MapboxSecretKey", "", 0),
  ("MapsSelected", "", 0),
  ("MapStyle", "0", 2),
  ("MaxDesiredAcceleration", "4.0", 3),
  ("MinimumLaneChangeSpeed", str(LANE_CHANGE_SPEED_MIN / CV.MPH_TO_MS), 2),
  ("Model", DEFAULT_CLASSIC_MODEL, 1),
  ("ModelDrivesAndScores", "", 2),
  ("ModelName", DEFAULT_CLASSIC_MODEL_NAME, 1),
  ("ModelRandomizer", "0", 2),
  ("ModelUI", "1", 2),
  ("MTSCCurvatureCheck", "1", 2),
  ("MTSCEnabled", "1", 1),
  ("NavigationModels", "", 1),
  ("NavigationUI", "1", 2),
  ("NewLongAPI", "0", 2),
  ("NewLongAPIGM", "1", 2),
  ("NNFF", "1", 2),
  ("NNFFLite", "1", 2),
  ("NoLogging", "0", 2),
  ("NoUploads", "0", 2),
  ("NudgelessLaneChange", "0", 0),
  ("NumericalTemp", "1", 3),
  ("OfflineMode", "0", 2),
  ("Offset1", "5", 0),
  ("Offset2", "5", 0),
  ("Offset3", "5", 0),
  ("Offset4", "10", 0),
  ("OneLaneChange", "1", 2),
  ("OnroadDistanceButton", "0", 0),
  ("openpilotMinutes", "0", 0),
  ("PathEdgeWidth", "20", 2),
  ("PathWidth", "6.1", 2),
  ("PauseAOLOnBrake", "0", 2),
  ("PauseLateralOnSignal", "0", 2),
  ("PauseLateralSpeed", "0", 2),
  ("PedalsOnUI", "0", 2),
  ("PersonalizeOpenpilot", "1", 0),
  ("PreferredSchedule", "2", 0),
  ("PromptDistractedVolume", "101", 2),
  ("PromptVolume", "101", 2),
  ("QOLLateral", "1", 2),
  ("QOLLongitudinal", "1", 2),
  ("QOLVisuals", "1", 0),
  ("RadarlessModels", "", 1),
  ("RainbowPath", "0", 1),
  ("RandomEvents", "0", 1),
  ("RefuseVolume", "101", 2),
  ("RelaxedFollow", "1.75", 2),
  ("RelaxedJerkAcceleration", "100", 3),
  ("RelaxedJerkDanger", "100", 3),
  ("RelaxedJerkDeceleration", "100", 3),
  ("RelaxedJerkSpeed", "100", 3),
  ("RelaxedJerkSpeedDecrease", "100", 3),
  ("RelaxedPersonalityProfile", "1", 2),
  ("ReverseCruise", "0", 2),
  ("RoadEdgesWidth", "2", 2),
  ("RoadNameUI", "1", 2),
  ("RotatingWheel", "1", 1),
  ("ScreenBrightness", "101", 2),
  ("ScreenBrightnessOnroad", "101", 2),
  ("ScreenManagement", "1", 2),
  ("ScreenRecorder", "1", 2),
  ("ScreenTimeout", "30", 2),
  ("ScreenTimeoutOnroad", "30", 2),
  ("SearchInput", "0", 0),
  ("SetSpeedLimit", "0", 2),
  ("SetSpeedOffset", "0", 2),
  ("ShowCPU", "1", 3),
  ("ShowGPU", "0", 3),
  ("ShowIP", "0", 3),
  ("ShowMemoryUsage", "1", 3),
  ("ShowSLCOffset", "1", 2),
  ("ShowSpeedLimits", "1", 0),
  ("ShowSteering", "0", 3),
  ("ShowStoppingPoint", "0", 3),
  ("ShowStoppingPointMetrics", "0", 3),
  ("ShowStorageLeft", "0", 3),
  ("ShowStorageUsed", "0", 3),
  ("Sidebar", "0", 0),
  ("SidebarMetrics", "1", 3),
  ("SignalMetrics", "0", 2),
  ("SLCConfirmation", "0", 0),
  ("SLCConfirmationHigher", "0", 0),
  ("SLCConfirmationLower", "0", 0),
  ("SLCFallback", "2", 2),
  ("SLCLookaheadHigher", "5", 2),
  ("SLCLookaheadLower", "5", 2),
  ("SLCOverride", "1", 2),
  ("SLCPriority1", "Navigation", 2),
  ("SLCPriority2", "Map Data", 2),
  ("SLCPriority3", "Dashboard", 2),
  ("SNGHack", "1", 2),
  ("SpeedLimitChangedAlert", "1", 0),
  ("SpeedLimitController", "1", 0),
  ("SpeedLimitSources", "0", 3),
  ("StartupMessageBottom", "Human-tested, frog-approved 🐸", 0),
  ("StartupMessageTop", "Hop in and buckle up!", 0),
  ("StandardFollow", "1.45", 2),
  ("StandardJerkAcceleration", "100", 3),
  ("StandardJerkDanger", "100", 3),
  ("StandardJerkDeceleration", "100", 3),
  ("StandardJerkSpeed", "100", 3),
  ("StandardJerkSpeedDecrease", "100", 3),
  ("StandardPersonalityProfile", "1", 2),
  ("StandbyMode", "0", 2),
  ("StaticPedalsOnUI", "0", 2),
  ("SteerFriction", "0", 3),
  ("SteerFrictionStock", "0", 3),
  ("SteerKP", "0", 3),
  ("SteerKPStock", "0", 3),
  ("SteerLatAccel", "0", 3),
  ("SteerLatAccelStock", "0", 3),
  ("SteerRatio", "0", 3),
  ("SteerRatioStock", "0", 3),
  ("StoppedTimer", "0", 1),
  ("TacoTune", "0", 2),
  ("ToyotaDoors", "1", 0),
  ("TrafficFollow", "0.5", 2),
  ("TrafficJerkAcceleration", "50", 3),
  ("TrafficJerkDanger", "100", 3),
  ("TrafficJerkDeceleration", "50", 3),
  ("TrafficJerkSpeed", "50", 3),
  ("TrafficJerkSpeedDecrease", "50", 3),
  ("TrafficPersonalityProfile", "1", 2),
  ("TuningInfo", "0", 3),
  ("TuningLevel", "0", 0),
  ("TuningLevelConfirmed", "0", 0),
  ("TurnAggressiveness", "100", 2),
  ("TurnDesires", "0", 2),
  ("UnlimitedLength", "1", 2),
  ("UnlockDoors", "1", 0),
  ("UseSI", "1", 3),
  ("UseVienna", "0", 2),
  ("VisionTurnControl", "1", 1),
  ("VoltSNG", "0", 2),
  ("WarningImmediateVolume", "101", 2),
  ("WarningSoftVolume", "101", 2),
  ("WheelIcon", "frog", 0),
  ("WheelSpeed", "0", 2)
]

misc_tuning_levels: list[tuple[str, str | bytes, int]] = [
  ("SLCPriority", "", 2),
  ("SLCQOL", "", 2),
  ("StartupAlert", "", 0)
]

class FrogPilotVariables:
  def __init__(self):
    self.frogpilot_toggles = SimpleNamespace()
    self.tuning_levels = {key: lvl for key, _, lvl in frogpilot_default_params + misc_tuning_levels}

    self.development_branch = params.get("GitBranch", encoding='utf-8') == "FrogPilot-Development"

    self.frogpilot_toggles.frogs_go_moo = Path("/persist/frogsgomoo.py").is_file()
    self.frogpilot_toggles.block_user = self.development_branch and not self.frogpilot_toggles.frogs_go_moo

    for k, v, _ in frogpilot_default_params:
      params_default.put(k, v)

    params_memory.put("FrogPilotTuningLevels", json.dumps(self.tuning_levels))

  def update(self, holiday_theme, started):
    openpilot_installed = params.get_bool("HasAcceptedTerms")

    key = "CarParams" if started else "CarParamsPersistent"
    msg_bytes = params.get(key, block=openpilot_installed and started)

    if msg_bytes:
      with car.CarParams.from_bytes(msg_bytes) as CP:
        always_on_lateral_set = CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL
        car_make = CP.carName
        car_model = CP.carFingerprint
        has_auto_tune = car_make in {"hyundai", "toyota"} and CP.lateralTuning.which == "torque"
        has_bsm = CP.enableBsm
        has_radar = not CP.radarUnavailable
        is_pid_car = CP.lateralTuning.which == "pid"
        max_acceleration_enabled = CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.RAISE_LONGITUDINAL_LIMITS_TO_ISO_MAX
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

    tuning_level = params.get_int("TuningLevel") if params.get_bool("TuningLevelConfirmed") else 3

    default = params_default
    level = self.tuning_levels
    toggle = self.frogpilot_toggles

    toggle.is_metric = params.get_bool("IsMetric")
    distance_conversion = 1 if toggle.is_metric else CV.FOOT_TO_METER
    small_distance_conversion = 1 if toggle.is_metric else CV.INCH_TO_CM
    speed_conversion = CV.KPH_TO_MS if toggle.is_metric else CV.MPH_TO_MS

    advanced_custom_ui = params.get_bool("AdvancedCustomUI") if tuning_level >= level["AdvancedCustomUI"] else default.get_bool("AdvancedCustomUI")
    toggle.hide_alerts = advanced_custom_ui and (params.get_bool("HideAlerts") if tuning_level >= level["HideAlerts"] else default.get_bool("HideAlerts"))
    toggle.hide_lead_marker = advanced_custom_ui and (params.get_bool("HideLeadMarker") if tuning_level >= level["HideLeadMarker"] else default.get_bool("HideLeadMarker"))
    toggle.hide_map_icon = advanced_custom_ui and (params.get_bool("HideMapIcon") if tuning_level >= level["HideMapIcon"] else default.get_bool("HideMapIcon"))
    toggle.hide_max_speed = advanced_custom_ui and (params.get_bool("HideMaxSpeed") if tuning_level >= level["HideMaxSpeed"] else default.get_bool("HideMaxSpeed"))
    toggle.hide_speed = advanced_custom_ui and (params.get_bool("HideSpeed") if tuning_level >= level["HideSpeed"] else default.get_bool("HideSpeed"))
    toggle.hide_speed_limit = advanced_custom_ui and (params.get_bool("HideSpeedLimit") if tuning_level >= level["HideSpeedLimit"] else default.get_bool("HideSpeedLimit"))
    toggle.use_wheel_speed = advanced_custom_ui and (params.get_bool("WheelSpeed") if tuning_level >= level["WheelSpeed"] else default.get_bool("WheelSpeed"))

    advanced_lateral_tuning = params.get_bool("AdvancedLateralTune") if tuning_level >= level["AdvancedLateralTune"] else default.get_bool("AdvancedLateralTune")
    toggle.force_auto_tune = advanced_lateral_tuning and not has_auto_tune and not is_pid_car and (params.get_bool("ForceAutoTune") if tuning_level >= level["ForceAutoTune"] else default.get_bool("ForceAutoTune"))
    toggle.force_auto_tune_off = advanced_lateral_tuning and has_auto_tune and not is_pid_car and (params.get_bool("ForceAutoTuneOff") if tuning_level >= level["ForceAutoTuneOff"] else default.get_bool("ForceAutoTuneOff"))
    stock_steer_friction = params.get_float("SteerFrictionStock")
    toggle.steer_friction = params.get_float("SteerFriction") if advanced_lateral_tuning and tuning_level >= level["SteerFriction"] else stock_steer_friction
    toggle.use_custom_steer_friction = toggle.steer_friction != stock_steer_friction and not is_pid_car and not toggle.force_auto_tune or toggle.force_auto_tune_off
    stock_steer_kp = params.get_float("SteerKPStock")
    toggle.steer_kp = params.get_float("SteerKP") if advanced_lateral_tuning and tuning_level >= level["SteerKP"] else stock_steer_kp
    toggle.use_custom_kp = toggle.steer_kp != stock_steer_kp and not is_pid_car
    stock_steer_lat_accel_factor = params.get_float("SteerLatAccelStock")
    toggle.steer_lat_accel_factor = params.get_float("SteerLatAccel") if advanced_lateral_tuning and tuning_level >= level["SteerLatAccel"] else stock_steer_lat_accel_factor
    toggle.use_custom_lat_accel_factor = toggle.steer_lat_accel_factor != stock_steer_lat_accel_factor and not is_pid_car and not toggle.force_auto_tune or toggle.force_auto_tune_off
    stock_steer_ratio = params.get_float("SteerRatioStock")
    toggle.steer_ratio = params.get_float("SteerRatio") if advanced_lateral_tuning and tuning_level >= level["SteerRatio"] else stock_steer_ratio
    toggle.use_custom_steer_ratio = toggle.steer_ratio != stock_steer_ratio and not toggle.force_auto_tune or toggle.force_auto_tune_off

    toggle.alert_volume_control = params.get_bool("AlertVolumeControl") if tuning_level >= level["AlertVolumeControl"] else default.get_bool("AlertVolumeControl")
    toggle.disengage_volume = params.get_int("DisengageVolume") if toggle.alert_volume_control and tuning_level >= level["DisengageVolume"] else default.get_int("DisengageVolume")
    toggle.engage_volume = params.get_int("EngageVolume") if toggle.alert_volume_control and tuning_level >= level["EngageVolume"] else default.get_int("EngageVolume")
    toggle.prompt_volume = params.get_int("PromptVolume") if toggle.alert_volume_control and tuning_level >= level["PromptVolume"] else default.get_int("PromptVolume")
    toggle.promptDistracted_volume = params.get_int("PromptDistractedVolume") if toggle.alert_volume_control and tuning_level >= level["PromptDistractedVolume"] else default.get_int("PromptDistractedVolume")
    toggle.refuse_volume = params.get_int("RefuseVolume") if toggle.alert_volume_control and tuning_level >= level["RefuseVolume"] else default.get_int("RefuseVolume")
    toggle.warningSoft_volume = params.get_int("WarningSoftVolume") if toggle.alert_volume_control and tuning_level >= level["WarningSoftVolume"] else default.get_int("WarningSoftVolume")
    toggle.warningImmediate_volume = max(params.get_int("WarningImmediateVolume"), 25) if toggle.alert_volume_control and tuning_level >= level["WarningImmediateVolume"] else default.get_int("WarningImmediateVolume")

    toggle.always_on_lateral = params.get_bool("AlwaysOnLateral") if tuning_level >= level["AlwaysOnLateral"] else default.get_bool("AlwaysOnLateral")
    toggle.always_on_lateral_set = toggle.always_on_lateral and always_on_lateral_set
    toggle.always_on_lateral_lkas = toggle.always_on_lateral_set and car_make != "subaru" and (params.get_bool("AlwaysOnLateralLKAS") if tuning_level >= level["AlwaysOnLateralLKAS"] else default.get_bool("AlwaysOnLateralLKAS"))
    toggle.always_on_lateral_main = toggle.always_on_lateral_set and (params.get_bool("AlwaysOnLateralMain") if tuning_level >= level["AlwaysOnLateralMain"] else default.get_bool("AlwaysOnLateralMain"))
    toggle.always_on_lateral_pause_speed = params.get_int("PauseAOLOnBrake") if toggle.always_on_lateral_set and tuning_level >= level["PauseAOLOnBrake"] else default.get_int("PauseAOLOnBrake")
    toggle.always_on_lateral_status_bar = toggle.always_on_lateral_set and not (params.get_bool("HideAOLStatusBar") if tuning_level >= level["HideAOLStatusBar"] else default.get_bool("HideAOLStatusBar"))

    toggle.automatic_updates = params.get_bool("AutomaticUpdates") if tuning_level >= level["AutomaticUpdates"] else default.get_bool("AutomaticUpdates")

    toggle.cluster_offset = params.get_float("ClusterOffset") if car_make == "toyota" and tuning_level >= level["ClusterOffset"] else default.get_float("ClusterOffset")

    toggle.conditional_experimental_mode = openpilot_longitudinal and (params.get_bool("ConditionalExperimental") if tuning_level >= level["ConditionalExperimental"] else default.get_bool("ConditionalExperimental"))
    toggle.conditional_curves = toggle.conditional_experimental_mode and (params.get_bool("CECurves") if tuning_level >= level["CECurves"] else default.get_bool("CECurves"))
    toggle.conditional_curves_lead = toggle.conditional_curves and (params.get_bool("CECurvesLead") if tuning_level >= level["CECurvesLead"] else default.get_bool("CECurvesLead"))
    toggle.conditional_lead = toggle.conditional_experimental_mode and (params.get_bool("CELead") if tuning_level >= level["CELead"] else default.get_bool("CELead"))
    toggle.conditional_slower_lead = toggle.conditional_lead and (params.get_bool("CESlowerLead") if tuning_level >= level["CESlowerLead"] else default.get_bool("CESlowerLead"))
    toggle.conditional_stopped_lead = toggle.conditional_lead and (params.get_bool("CEStoppedLead") if tuning_level >= level["CEStoppedLead"] else default.get_bool("CEStoppedLead"))
    toggle.conditional_limit = params.get_int("CESpeed") * speed_conversion if toggle.conditional_experimental_mode and tuning_level >= level["CESpeed"] else default.get_int("CESpeed") * CV.MPH_TO_MS
    toggle.conditional_limit_lead = params.get_int("CESpeedLead") * speed_conversion if toggle.conditional_experimental_mode and tuning_level >= level["CESpeedLead"] else default.get_int("CESpeedLead") * CV.MPH_TO_MS
    toggle.conditional_navigation = toggle.conditional_experimental_mode and (params.get_bool("CENavigation") if tuning_level >= level["CENavigation"] else default.get_bool("CENavigation"))
    toggle.conditional_navigation_intersections = toggle.conditional_navigation and (params.get_bool("CENavigationIntersections") if tuning_level >= level["CENavigationIntersections"] else default.get_bool("CENavigationIntersections"))
    toggle.conditional_navigation_lead = toggle.conditional_navigation and (params.get_bool("CENavigationLead") if tuning_level >= level["CENavigationLead"] else default.get_bool("CENavigationLead"))
    toggle.conditional_navigation_turns = toggle.conditional_navigation and (params.get_bool("CENavigationTurns") if tuning_level >= level["CENavigationTurns"] else default.get_bool("CENavigationTurns"))
    toggle.conditional_model_stop_time = params.get_int("CEModelStopTime") if toggle.conditional_experimental_mode and tuning_level >= level["CEModelStopTime"] else default.get_int("CEModelStopTime")
    toggle.conditional_signal = params.get_int("CESignalSpeed") if toggle.conditional_experimental_mode and tuning_level >= level["CESignalSpeed"] else default.get_int("CESignalSpeed")
    toggle.conditional_signal_lane_detection = toggle.conditional_signal != 0 and (params.get_bool("CESignalLaneDetection") if tuning_level >= level["CESignalLaneDetection"] else default.get_bool("CESignalLaneDetection"))
    toggle.conditional_status_bar = toggle.conditional_experimental_mode and not (params.get_bool("HideCEMStatusBar") if tuning_level >= level["HideCEMStatusBar"] else default.get_bool("HideCEMStatusBar"))
    if toggle.conditional_experimental_mode:
      params.put_bool("ExperimentalMode", True)

    toggle.crosstrek_torque = car_model == "SUBARU_IMPREZA" and params.get_bool("CrosstrekTorque") if tuning_level >= level["CrosstrekTorque"] else default.get_bool("CrosstrekTorque")

    toggle.curve_speed_controller = openpilot_longitudinal and (params.get_bool("CurveSpeedControl") if tuning_level >= level["CurveSpeedControl"] else default.get_bool("CurveSpeedControl"))
    toggle.curve_sensitivity = params.get_int("CurveSensitivity") / 100 if toggle.curve_speed_controller and tuning_level >= level["CurveSensitivity"] else default.get_int("CurveSensitivity") / 100
    toggle.hide_csc_ui = toggle.curve_speed_controller and (params.get_bool("HideCSCUI") if tuning_level >= level["HideCSCUI"] else default.get_bool("HideCSCUI"))
    toggle.turn_aggressiveness = params.get_int("TurnAggressiveness") / 100 if toggle.curve_speed_controller and tuning_level >= level["TurnAggressiveness"] else default.get_int("TurnAggressiveness") / 100
    toggle.map_turn_speed_controller = toggle.curve_speed_controller and (params.get_bool("MTSCEnabled") if tuning_level >= level["MTSCEnabled"] else default.get_bool("MTSCEnabled"))
    toggle.mtsc_curvature_check = toggle.map_turn_speed_controller and (params.get_bool("MTSCCurvatureCheck") if tuning_level >= level["MTSCCurvatureCheck"] else default.get_bool("MTSCCurvatureCheck"))
    toggle.vision_turn_controller = toggle.curve_speed_controller and (params.get_bool("VisionTurnControl") if tuning_level >= level["VisionTurnControl"] else default.get_bool("VisionTurnControl"))

    toggle.custom_alerts = params.get_bool("CustomAlerts") if tuning_level >= level["CustomAlerts"] else default.get_bool("CustomAlerts")
    toggle.goat_scream_alert = toggle.custom_alerts and (params.get_bool("GoatScream") if tuning_level >= level["GoatScream"] else default.get_bool("GoatScream"))
    toggle.green_light_alert = toggle.custom_alerts and (params.get_bool("GreenLightAlert") if tuning_level >= level["GreenLightAlert"] else default.get_bool("GreenLightAlert"))
    toggle.lead_departing_alert = toggle.custom_alerts and (params.get_bool("LeadDepartingAlert") if tuning_level >= level["LeadDepartingAlert"] else default.get_bool("LeadDepartingAlert"))
    toggle.loud_blindspot_alert = has_bsm and toggle.custom_alerts and (params.get_bool("LoudBlindspotAlert") if tuning_level >= level["LoudBlindspotAlert"] else default.get_bool("LoudBlindspotAlert"))
    toggle.speed_limit_changed_alert = toggle.custom_alerts and (params.get_bool("SpeedLimitChangedAlert") if tuning_level >= level["SpeedLimitChangedAlert"] else default.get_bool("SpeedLimitChangedAlert"))

    toggle.custom_personalities = openpilot_longitudinal and params.get_bool("CustomPersonalities") if tuning_level >= level["CustomPersonalities"] else default.get_bool("CustomPersonalities")
    aggressive_profile = toggle.custom_personalities and (params.get_bool("AggressivePersonalityProfile") if tuning_level >= level["AggressivePersonalityProfile"] else default.get_bool("AggressivePersonalityProfile"))
    toggle.aggressive_jerk_acceleration = clip(params.get_int("AggressiveJerkAcceleration") / 100, 0.01, 5) if aggressive_profile and tuning_level >= level["AggressiveJerkAcceleration"] else clip(default.get_int("AggressiveJerkAcceleration") / 100, 0.01, 5)
    toggle.aggressive_jerk_deceleration = clip(params.get_int("AggressiveJerkDeceleration") / 100, 0.01, 5) if aggressive_profile and tuning_level >= level["AggressiveJerkDeceleration"] else clip(default.get_int("AggressiveJerkDeceleration") / 100, 0.01, 5)
    toggle.aggressive_jerk_danger = clip(params.get_int("AggressiveJerkDanger") / 100, 0.01, 5) if aggressive_profile and tuning_level >= level["AggressiveJerkDanger"] else clip(default.get_int("AggressiveJerkDanger") / 100, 0.01, 5)
    toggle.aggressive_jerk_speed = clip(params.get_int("AggressiveJerkSpeed") / 100, 0.01, 5) if aggressive_profile and tuning_level >= level["AggressiveJerkSpeed"] else clip(default.get_int("AggressiveJerkSpeed") / 100, 0.01, 5)
    toggle.aggressive_jerk_speed_decrease = clip(params.get_int("AggressiveJerkSpeedDecrease") / 100, 0.01, 5) if aggressive_profile and tuning_level >= level["AggressiveJerkSpeedDecrease"] else clip(default.get_int("AggressiveJerkSpeedDecrease") / 100, 0.01, 5)
    toggle.aggressive_follow = clip(params.get_float("AggressiveFollow"), 1, 5) if aggressive_profile and tuning_level >= level["AggressiveFollow"] else clip(default.get_float("AggressiveFollow"), 1, 5)
    standard_profile = toggle.custom_personalities and (params.get_bool("StandardPersonalityProfile") if tuning_level >= level["StandardPersonalityProfile"] else default.get_bool("StandardPersonalityProfile"))
    toggle.standard_jerk_acceleration = clip(params.get_int("StandardJerkAcceleration") / 100, 0.01, 5) if standard_profile and tuning_level >= level["StandardJerkAcceleration"] else clip(default.get_int("StandardJerkAcceleration") / 100, 0.01, 5)
    toggle.standard_jerk_deceleration = clip(params.get_int("StandardJerkDeceleration") / 100, 0.01, 5) if standard_profile and tuning_level >= level["StandardJerkDeceleration"] else clip(default.get_int("StandardJerkDeceleration") / 100, 0.01, 5)
    toggle.standard_jerk_danger = clip(params.get_int("StandardJerkDanger") / 100, 0.01, 5) if standard_profile and tuning_level >= level["StandardJerkDanger"] else clip(default.get_int("StandardJerkDanger") / 100, 0.01, 5)
    toggle.standard_jerk_speed = clip(params.get_int("StandardJerkSpeed") / 100, 0.01, 5) if standard_profile and tuning_level >= level["StandardJerkSpeed"] else clip(default.get_int("StandardJerkSpeed") / 100, 0.01, 5)
    toggle.standard_jerk_speed_decrease = clip(params.get_int("StandardJerkSpeedDecrease") / 100, 0.01, 5) if standard_profile and tuning_level >= level["StandardJerkSpeedDecrease"] else clip(default.get_int("StandardJerkSpeedDecrease") / 100, 0.01, 5)
    toggle.standard_follow = clip(params.get_float("StandardFollow"), 1, 5) if standard_profile and tuning_level >= level["StandardFollow"] else clip(default.get_float("StandardFollow"), 1, 5)
    relaxed_profile = toggle.custom_personalities and (params.get_bool("RelaxedPersonalityProfile") if tuning_level >= level["RelaxedPersonalityProfile"] else default.get_bool("RelaxedPersonalityProfile"))
    toggle.relaxed_jerk_acceleration = clip(params.get_int("RelaxedJerkAcceleration") / 100, 0.01, 5) if relaxed_profile and tuning_level >= level["RelaxedJerkAcceleration"] else clip(default.get_int("RelaxedJerkAcceleration") / 100, 0.01, 5)
    toggle.relaxed_jerk_deceleration = clip(params.get_int("RelaxedJerkDeceleration") / 100, 0.01, 5) if relaxed_profile and tuning_level >= level["RelaxedJerkDeceleration"] else clip(default.get_int("RelaxedJerkDeceleration") / 100, 0.01, 5)
    toggle.relaxed_jerk_danger = clip(params.get_int("RelaxedJerkDanger") / 100, 0.01, 5) if relaxed_profile and tuning_level >= level["RelaxedJerkDanger"] else clip(default.get_int("RelaxedJerkDanger") / 100, 0.01, 5)
    toggle.relaxed_jerk_speed = clip(params.get_int("RelaxedJerkSpeed") / 100, 0.01, 5) if relaxed_profile and tuning_level >= level["RelaxedJerkSpeed"] else clip(default.get_int("RelaxedJerkSpeed") / 100, 0.01, 5)
    toggle.relaxed_jerk_speed_decrease = clip(params.get_int("RelaxedJerkSpeedDecrease") / 100, 0.01, 5) if relaxed_profile and tuning_level >= level["RelaxedJerkSpeedDecrease"] else clip(default.get_int("RelaxedJerkSpeedDecrease") / 100, 0.01, 5)
    toggle.relaxed_follow = clip(params.get_float("RelaxedFollow"), 1, 5) if relaxed_profile and tuning_level >= level["RelaxedFollow"] else clip(default.get_float("RelaxedFollow"), 1, 5)
    traffic_profile = toggle.custom_personalities and (params.get_bool("TrafficPersonalityProfile") if tuning_level >= level["TrafficPersonalityProfile"] else default.get_bool("TrafficPersonalityProfile"))
    toggle.traffic_mode_jerk_acceleration = [clip(params.get_int("TrafficJerkAcceleration") / 100, 0.01, 5) if traffic_profile and tuning_level >= level["TrafficJerkAcceleration"] else clip(default.get_int("TrafficJerkAcceleration") / 100, 0.01, 5), toggle.aggressive_jerk_acceleration]
    toggle.traffic_mode_jerk_deceleration = [clip(params.get_int("TrafficJerkDeceleration") / 100, 0.01, 5) if traffic_profile and tuning_level >= level["TrafficJerkDeceleration"] else clip(default.get_int("TrafficJerkDeceleration") / 100, 0.01, 5), toggle.aggressive_jerk_deceleration]
    toggle.traffic_mode_jerk_danger = [clip(params.get_int("TrafficJerkDanger") / 100, 0.01, 5) if traffic_profile and tuning_level >= level["TrafficJerkDanger"] else clip(default.get_int("TrafficJerkDanger") / 100, 0.01, 5), toggle.aggressive_jerk_danger]
    toggle.traffic_mode_jerk_speed = [clip(params.get_int("TrafficJerkSpeed") / 100, 0.01, 5) if traffic_profile and tuning_level >= level["TrafficJerkSpeed"] else clip(default.get_int("TrafficJerkSpeed") / 100, 0.01, 5), toggle.aggressive_jerk_speed]
    toggle.traffic_mode_jerk_speed_decrease = [clip(params.get_int("TrafficJerkSpeedDecrease") / 100, 0.01, 5) if traffic_profile and tuning_level >= level["TrafficJerkSpeedDecrease"] else clip(default.get_int("TrafficJerkSpeedDecrease") / 100, 0.01, 5), toggle.aggressive_jerk_speed_decrease]
    toggle.traffic_mode_follow = [clip(params.get_float("TrafficFollow"), 0.5, 5) if traffic_profile and tuning_level >= level["TrafficFollow"] else clip(default.get_float("TrafficFollow"), 0.5, 5), toggle.aggressive_follow]

    custom_ui = params.get_bool("CustomUI") if tuning_level >= level["CustomUI"] else default.get_bool("CustomUI")
    toggle.acceleration_path = custom_ui and (params.get_bool("AccelerationPath") if tuning_level >= level["AccelerationPath"] else default.get_bool("AccelerationPath"))
    toggle.adjacent_paths = custom_ui and (params.get_bool("AdjacentPath") if tuning_level >= level["AdjacentPath"] else default.get_bool("AdjacentPath"))
    toggle.blind_spot_path = has_bsm and custom_ui and (params.get_bool("BlindSpotPath") if tuning_level >= level["BlindSpotPath"] else default.get_bool("BlindSpotPath"))
    toggle.compass = custom_ui and (params.get_bool("Compass") if tuning_level >= level["Compass"] else default.get_bool("Compass"))
    toggle.pedals_on_ui = custom_ui and (params.get_bool("PedalsOnUI") if tuning_level >= level["PedalsOnUI"] else default.get_bool("PedalsOnUI"))
    toggle.dynamic_pedals_on_ui = toggle.pedals_on_ui and (params.get_bool("DynamicPedalsOnUI") if tuning_level >= level["DynamicPedalsOnUI"] else default.get_bool("DynamicPedalsOnUI"))
    toggle.static_pedals_on_ui = toggle.pedals_on_ui and (params.get_bool("StaticPedalsOnUI") if tuning_level >= level["StaticPedalsOnUI"] else default.get_bool("StaticPedalsOnUI"))
    toggle.rotating_wheel = custom_ui and (params.get_bool("RotatingWheel") if tuning_level >= level["RotatingWheel"] else default.get_bool("RotatingWheel"))

    toggle.developer_ui = params.get_bool("DeveloperUI") if tuning_level >= level["DeveloperUI"] else default.get_bool("DeveloperUI")
    border_metrics = toggle.developer_ui and (params.get_bool("BorderMetrics") if tuning_level >= level["BorderMetrics"] else default.get_bool("BorderMetrics"))
    toggle.blind_spot_metrics = has_bsm and border_metrics and (params.get_bool("BlindSpotMetrics") if tuning_level >= level["BlindSpotMetrics"] else default.get_bool("BlindSpotMetrics"))
    toggle.signal_metrics = border_metrics and (params.get_bool("SignalMetrics") if tuning_level >= level["SignalMetrics"] else default.get_bool("SignalMetrics"))
    toggle.steering_metrics = border_metrics and (params.get_bool("ShowSteering") if tuning_level >= level["ShowSteering"] else default.get_bool("ShowSteering"))
    toggle.show_fps = toggle.developer_ui and (params.get_bool("FPSCounter") if tuning_level >= level["FPSCounter"] else default.get_bool("FPSCounter"))
    lateral_metrics = toggle.developer_ui and (params.get_bool("LateralMetrics") if tuning_level >= level["LateralMetrics"] else default.get_bool("LateralMetrics"))
    toggle.adjacent_path_metrics = lateral_metrics and (params.get_bool("AdjacentPathMetrics") if tuning_level >= level["AdjacentPathMetrics"] else default.get_bool("AdjacentPathMetrics"))
    toggle.lateral_tuning_metrics = (has_auto_tune or toggle.force_auto_tune) and lateral_metrics and (params.get_bool("TuningInfo") if tuning_level >= level["TuningInfo"] else default.get_bool("TuningInfo"))
    longitudinal_metrics = toggle.developer_ui and (params.get_bool("LongitudinalMetrics") if tuning_level >= level["LongitudinalMetrics"] else default.get_bool("LongitudinalMetrics"))
    toggle.adjacent_lead_tracking = has_radar and longitudinal_metrics and (params.get_bool("AdjacentLeadsUI") if tuning_level >= level["AdjacentLeadsUI"] else default.get_bool("AdjacentLeadsUI"))
    toggle.lead_metrics = longitudinal_metrics and (params.get_bool("LeadInfo") if tuning_level >= level["LeadInfo"] else default.get_bool("LeadInfo"))
    toggle.jerk_metrics = longitudinal_metrics and (params.get_bool("JerkInfo") if tuning_level >= level["JerkInfo"] else default.get_bool("JerkInfo"))
    toggle.numerical_temp = toggle.developer_ui and (params.get_bool("NumericalTemp") if tuning_level >= level["NumericalTemp"] else default.get_bool("NumericalTemp"))
    toggle.fahrenheit = toggle.numerical_temp and (params.get_bool("Fahrenheit") if tuning_level >= level["Fahrenheit"] else default.get_bool("Fahrenheit"))
    toggle.sidebar_metrics = toggle.developer_ui and (params.get_bool("SidebarMetrics") if tuning_level >= level["SidebarMetrics"] else default.get_bool("SidebarMetrics"))
    toggle.cpu_metrics = toggle.sidebar_metrics and (params.get_bool("ShowCPU") if tuning_level >= level["ShowCPU"] else default.get_bool("ShowCPU"))
    toggle.gpu_metrics = toggle.sidebar_metrics and (params.get_bool("ShowGPU") if tuning_level >= level["ShowGPU"] else default.get_bool("ShowGPU"))
    toggle.ip_metrics = toggle.sidebar_metrics and (params.get_bool("ShowIP") if tuning_level >= level["ShowIP"] else default.get_bool("ShowIP"))
    toggle.memory_metrics = toggle.sidebar_metrics and (params.get_bool("ShowMemoryUsage") if tuning_level >= level["ShowMemoryUsage"] else default.get_bool("ShowMemoryUsage"))
    toggle.storage_left_metrics = toggle.sidebar_metrics and (params.get_bool("ShowStorageLeft") if tuning_level >= level["ShowStorageLeft"] else default.get_bool("ShowStorageLeft"))
    toggle.storage_used_metrics = toggle.sidebar_metrics and (params.get_bool("ShowStorageUsed") if tuning_level >= level["ShowStorageUsed"] else default.get_bool("ShowStorageUsed"))
    toggle.use_si_metrics = toggle.developer_ui and (params.get_bool("UseSI") if tuning_level >= level["UseSI"] else default.get_bool("UseSI"))

    device_management = params.get_bool("DeviceManagement") if tuning_level >= level["DeviceManagement"] else default.get_bool("DeviceManagement")
    device_shutdown_setting = params.get_int("DeviceShutdown") if device_management and tuning_level >= level["DeviceShutdown"] else default.get_int("DeviceShutdown")
    toggle.device_shutdown_time = (device_shutdown_setting - 3) * 3600 if device_shutdown_setting >= 4 else device_shutdown_setting * (60 * 15)
    toggle.increase_thermal_limits = device_management and (params.get_bool("IncreaseThermalLimits") if tuning_level >= level["IncreaseThermalLimits"] else default.get_bool("IncreaseThermalLimits"))
    toggle.low_voltage_shutdown = clip(params.get_float("LowVoltageShutdown"), VBATT_PAUSE_CHARGING, 12.5) if device_management and tuning_level >= level["LowVoltageShutdown"] else default.get_float("LowVoltageShutdown")
    toggle.no_logging = device_management and (params.get_bool("NoLogging") if tuning_level >= level["NoLogging"] else default.get_bool("NoLogging"))
    toggle.no_uploads = device_management and (params.get_bool("NoUploads") if tuning_level >= level["NoUploads"] else default.get_bool("NoUploads"))
    toggle.no_onroad_uploads = toggle.no_uploads and (params.get_bool("DisableOnroadUploads") if tuning_level >= level["DisableOnroadUploads"] else default.get_bool("DisableOnroadUploads"))
    toggle.offline_mode = device_management and (params.get_bool("OfflineMode") if tuning_level >= level["OfflineMode"] else default.get_bool("OfflineMode"))

    toggle.experimental_gm_tune = openpilot_longitudinal and car_make == "gm" and (params.get_bool("ExperimentalGMTune") if tuning_level >= level["ExperimentalGMTune"] else default.get_bool("ExperimentalGMTune"))

    toggle.experimental_mode_via_press = openpilot_longitudinal and (params.get_bool("ExperimentalModeActivation") if tuning_level >= level["ExperimentalModeActivation"] else default.get_bool("ExperimentalModeActivation"))
    toggle.experimental_mode_via_distance = toggle.experimental_mode_via_press and (params.get_bool("ExperimentalModeViaDistance") if tuning_level >= level["ExperimentalModeViaDistance"] else default.get_bool("ExperimentalModeViaDistance"))
    toggle.experimental_mode_via_lkas = not toggle.always_on_lateral_lkas and toggle.experimental_mode_via_press and car_make != "subaru" and (params.get_bool("ExperimentalModeViaLKAS") if tuning_level >= level["ExperimentalModeViaLKAS"] else default.get_bool("ExperimentalModeViaLKAS"))
    toggle.experimental_mode_via_tap = toggle.experimental_mode_via_press and (params.get_bool("ExperimentalModeViaTap") if tuning_level >= level["ExperimentalModeViaTap"] else default.get_bool("ExperimentalModeViaTap"))

    toggle.frogsgomoo_tweak = openpilot_longitudinal and car_make == "toyota" and (params.get_bool("FrogsGoMoosTweak") if tuning_level >= level["FrogsGoMoosTweak"] else default.get_bool("FrogsGoMoosTweak"))

    toggle.holiday_themes = params.get_bool("HolidayThemes") if tuning_level >= level["HolidayThemes"] else default.get_bool("HolidayThemes")
    toggle.current_holiday_theme = holiday_theme if toggle.holiday_themes else "stock"

    lane_change_customizations = params.get_bool("LaneChangeCustomizations") if tuning_level >= level["LaneChangeCustomizations"] else default.get_bool("LaneChangeCustomizations")
    toggle.lane_change_delay = params.get_float("LaneChangeTime") if lane_change_customizations and tuning_level >= level["LaneChangeTime"] else default.get_float("LaneChangeTime")
    toggle.lane_detection_width = params.get_float("LaneDetectionWidth") * distance_conversion if lane_change_customizations and tuning_level >= level["LaneDetectionWidth"] else default.get_float("LaneDetectionWidth") * CV.FOOT_TO_METER
    toggle.lane_detection = toggle.lane_detection_width > 0
    toggle.minimum_lane_change_speed = params.get_float("MinimumLaneChangeSpeed") * speed_conversion if lane_change_customizations and tuning_level >= level["MinimumLaneChangeSpeed"] else default.get_float("MinimumLaneChangeSpeed") * CV.MPH_TO_MS
    toggle.nudgeless = lane_change_customizations and (params.get_bool("NudgelessLaneChange") if tuning_level >= level["NudgelessLaneChange"] else default.get_bool("NudgelessLaneChange"))
    toggle.one_lane_change = lane_change_customizations and (params.get_bool("OneLaneChange") if tuning_level >= level["OneLaneChange"] else default.get_bool("OneLaneChange"))

    lateral_tuning = params.get_bool("LateralTune") if tuning_level >= level["LateralTune"] else default.get_bool("LateralTune")
    toggle.nnff = lateral_tuning and (params.get_bool("NNFF") if tuning_level >= level["NNFF"] else default.get_bool("NNFF"))
    toggle.nnff_lite = lateral_tuning and (params.get_bool("NNFFLite") if tuning_level >= level["NNFFLite"] else default.get_bool("NNFFLite"))
    toggle.use_turn_desires = lateral_tuning and (params.get_bool("TurnDesires") if tuning_level >= level["TurnDesires"] else default.get_bool("TurnDesires"))

    toggle.long_pitch = openpilot_longitudinal and car_make == "gm" and (params.get_bool("LongPitch") if tuning_level >= level["LongPitch"] else default.get_bool("LongPitch"))

    longitudinal_tuning = openpilot_longitudinal and (params.get_bool("LongitudinalTune") if tuning_level >= level["LongitudinalTune"] else default.get_bool("LongitudinalTune"))
    toggle.acceleration_profile = params.get_int("AccelerationProfile") if longitudinal_tuning and tuning_level >= level["AccelerationProfile"] else default.get_int("AccelerationProfile")
    toggle.sport_plus = max_acceleration_enabled and toggle.acceleration_profile == 3
    toggle.deceleration_profile = params.get_int("DecelerationProfile") if longitudinal_tuning and tuning_level >= level["DecelerationProfile"] else default.get_int("DecelerationProfile")
    toggle.human_acceleration = longitudinal_tuning and (params.get_bool("HumanAcceleration") if tuning_level >= level["HumanAcceleration"] else default.get_bool("HumanAcceleration"))
    toggle.human_following = longitudinal_tuning and (params.get_bool("HumanFollowing") if tuning_level >= level["HumanFollowing"] else default.get_bool("HumanFollowing"))
    toggle.lead_detection_probability = clip(params.get_int("LeadDetectionThreshold") / 100, 0.01, 0.99) if longitudinal_tuning and tuning_level >= level["LeadDetectionThreshold"] else clip(default.get_int("LeadDetectionThreshold") / 100, 0.01, 0.99)
    toggle.max_desired_acceleration = clip(params.get_float("MaxDesiredAcceleration"), 0.1, 4.0) if longitudinal_tuning and tuning_level >= level["MaxDesiredAcceleration"] else default.get_float("MaxDesiredAcceleration")
    toggle.taco_tune = longitudinal_tuning and (params.get_bool("TacoTune") if tuning_level >= level["TacoTune"] else default.get_bool("TacoTune"))

    available_models = params.get("AvailableModels", encoding='utf-8')
    available_model_names = params.get("AvailableModelNames", encoding='utf-8')
    toggle.model_randomizer = params.get_bool("ModelRandomizer") if tuning_level >= level["ModelRandomizer"] else default.get_bool("ModelRandomizer")
    if available_models:
      if toggle.model_randomizer:
        if not started or not toggle.model:
          blacklisted_models = (params.get("BlacklistedModels", encoding='utf-8') or "").split(",")
          existing_models = [model for model in available_models.split(",") if model not in blacklisted_models and (MODELS_PATH / f"{model}.thneed").exists()]
          toggle.model = random.choice(existing_models) if existing_models else default.get("Model", encoding='utf-8')
      else:
        toggle.model = params.get("Model", encoding='utf-8') if tuning_level >= level["Model"] else default.get("Model", encoding='utf-8')
    else:
      toggle.model = default.get("Model", encoding='utf-8')
    if available_models and available_model_names and toggle.model in available_models.split(",") and (MODELS_PATH / f"{toggle.model}.thneed").exists():
      toggle.model_name = available_model_names.split(",")[available_models.split(",").index(toggle.model)]
    else:
      toggle.model = default.get("Model", encoding='utf-8')
      toggle.model_name = default.get("ModelName", encoding='utf-8')
    classic_models = params.get("ClassicModels", encoding='utf-8') or ""
    toggle.classic_model = classic_models and toggle.model in classic_models.split(",")
    clipped_curvature_models = params.get("ClippedCurvatureModels", encoding='utf-8') or ""
    toggle.clipped_curvature_model = clipped_curvature_models and toggle.model in clipped_curvature_models.split(",")
    desired_curvature_models = params.get("DesiredCurvatureModels", encoding='utf-8') or ""
    toggle.desired_curvature_model = desired_curvature_models and toggle.model in desired_curvature_models.split(",")
    navigation_models = params.get("NavigationModels", encoding='utf-8') or ""
    toggle.navigationless_model = navigation_models and toggle.model not in navigation_models.split(",")
    radarless_models = params.get("RadarlessModels", encoding='utf-8') or ""
    toggle.radarless_model = radarless_models and toggle.model in radarless_models.split(",")

    toggle.model_ui = params.get_bool("ModelUI") if tuning_level >= level["ModelUI"] else default.get_bool("ModelUI")
    toggle.dynamic_path_width = toggle.model_ui and (params.get_bool("DynamicPathWidth") if tuning_level >= level["DynamicPathWidth"] else default.get_bool("DynamicPathWidth"))
    toggle.lane_line_width = params.get_int("LaneLinesWidth") * small_distance_conversion / 200 if toggle.model_ui and tuning_level >= level["LaneLinesWidth"] else default.get_int("LaneLinesWidth") * CV.INCH_TO_CM / 200
    toggle.path_edge_width = params.get_int("PathEdgeWidth") if toggle.model_ui and tuning_level >= level["PathEdgeWidth"] else default.get_int("PathEdgeWidth")
    toggle.path_width = params.get_float("PathWidth") * distance_conversion / 2 if toggle.model_ui and tuning_level >= level["PathWidth"] else default.get_float("PathWidth") * CV.FOOT_TO_METER / 2
    toggle.road_edge_width = params.get_int("RoadEdgesWidth") * small_distance_conversion / 200 if toggle.model_ui and tuning_level >= level["RoadEdgesWidth"] else default.get_int("RoadEdgesWidth") * CV.INCH_TO_CM / 200
    toggle.show_stopping_point = toggle.model_ui and (params.get_bool("ShowStoppingPoint") if tuning_level >= level["ShowStoppingPoint"] else default.get_bool("ShowStoppingPoint"))
    toggle.show_stopping_point_metrics = toggle.show_stopping_point and (params.get_bool("ShowStoppingPointMetrics") if tuning_level >= level["ShowStoppingPointMetrics"] else default.get_bool("ShowStoppingPointMetrics"))
    toggle.unlimited_road_ui_length = toggle.model_ui and (params.get_bool("UnlimitedLength") if tuning_level >= level["UnlimitedLength"] else default.get_bool("UnlimitedLength"))

    toggle.navigation_ui = params.get_bool("NavigationUI") if tuning_level >= level["NavigationUI"] else default.get_bool("NavigationUI")
    toggle.big_map = toggle.navigation_ui and (params.get_bool("BigMap") if tuning_level >= level["BigMap"] else default.get_bool("BigMap"))
    toggle.full_map = toggle.big_map and (params.get_bool("FullMap") if tuning_level >= level["FullMap"] else default.get_bool("FullMap"))
    toggle.map_style = params.get_int("MapStyle") if toggle.navigation_ui and tuning_level >= level["MapStyle"] else default.get_int("MapStyle")
    toggle.road_name_ui = toggle.navigation_ui and (params.get_bool("RoadNameUI") if tuning_level >= level["RoadNameUI"] else default.get_bool("RoadNameUI"))
    toggle.show_speed_limit_offset = toggle.navigation_ui and (params.get_bool("ShowSLCOffset") if tuning_level >= level["ShowSLCOffset"] else default.get_bool("ShowSLCOffset"))
    toggle.show_speed_limits = toggle.navigation_ui and (params.get_bool("ShowSpeedLimits") if tuning_level >= level["ShowSpeedLimits"] else default.get_bool("ShowSpeedLimits"))
    toggle.speed_limit_vienna = toggle.navigation_ui and (params.get_bool("UseVienna") if tuning_level >= level["UseVienna"] else default.get_bool("UseVienna"))

    toggle.old_long_api = openpilot_longitudinal and car_make == "gm" and not (params.get_bool("NewLongAPIGM") if tuning_level >= level["NewLongAPIGM"] else default.get_bool("NewLongAPIGM"))
    toggle.old_long_api |= openpilot_longitudinal and car_make == "hyundai" and not (params.get_bool("NewLongAPI") if tuning_level >= level["NewLongAPI"] else default.get_bool("NewLongAPI"))

    personalize_openpilot = params.get_bool("PersonalizeOpenpilot") if tuning_level >= level["PersonalizeOpenpilot"] else default.get_bool("PersonalizeOpenpilot")
    toggle.color_scheme = params.get("CustomColors", encoding='utf-8') if personalize_openpilot else "stock"
    toggle.distance_icons = params.get("CustomDistanceIcons", encoding='utf-8') if personalize_openpilot else "stock"
    toggle.icon_pack = params.get("CustomIcons", encoding='utf-8') if personalize_openpilot else "stock"
    toggle.signal_icons = params.get("CustomSignals", encoding='utf-8') if personalize_openpilot else "stock"
    toggle.sound_pack = params.get("CustomSounds", encoding='utf-8') if personalize_openpilot else "stock"
    toggle.wheel_image = params.get("WheelIcon", encoding='utf-8') if personalize_openpilot else "stock"

    quality_of_life_lateral = params.get_bool("QOLLateral") if tuning_level >= level["QOLLateral"] else default.get_bool("QOLLateral")
    toggle.pause_lateral_below_speed = params.get_int("PauseLateralSpeed") * speed_conversion if quality_of_life_lateral and tuning_level >= level["PauseLateralSpeed"] else default.get_int("PauseLateralSpeed") * CV.MPH_TO_MS
    toggle.pause_lateral_below_signal = toggle.pause_lateral_below_speed != 0 and (params.get_bool("PauseLateralOnSignal") if tuning_level >= level["PauseLateralOnSignal"] else default.get_bool("PauseLateralOnSignal"))

    quality_of_life_longitudinal = params.get_bool("QOLLongitudinal") if tuning_level >= level["QOLLongitudinal"] else default.get_bool("QOLLongitudinal")
    toggle.custom_cruise_increase = params.get_int("CustomCruise") if quality_of_life_longitudinal and not pcm_cruise and tuning_level >= level["CustomCruise"] else default.get_int("CustomCruise")
    toggle.custom_cruise_increase_long = params.get_int("CustomCruiseLong") if quality_of_life_longitudinal and not pcm_cruise and tuning_level >= level["CustomCruiseLong"] else default.get_int("CustomCruiseLong")
    toggle.force_standstill = quality_of_life_longitudinal and (params.get_bool("ForceStandstill") if tuning_level >= level["ForceStandstill"] else default.get_bool("ForceStandstill"))
    toggle.force_stops = quality_of_life_longitudinal and (params.get_bool("ForceStops") if tuning_level >= level["ForceStops"] else default.get_bool("ForceStops"))
    toggle.increased_stopped_distance = params.get_int("IncreasedStoppedDistance") * distance_conversion if quality_of_life_longitudinal and tuning_level >= level["IncreasedStoppedDistance"] else default.get_int("IncreasedStoppedDistance") * CV.FOOT_TO_METER
    map_gears = quality_of_life_longitudinal and (params.get_bool("MapGears") if tuning_level >= level["MapGears"] else default.get_bool("MapGears"))
    toggle.map_acceleration = map_gears and (params.get_bool("MapAcceleration") if tuning_level >= level["MapAcceleration"] else default.get_bool("MapAcceleration"))
    toggle.map_deceleration = map_gears and (params.get_bool("MapDeceleration") if tuning_level >= level["MapDeceleration"] else default.get_bool("MapDeceleration"))
    toggle.reverse_cruise_increase = quality_of_life_longitudinal and pcm_cruise and (params.get_bool("ReverseCruise") if tuning_level >= level["ReverseCruise"] else default.get_bool("ReverseCruise"))
    toggle.set_speed_offset = params.get_int("SetSpeedOffset") * (1 if toggle.is_metric else CV.MPH_TO_KPH) if quality_of_life_longitudinal and not pcm_cruise and tuning_level >= level["SetSpeedOffset"] else default.get_int("SetSpeedOffset") * CV.MPH_TO_KPH

    quality_of_life_visuals = params.get_bool("QOLVisuals") if tuning_level >= level["QOLVisuals"] else default.get_bool("QOLVisuals")
    toggle.camera_view = params.get_int("CameraView") if quality_of_life_visuals and tuning_level >= level["CameraView"] else default.get_int("CameraView")
    toggle.driver_camera_in_reverse = quality_of_life_visuals and (params.get_bool("DriverCamera") if tuning_level >= level["DriverCamera"] else default.get_bool("DriverCamera"))
    toggle.onroad_distance_button = openpilot_longitudinal and quality_of_life_visuals and (params.get_bool("OnroadDistanceButton") if tuning_level >= level["OnroadDistanceButton"] else default.get_bool("OnroadDistanceButton"))
    toggle.standby_mode = quality_of_life_visuals and (params.get_bool("StandbyMode") if tuning_level >= level["StandbyMode"] else default.get_bool("StandbyMode"))
    toggle.stopped_timer = quality_of_life_visuals and (params.get_bool("StoppedTimer") if tuning_level >= level["StoppedTimer"] else default.get_bool("StoppedTimer"))

    toggle.rainbow_path = params.get_bool("RainbowPath") if tuning_level >= level["RainbowPath"] else default.get_bool("RainbowPath")

    toggle.random_events = params.get_bool("RandomEvents") if tuning_level >= level["RandomEvents"] else default.get_bool("RandomEvents")

    screen_management = params.get_bool("ScreenManagement") if tuning_level >= level["ScreenManagement"] else default.get_bool("ScreenManagement")
    toggle.screen_brightness = params.get_int("ScreenBrightness") if screen_management and tuning_level >= level["ScreenBrightness"] else default.get_int("ScreenBrightness")
    toggle.screen_brightness_onroad = params.get_int("ScreenBrightnessOnroad") if screen_management and tuning_level >= level["ScreenBrightnessOnroad"] else default.get_int("ScreenBrightnessOnroad")
    toggle.screen_recorder = screen_management and (params.get_bool("ScreenRecorder") if tuning_level >= level["ScreenRecorder"] else default.get_bool("ScreenRecorder"))
    toggle.screen_timeout = params.get_int("ScreenTimeout") if screen_management and tuning_level >= level["ScreenTimeout"] else default.get_int("ScreenTimeout")
    toggle.screen_timeout_onroad = params.get_int("ScreenTimeoutOnroad") if screen_management and tuning_level >= level["ScreenTimeoutOnroad"] else default.get_int("ScreenTimeoutOnroad")

    toggle.sng_hack = openpilot_longitudinal and car_make == "toyota" and (params.get_bool("SNGHack") if tuning_level >= level["SNGHack"] else default.get_bool("SNGHack"))

    toggle.speed_limit_controller = openpilot_longitudinal and (params.get_bool("SpeedLimitController") if tuning_level >= level["SpeedLimitController"] else default.get_bool("SpeedLimitController"))
    toggle.force_mph_dashboard = toggle.speed_limit_controller and (params.get_bool("ForceMPHDashboard") if tuning_level >= level["ForceMPHDashboard"] else default.get_bool("ForceMPHDashboard"))
    toggle.map_speed_lookahead_higher = params.get_int("SLCLookaheadHigher") if toggle.speed_limit_controller and tuning_level >= level["SLCLookaheadHigher"] else default.get_int("SLCLookaheadHigher")
    toggle.map_speed_lookahead_lower = params.get_int("SLCLookaheadLower") if toggle.speed_limit_controller and tuning_level >= level["SLCLookaheadLower"] else default.get_int("SLCLookaheadLower")
    toggle.set_speed_limit = toggle.speed_limit_controller and (params.get_bool("SetSpeedLimit") if tuning_level >= level["SetSpeedLimit"] else default.get_bool("SetSpeedLimit"))
    slc_fallback_method = params.get_int("SLCFallback") if toggle.speed_limit_controller and tuning_level >= level["SLCFallback"] else default.get_int("SLCFallback")
    toggle.slc_fallback_experimental_mode = slc_fallback_method == 1
    toggle.slc_fallback_previous_speed_limit = slc_fallback_method == 2
    toggle.slc_fallback_set_speed = slc_fallback_method == 0
    toggle.speed_limit_confirmation = toggle.speed_limit_controller and (params.get_bool("SLCConfirmation") if tuning_level >= level["SLCConfirmation"] else default.get_bool("SLCConfirmation"))
    toggle.speed_limit_confirmation_higher = toggle.speed_limit_confirmation and (params.get_bool("SLCConfirmationHigher") if tuning_level >= level["SLCConfirmationHigher"] else default.get_bool("SLCConfirmationHigher"))
    toggle.speed_limit_confirmation_lower = toggle.speed_limit_confirmation and (params.get_bool("SLCConfirmationLower") if tuning_level >= level["SLCConfirmationLower"] else default.get_bool("SLCConfirmationLower"))
    slc_override_method = params.get_int("SLCOverride") if toggle.speed_limit_controller and tuning_level >= level["SLCOverride"] else default.get_int("SLCOverride")
    toggle.speed_limit_controller_override_manual = slc_override_method == 1
    toggle.speed_limit_controller_override_set_speed = slc_override_method == 2
    toggle.speed_limit_offset1 = (params.get_int("Offset1") * speed_conversion if tuning_level >= level["Offset1"] else default.get_int("Offset1") * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset2 = (params.get_int("Offset2") * speed_conversion if tuning_level >= level["Offset2"] else default.get_int("Offset2") * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset3 = (params.get_int("Offset3") * speed_conversion if tuning_level >= level["Offset3"] else default.get_int("Offset3") * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset4 = (params.get_int("Offset4") * speed_conversion if tuning_level >= level["Offset4"] else default.get_int("Offset4") * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_priority1 = params.get("SLCPriority1", encoding='utf-8') if toggle.speed_limit_controller and tuning_level >= level["SLCPriority1"] else default.get("SLCPriority1", encoding='utf-8')
    toggle.speed_limit_priority2 = params.get("SLCPriority2", encoding='utf-8') if toggle.speed_limit_controller and tuning_level >= level["SLCPriority2"] else default.get("SLCPriority2", encoding='utf-8')
    toggle.speed_limit_priority3 = params.get("SLCPriority3", encoding='utf-8') if toggle.speed_limit_controller and tuning_level >= level["SLCPriority3"] else default.get("SLCPriority3", encoding='utf-8')
    toggle.speed_limit_priority_highest = toggle.speed_limit_priority1 == "Highest"
    toggle.speed_limit_priority_lowest = toggle.speed_limit_priority1 == "Lowest"
    toggle.speed_limit_sources = toggle.speed_limit_controller and (params.get_bool("SpeedLimitSources") if tuning_level >= level["SpeedLimitSources"] else default.get_bool("SpeedLimitSources"))

    toggle.startup_alert_top = params.get("StartupMessageTop", encoding='utf-8') if tuning_level >= level["StartupMessageTop"] else default.get("StartupMessageTop", encoding='utf-8')
    toggle.startup_alert_bottom = params.get("StartupMessageBottom", encoding='utf-8') if tuning_level >= level["StartupMessageBottom"] else default.get("StartupMessageBottom", encoding='utf-8')

    toggle.tethering_config = params.get_int("TetheringEnabled")

    toyota_doors = car_make == "toyota" and (params.get_bool("ToyotaDoors") if tuning_level >= level["ToyotaDoors"] else default.get_bool("ToyotaDoors"))
    toggle.lock_doors = toyota_doors and (params.get_bool("LockDoors") if tuning_level >= level["LockDoors"] else default.get_bool("LockDoors"))
    toggle.unlock_doors = toyota_doors and (params.get_bool("UnlockDoors") if tuning_level >= level["UnlockDoors"] else default.get_bool("UnlockDoors"))

    toggle.volt_sng = car_model == "CHEVROLET_VOLT" and (params.get_bool("VoltSNG") if tuning_level >= level["VoltSNG"] else default.get_bool("VoltSNG"))

    params_memory.put("FrogPilotToggles", json.dumps(toggle.__dict__))
    params_memory.remove("FrogPilotTogglesUpdated")
