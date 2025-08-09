#!/usr/bin/env python3
import json
import numpy as np
import random

from functools import cache
from pathlib import Path
from types import SimpleNamespace

from cereal import car, custom, log
from openpilot.common.basedir import BASEDIR
from openpilot.common.conversions import Conversions as CV
from openpilot.common.params import Params
from openpilot.selfdrive.car import gen_empty_fingerprint
from openpilot.selfdrive.car.car_helpers import interfaces
from openpilot.selfdrive.car.gm.values import GMFlags
from openpilot.selfdrive.car.interfaces import CarInterfaceBase
from openpilot.selfdrive.car.mock.interface import CarInterface
from openpilot.selfdrive.car.mock.values import CAR as MOCK
from openpilot.selfdrive.car.toyota.values import ToyotaFlags, ToyotaFrogPilotFlags
from openpilot.selfdrive.controls.lib.desire_helper import LANE_CHANGE_SPEED_MIN
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.system.hardware import HARDWARE
from openpilot.system.hardware.power_monitoring import VBATT_PAUSE_CHARGING
from openpilot.system.version import get_build_metadata
from panda import ALTERNATIVE_EXPERIENCE, Panda

params = Params()
params_cache = Params("/cache/params")
params_default = Params("/dev/shm/params_default")
params_memory = Params("/dev/shm/params")
params_tracking = Params("/cache/tracking")

GearShifter = car.CarState.GearShifter
SafetyModel = car.CarParams.SafetyModel

CITY_SPEED_LIMIT = 25                     # 55mph is typically the minimum speed for highways
CRUISING_SPEED = 5                        # Roughly the speed cars go when not touching the gas while in drive
EARTH_RADIUS = 6378137                    # Radius of the Earth in meters
MINIMUM_LATERAL_ACCELERATION = 1.3        # m/s^2, typical minimum lateral acceleration when taking curves
PLANNER_TIME = ModelConstants.T_IDXS[-1]  # Length of time the model projects out for
THRESHOLD = 0.63                          # Requires the condition to be true for ~1 second

NON_DRIVING_GEARS = [GearShifter.neutral, GearShifter.park, GearShifter.reverse, GearShifter.unknown]

RESOURCES_REPO = "FrogAi/FrogPilot-Resources"

ACTIVE_THEME_PATH = Path(__file__).parents[1] / "assets/active_theme"
METADATAS_PATH = Path(__file__).parents[1] / "assets/model_metadata"
MODELS_PATH = Path("/data/models")
RANDOM_EVENTS_PATH = Path(__file__).parents[1] / "assets/random_events"
THEME_SAVE_PATH = Path("/data/themes")

ERROR_LOGS_PATH = Path("/data/error_logs")
SCREEN_RECORDINGS_PATH = Path("/data/media/screen_recordings")

BACKUP_PATH = Path("/cache/on_backup")

HD_LOGS_PATH = Path("/data/media/0/realdata_HD")
HD_PATH = Path("/cache/use_HD")

KONIK_LOGS_PATH = Path("/data/media/0/realdata_konik")
KONIK_PATH = Path("/cache/use_konik")

MAPD_PATH = Path("/data/media/0/osm/mapd")
MAPS_PATH = Path("/data/media/0/osm/offline")

NEURAL_PARAMS_PATH = Path(BASEDIR) / "selfdrive/car/torque_data/neural_ff_weights.json"
TORQUE_NN_MODEL_PATH = Path(BASEDIR) / "frogpilot/assets/nnff_models"

DEFAULT_CLASSIC_MODEL = "wd-40"
DEFAULT_CLASSIC_MODEL_NAME = "WD-40 (Default) 👀📡"
DEFAULT_CLASSIC_MODEL_VERSION = "v2"

DEFAULT_MODEL = "national-public-radio"
DEFAULT_MODEL_NAME = "National Public Radio 👀📡"
DEFAULT_MODEL_VERSION = "v6"

DEFAULT_TINYGRAD_MODEL = "space-lab"
DEFAULT_TINYGRAD_MODEL_NAME = "Space Lab 👀📡"
DEFAULT_TINYGRAD_MODEL_VERSION = "v7"

BUTTON_FUNCTIONS = {
  "NOTHING": 0,
  "PERSONALITY_PROFILE": 1,
  "FORCE_COAST": 2,
  "PAUSE_LATERAL": 3,
  "PAUSE_LONGITUDINAL": 4,
  "EXPERIMENTAL_MODE": 5,
  "TRAFFIC_MODE": 6
}

EXCLUDED_KEYS = {
  "AvailableModels", "AvailableModelNames", "CarParamsPersistent",
  "ExperimentalLongitudinalEnabled", "KonikMinutes", "MapBoxRequests", "ModelDrivesAndScores",
  "ModelVersions", "openpilotMinutes", "OverpassRequests", "SpeedLimits", "SpeedLimitsFiltered", "UpdaterAvailableBranches"
}

TINYGRAD_FILES = [
  ("driving_policy_metadata.pkl", "policy metadata"),
  ("driving_policy_tinygrad.pkl", "policy model"),
  ("driving_vision_metadata.pkl", "vision metadata"),
  ("driving_vision_tinygrad.pkl", "vision model"),
]

@cache
def get_comma_nnff_model_file():
  with open(NEURAL_PARAMS_PATH, "r") as file:
    return json.load(file)

def comma_nnff_supported(car):
  return car in get_comma_nnff_model_file()

@cache
def get_nnff_model_files():
  model_dir = Path(TORQUE_NN_MODEL_PATH)
  return [file.stem for file in model_dir.iterdir() if file.is_file()]

def nnff_supported(car_fingerprint):
  for file in get_nnff_model_files():
    if file.startswith(car_fingerprint):
      return True

  return False

def get_frogpilot_toggles(block=True):
  return SimpleNamespace(**json.loads(params_memory.get("FrogPilotToggles", block=block) or "{}"))

def update_frogpilot_toggles():
  params_memory.put_bool("FrogPilotTogglesUpdated", True)

frogpilot_default_params: list[tuple[str, str | bytes, int, str]] = [
  ("AccelerationPath", "1", 2, "0"),
  ("AccelerationProfile", "2", 0, "0"),
  ("AdjacentLeadsUI", "0", 3, "0"),
  ("AdjacentPath", "0", 3, "0"),
  ("AdjacentPathMetrics", "0", 3, "0"),
  ("AdvancedCustomUI", "0", 2, "0"),
  ("AdvancedLateralTune", "0", 2, "0"),
  ("AdvancedLongitudinalTune", "0", 3, "0"),
  ("AggressiveFollow", "1.25", 2, "1.25"),
  ("AggressiveJerkAcceleration", "50", 3, "50"),
  ("AggressiveJerkDanger", "100", 3, "100"),
  ("AggressiveJerkDeceleration", "50", 3, "50"),
  ("AggressiveJerkSpeed", "50", 3, "50"),
  ("AggressiveJerkSpeedDecrease", "50", 3, "50"),
  ("AggressivePersonalityProfile", "1", 2, "0"),
  ("AlertVolumeControl", "0", 2, "0"),
  ("AlwaysOnDM", "0", 0, "0"),
  ("AlwaysOnLateral", "1", 0, "0"),
  ("AlwaysOnLateralLKAS", "1", 0, "0"),
  ("AlwaysOnLateralMain", "1", 0, "0"),
  ("AMapKey1", "", 0, ""),
  ("AMapKey2", "", 0, ""),
  ("AutomaticallyDownloadModels", "1", 1, "0"),
  ("AutomaticUpdates", "1", 0, "1"),
  ("AvailableModelNames", "", 1, ""),
  ("AvailableModels", "", 1, ""),
  ("BigMap", "0", 2, "0"),
  ("BlacklistedModels", "", 2, ""),
  ("BlindSpotMetrics", "1", 3, "0"),
  ("BlindSpotPath", "1", 1, "0"),
  ("BorderMetrics", "0", 3, "0"),
  ("CameraView", "3", 2, "0"),
  ("CarMake", "", 0, ""),
  ("CarModel", "", 0, ""),
  ("CarModelName", "", 0, ""),
  ("CarParamsPersistent", "", 0, ""),
  ("CECurves", "0", 1, "0"),
  ("CECurvesLead", "0", 1, "0"),
  ("CELead", "0", 1, "0"),
  ("CEModelStopTime", str(PLANNER_TIME - 2), 2, "0"),
  ("CENavigation", "1", 2, "0"),
  ("CENavigationIntersections", "1", 2, "0"),
  ("CENavigationLead", "1", 2, "0"),
  ("CENavigationTurns", "1", 2, "0"),
  ("CESignalSpeed", "55", 2, "0"),
  ("CESignalLaneDetection", "1", 2, "0"),
  ("CESlowerLead", "0", 1, "0"),
  ("CESpeed", "0", 1, "0"),
  ("CESpeedLead", "0", 1, "0"),
  ("CEStoppedLead", "0", 1, "0"),
  ("ClusterOffset", "1.015", 2, "1.015"),
  ("Compass", "0", 1, "0"),
  ("ConditionalExperimental", "1", 0, "0"),
  ("CurveSensitivity", "100", 2, "100"),
  ("CurveSpeedControl", "1", 1, "0"),
  ("CustomAlerts", "1", 0, "0"),
  ("CustomColors", "frog", 0, "stock"),
  ("CustomCruise", "1", 2, "1"),
  ("CustomCruiseLong", "5", 2, "5"),
  ("CustomDistanceIcons", "stock", 0, "stock"),
  ("CustomIcons", "frog-animated", 0, "stock"),
  ("CustomPersonalities", "0", 2, "0"),
  ("CustomSignals", "frog", 0, "stock"),
  ("CustomSounds", "frog", 0, "stock"),
  ("CustomUI", "1", 1, "0"),
  ("DecelerationProfile", "1", 2, "0"),
  ("DeveloperMetrics", "1", 3, "0"),
  ("DeveloperSidebar", "1", 3, "0"),
  ("DeveloperSidebarMetric1", "1", 3, "0"),
  ("DeveloperSidebarMetric2", "2", 3, "0"),
  ("DeveloperSidebarMetric3", "3", 3, "0"),
  ("DeveloperSidebarMetric4", "4", 3, "0"),
  ("DeveloperSidebarMetric5", "5", 3, "0"),
  ("DeveloperSidebarMetric6", "6", 3, "0"),
  ("DeveloperSidebarMetric7", "7", 3, "0"),
  ("DeveloperWidgets", "1", 3, "0"),
  ("DeveloperUI", "0", 3, "0"),
  ("DeviceManagement", "1", 1, "0"),
  ("DeviceShutdown", "9", 1, "33"),
  ("DisableOnroadUploads", "0", 2, "0"),
  ("DisableOpenpilotLongitudinal", "0", 0, "0"),
  ("DiscordUsername", "", 0, ""),
  ("DisengageVolume", "101", 2, "101"),
  ("DistanceButtonControl", "1", 2, "0"),
  ("DriverCamera", "0", 1, "0"),
  ("DynamicPathWidth", "0", 2, "0"),
  ("DynamicPedalsOnUI", "1", 2, "0"),
  ("EngageVolume", "101", 2, "101"),
  ("ExperimentalGMTune", "0", 2, "0"),
  ("ExperimentalLongitudinalEnabled", "0", 0, "0"),
  ("ExperimentalModeConfirmed", "0", 0, "0"),
  ("Fahrenheit", "0", 3, "0"),
  ("FavoriteDestinations", "", 0, ""),
  ("ForceAutoTune", "0", 2, "0"),
  ("ForceAutoTuneOff", "0", 2, "0"),
  ("ForceFingerprint", "0", 2, "0"),
  ("ForceMPHDashboard", "0", 2, "0"),
  ("ForceStandstill", "0", 2, "0"),
  ("ForceStops", "0", 2, "0"),
  ("FPSCounter", "1", 3, "0"),
  ("FrogPilotDongleId", "", 0, ""),
  ("FrogPilotStats", "", 0, ""),
  ("FrogsGoMoosTweak", "1", 2, "0"),
  ("FullMap", "0", 2, "0"),
  ("GasRegenCmd", "1", 2, "0"),
  ("GithubSshKeys", "", 0, ""),
  ("GithubUsername", "", 0, ""),
  ("GoatScream", "0", 1, "0"),
  ("GreenLightAlert", "0", 0, "0"),
  ("GsmApn", "", 0, ""),
  ("GsmRoaming", "1", 0, "0"),
  ("HideAlerts", "0", 2, "0"),
  ("HideLeadMarker", "0", 2, "0"),
  ("HideMapIcon", "0", 2, "0"),
  ("HideMaxSpeed", "0", 2, "0"),
  ("HideSpeed", "0", 2, "0"),
  ("HideSpeedLimit", "0", 2, "0"),
  ("HigherBitrate", "0", 3, "0"),
  ("HolidayThemes", "1", 0, "0"),
  ("HumanAcceleration", "1", 2, "0"),
  ("HumanFollowing", "1", 2, "0"),
  ("IncreasedStoppedDistance", "0", 1, "0"),
  ("IncreaseThermalLimits", "0", 2, "0"),
  ("IsLdwEnabled", "0", 0, "0"),
  ("IsMetric", "0", 0, "0"),
  ("KonikDongleId", "", 0, ""),
  ("KonikMinutes", "0", 0, "0"),
  ("LaneChangeCustomizations", "1", 0, "1"),
  ("LaneChangeTime", "2.0", 0, "0"),
  ("LaneDetectionWidth", "0", 2, "0"),
  ("LaneLinesWidth", "4", 2, "2"),
  ("LateralTune", "1", 2, "0"),
  ("LeadDepartingAlert", "0", 0, "0"),
  ("LeadDetectionThreshold", "35", 3, "50"),
  ("LeadInfo", "1", 2, "0"),
  ("LiveDelay", "", 0, ""),
  ("LKASButtonControl", "5", 2, "0"),
  ("LockDoors", "1", 0, "0"),
  ("LockDoorsTimer", "0", 0, "0"),
  ("LongDistanceButtonControl", "5", 2, "0"),
  ("LongitudinalActuatorDelay", "", 3, ""),
  ("LongitudinalActuatorDelayStock", "", 3, ""),
  ("LongitudinalTune", "1", 0, "0"),
  ("LongPitch", "1", 2, "0"),
  ("LoudBlindspotAlert", "0", 0, "0"),
  ("LowVoltageShutdown", str(VBATT_PAUSE_CHARGING), 2, str(VBATT_PAUSE_CHARGING)),
  ("MapAcceleration", "0", 1, "0"),
  ("MapboxPublicKey", "", 0, ""),
  ("MapboxSecretKey", "", 0, ""),
  ("MapDeceleration", "0", 1, "0"),
  ("MapGears", "0", 1, "0"),
  ("MapsSelected", "", 0, ""),
  ("MapStyle", "1", 2, "0"),
  ("MapTurnControl", "1", 1, "0"),
  ("MaxDesiredAcceleration", "4.0", 3, "2.0"),
  ("MinimumLaneChangeSpeed", str(LANE_CHANGE_SPEED_MIN / CV.MPH_TO_MS), 2, str(LANE_CHANGE_SPEED_MIN / CV.MPH_TO_MS)),
  ("Model", DEFAULT_CLASSIC_MODEL, 1, DEFAULT_CLASSIC_MODEL),
  ("ModelDrivesAndScores", "", 2, ""),
  ("ModelRandomizer", "0", 2, "0"),
  ("ModelUI", "1", 2, "0"),
  ("ModelVersions", "", 2, ""),
  ("MTSCCurvatureCheck", "1", 2, "1"),
  ("NavigationUI", "1", 1, "0"),
  ("NavSettingLeftSide", "0", 0, "0"),
  ("NavSettingTime24h", "0", 0, "0"),
  ("NewLongAPI", "1", 2, "1"),
  ("NNFF", "1", 2, "0"),
  ("NNFFLite", "1", 2, "0"),
  ("NoLogging", "0", 2, "0"),
  ("NoUploads", "0", 2, "0"),
  ("NudgelessLaneChange", "0", 0, "0"),
  ("NumericalTemp", "1", 2, "0"),
  ("Offset1", "5", 0, "0"),
  ("Offset2", "5", 0, "0"),
  ("Offset3", "5", 0, "0"),
  ("Offset4", "5", 0, "0"),
  ("Offset5", "10", 0, "0"),
  ("Offset6", "10", 0, "0"),
  ("Offset7", "10", 0, "0"),
  ("OneLaneChange", "1", 2, "0"),
  ("OnroadDistanceButton", "0", 0, "0"),
  ("openpilotMinutes", "0", 0, "0"),
  ("PathEdgeWidth", "20", 2, "0"),
  ("PathWidth", "6.1", 2, "5.9"),
  ("PauseAOLOnBrake", "0", 2, "0"),
  ("PauseLateralOnSignal", "0", 2, "0"),
  ("PauseLateralSpeed", "0", 2, "0"),
  ("PedalsOnUI", "0", 2, "0"),
  ("PersonalizeOpenpilot", "1", 0, "0"),
  ("PreferredSchedule", "2", 0, "0"),
  ("PromptDistractedVolume", "101", 2, "101"),
  ("PromptVolume", "101", 2, "101"),
  ("QOLLateral", "1", 2, "0"),
  ("QOLLongitudinal", "1", 1, "0"),
  ("QOLVisuals", "1", 0, "0"),
  ("RadarTracksUI", "0", 3, "0"),
  ("RainbowPath", "0", 1, "0"),
  ("RandomEvents", "0", 1, "0"),
  ("RandomThemes", "0", 1, "0"),
  ("RecordFront", "0", 0, "0"),
  ("RefuseVolume", "101", 2, "101"),
  ("RelaxedFollow", "1.75", 2, "1.75"),
  ("RelaxedJerkAcceleration", "100", 3, "100"),
  ("RelaxedJerkDanger", "100", 3, "100"),
  ("RelaxedJerkDeceleration", "100", 3, "100"),
  ("RelaxedJerkSpeed", "100", 3, "100"),
  ("RelaxedJerkSpeedDecrease", "100", 3, "100"),
  ("RelaxedPersonalityProfile", "1", 2, "0"),
  ("ReverseCruise", "0", 1, "0"),
  ("RoadEdgesWidth", "2", 2, "2"),
  ("RoadNameUI", "1", 2, "0"),
  ("RotatingWheel", "1", 1, "0"),
  ("ScreenBrightness", "101", 2, "101"),
  ("ScreenBrightnessOnroad", "101", 2, "101"),
  ("ScreenManagement", "1", 2, "0"),
  ("ScreenRecorder", "1", 2, "0"),
  ("ScreenTimeout", "30", 2, "30"),
  ("ScreenTimeoutOnroad", "30", 2, "10"),
  ("SearchInput", "0", 0, "0"),
  ("SecOCKey", "", 0, ""),
  ("SecOCKeys", "", 0, ""),
  ("SetSpeedLimit", "0", 1, "0"),
  ("SetSpeedOffset", "0", 2, "0"),
  ("ShowCEMStatus", "1", 2, "0"),
  ("ShowCPU", "1", 3, "0"),
  ("ShowCSCStatus", "1", 2, "0"),
  ("ShowGPU", "0", 3, "0"),
  ("ShowIP", "0", 3, "0"),
  ("ShowMemoryUsage", "1", 3, "0"),
  ("ShowSLCOffset", "1", 0, "0"),
  ("ShowSpeedLimits", "1", 1, "0"),
  ("ShowSteering", "0", 3, "0"),
  ("ShowStoppingPoint", "0", 2, "0"),
  ("ShowStoppingPointMetrics", "0", 2, "0"),
  ("ShowStorageLeft", "0", 3, "0"),
  ("ShowStorageUsed", "0", 3, "0"),
  ("Sidebar", "0", 0, "0"),
  ("SignalMetrics", "0", 2, "0"),
  ("SLCConfirmation", "0", 0, "0"),
  ("SLCConfirmationHigher", "0", 0, "0"),
  ("SLCConfirmationLower", "0", 0, "0"),
  ("SLCFallback", "2", 1, "0"),
  ("SLCLookaheadHigher", "0", 2, "0"),
  ("SLCLookaheadLower", "0", 2, "0"),
  ("SLCMapboxFiller", "1", 1, "0"),
  ("SLCOverride", "1", 1, "0"),
  ("SLCPriority1", "Navigation", 2, "Navigation"),
  ("SLCPriority2", "Map Data", 2, "Map Data"),
  ("SLCPriority3", "Dashboard", 2, "Dashboard"),
  ("SNGHack", "1", 2, "0"),
  ("SpeedLimitChangedAlert", "1", 0, "0"),
  ("SpeedLimitController", "1", 0, "0"),
  ("SpeedLimitFiller", "0", 0, "0"),
  ("SpeedLimitSources", "0", 2, "0"),
  ("SshEnabled", "0", 0, "0"),
  ("StartupMessageBottom", "Human-tested, frog-approved 🐸", 0, "Always keep hands on wheel and eyes on road"),
  ("StartupMessageTop", "Hop in and buckle up!", 0, "Be ready to take over at any time"),
  ("StandardFollow", "1.45", 2, "1.45"),
  ("StandardJerkAcceleration", "100", 3, "100"),
  ("StandardJerkDanger", "100", 3, "100"),
  ("StandardJerkDeceleration", "100", 3, "100"),
  ("StandardJerkSpeed", "100", 3, "100"),
  ("StandardJerkSpeedDecrease", "100", 3, "100"),
  ("StandardPersonalityProfile", "1", 2, "0"),
  ("StandbyMode", "0", 2, "0"),
  ("StartAccel", "", 3, ""),
  ("StartAccelStock", "", 3, ""),
  ("StaticPedalsOnUI", "0", 2, "0"),
  ("SteerDelay", "", 3, ""),
  ("SteerDelayStock", "", 3, ""),
  ("SteerFriction", "", 3, ""),
  ("SteerFrictionStock", "", 3, ""),
  ("SteerKP", "", 3, ""),
  ("SteerKPStock", "", 3, ""),
  ("SteerLatAccel", "", 3, ""),
  ("SteerLatAccelStock", "", 3, ""),
  ("SteerRatio", "", 3, ""),
  ("SteerRatioStock", "", 3, ""),
  ("StopAccel", "", 3, ""),
  ("StopAccelStock", "", 3, ""),
  ("StoppingDecelRate", "", 3, ""),
  ("StoppingDecelRateStock", "", 3, ""),
  ("StoppedTimer", "0", 1, "0"),
  ("TacoTune", "0", 2, "0"),
  ("TacoTuneHacks", "0", 2, "0"),
  ("TetheringEnabled", "0", 0, "0"),
  ("ToyotaDoors", "1", 0, "0"),
  ("TrafficFollow", "0.5", 2, "0.5"),
  ("TrafficJerkAcceleration", "50", 3, "50"),
  ("TrafficJerkDanger", "100", 3, "100"),
  ("TrafficJerkDeceleration", "50", 3, "50"),
  ("TrafficJerkSpeed", "50", 3, "50"),
  ("TrafficJerkSpeedDecrease", "50", 3, "50"),
  ("TrafficPersonalityProfile", "1", 2, "0"),
  ("TuningLevel", "0", 0, "0"),
  ("TuningLevelConfirmed", "0", 0, "0"),
  ("TurnAggressiveness", "100", 2, "100"),
  ("TurnDesires", "0", 2, "0"),
  ("UnlimitedLength", "1", 2, "0"),
  ("UnlockDoors", "1", 0, "0"),
  ("UpdaterAvailableBranches", "", 0, ""),
  ("UseKonikServer", "0", 2, "0"),
  ("UseSI", "1", 3, "1"),
  ("UseVienna", "0", 1, "0"),
  ("VEgoStarting", "", 3, ""),
  ("VEgoStartingStock", "", 3, ""),
  ("VEgoStopping", "", 3, ""),
  ("VEgoStoppingStock", "", 3, ""),
  ("VeryLongDistanceButtonControl", "6", 2, "0"),
  ("VisionTurnControl", "1", 1, "0"),
  ("VoltSNG", "0", 2, "0"),
  ("WarningImmediateVolume", "101", 2, "101"),
  ("WarningSoftVolume", "101", 2, "101"),
  ("WheelIcon", "frog", 0, "stock"),
  ("WheelSpeed", "0", 2, "0")
]

misc_tuning_levels: list[tuple[str, str | bytes, int, str]] = [
  ("SidebarMetrics", "1", 3, "0"),
  ("SLCPriority", "", 2, ""),
  ("WheelControls", "", 2, "")
]

class FrogPilotVariables:
  def __init__(self):
    self.frogpilot_toggles = get_frogpilot_toggles(block=False)
    self.tuning_levels = {key: lvl for key, _, lvl, _ in frogpilot_default_params + misc_tuning_levels}

    short_branch = get_build_metadata().channel
    self.development_branch = short_branch == "FrogPilot-Development"
    self.release_branch = short_branch == "FrogPilot"
    self.staging_branch = short_branch == "FrogPilot-Staging"
    self.testing_branch = short_branch == "FrogPilot-Testing"
    self.vetting_branch = short_branch == "FrogPilot-Vetting"

    default = params_default
    level = self.tuning_levels
    toggle = self.frogpilot_toggles

    toggle.frogs_go_moo = Path("/persist/frogsgomoo.py").is_file()
    toggle.block_user = (self.development_branch or short_branch == "MAKE-PRS-HERE" or self.vetting_branch) and not toggle.frogs_go_moo

    tuning_level = params.get_int("TuningLevel") if params.get_bool("TuningLevelConfirmed") else 3

    device_management = params.get_bool("DeviceManagement") if tuning_level >= level["DeviceManagement"] else default.get_bool("DeviceManagement")

    toggle.use_higher_bitrate = device_management
    toggle.use_higher_bitrate &= params.get_bool("HigherBitrate") if tuning_level >= level["HigherBitrate"] else default.get_bool("HigherBitrate")
    toggle.use_higher_bitrate &= params.get_bool("NoUploads") if tuning_level >= level["NoUploads"] else default.get_bool("NoUploads")
    toggle.use_higher_bitrate &= not (params.get_bool("DisableOnroadUploads") if tuning_level >= level["DisableOnroadUploads"] else default.get_bool("DisableOnroadUploads"))
    toggle.use_higher_bitrate &= not self.vetting_branch
    toggle.use_higher_bitrate |= self.development_branch

    if not HD_PATH.is_file() and toggle.use_higher_bitrate:
      HD_PATH.touch()

      HARDWARE.reboot()
    elif HD_PATH.is_file() and not toggle.use_higher_bitrate:
      HD_PATH.unlink()

      HARDWARE.reboot()

    toggle.use_konik_server = device_management
    toggle.use_konik_server &= params.get_bool("UseKonikServer") if tuning_level >= level["UseKonikServer"] else default.get_bool("UseKonikServer")
    toggle.use_konik_server |= Path("/data/openpilot/not_vetted").is_file()

    if not KONIK_PATH.is_file() and toggle.use_konik_server:
      KONIK_PATH.touch()

      HARDWARE.reboot()
    elif KONIK_PATH.is_file() and not toggle.use_konik_server:
      KONIK_PATH.unlink()

      HARDWARE.reboot()

    for k, v, _, _ in frogpilot_default_params:
      params_default.put(k, v)

    params_memory.put("FrogPilotTuningLevels", json.dumps(self.tuning_levels))

  def update(self, holiday_theme, started, boot_run=False):
    default = params_default
    level = self.tuning_levels
    toggle = self.frogpilot_toggles

    toggle.debug_mode = params.get_bool("DebugMode")
    toggle.force_offroad = params_memory.get_bool("ForceOffroad")
    toggle.force_onroad = params_memory.get_bool("ForceOnroad")

    tuning_level = params.get_int("TuningLevel") if params.get_bool("TuningLevelConfirmed") else 3

    toggle.is_metric = params.get_bool("IsMetric")
    distance_conversion = 1 if toggle.is_metric else CV.FOOT_TO_METER
    small_distance_conversion = 1 if toggle.is_metric else CV.INCH_TO_CM
    speed_conversion = CV.KPH_TO_MS if toggle.is_metric else CV.MPH_TO_MS

    msg_bytes = params.get("CarParams" if started else "CarParamsPersistent", block=started)
    if msg_bytes:
      with car.CarParams.from_bytes(msg_bytes) as cp_reader:
        CP = cp_reader.as_builder()
    else:
      CarInterface, _, _ = interfaces[MOCK.MOCK]
      CP = CarInterface.get_params(MOCK.MOCK, gen_empty_fingerprint(), [], False, toggle, params, False)
      CarInterface.configure_torque_tune(MOCK.MOCK, CP.lateralTuning)

      safety_config = car.CarParams.SafetyConfig.new_message()
      safety_config.safetyModel = car.CarParams.SafetyModel.noOutput
      CP.safetyConfigs = [safety_config]

    fpmsg_bytes = params.get("FrogPilotCarParams" if started else "FrogPilotCarParamsPersistent", block=started)
    if fpmsg_bytes:
      with custom.FrogPilotCarParams.from_bytes(fpmsg_bytes) as fpcp_reader:
        FPCP = fpcp_reader.as_builder()
    else:
      CarInterface, _, _ = interfaces[MOCK.MOCK]
      FPCP = CarInterface.get_frogpilot_params(MOCK.MOCK, gen_empty_fingerprint(), [], toggle)

    is_torque_car = CP.lateralTuning.which() == "torque"
    if not is_torque_car:
      CarInterfaceBase.configure_torque_tune("MOCK", CP.lateralTuning)

    always_on_lateral_set = bool(CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL)
    toggle.car_make = CP.carName
    toggle.car_model = CP.carFingerprint
    toggle.disable_openpilot_long = params.get_bool("DisableOpenpilotLongitudinal") if tuning_level >= level["DisableOpenpilotLongitudinal"] else default.get_bool("DisableOpenpilotLongitudinal")
    friction = CP.lateralTuning.torque.friction
    has_auto_tune = toggle.car_make in {"hyundai", "toyota"} and CP.lateralTuning.which() == "torque"
    has_bsm = CP.enableBsm
    toggle.has_cc_long = toggle.car_make == "gm" and bool(CP.flags & GMFlags.CC_LONG.value)
    has_nnff = not comma_nnff_supported(toggle.car_model) and nnff_supported(toggle.car_model)
    toggle.has_pedal = CP.enableGasInterceptor
    has_radar = not CP.radarUnavailable
    toggle.has_sdsu = toggle.car_make == "toyota" and bool(CP.flags & ToyotaFlags.SMART_DSU.value)
    has_sng = CP.autoResumeSng
    toggle.has_zss = toggle.car_make == "toyota" and bool(FPCP.fpFlags & ToyotaFrogPilotFlags.ZSS.value)
    is_angle_car = CP.steerControlType == car.CarParams.SteerControlType.angle
    latAccelFactor = CP.lateralTuning.torque.latAccelFactor
    longitudinalActuatorDelay = CP.longitudinalActuatorDelay
    max_acceleration_enabled = bool(CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.RAISE_LONGITUDINAL_LIMITS_TO_ISO_MAX)
    toggle.openpilot_longitudinal = CP.openpilotLongitudinalControl and not toggle.disable_openpilot_long
    pcm_cruise = CP.pcmCruise
    startAccel = CP.startAccel
    stopAccel = CP.stopAccel
    steerActuatorDelay = CP.steerActuatorDelay
    steerKp = CP.lateralTuning.torque.kp
    steerRatio = CP.steerRatio
    toggle.stoppingDecelRate = CP.stoppingDecelRate
    taco_hacks_allowed = CP.safetyConfigs[0].safetyModel == SafetyModel.hyundaiCanfd
    toggle.use_lkas_for_aol = not toggle.openpilot_longitudinal and CP.safetyConfigs[0].safetyModel == SafetyModel.hyundaiCanfd
    toggle.vEgoStarting = CP.vEgoStarting
    toggle.vEgoStopping = CP.vEgoStopping

    msg_bytes = params.get("LiveTorqueParameters")
    if msg_bytes:
      with log.LiveTorqueParametersData.from_bytes(msg_bytes) as LTP:
        toggle.liveValid = LTP.liveValid
    else:
      toggle.liveValid = False

    if not toggle.use_lkas_for_aol:
      params.remove("AlwaysOnLateralLKAS")

    advanced_custom_ui = params.get_bool("AdvancedCustomUI") if tuning_level >= level["AdvancedCustomUI"] else default.get_bool("AdvancedCustomUI")
    toggle.hide_alerts = advanced_custom_ui and (params.get_bool("HideAlerts") if tuning_level >= level["HideAlerts"] else default.get_bool("HideAlerts")) and not toggle.debug_mode
    toggle.hide_lead_marker = toggle.openpilot_longitudinal and (advanced_custom_ui and (params.get_bool("HideLeadMarker") if tuning_level >= level["HideLeadMarker"] else default.get_bool("HideLeadMarker")) and not toggle.debug_mode)
    toggle.hide_map_icon = advanced_custom_ui and (params.get_bool("HideMapIcon") if tuning_level >= level["HideMapIcon"] else default.get_bool("HideMapIcon")) and not toggle.debug_mode
    toggle.hide_max_speed = advanced_custom_ui and (params.get_bool("HideMaxSpeed") if tuning_level >= level["HideMaxSpeed"] else default.get_bool("HideMaxSpeed")) and not toggle.debug_mode
    toggle.hide_speed = advanced_custom_ui and (params.get_bool("HideSpeed") if tuning_level >= level["HideSpeed"] else default.get_bool("HideSpeed")) and not toggle.debug_mode
    toggle.hide_speed_limit = advanced_custom_ui and (params.get_bool("HideSpeedLimit") if tuning_level >= level["HideSpeedLimit"] else default.get_bool("HideSpeedLimit")) and not toggle.debug_mode
    toggle.use_wheel_speed = advanced_custom_ui and (params.get_bool("WheelSpeed") if tuning_level >= level["WheelSpeed"] else default.get_bool("WheelSpeed"))

    advanced_lateral_tuning = params.get_bool("AdvancedLateralTune") if tuning_level >= level["AdvancedLateralTune"] else default.get_bool("AdvancedLateralTune")
    toggle.force_auto_tune = advanced_lateral_tuning and not has_auto_tune and is_torque_car and (params.get_bool("ForceAutoTune") if tuning_level >= level["ForceAutoTune"] else default.get_bool("ForceAutoTune"))
    toggle.force_auto_tune_off = advanced_lateral_tuning and has_auto_tune and is_torque_car and (params.get_bool("ForceAutoTuneOff") if tuning_level >= level["ForceAutoTuneOff"] else default.get_bool("ForceAutoTuneOff"))
    toggle.steerActuatorDelay = np.clip(params.get_float("SteerDelay"), 0.01, 1.0) if advanced_lateral_tuning and tuning_level >= level["SteerDelay"] else steerActuatorDelay
    toggle.use_custom_steerActuatorDelay = bool(toggle.steerActuatorDelay != steerActuatorDelay)
    toggle.friction = np.clip(params.get_float("SteerFriction"), 0, 0.5) if advanced_lateral_tuning and tuning_level >= level["SteerFriction"] else friction
    toggle.use_custom_friction = toggle.friction != friction and is_torque_car and not toggle.force_auto_tune or toggle.force_auto_tune_off
    toggle.steerKp = [[0], [np.clip(params.get_float("SteerKP"), steerKp * 0.5, steerKp * 1.5) if advanced_lateral_tuning and is_torque_car and tuning_level >= level["SteerKP"] else steerKp]]
    toggle.latAccelFactor = np.clip(params.get_float("SteerLatAccel"), latAccelFactor * 0.75, latAccelFactor * 1.25) if advanced_lateral_tuning and tuning_level >= level["SteerLatAccel"] else latAccelFactor
    toggle.use_custom_latAccelFactor = toggle.latAccelFactor != latAccelFactor and is_torque_car and not toggle.force_auto_tune or toggle.force_auto_tune_off
    toggle.steerRatio = np.clip(params.get_float("SteerRatio"), steerRatio * 0.5, steerRatio * 1.5) if advanced_lateral_tuning and tuning_level >= level["SteerRatio"] else steerRatio
    toggle.use_custom_steerRatio = toggle.steerRatio != steerRatio and not toggle.force_auto_tune or toggle.force_auto_tune_off

    advanced_longitudinal_tuning = params.get_bool("AdvancedLongitudinalTune") if tuning_level >= level["AdvancedLongitudinalTune"] else default.get_bool("AdvancedLongitudinalTune")
    toggle.longitudinalActuatorDelay = np.clip(params.get_float("LongitudinalActuatorDelay"), 0, 1) if advanced_longitudinal_tuning and tuning_level >= level["LongitudinalActuatorDelay"] else longitudinalActuatorDelay
    toggle.startAccel = np.clip(params.get_float("StartAccel"), 0, 4) if advanced_longitudinal_tuning and tuning_level >= level["StartAccel"] else startAccel
    toggle.stopAccel = np.clip(params.get_float("StopAccel"), -4, 0) if advanced_longitudinal_tuning and tuning_level >= level["StopAccel"] else stopAccel
    toggle.stoppingDecelRate = np.clip(params.get_float("StoppingDecelRate"), 0.001, 1) if advanced_longitudinal_tuning and tuning_level >= level["StoppingDecelRate"] else toggle.stoppingDecelRate
    toggle.vEgoStarting = np.clip(params.get_float("VEgoStarting"), 0.01, 1) if advanced_longitudinal_tuning and tuning_level >= level["VEgoStarting"] else toggle.vEgoStarting
    toggle.vEgoStopping = np.clip(params.get_float("VEgoStopping"), 0.01, 1) if advanced_longitudinal_tuning and tuning_level >= level["VEgoStopping"] else toggle.vEgoStopping

    toggle.alert_volume_controller = params.get_bool("AlertVolumeControl") if tuning_level >= level["AlertVolumeControl"] else default.get_bool("AlertVolumeControl")
    toggle.disengage_volume = params.get_int("DisengageVolume") if toggle.alert_volume_controller and tuning_level >= level["DisengageVolume"] else default.get_int("DisengageVolume")
    toggle.engage_volume = params.get_int("EngageVolume") if toggle.alert_volume_controller and tuning_level >= level["EngageVolume"] else default.get_int("EngageVolume")
    toggle.prompt_volume = params.get_int("PromptVolume") if toggle.alert_volume_controller and tuning_level >= level["PromptVolume"] else default.get_int("PromptVolume")
    toggle.promptDistracted_volume = params.get_int("PromptDistractedVolume") if toggle.alert_volume_controller and tuning_level >= level["PromptDistractedVolume"] else default.get_int("PromptDistractedVolume")
    toggle.refuse_volume = params.get_int("RefuseVolume") if toggle.alert_volume_controller and tuning_level >= level["RefuseVolume"] else default.get_int("RefuseVolume")
    toggle.warningSoft_volume = params.get_int("WarningSoftVolume") if toggle.alert_volume_controller and tuning_level >= level["WarningSoftVolume"] else default.get_int("WarningSoftVolume")
    toggle.warningImmediate_volume = max(params.get_int("WarningImmediateVolume"), 25) if toggle.alert_volume_controller and tuning_level >= level["WarningImmediateVolume"] else default.get_int("WarningImmediateVolume")

    toggle.always_on_lateral = params.get_bool("AlwaysOnLateral") if tuning_level >= level["AlwaysOnLateral"] else default.get_bool("AlwaysOnLateral")
    toggle.always_on_lateral_set = toggle.always_on_lateral and always_on_lateral_set
    toggle.always_on_lateral_lkas = toggle.always_on_lateral_set and toggle.use_lkas_for_aol and (params.get_bool("AlwaysOnLateralLKAS") if tuning_level >= level["AlwaysOnLateralLKAS"] else default.get_bool("AlwaysOnLateralLKAS"))
    toggle.always_on_lateral_main = toggle.always_on_lateral_set and not toggle.use_lkas_for_aol and (params.get_bool("AlwaysOnLateralMain") if tuning_level >= level["AlwaysOnLateralMain"] else default.get_bool("AlwaysOnLateralMain"))
    toggle.always_on_lateral_pause_speed = params.get_int("PauseAOLOnBrake") if toggle.always_on_lateral_set and tuning_level >= level["PauseAOLOnBrake"] else default.get_int("PauseAOLOnBrake")

    toggle.automatic_updates = (params.get_bool("AutomaticUpdates") if tuning_level >= level["AutomaticUpdates"] and (self.release_branch or self.vetting_branch) else default.get_bool("AutomaticUpdates")) and not BACKUP_PATH.is_file()

    toggle.car_model = params.get("CarModel", encoding="utf-8") or toggle.car_model

    toggle.cluster_offset = params.get_float("ClusterOffset") if toggle.car_make == "toyota" and tuning_level >= level["ClusterOffset"] else default.get_float("ClusterOffset")

    toggle.conditional_experimental_mode = toggle.openpilot_longitudinal and (params.get_bool("ConditionalExperimental") if tuning_level >= level["ConditionalExperimental"] else default.get_bool("ConditionalExperimental"))
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
    toggle.cem_status = toggle.conditional_experimental_mode and (params.get_bool("ShowCEMStatus") if tuning_level >= level["ShowCEMStatus"] else default.get_bool("ShowCEMStatus")) or toggle.debug_mode

    toggle.curve_speed_controller = toggle.openpilot_longitudinal and (params.get_bool("CurveSpeedControl") if tuning_level >= level["CurveSpeedControl"] else default.get_bool("CurveSpeedControl"))
    toggle.curve_sensitivity = params.get_int("CurveSensitivity") / 100 if toggle.curve_speed_controller and tuning_level >= level["CurveSensitivity"] else default.get_int("CurveSensitivity") / 100
    toggle.turn_aggressiveness = params.get_int("TurnAggressiveness") / 100 if toggle.curve_speed_controller and tuning_level >= level["TurnAggressiveness"] else default.get_int("TurnAggressiveness") / 100
    toggle.map_turn_speed_controller = toggle.curve_speed_controller and (params.get_bool("MapTurnControl") if tuning_level >= level["MapTurnControl"] else default.get_bool("MapTurnControl"))
    toggle.mtsc_curvature_check = toggle.map_turn_speed_controller and (params.get_bool("MTSCCurvatureCheck") if tuning_level >= level["MTSCCurvatureCheck"] else default.get_bool("MTSCCurvatureCheck"))
    toggle.vision_turn_speed_controller = toggle.curve_speed_controller and (params.get_bool("VisionTurnControl") if tuning_level >= level["VisionTurnControl"] else default.get_bool("VisionTurnControl"))
    toggle.csc_status = toggle.curve_speed_controller and (params.get_bool("ShowCSCStatus") if tuning_level >= level["ShowCSCStatus"] else default.get_bool("ShowCSCStatus")) or toggle.debug_mode

    toggle.custom_alerts = params.get_bool("CustomAlerts") if tuning_level >= level["CustomAlerts"] else default.get_bool("CustomAlerts")
    toggle.goat_scream_alert = toggle.custom_alerts and (params.get_bool("GoatScream") if tuning_level >= level["GoatScream"] else default.get_bool("GoatScream"))
    toggle.green_light_alert = toggle.custom_alerts and (params.get_bool("GreenLightAlert") if tuning_level >= level["GreenLightAlert"] else default.get_bool("GreenLightAlert"))
    toggle.lead_departing_alert = toggle.custom_alerts and (params.get_bool("LeadDepartingAlert") if tuning_level >= level["LeadDepartingAlert"] else default.get_bool("LeadDepartingAlert"))
    toggle.loud_blindspot_alert = has_bsm and toggle.custom_alerts and (params.get_bool("LoudBlindspotAlert") if tuning_level >= level["LoudBlindspotAlert"] else default.get_bool("LoudBlindspotAlert"))
    toggle.speed_limit_changed_alert = toggle.custom_alerts and (params.get_bool("SpeedLimitChangedAlert") if tuning_level >= level["SpeedLimitChangedAlert"] else default.get_bool("SpeedLimitChangedAlert"))

    toggle.custom_personalities = toggle.openpilot_longitudinal and params.get_bool("CustomPersonalities") if tuning_level >= level["CustomPersonalities"] else default.get_bool("CustomPersonalities")
    aggressive_profile = toggle.custom_personalities and (params.get_bool("AggressivePersonalityProfile") if tuning_level >= level["AggressivePersonalityProfile"] else default.get_bool("AggressivePersonalityProfile"))
    toggle.aggressive_jerk_acceleration = np.clip(params.get_int("AggressiveJerkAcceleration") / 100, 0.25, 2) if aggressive_profile and tuning_level >= level["AggressiveJerkAcceleration"] else default.get_int("AggressiveJerkAcceleration") / 100
    toggle.aggressive_jerk_deceleration = np.clip(params.get_int("AggressiveJerkDeceleration") / 100, 0.25, 2) if aggressive_profile and tuning_level >= level["AggressiveJerkDeceleration"] else default.get_int("AggressiveJerkDeceleration") / 100
    toggle.aggressive_jerk_danger = np.clip(params.get_int("AggressiveJerkDanger") / 100, 0.25, 2) if aggressive_profile and tuning_level >= level["AggressiveJerkDanger"] else default.get_int("AggressiveJerkDanger") / 100
    toggle.aggressive_jerk_speed = np.clip(params.get_int("AggressiveJerkSpeed") / 100, 0.25, 2) if aggressive_profile and tuning_level >= level["AggressiveJerkSpeed"] else default.get_int("AggressiveJerkSpeed") / 100
    toggle.aggressive_jerk_speed_decrease = np.clip(params.get_int("AggressiveJerkSpeedDecrease") / 100, 0.25, 2) if aggressive_profile and tuning_level >= level["AggressiveJerkSpeedDecrease"] else default.get_int("AggressiveJerkSpeedDecrease") / 100
    toggle.aggressive_follow = np.clip(params.get_float("AggressiveFollow"), 1, 5) if aggressive_profile and tuning_level >= level["AggressiveFollow"] else default.get_float("AggressiveFollow")
    standard_profile = toggle.custom_personalities and (params.get_bool("StandardPersonalityProfile") if tuning_level >= level["StandardPersonalityProfile"] else default.get_bool("StandardPersonalityProfile"))
    toggle.standard_jerk_acceleration = np.clip(params.get_int("StandardJerkAcceleration") / 100, 0.25, 2) if standard_profile and tuning_level >= level["StandardJerkAcceleration"] else default.get_int("StandardJerkAcceleration") / 100
    toggle.standard_jerk_deceleration = np.clip(params.get_int("StandardJerkDeceleration") / 100, 0.25, 2) if standard_profile and tuning_level >= level["StandardJerkDeceleration"] else default.get_int("StandardJerkDeceleration") / 100
    toggle.standard_jerk_danger = np.clip(params.get_int("StandardJerkDanger") / 100, 0.25, 2) if standard_profile and tuning_level >= level["StandardJerkDanger"] else default.get_int("StandardJerkDanger") / 100
    toggle.standard_jerk_speed = np.clip(params.get_int("StandardJerkSpeed") / 100, 0.25, 2) if standard_profile and tuning_level >= level["StandardJerkSpeed"] else default.get_int("StandardJerkSpeed") / 100
    toggle.standard_jerk_speed_decrease = np.clip(params.get_int("StandardJerkSpeedDecrease") / 100, 0.25, 2) if standard_profile and tuning_level >= level["StandardJerkSpeedDecrease"] else default.get_int("StandardJerkSpeedDecrease") / 100
    toggle.standard_follow = np.clip(params.get_float("StandardFollow"), 1, 5) if standard_profile and tuning_level >= level["StandardFollow"] else default.get_float("StandardFollow")
    relaxed_profile = toggle.custom_personalities and (params.get_bool("RelaxedPersonalityProfile") if tuning_level >= level["RelaxedPersonalityProfile"] else default.get_bool("RelaxedPersonalityProfile"))
    toggle.relaxed_jerk_acceleration = np.clip(params.get_int("RelaxedJerkAcceleration") / 100, 0.25, 2) if relaxed_profile and tuning_level >= level["RelaxedJerkAcceleration"] else default.get_int("RelaxedJerkAcceleration") / 100
    toggle.relaxed_jerk_deceleration = np.clip(params.get_int("RelaxedJerkDeceleration") / 100, 0.25, 2) if relaxed_profile and tuning_level >= level["RelaxedJerkDeceleration"] else default.get_int("RelaxedJerkDeceleration") / 100
    toggle.relaxed_jerk_danger = np.clip(params.get_int("RelaxedJerkDanger") / 100, 0.25, 2) if relaxed_profile and tuning_level >= level["RelaxedJerkDanger"] else default.get_int("RelaxedJerkDanger") / 100
    toggle.relaxed_jerk_speed = np.clip(params.get_int("RelaxedJerkSpeed") / 100, 0.25, 2) if relaxed_profile and tuning_level >= level["RelaxedJerkSpeed"] else default.get_int("RelaxedJerkSpeed") / 100
    toggle.relaxed_jerk_speed_decrease = np.clip(params.get_int("RelaxedJerkSpeedDecrease") / 100, 0.25, 2) if relaxed_profile and tuning_level >= level["RelaxedJerkSpeedDecrease"] else default.get_int("RelaxedJerkSpeedDecrease") / 100
    toggle.relaxed_follow = np.clip(params.get_float("RelaxedFollow"), 1, 5) if relaxed_profile and tuning_level >= level["RelaxedFollow"] else default.get_float("RelaxedFollow")
    traffic_profile = toggle.custom_personalities and (params.get_bool("TrafficPersonalityProfile") if tuning_level >= level["TrafficPersonalityProfile"] else default.get_bool("TrafficPersonalityProfile"))
    toggle.traffic_mode_jerk_acceleration = [np.clip(params.get_int("TrafficJerkAcceleration") / 100, 0.25, 2) if traffic_profile and tuning_level >= level["TrafficJerkAcceleration"] else default.get_int("TrafficJerkAcceleration") / 100, toggle.aggressive_jerk_acceleration]
    toggle.traffic_mode_jerk_deceleration = [np.clip(params.get_int("TrafficJerkDeceleration") / 100, 0.25, 2) if traffic_profile and tuning_level >= level["TrafficJerkDeceleration"] else default.get_int("TrafficJerkDeceleration") / 100, toggle.aggressive_jerk_deceleration]
    toggle.traffic_mode_jerk_danger = [np.clip(params.get_int("TrafficJerkDanger") / 100, 0.25, 2) if traffic_profile and tuning_level >= level["TrafficJerkDanger"] else default.get_int("TrafficJerkDanger") / 100, toggle.aggressive_jerk_danger]
    toggle.traffic_mode_jerk_speed = [np.clip(params.get_int("TrafficJerkSpeed") / 100, 0.25, 2) if traffic_profile and tuning_level >= level["TrafficJerkSpeed"] else default.get_int("TrafficJerkSpeed") / 100, toggle.aggressive_jerk_speed]
    toggle.traffic_mode_jerk_speed_decrease = [np.clip(params.get_int("TrafficJerkSpeedDecrease") / 100, 0.25, 2) if traffic_profile and tuning_level >= level["TrafficJerkSpeedDecrease"] else default.get_int("TrafficJerkSpeedDecrease") / 100, toggle.aggressive_jerk_speed_decrease]
    toggle.traffic_mode_follow = [np.clip(params.get_float("TrafficFollow"), 0.5, 5) if traffic_profile and tuning_level >= level["TrafficFollow"] else default.get_float("TrafficFollow"), toggle.aggressive_follow]

    custom_ui = params.get_bool("CustomUI") if tuning_level >= level["CustomUI"] else default.get_bool("CustomUI")
    toggle.acceleration_path = toggle.openpilot_longitudinal and (custom_ui and (params.get_bool("AccelerationPath") if tuning_level >= level["AccelerationPath"] else default.get_bool("AccelerationPath")) or toggle.debug_mode)
    toggle.adjacent_paths = custom_ui and (params.get_bool("AdjacentPath") if tuning_level >= level["AdjacentPath"] else default.get_bool("AdjacentPath"))
    toggle.blind_spot_path = has_bsm and (custom_ui and (params.get_bool("BlindSpotPath") if tuning_level >= level["BlindSpotPath"] else default.get_bool("BlindSpotPath")) or toggle.debug_mode)
    toggle.compass = custom_ui and (params.get_bool("Compass") if tuning_level >= level["Compass"] else default.get_bool("Compass"))
    toggle.pedals_on_ui = toggle.openpilot_longitudinal and (custom_ui and (params.get_bool("PedalsOnUI") if tuning_level >= level["PedalsOnUI"] else default.get_bool("PedalsOnUI")))
    toggle.dynamic_pedals_on_ui = toggle.pedals_on_ui and (params.get_bool("DynamicPedalsOnUI") if tuning_level >= level["DynamicPedalsOnUI"] else default.get_bool("DynamicPedalsOnUI"))
    toggle.static_pedals_on_ui = toggle.pedals_on_ui and (params.get_bool("StaticPedalsOnUI") if tuning_level >= level["StaticPedalsOnUI"] else default.get_bool("StaticPedalsOnUI"))
    toggle.rotating_wheel = custom_ui and (params.get_bool("RotatingWheel") if tuning_level >= level["RotatingWheel"] else default.get_bool("RotatingWheel"))

    toggle.developer_ui = params.get_bool("DeveloperUI") if tuning_level >= level["DeveloperUI"] else default.get_bool("DeveloperUI")
    developer_metrics = toggle.developer_ui and params.get_bool("DeveloperMetrics") if tuning_level >= level["DeveloperMetrics"] else default.get_bool("DeveloperMetrics")
    border_metrics = developer_metrics and (params.get_bool("BorderMetrics") if tuning_level >= level["BorderMetrics"] else default.get_bool("BorderMetrics"))
    toggle.blind_spot_metrics = has_bsm and border_metrics and (params.get_bool("BlindSpotMetrics") if tuning_level >= level["BlindSpotMetrics"] else default.get_bool("BlindSpotMetrics")) or toggle.debug_mode
    toggle.signal_metrics = border_metrics and (params.get_bool("SignalMetrics") if tuning_level >= level["SignalMetrics"] else default.get_bool("SignalMetrics")) or toggle.debug_mode
    toggle.steering_metrics = border_metrics and (params.get_bool("ShowSteering") if tuning_level >= level["ShowSteering"] else default.get_bool("ShowSteering")) or toggle.debug_mode
    toggle.show_fps = developer_metrics and (params.get_bool("FPSCounter") if tuning_level >= level["FPSCounter"] else default.get_bool("FPSCounter")) or toggle.debug_mode
    toggle.adjacent_path_metrics = (developer_metrics and params.get_bool("AdjacentPathMetrics") if tuning_level >= level["AdjacentPathMetrics"] else default.get_bool("AdjacentPathMetrics")) or toggle.debug_mode
    toggle.lead_metrics = (developer_metrics and params.get_bool("LeadInfo") if tuning_level >= level["LeadInfo"] else default.get_bool("LeadInfo")) or toggle.debug_mode
    toggle.numerical_temp = developer_metrics and (params.get_bool("NumericalTemp") if tuning_level >= level["NumericalTemp"] else default.get_bool("NumericalTemp")) or toggle.debug_mode
    toggle.fahrenheit = toggle.numerical_temp and (params.get_bool("Fahrenheit") if tuning_level >= level["Fahrenheit"] else default.get_bool("Fahrenheit")) and not toggle.debug_mode
    toggle.cpu_metrics = developer_metrics and (params.get_bool("ShowCPU") if tuning_level >= level["ShowCPU"] else default.get_bool("ShowCPU")) or toggle.debug_mode
    toggle.gpu_metrics = developer_metrics and (params.get_bool("ShowGPU") if tuning_level >= level["ShowGPU"] else default.get_bool("ShowGPU")) and not toggle.debug_mode
    toggle.ip_metrics = developer_metrics and (params.get_bool("ShowIP") if tuning_level >= level["ShowIP"] else default.get_bool("ShowIP"))
    toggle.memory_metrics = developer_metrics and (params.get_bool("ShowMemoryUsage") if tuning_level >= level["ShowMemoryUsage"] else default.get_bool("ShowMemoryUsage")) or toggle.debug_mode
    toggle.storage_left_metrics = developer_metrics and (params.get_bool("ShowStorageLeft") if tuning_level >= level["ShowStorageLeft"] else default.get_bool("ShowStorageLeft")) and not toggle.debug_mode
    toggle.storage_used_metrics = developer_metrics and (params.get_bool("ShowStorageUsed") if tuning_level >= level["ShowStorageUsed"] else default.get_bool("ShowStorageUsed")) and not toggle.debug_mode
    toggle.use_si_metrics = developer_metrics and (params.get_bool("UseSI") if tuning_level >= level["UseSI"] else default.get_bool("UseSI")) or toggle.debug_mode
    toggle.developer_sidebar = toggle.developer_ui and (params.get_bool("DeveloperSidebar") if tuning_level >= level["DeveloperSidebar"] else default.get_bool("DeveloperSidebar")) or toggle.debug_mode
    toggle.developer_sidebar_metric1 = params.get_int("DeveloperSidebarMetric1") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric1"] else 1 if toggle.debug_mode else default.get_float("DeveloperSidebarMetric1")
    toggle.developer_sidebar_metric2 = params.get_int("DeveloperSidebarMetric2") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric2"] else 3 if toggle.debug_mode else default.get_float("DeveloperSidebarMetric2")
    toggle.developer_sidebar_metric3 = params.get_int("DeveloperSidebarMetric3") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric3"] else 4 if toggle.debug_mode else default.get_float("DeveloperSidebarMetric3")
    toggle.developer_sidebar_metric4 = params.get_int("DeveloperSidebarMetric4") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric4"] else 5 if toggle.debug_mode else default.get_float("DeveloperSidebarMetric4")
    toggle.developer_sidebar_metric5 = params.get_int("DeveloperSidebarMetric5") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric5"] else 6 if toggle.debug_mode else default.get_float("DeveloperSidebarMetric5")
    toggle.developer_sidebar_metric6 = params.get_int("DeveloperSidebarMetric6") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric6"] else 7 if toggle.debug_mode else default.get_float("DeveloperSidebarMetric6")
    toggle.developer_sidebar_metric7 = params.get_int("DeveloperSidebarMetric7") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric7"] else 11 if toggle.debug_mode else default.get_float("DeveloperSidebarMetric7")
    developer_widgets = toggle.developer_ui and params.get_bool("DeveloperWidgets") if tuning_level >= level["DeveloperWidgets"] else default.get_bool("DeveloperWidgets")
    toggle.adjacent_lead_tracking = has_radar and ((developer_widgets and params.get_bool("AdjacentLeadsUI") if tuning_level >= level["AdjacentLeadsUI"] else default.get_bool("AdjacentLeadsUI")) or toggle.debug_mode)
    toggle.radar_tracks = has_radar and ((developer_widgets and params.get_bool("RadarTracksUI") if tuning_level >= level["RadarTracksUI"] else default.get_bool("RadarTracksUI")) or toggle.debug_mode)
    toggle.show_stopping_point = toggle.openpilot_longitudinal and (developer_widgets and (params.get_bool("ShowStoppingPoint") if tuning_level >= level["ShowStoppingPoint"] else default.get_bool("ShowStoppingPoint")) or toggle.debug_mode)
    toggle.show_stopping_point_metrics = toggle.show_stopping_point and (params.get_bool("ShowStoppingPointMetrics") if tuning_level >= level["ShowStoppingPointMetrics"] else default.get_bool("ShowStoppingPointMetrics") or toggle.debug_mode)

    device_management = params.get_bool("DeviceManagement") if tuning_level >= level["DeviceManagement"] else default.get_bool("DeviceManagement")
    device_shutdown_setting = params.get_int("DeviceShutdown") if device_management and tuning_level >= level["DeviceShutdown"] else default.get_int("DeviceShutdown")
    toggle.device_shutdown_time = (device_shutdown_setting - 3) * 3600 if device_shutdown_setting >= 4 else device_shutdown_setting * (60 * 15)
    toggle.increase_thermal_limits = device_management and (params.get_bool("IncreaseThermalLimits") if tuning_level >= level["IncreaseThermalLimits"] else default.get_bool("IncreaseThermalLimits"))
    toggle.low_voltage_shutdown = np.clip(params.get_float("LowVoltageShutdown"), VBATT_PAUSE_CHARGING, 12.5) if device_management and tuning_level >= level["LowVoltageShutdown"] else default.get_float("LowVoltageShutdown")
    toggle.no_logging = device_management and (params.get_bool("NoLogging") if tuning_level >= level["NoLogging"] else default.get_bool("NoLogging")) and not self.vetting_branch or toggle.force_onroad
    toggle.no_uploads = device_management and (params.get_bool("NoUploads") if tuning_level >= level["NoUploads"] else default.get_bool("NoUploads")) and not self.vetting_branch or toggle.use_higher_bitrate
    toggle.no_onroad_uploads = toggle.no_uploads and (params.get_bool("DisableOnroadUploads") if tuning_level >= level["DisableOnroadUploads"] else default.get_bool("DisableOnroadUploads")) and not toggle.use_higher_bitrate

    distance_button_control = params.get_int("DistanceButtonControl") if tuning_level >= level["DistanceButtonControl"] else default.get_int("DistanceButtonControl")
    toggle.experimental_mode_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press = toggle.experimental_mode_via_distance
    toggle.force_coast_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance = distance_button_control == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    distance_button_control_long = params.get_int("LongDistanceButtonControl") if tuning_level >= level["LongDistanceButtonControl"] else default.get_int("LongDistanceButtonControl")
    toggle.experimental_mode_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_distance_long
    toggle.force_coast_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance_long = distance_button_control_long == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    distance_button_control_very_long = params.get_int("VeryLongDistanceButtonControl") if tuning_level >= level["VeryLongDistanceButtonControl"] else default.get_int("VeryLongDistanceButtonControl")
    toggle.experimental_mode_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_distance_very_long
    toggle.force_coast_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance_very_long = distance_button_control_very_long == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    toggle.experimental_gm_tune = toggle.openpilot_longitudinal and toggle.car_make == "gm" and (params.get_bool("ExperimentalGMTune") if tuning_level >= level["ExperimentalGMTune"] else default.get_bool("ExperimentalGMTune"))
    toggle.stoppingDecelRate = 0.3 if toggle.experimental_gm_tune else toggle.stoppingDecelRate
    toggle.vEgoStarting = 0.15 if toggle.experimental_gm_tune else toggle.vEgoStarting
    toggle.vEgoStopping = 0.15 if toggle.experimental_gm_tune else toggle.vEgoStopping

    toggle.force_fingerprint = (params.get_bool("ForceFingerprint") if tuning_level >= level["ForceFingerprint"] else default.get_bool("ForceFingerprint")) and toggle.car_model is not None

    toggle.frogsgomoo_tweak = toggle.openpilot_longitudinal and toggle.car_make == "toyota" and (params.get_bool("FrogsGoMoosTweak") if tuning_level >= level["FrogsGoMoosTweak"] else default.get_bool("FrogsGoMoosTweak"))
    toggle.stoppingDecelRate = 0.01 if toggle.frogsgomoo_tweak else toggle.stoppingDecelRate
    toggle.vEgoStarting = 0.1 if toggle.frogsgomoo_tweak else toggle.vEgoStarting
    toggle.vEgoStopping = 0.5 if toggle.frogsgomoo_tweak else toggle.vEgoStopping

    toggle.holiday_themes = params.get_bool("HolidayThemes") if tuning_level >= level["HolidayThemes"] else default.get_bool("HolidayThemes")
    toggle.current_holiday_theme = holiday_theme if toggle.holiday_themes else "stock"

    toggle.lane_changes = params.get_bool("LaneChangeCustomizations") if tuning_level >= level["LaneChangeCustomizations"] else default.get_bool("LaneChangeCustomizations")
    toggle.lane_change_delay = params.get_float("LaneChangeTime") if toggle.lane_changes and tuning_level >= level["LaneChangeTime"] else default.get_float("LaneChangeTime")
    toggle.lane_detection_width = params.get_float("LaneDetectionWidth") * distance_conversion if toggle.lane_changes and tuning_level >= level["LaneDetectionWidth"] else default.get_float("LaneDetectionWidth") * CV.FOOT_TO_METER
    toggle.lane_detection = toggle.lane_detection_width > 0
    toggle.minimum_lane_change_speed = params.get_float("MinimumLaneChangeSpeed") * speed_conversion if toggle.lane_changes and tuning_level >= level["MinimumLaneChangeSpeed"] else default.get_float("MinimumLaneChangeSpeed") * CV.MPH_TO_MS
    toggle.nudgeless = toggle.lane_changes and (params.get_bool("NudgelessLaneChange") if tuning_level >= level["NudgelessLaneChange"] else default.get_bool("NudgelessLaneChange"))
    toggle.one_lane_change = toggle.lane_changes and (params.get_bool("OneLaneChange") if tuning_level >= level["OneLaneChange"] else default.get_bool("OneLaneChange"))

    lateral_tuning = params.get_bool("LateralTune") if tuning_level >= level["LateralTune"] else default.get_bool("LateralTune")
    toggle.nnff = lateral_tuning and has_nnff and not is_angle_car and (params.get_bool("NNFF") if tuning_level >= level["NNFF"] else default.get_bool("NNFF"))
    toggle.nnff_lite = not toggle.nnff and lateral_tuning and not is_angle_car and (params.get_bool("NNFFLite") if tuning_level >= level["NNFFLite"] else default.get_bool("NNFFLite"))
    toggle.use_turn_desires = lateral_tuning and (params.get_bool("TurnDesires") if tuning_level >= level["TurnDesires"] else default.get_bool("TurnDesires"))

    lkas_button_control = (params.get_int("LKASButtonControl") if tuning_level >= level["LKASButtonControl"] else default.get_int("LKASButtonControl")) if toggle.car_make != "subaru" else 0
    toggle.experimental_mode_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_lkas
    toggle.force_coast_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_lkas = lkas_button_control == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    toggle.lock_doors_timer = params.get_int("LockDoorsTimer") if toggle.car_make == "toyota" and tuning_level >= level["LockDoorsTimer"] else default.get_int("LockDoorsTimer")

    toggle.long_pitch = toggle.openpilot_longitudinal and toggle.car_make == "gm" and (params.get_bool("LongPitch") if tuning_level >= level["LongPitch"] else default.get_bool("LongPitch"))

    longitudinal_tuning = toggle.openpilot_longitudinal and (params.get_bool("LongitudinalTune") if tuning_level >= level["LongitudinalTune"] else default.get_bool("LongitudinalTune"))
    toggle.acceleration_profile = params.get_int("AccelerationProfile") if longitudinal_tuning and tuning_level >= level["AccelerationProfile"] else default.get_int("AccelerationProfile")
    toggle.sport_plus = max_acceleration_enabled and toggle.acceleration_profile == 3
    toggle.deceleration_profile = params.get_int("DecelerationProfile") if longitudinal_tuning and tuning_level >= level["DecelerationProfile"] else default.get_int("DecelerationProfile")
    toggle.human_acceleration = longitudinal_tuning and (params.get_bool("HumanAcceleration") if tuning_level >= level["HumanAcceleration"] else default.get_bool("HumanAcceleration"))
    toggle.human_following = longitudinal_tuning and (params.get_bool("HumanFollowing") if tuning_level >= level["HumanFollowing"] else default.get_bool("HumanFollowing"))
    toggle.lead_detection_probability = np.clip(params.get_int("LeadDetectionThreshold") / 100, 0.25, 0.50) if longitudinal_tuning and tuning_level >= level["LeadDetectionThreshold"] else default.get_int("LeadDetectionThreshold") / 100
    toggle.max_desired_acceleration = np.clip(params.get_float("MaxDesiredAcceleration"), 0.1, 4.0) if longitudinal_tuning and tuning_level >= level["MaxDesiredAcceleration"] else default.get_float("MaxDesiredAcceleration")
    toggle.taco_tune = longitudinal_tuning and (params.get_bool("TacoTune") if tuning_level >= level["TacoTune"] else default.get_bool("TacoTune"))

    toggle.available_models = params.get("AvailableModels", encoding="utf-8") or ""
    toggle.available_model_names = params.get("AvailableModelNames", encoding="utf-8") or ""
    toggle.model_versions = params.get("ModelVersions", encoding="utf-8") or ""
    downloaded_models = [model for model in toggle.available_models.split(",") if any(MODELS_PATH.glob(f"{model}.*"))]
    toggle.model_randomizer = downloaded_models and (params.get_bool("ModelRandomizer") if tuning_level >= level["ModelRandomizer"] else default.get_bool("ModelRandomizer"))
    if toggle.available_models and toggle.available_model_names and downloaded_models and toggle.model_versions:
      toggle.available_models += f",{DEFAULT_TINYGRAD_MODEL}"
      toggle.available_model_names += f",{DEFAULT_TINYGRAD_MODEL_NAME}"
      toggle.model_versions += f",{DEFAULT_TINYGRAD_MODEL_VERSION}"
      downloaded_models += [DEFAULT_TINYGRAD_MODEL]
      if toggle.model_randomizer:
        if not started:
          blacklisted_models = (params.get("BlacklistedModels", encoding="utf-8") or "").split(",")
          selectable_models = [model for model in downloaded_models if model not in blacklisted_models]
          toggle.model = random.choice(selectable_models) if selectable_models else default.get("Model", encoding="utf-8")
          toggle.model_name = "Mystery Model 👻"
          toggle.model_version = toggle.model_versions.split(",")[toggle.available_models.split(",").index(toggle.model)]
      else:
        toggle.model = params.get("Model", encoding="utf-8") if tuning_level >= level["Model"] else default.get("Model", encoding="utf-8")
        if toggle.model in downloaded_models:
          toggle.model_name = toggle.available_model_names.split(",")[toggle.available_models.split(",").index(toggle.model)]
          toggle.model_version = toggle.model_versions.split(",")[toggle.available_models.split(",").index(toggle.model)]
        else:
          toggle.model = default.get("Model", encoding="utf-8")
          toggle.model_name = toggle.available_model_names.split(",")[toggle.available_models.split(",").index(toggle.model)]
          toggle.model_version = toggle.model_versions.split(",")[toggle.available_models.split(",").index(toggle.model)]
    else:
      toggle.model = DEFAULT_CLASSIC_MODEL
      toggle.model_name = DEFAULT_CLASSIC_MODEL_NAME
      toggle.model_version = DEFAULT_CLASSIC_MODEL_VERSION
    toggle.classic_model = toggle.model_version in {"v1", "v2", "v3", "v4"}
    toggle.tinygrad_model = toggle.model_version in {"v7"}
    toggle.tomb_raider = toggle.model == "space-lab"

    toggle.model_ui = params.get_bool("ModelUI") if tuning_level >= level["ModelUI"] else default.get_bool("ModelUI")
    toggle.dynamic_path_width = toggle.model_ui and (params.get_bool("DynamicPathWidth") if tuning_level >= level["DynamicPathWidth"] else default.get_bool("DynamicPathWidth"))
    toggle.lane_line_width = params.get_int("LaneLinesWidth") * small_distance_conversion / 200 if toggle.model_ui and tuning_level >= level["LaneLinesWidth"] else default.get_int("LaneLinesWidth") * CV.INCH_TO_CM / 200
    toggle.path_edge_width = params.get_int("PathEdgeWidth") if toggle.model_ui and tuning_level >= level["PathEdgeWidth"] else default.get_int("PathEdgeWidth")
    toggle.path_width = params.get_float("PathWidth") * distance_conversion / 2 if toggle.model_ui and tuning_level >= level["PathWidth"] else default.get_float("PathWidth") * CV.FOOT_TO_METER / 2
    toggle.road_edge_width = params.get_int("RoadEdgesWidth") * small_distance_conversion / 200 if toggle.model_ui and tuning_level >= level["RoadEdgesWidth"] else default.get_int("RoadEdgesWidth") * CV.INCH_TO_CM / 200
    toggle.unlimited_road_ui_length = toggle.model_ui and (params.get_bool("UnlimitedLength") if tuning_level >= level["UnlimitedLength"] else default.get_bool("UnlimitedLength"))

    toggle.navigation_ui = params.get_bool("NavigationUI") if tuning_level >= level["NavigationUI"] else default.get_bool("NavigationUI")
    toggle.big_map = toggle.navigation_ui and (params.get_bool("BigMap") if tuning_level >= level["BigMap"] else default.get_bool("BigMap"))
    toggle.full_map = toggle.big_map and (params.get_bool("FullMap") if tuning_level >= level["FullMap"] else default.get_bool("FullMap"))
    toggle.map_style = params.get_int("MapStyle") if toggle.navigation_ui and tuning_level >= level["MapStyle"] else default.get_int("MapStyle")
    toggle.road_name_ui = toggle.navigation_ui and (params.get_bool("RoadNameUI") if tuning_level >= level["RoadNameUI"] else default.get_bool("RoadNameUI"))
    toggle.show_speed_limits = toggle.navigation_ui and (params.get_bool("ShowSpeedLimits") if tuning_level >= level["ShowSpeedLimits"] else default.get_bool("ShowSpeedLimits"))
    toggle.speed_limit_vienna = toggle.navigation_ui and (params.get_bool("UseVienna") if tuning_level >= level["UseVienna"] else default.get_bool("UseVienna"))

    toggle.old_long_api = toggle.openpilot_longitudinal and toggle.car_make == "gm" and toggle.has_cc_long and not toggle.has_pedal
    toggle.old_long_api |= toggle.openpilot_longitudinal and toggle.car_make == "hyundai" and not (params.get_bool("NewLongAPI") if tuning_level >= level["NewLongAPI"] else default.get_bool("NewLongAPI"))

    personalize_openpilot = params.get_bool("PersonalizeOpenpilot") if tuning_level >= level["PersonalizeOpenpilot"] else default.get_bool("PersonalizeOpenpilot")
    toggle.color_scheme = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else params.get("CustomColors", encoding="utf-8") if personalize_openpilot else "stock"
    toggle.distance_icons = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else params.get("CustomDistanceIcons", encoding="utf-8") if personalize_openpilot else "stock"
    toggle.icon_pack = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else params.get("CustomIcons", encoding="utf-8") if personalize_openpilot else "stock"
    toggle.random_themes = personalize_openpilot and (params.get_bool("RandomThemes") if tuning_level >= level["RandomThemes"] else default.get_bool("RandomThemes"))
    toggle.signal_icons = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else params.get("CustomSignals", encoding="utf-8") if personalize_openpilot else "stock"
    toggle.sound_pack = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else params.get("CustomSounds", encoding="utf-8") if personalize_openpilot else "stock"
    if not toggle.random_themes or boot_run:
      toggle.wheel_image = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else params.get("WheelIcon", encoding="utf-8") if personalize_openpilot else "stock"
    else:
      toggle.wheel_image = next((file.resolve().stem for file in (ACTIVE_THEME_PATH / "steering_wheel").glob("wheel.*")), "none")

    quality_of_life_lateral = params.get_bool("QOLLateral") if tuning_level >= level["QOLLateral"] else default.get_bool("QOLLateral")
    toggle.pause_lateral_below_speed = params.get_int("PauseLateralSpeed") * speed_conversion if quality_of_life_lateral and tuning_level >= level["PauseLateralSpeed"] else default.get_int("PauseLateralSpeed") * CV.MPH_TO_MS
    toggle.pause_lateral_below_signal = toggle.pause_lateral_below_speed != 0 and (params.get_bool("PauseLateralOnSignal") if tuning_level >= level["PauseLateralOnSignal"] else default.get_bool("PauseLateralOnSignal"))

    quality_of_life_longitudinal = params.get_bool("QOLLongitudinal") if tuning_level >= level["QOLLongitudinal"] else default.get_bool("QOLLongitudinal")
    toggle.cruise_increase = params.get_int("CustomCruise") if quality_of_life_longitudinal and not pcm_cruise and tuning_level >= level["CustomCruise"] else default.get_int("CustomCruise")
    toggle.cruise_increase_long = params.get_int("CustomCruiseLong") if quality_of_life_longitudinal and not pcm_cruise and tuning_level >= level["CustomCruiseLong"] else default.get_int("CustomCruiseLong")
    toggle.force_standstill = quality_of_life_longitudinal and (params.get_bool("ForceStandstill") if tuning_level >= level["ForceStandstill"] else default.get_bool("ForceStandstill"))
    toggle.force_stops = quality_of_life_longitudinal and (params.get_bool("ForceStops") if tuning_level >= level["ForceStops"] else default.get_bool("ForceStops"))
    toggle.increased_stopped_distance = params.get_int("IncreasedStoppedDistance") * distance_conversion if quality_of_life_longitudinal and tuning_level >= level["IncreasedStoppedDistance"] else default.get_int("IncreasedStoppedDistance") * CV.FOOT_TO_METER
    map_gears = quality_of_life_longitudinal and (params.get_bool("MapGears") if tuning_level >= level["MapGears"] else default.get_bool("MapGears"))
    toggle.map_acceleration = map_gears and (params.get_bool("MapAcceleration") if tuning_level >= level["MapAcceleration"] else default.get_bool("MapAcceleration"))
    toggle.map_deceleration = map_gears and (params.get_bool("MapDeceleration") if tuning_level >= level["MapDeceleration"] else default.get_bool("MapDeceleration"))
    toggle.reverse_cruise_increase = quality_of_life_longitudinal and toggle.car_make == "toyota" and pcm_cruise and (params.get_bool("ReverseCruise") if tuning_level >= level["ReverseCruise"] else default.get_bool("ReverseCruise"))
    toggle.set_speed_offset = params.get_int("SetSpeedOffset") * (1 if toggle.is_metric else CV.MPH_TO_KPH) if quality_of_life_longitudinal and not pcm_cruise and tuning_level >= level["SetSpeedOffset"] else default.get_int("SetSpeedOffset") * CV.MPH_TO_KPH

    quality_of_life_visuals = params.get_bool("QOLVisuals") if tuning_level >= level["QOLVisuals"] else default.get_bool("QOLVisuals")
    toggle.camera_view = params.get_int("CameraView") if quality_of_life_visuals and tuning_level >= level["CameraView"] else default.get_int("CameraView")
    toggle.driver_camera_in_reverse = quality_of_life_visuals and (params.get_bool("DriverCamera") if tuning_level >= level["DriverCamera"] else default.get_bool("DriverCamera"))
    toggle.onroad_distance_button = toggle.openpilot_longitudinal and (quality_of_life_visuals and (params.get_bool("OnroadDistanceButton") if tuning_level >= level["OnroadDistanceButton"] else default.get_bool("OnroadDistanceButton")) or toggle.debug_mode)
    toggle.standby_mode = quality_of_life_visuals and (params.get_bool("StandbyMode") if tuning_level >= level["StandbyMode"] else default.get_bool("StandbyMode"))
    toggle.stopped_timer = quality_of_life_visuals and (params.get_bool("StoppedTimer") if tuning_level >= level["StoppedTimer"] else default.get_bool("StoppedTimer"))

    toggle.rainbow_path = params.get_bool("RainbowPath") if tuning_level >= level["RainbowPath"] else default.get_bool("RainbowPath")

    toggle.random_events = params.get_bool("RandomEvents") if tuning_level >= level["RandomEvents"] else default.get_bool("RandomEvents")

    screen_management = params.get_bool("ScreenManagement") if tuning_level >= level["ScreenManagement"] else default.get_bool("ScreenManagement")
    toggle.screen_brightness = params.get_int("ScreenBrightness") if screen_management and tuning_level >= level["ScreenBrightness"] else default.get_int("ScreenBrightness")
    toggle.screen_brightness_onroad = params.get_int("ScreenBrightnessOnroad") if screen_management and tuning_level >= level["ScreenBrightnessOnroad"] else default.get_int("ScreenBrightnessOnroad")
    toggle.screen_recorder = screen_management and (params.get_bool("ScreenRecorder") if tuning_level >= level["ScreenRecorder"] else default.get_bool("ScreenRecorder")) or toggle.debug_mode
    toggle.screen_timeout = params.get_int("ScreenTimeout") if screen_management and tuning_level >= level["ScreenTimeout"] else default.get_int("ScreenTimeout")
    toggle.screen_timeout_onroad = params.get_int("ScreenTimeoutOnroad") if screen_management and tuning_level >= level["ScreenTimeoutOnroad"] else default.get_int("ScreenTimeoutOnroad")

    toggle.sng_hack = toggle.openpilot_longitudinal and toggle.car_make == "toyota" and not toggle.has_pedal and not has_sng and (params.get_bool("SNGHack") if tuning_level >= level["SNGHack"] else default.get_bool("SNGHack"))

    toggle.speed_limit_controller = toggle.openpilot_longitudinal and (params.get_bool("SpeedLimitController") if tuning_level >= level["SpeedLimitController"] else default.get_bool("SpeedLimitController"))
    toggle.force_mph_dashboard = toggle.speed_limit_controller and (params.get_bool("ForceMPHDashboard") if tuning_level >= level["ForceMPHDashboard"] else default.get_bool("ForceMPHDashboard"))
    toggle.map_speed_lookahead_higher = params.get_int("SLCLookaheadHigher") if toggle.speed_limit_controller and tuning_level >= level["SLCLookaheadHigher"] else default.get_int("SLCLookaheadHigher")
    toggle.map_speed_lookahead_lower = params.get_int("SLCLookaheadLower") if toggle.speed_limit_controller and tuning_level >= level["SLCLookaheadLower"] else default.get_int("SLCLookaheadLower")
    toggle.set_speed_limit = toggle.speed_limit_controller and (params.get_bool("SetSpeedLimit") if tuning_level >= level["SetSpeedLimit"] else default.get_bool("SetSpeedLimit"))
    toggle.show_speed_limit_offset = toggle.speed_limit_controller and (params.get_bool("ShowSLCOffset") if tuning_level >= level["ShowSLCOffset"] else default.get_bool("ShowSLCOffset")) or toggle.debug_mode
    slc_fallback_method = params.get_int("SLCFallback") if toggle.speed_limit_controller and tuning_level >= level["SLCFallback"] else default.get_int("SLCFallback")
    toggle.slc_fallback_experimental_mode = slc_fallback_method == 1
    toggle.slc_fallback_previous_speed_limit = slc_fallback_method == 2
    toggle.slc_fallback_set_speed = slc_fallback_method == 0
    toggle.slc_mapbox_filler = (toggle.show_speed_limits or toggle.speed_limit_controller) and params.get("MapboxSecretKey", encoding="utf-8") != None and (params.get_bool("SLCMapboxFiller") if tuning_level >= level["SLCMapboxFiller"] else default.get_bool("SLCMapboxFiller"))
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
    toggle.speed_limit_offset5 = (params.get_int("Offset5") * speed_conversion if tuning_level >= level["Offset5"] else default.get_int("Offset5") * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset6 = (params.get_int("Offset6") * speed_conversion if tuning_level >= level["Offset6"] else default.get_int("Offset6") * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset7 = (params.get_int("Offset7") * speed_conversion if tuning_level >= level["Offset7"] else default.get_int("Offset7") * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_priority1 = params.get("SLCPriority1", encoding="utf-8") if toggle.speed_limit_controller and tuning_level >= level["SLCPriority1"] else default.get("SLCPriority1", encoding="utf-8")
    toggle.speed_limit_priority2 = params.get("SLCPriority2", encoding="utf-8") if toggle.speed_limit_controller and tuning_level >= level["SLCPriority2"] else default.get("SLCPriority2", encoding="utf-8")
    toggle.speed_limit_priority3 = params.get("SLCPriority3", encoding="utf-8") if toggle.speed_limit_controller and tuning_level >= level["SLCPriority3"] else default.get("SLCPriority3", encoding="utf-8")
    toggle.speed_limit_priority_highest = toggle.speed_limit_priority1 == "Highest"
    toggle.speed_limit_priority_lowest = toggle.speed_limit_priority1 == "Lowest"
    toggle.speed_limit_sources = toggle.speed_limit_controller and (params.get_bool("SpeedLimitSources") if tuning_level >= level["SpeedLimitSources"] else default.get_bool("SpeedLimitSources"))

    toggle.speed_limit_filler = params.get_bool("SpeedLimitFiller") if tuning_level >= level["SpeedLimitFiller"] else default.get_bool("SpeedLimitFiller")

    toggle.startup_alert_top = params.get("StartupMessageTop", encoding="utf-8") if tuning_level >= level["StartupMessageTop"] else default.get("StartupMessageTop", encoding="utf-8")
    toggle.startup_alert_bottom = params.get("StartupMessageBottom", encoding="utf-8") if tuning_level >= level["StartupMessageBottom"] else default.get("StartupMessageBottom", encoding="utf-8")

    toggle.taco_tune_hacks = taco_hacks_allowed and (params.get_bool("TacoTuneHacks") if tuning_level >= level["TacoTuneHacks"] else default.get_bool("TacoTuneHacks"))

    toggle.tethering_config = params.get_int("TetheringEnabled")

    toyota_doors = toggle.car_make == "toyota" and (params.get_bool("ToyotaDoors") if tuning_level >= level["ToyotaDoors"] else default.get_bool("ToyotaDoors"))
    toggle.lock_doors = toyota_doors and (params.get_bool("LockDoors") if tuning_level >= level["LockDoors"] else default.get_bool("LockDoors"))
    toggle.unlock_doors = toyota_doors and (params.get_bool("UnlockDoors") if tuning_level >= level["UnlockDoors"] else default.get_bool("UnlockDoors"))

    toggle.volt_sng = toggle.car_model == "CHEVROLET_VOLT" and (params.get_bool("VoltSNG") if tuning_level >= level["VoltSNG"] else default.get_bool("VoltSNG"))

    params_memory.put("FrogPilotToggles", json.dumps(toggle.__dict__))
    params_memory.remove("FrogPilotTogglesUpdated")
