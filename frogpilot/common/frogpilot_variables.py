#!/usr/bin/env python3
import numpy as np
import os
import random

from functools import cache
from pathlib import Path
from types import SimpleNamespace

from cereal import custom, log
from opendbc.car import structs
from opendbc.car import gen_empty_fingerprint
from opendbc.car.car_helpers import interfaces
from opendbc.car.gm.values import GMFlags
from opendbc.car.interfaces import CarInterfaceBase
from opendbc.car.mock.interface import CarInterface
from opendbc.car.mock.values import CAR as MOCK
from opendbc.car.toyota.values import ToyotaFrogPilotFlags
from opendbc.safety import ALTERNATIVE_EXPERIENCE
from openpilot.common.basedir import BASEDIR
from openpilot.common.constants import CV
from openpilot.common.params import Params
from openpilot.selfdrive.controls.lib.desire_helper import LANE_CHANGE_SPEED_MIN
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.system.hardware import HARDWARE
from openpilot.system.hardware.power_monitoring import VBATT_PAUSE_CHARGING
from openpilot.system.version import get_build_metadata

GearShifter = structs.CarState.GearShifter
SafetyModel = structs.CarParams.SafetyModel

CITY_SPEED_LIMIT = 25                     # 55mph is typically the minimum speed for highways
CRUISING_SPEED = 5                        # Roughly the speed cars go when not touching the gas while in drive
DEFAULT_LATERAL_ACCELERATION = 2.0        # m/s^2, typical lateral acceleration when taking curves
DISPLAY_MENU_TIMER = 350                  # The length of time the following distance menu appears on some GM vehicles to prevent things getting out of sync
EARTH_RADIUS = 6378137                    # Radius of the Earth in meters
MAX_T_FOLLOW = 3.0                        # Maximum allowed following duration. Larger values risk losing track of the lead but may be increased as models improve
MINIMUM_LATERAL_ACCELERATION = 1.3        # m/s^2, typical minimum lateral acceleration when taking curves
PLANNER_TIME = ModelConstants.T_IDXS[-1]  # Length of time the model projects out for
THRESHOLD = 0.63                          # Requires the condition to be true for ~1 second

NON_DRIVING_GEARS = [GearShifter.neutral, GearShifter.park, GearShifter.reverse, GearShifter.unknown]

DISCORD_WEBHOOK_URL_REPORT = os.getenv("DISCORD_WEBHOOK_URL_REPORT")
DISCORD_WEBHOOK_URL_THEME = os.getenv("DISCORD_WEBHOOK_URL_THEME")

RESOURCES_REPO = "FrogAi/FrogPilot-Resources"

ACTIVE_THEME_PATH = Path(__file__).parents[1] / "assets/active_theme"
METADATAS_PATH = Path(__file__).parents[1] / "assets/model_metadata"
MODELS_PATH = Path("/data/models")
RANDOM_EVENTS_PATH = Path(__file__).parents[1] / "assets/random_events"
THEME_SAVE_PATH = Path("/data/themes")

ERROR_LOGS_PATH = Path("/data/error_logs")
SCREEN_RECORDINGS_PATH = Path("/data/media/screen_recordings")
VIDEO_CACHE_PATH = Path("/data/video_cache")

BACKUP_PATH = Path("/cache/on_backup")

HD_LOGS_PATH = Path("/data/media/0/realdata_HD")
HD_PATH = Path("/cache/use_HD")

KONIK_LOGS_PATH = Path("/data/media/0/realdata_konik")
KONIK_PATH = Path("/cache/use_konik")

MAPD_PATH = Path("/data/media/0/osm/mapd")
MAPS_PATH = Path("/data/media/0/osm/offline")

DEFAULT_MODEL = "firehose"
DEFAULT_MODEL_NAME = "Firehose (Default)"
DEFAULT_MODEL_VERSION = "v9"

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
  "AvailableModels", "AvailableModelNames", "CalibratedLateralAcceleration", "CalibrationProgress", "CarParamsPersistent",
  "FrogPilotCarParamsPersistent", "CurvatureData", "ExperimentalLongitudinalEnabled", "KonikMinutes", "MapBoxRequests",
  "ModelDrivesAndScores", "openpilotMinutes", "OverpassRequests", "SpeedLimits", "SpeedLimitsFiltered", "UpdaterAvailableBranches"
}

TINYGRAD_FILES = [
  ("driving_policy_metadata.pkl", "policy metadata"),
  ("driving_policy_tinygrad.pkl", "policy model"),
  ("driving_vision_metadata.pkl", "vision metadata"),
  ("driving_vision_tinygrad.pkl", "vision model"),
]

def get_frogpilot_toggles():
  if not hasattr(get_frogpilot_toggles, "_params_memory"):
    get_frogpilot_toggles._params_memory = Params(memory=True)

  return SimpleNamespace(**get_frogpilot_toggles._params_memory.get("FrogPilotToggles", return_default=True))

def update_frogpilot_toggles():
  if not hasattr(update_frogpilot_toggles, "_params_memory"):
    update_frogpilot_toggles._params_memory = Params(memory=True)

  update_frogpilot_toggles._params_memory.put_bool("FrogPilotTogglesUpdated", True)

class FrogPilotVariables:
  def __init__(self):
    self.params = Params()
    self.params_memory = Params(memory=True)

    self.frogpilot_toggles = get_frogpilot_toggles()

    self.default_values = {key.decode(): self.params.get_default_value(key) for key in self.params.all_keys()}
    self.tuning_levels = {key.decode(): self.params.get_tuning_level(key) for key in self.params.all_keys()}

    short_branch = get_build_metadata().channel
    self.development_branch = short_branch == "FrogPilot-Development"
    self.release_branch = short_branch == "FrogPilot"
    self.staging_branch = short_branch == "FrogPilot-Staging"
    self.testing_branch = short_branch == "FrogPilot-Testing"
    self.vetting_branch = short_branch == "FrogPilot-Vetting"

    default = self.default_values
    level = self.tuning_levels
    toggle = self.frogpilot_toggles

    toggle.frogs_go_moo = Path("/persist/frogsgomoo.py").is_file()
    toggle.block_user = (self.development_branch or short_branch == "MAKE-PRS-HERE" or self.vetting_branch) and not toggle.frogs_go_moo

  def update(self, holiday_theme, started):
    default = self.default_values
    level = self.tuning_levels
    toggle = self.frogpilot_toggles

    toggle.force_offroad = self.params_memory.get_bool("ForceOffroad")
    toggle.force_onroad = self.params_memory.get_bool("ForceOnroad")

    tuning_level = self.params.get("TuningLevel") if self.params.get_bool("TuningLevelConfirmed") else 3

    toggle.is_metric = self.params.get_bool("IsMetric")
    distance_conversion = 1 if toggle.is_metric else CV.FOOT_TO_METER
    small_distance_conversion = 1 if toggle.is_metric else CV.INCH_TO_CM
    speed_conversion = CV.KPH_TO_MS if toggle.is_metric else CV.MPH_TO_MS

    msg_bytes = self.params.get("CarParams" if started else "CarParamsPersistent", block=started)
    if msg_bytes:
      with structs.CarParams.from_bytes(msg_bytes) as cp_reader:
        CP = cp_reader.as_builder()
    else:
      CarInterface = interfaces[MOCK.MOCK]
      CP = CarInterface.get_params(MOCK.MOCK, gen_empty_fingerprint(), [], False, False, False, toggle)
      CarInterface.configure_torque_tune(MOCK.MOCK, CP.lateralTuning)

      safety_config = structs.CarParams.SafetyConfig.new_message()
      safety_config.safetyModel = structs.CarParams.SafetyModel.noOutput
      CP.safetyConfigs = [safety_config]

    fpmsg_bytes = self.params.get("FrogPilotCarParams" if started else "FrogPilotCarParamsPersistent", block=started)
    if fpmsg_bytes:
      with custom.FrogPilotCarParams.from_bytes(fpmsg_bytes) as fpcp_reader:
        FPCP = fpcp_reader.as_builder()
    else:
      CarInterface = interfaces[MOCK.MOCK]
      FPCP = CarInterface.get_frogpilot_params(MOCK.MOCK, gen_empty_fingerprint(), [], CP, toggle)

    is_torque_car = FPCP.lateralTuning.which() == "torque"
    if not is_torque_car:
      CarInterfaceBase.configure_torque_tune(MOCK.MOCK, FPCP.lateralTuning)

    toggle.always_on_lateral_set = bool(CP.alternativeExperience & ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL)
    toggle.car_make = CP.brand
    toggle.car_model = CP.carFingerprint
    toggle.disable_openpilot_long = self.params.get_bool("DisableOpenpilotLongitudinal") if tuning_level >= level["DisableOpenpilotLongitudinal"] else default["DisableOpenpilotLongitudinal"]
    friction = FPCP.lateralTuning.torque.friction
    has_bsm = CP.enableBsm
    toggle.has_cc_long = toggle.car_make == "gm" and bool(CP.flags & GMFlags.CC_LONG.value)
    toggle.has_pedal = CP.enableGasInterceptorDEPRECATED
    has_radar = not CP.radarUnavailable
    toggle.has_sdsu = toggle.car_make == "toyota" and bool(FPCP.flags & ToyotaFrogPilotFlags.SMART_DSU.value)
    has_sng = CP.autoResumeSng
    toggle.has_zss = toggle.car_make == "toyota" and bool(FPCP.flags & ToyotaFrogPilotFlags.ZSS.value)
    is_angle_car = CP.steerControlType == structs.CarParams.SteerControlType.angle
    latAccelFactor = FPCP.lateralTuning.torque.latAccelFactor
    longitudinalActuatorDelay = CP.longitudinalActuatorDelay
    toggle.openpilot_longitudinal = CP.openpilotLongitudinalControl and not toggle.disable_openpilot_long
    pcm_cruise = CP.pcmCruise
    startAccel = CP.startAccel
    stopAccel = CP.stopAccel
    steerActuatorDelay = CP.steerActuatorDelay
    steerKp = FPCP.lateralTuning.torque.kp
    steerRatio = CP.steerRatio
    toggle.stoppingDecelRate = CP.stoppingDecelRate
    taco_hacks_allowed = CP.safetyConfigs[0].safetyModel == SafetyModel.hyundaiCanfd
    toggle.use_lkas_for_aol = not toggle.openpilot_longitudinal and CP.safetyConfigs[0].safetyModel == SafetyModel.hyundaiCanfd
    toggle.vEgoStarting = CP.vEgoStarting
    toggle.vEgoStopping = CP.vEgoStopping

    msg_bytes = self.params.get("LiveTorqueParameters")
    if msg_bytes:
      with log.LiveTorqueParametersData.from_bytes(msg_bytes) as LTP:
        has_auto_tune = LTP.useParams
        toggle.liveValid = LTP.liveValid
    else:
      has_auto_tune = False
      toggle.liveValid = False

    if not toggle.use_lkas_for_aol:
      self.params.remove("AlwaysOnLateralLKAS")

    advanced_custom_ui = self.params.get_bool("AdvancedCustomUI") if tuning_level >= level["AdvancedCustomUI"] else default["AdvancedCustomUI"]
    toggle.hide_alerts = advanced_custom_ui and (self.params.get_bool("HideAlerts") if tuning_level >= level["HideAlerts"] else default["HideAlerts"])
    toggle.hide_lead_marker = toggle.openpilot_longitudinal and (advanced_custom_ui and (self.params.get_bool("HideLeadMarker") if tuning_level >= level["HideLeadMarker"] else default["HideLeadMarker"]))
    toggle.hide_max_speed = advanced_custom_ui and (self.params.get_bool("HideMaxSpeed") if tuning_level >= level["HideMaxSpeed"] else default["HideMaxSpeed"])
    toggle.hide_speed = advanced_custom_ui and (self.params.get_bool("HideSpeed") if tuning_level >= level["HideSpeed"] else default["HideSpeed"])
    toggle.hide_speed_limit = advanced_custom_ui and (self.params.get_bool("HideSpeedLimit") if tuning_level >= level["HideSpeedLimit"] else default["HideSpeedLimit"])
    toggle.use_wheel_speed = advanced_custom_ui and (self.params.get_bool("WheelSpeed") if tuning_level >= level["WheelSpeed"] else default["WheelSpeed"])

    advanced_lateral_tuning = self.params.get_bool("AdvancedLateralTune") if tuning_level >= level["AdvancedLateralTune"] else default["AdvancedLateralTune"]
    toggle.force_auto_tune = advanced_lateral_tuning and not has_auto_tune and is_torque_car and (self.params.get_bool("ForceAutoTune") if tuning_level >= level["ForceAutoTune"] else default["ForceAutoTune"])
    toggle.force_auto_tune_off = advanced_lateral_tuning and has_auto_tune and is_torque_car and (self.params.get_bool("ForceAutoTuneOff") if tuning_level >= level["ForceAutoTuneOff"] else default["ForceAutoTuneOff"])
    toggle.steerActuatorDelay = np.clip(self.params.get("SteerDelay") if advanced_lateral_tuning and tuning_level >= level["SteerDelay"] else default["SteerDelay"], 0.01, 1.0)
    toggle.use_custom_steerActuatorDelay = bool(round(toggle.steerActuatorDelay, 2) != round(steerActuatorDelay, 2))
    toggle.friction = np.clip(self.params.get("SteerFriction") if advanced_lateral_tuning and tuning_level >= level["SteerFriction"] else default["SteerFriction"], 0, 0.5)
    toggle.use_custom_friction = bool(round(toggle.friction, 2) != round(friction, 2)) and is_torque_car and not toggle.force_auto_tune or toggle.force_auto_tune_off
    toggle.steerKp = [[0], [np.clip(self.params.get("SteerKP") if advanced_lateral_tuning and is_torque_car and tuning_level >= level["SteerKP"] else default["SteerKP"], steerKp * 0.5, steerKp * 1.5)]]
    toggle.latAccelFactor = np.clip(self.params.get("SteerLatAccel") if advanced_lateral_tuning and tuning_level >= level["SteerLatAccel"] else default["SteerLatAccel"], latAccelFactor * 0.75, latAccelFactor * 1.25)
    toggle.use_custom_latAccelFactor = bool(round(toggle.latAccelFactor, 2) != round(latAccelFactor, 2)) and is_torque_car and not toggle.force_auto_tune or toggle.force_auto_tune_off
    toggle.steerRatio = np.clip(self.params.get("SteerRatio") if advanced_lateral_tuning and tuning_level >= level["SteerRatio"] else default["SteerRatio"], steerRatio * 0.5, steerRatio * 1.5)
    toggle.use_custom_steerRatio = bool(round(toggle.steerRatio, 2) != round(steerRatio, 2)) and not toggle.force_auto_tune or toggle.force_auto_tune_off

    advanced_longitudinal_tuning = self.params.get_bool("AdvancedLongitudinalTune") if tuning_level >= level["AdvancedLongitudinalTune"] else default["AdvancedLongitudinalTune"]
    toggle.longitudinalActuatorDelay = np.clip(self.params.get("LongitudinalActuatorDelay") if advanced_longitudinal_tuning and tuning_level >= level["LongitudinalActuatorDelay"] else default["LongitudinalActuatorDelay"], 0, 1)
    toggle.max_desired_acceleration = np.clip(self.params.get("MaxDesiredAcceleration") if advanced_longitudinal_tuning and tuning_level >= level["MaxDesiredAcceleration"] else default["MaxDesiredAcceleration"], 0.1, 4.0)
    toggle.startAccel = np.clip(self.params.get("StartAccel") if advanced_longitudinal_tuning and tuning_level >= level["StartAccel"] else default["StartAccel"], 0, 4)
    toggle.stopAccel = np.clip(self.params.get("StopAccel") if advanced_longitudinal_tuning and tuning_level >= level["StopAccel"] else default["StopAccel"], -4, 0)
    toggle.stoppingDecelRate = np.clip(self.params.get("StoppingDecelRate") if advanced_longitudinal_tuning and tuning_level >= level["StoppingDecelRate"] else default["StoppingDecelRate"], 0.001, 1)
    toggle.vEgoStarting = np.clip(self.params.get("VEgoStarting") if advanced_longitudinal_tuning and tuning_level >= level["VEgoStarting"] else default["VEgoStarting"], 0.01, 1)
    toggle.vEgoStopping = np.clip(self.params.get("VEgoStopping") if advanced_longitudinal_tuning and tuning_level >= level["VEgoStopping"] else default["VEgoStopping"], 0.01, 1)

    toggle.alert_volume_controller = self.params.get_bool("AlertVolumeControl") if tuning_level >= level["AlertVolumeControl"] else default["AlertVolumeControl"]
    toggle.disengage_volume = self.params.get("DisengageVolume") if toggle.alert_volume_controller and tuning_level >= level["DisengageVolume"] else default["DisengageVolume"]
    toggle.engage_volume = self.params.get("EngageVolume") if toggle.alert_volume_controller and tuning_level >= level["EngageVolume"] else default["EngageVolume"]
    toggle.prompt_volume = self.params.get("PromptVolume") if toggle.alert_volume_controller and tuning_level >= level["PromptVolume"] else default["PromptVolume"]
    toggle.promptDistracted_volume = self.params.get("PromptDistractedVolume") if toggle.alert_volume_controller and tuning_level >= level["PromptDistractedVolume"] else default["PromptDistractedVolume"]
    toggle.refuse_volume = self.params.get("RefuseVolume") if toggle.alert_volume_controller and tuning_level >= level["RefuseVolume"] else default["RefuseVolume"]
    toggle.warningSoft_volume = self.params.get("WarningSoftVolume") if toggle.alert_volume_controller and tuning_level >= level["WarningSoftVolume"] else default["WarningSoftVolume"]
    toggle.warningImmediate_volume = max(self.params.get("WarningImmediateVolume") if toggle.alert_volume_controller and tuning_level >= level["WarningImmediateVolume"] else default["WarningImmediateVolume"], 25)

    toggle.always_on_lateral = self.params.get_bool("AlwaysOnLateral") if tuning_level >= level["AlwaysOnLateral"] else default["AlwaysOnLateral"]
    toggle.always_on_lateral_set &= toggle.always_on_lateral
    toggle.always_on_lateral_lkas = toggle.always_on_lateral_set and toggle.use_lkas_for_aol and (self.params.get_bool("AlwaysOnLateralLKAS") if tuning_level >= level["AlwaysOnLateralLKAS"] else default["AlwaysOnLateralLKAS"])
    toggle.always_on_lateral_main = toggle.always_on_lateral_set and not toggle.use_lkas_for_aol and (self.params.get_bool("AlwaysOnLateralMain") if tuning_level >= level["AlwaysOnLateralMain"] else default["AlwaysOnLateralMain"])
    toggle.always_on_lateral_pause_speed = self.params.get("PauseAOLOnBrake") if toggle.always_on_lateral_set and tuning_level >= level["PauseAOLOnBrake"] else default["PauseAOLOnBrake"]

    toggle.automatic_updates = (self.params.get_bool("AutomaticUpdates") if tuning_level >= level["AutomaticUpdates"] and (self.release_branch or self.vetting_branch) else default["AutomaticUpdates"]) and not BACKUP_PATH.is_file()

    toggle.car_model = self.params.get("CarModel") if tuning_level >= level["CarModel"] else default["CarModel"] or toggle.car_model

    toggle.cluster_offset = self.params.get("ClusterOffset") if toggle.car_make == "toyota" and tuning_level >= level["ClusterOffset"] else default["ClusterOffset"]

    toggle.conditional_experimental_mode = toggle.openpilot_longitudinal and (self.params.get_bool("ConditionalExperimental") if tuning_level >= level["ConditionalExperimental"] else default["ConditionalExperimental"])
    toggle.conditional_curves = toggle.conditional_experimental_mode and (self.params.get_bool("CECurves") if tuning_level >= level["CECurves"] else default["CECurves"])
    toggle.conditional_curves_lead = toggle.conditional_curves and (self.params.get_bool("CECurvesLead") if tuning_level >= level["CECurvesLead"] else default["CECurvesLead"])
    toggle.conditional_lead = toggle.conditional_experimental_mode and (self.params.get_bool("CELead") if tuning_level >= level["CELead"] else default["CELead"])
    toggle.conditional_slower_lead = toggle.conditional_lead and (self.params.get_bool("CESlowerLead") if tuning_level >= level["CESlowerLead"] else default["CESlowerLead"])
    toggle.conditional_stopped_lead = toggle.conditional_lead and (self.params.get_bool("CEStoppedLead") if tuning_level >= level["CEStoppedLead"] else default["CEStoppedLead"])
    toggle.conditional_limit = self.params.get("CESpeed") * speed_conversion if toggle.conditional_experimental_mode and tuning_level >= level["CESpeed"] else default["CESpeed"] * CV.MPH_TO_MS
    toggle.conditional_limit_lead = self.params.get("CESpeedLead") * speed_conversion if toggle.conditional_experimental_mode and tuning_level >= level["CESpeedLead"] else default["CESpeedLead"] * CV.MPH_TO_MS
    toggle.conditional_model_stop_time = self.params.get("CEModelStopTime") if toggle.conditional_experimental_mode and tuning_level >= level["CEModelStopTime"] else default["CEModelStopTime"]
    toggle.conditional_signal = self.params.get("CESignalSpeed") * speed_conversion if toggle.conditional_experimental_mode and tuning_level >= level["CESignalSpeed"] else default["CESignalSpeed"] * CV.MPH_TO_MS
    toggle.conditional_signal_lane_detection = toggle.conditional_signal != 0 and (self.params.get_bool("CESignalLaneDetection") if tuning_level >= level["CESignalLaneDetection"] else default["CESignalLaneDetection"])
    toggle.cem_status = toggle.conditional_experimental_mode and (self.params.get_bool("ShowCEMStatus") if tuning_level >= level["ShowCEMStatus"] else default["ShowCEMStatus"])

    toggle.curve_speed_controller = toggle.openpilot_longitudinal and (self.params.get_bool("CurveSpeedController") if tuning_level >= level["CurveSpeedController"] else default["CurveSpeedController"])
    toggle.csc_status = toggle.curve_speed_controller and (self.params.get_bool("ShowCSCStatus") if tuning_level >= level["ShowCSCStatus"] else default["ShowCSCStatus"])

    toggle.custom_alerts = self.params.get_bool("CustomAlerts") if tuning_level >= level["CustomAlerts"] else default["CustomAlerts"]
    toggle.goat_scream_alert = toggle.custom_alerts and (self.params.get_bool("GoatScream") if tuning_level >= level["GoatScream"] else default["GoatScream"])
    toggle.green_light_alert = toggle.custom_alerts and (self.params.get_bool("GreenLightAlert") if tuning_level >= level["GreenLightAlert"] else default["GreenLightAlert"])
    toggle.lead_departing_alert = toggle.custom_alerts and (self.params.get_bool("LeadDepartingAlert") if tuning_level >= level["LeadDepartingAlert"] else default["LeadDepartingAlert"])
    toggle.loud_blindspot_alert = has_bsm and toggle.custom_alerts and (self.params.get_bool("LoudBlindspotAlert") if tuning_level >= level["LoudBlindspotAlert"] else default["LoudBlindspotAlert"])
    toggle.speed_limit_changed_alert = toggle.custom_alerts and (self.params.get_bool("SpeedLimitChangedAlert") if tuning_level >= level["SpeedLimitChangedAlert"] else default["SpeedLimitChangedAlert"])

    toggle.custom_personalities = toggle.openpilot_longitudinal and (self.params.get_bool("CustomPersonalities") if tuning_level >= level["CustomPersonalities"] else default["CustomPersonalities"])
    aggressive_profile = toggle.custom_personalities and (self.params.get_bool("AggressivePersonalityProfile") if tuning_level >= level["AggressivePersonalityProfile"] else default["AggressivePersonalityProfile"])
    toggle.aggressive_jerk_acceleration = np.clip((self.params.get("AggressiveJerkAcceleration") if aggressive_profile and tuning_level >= level["AggressiveJerkAcceleration"] else default["AggressiveJerkAcceleration"]) / 100, 0.25, 2)
    toggle.aggressive_jerk_deceleration = np.clip((self.params.get("AggressiveJerkDeceleration") if aggressive_profile and tuning_level >= level["AggressiveJerkDeceleration"] else default["AggressiveJerkDeceleration"]) / 100, 0.25, 2)
    toggle.aggressive_jerk_danger = np.clip((self.params.get("AggressiveJerkDanger") if aggressive_profile and tuning_level >= level["AggressiveJerkDanger"] else default["AggressiveJerkDanger"]) / 100, 0.25, 2)
    toggle.aggressive_jerk_speed = np.clip((self.params.get("AggressiveJerkSpeed") if aggressive_profile and tuning_level >= level["AggressiveJerkSpeed"] else default["AggressiveJerkSpeed"]) / 100, 0.25, 2)
    toggle.aggressive_jerk_speed_decrease = np.clip((self.params.get("AggressiveJerkSpeedDecrease") if aggressive_profile and tuning_level >= level["AggressiveJerkSpeedDecrease"] else default["AggressiveJerkSpeedDecrease"]) / 100, 0.25, 2)
    toggle.aggressive_follow = np.clip(self.params.get("AggressiveFollow") if aggressive_profile and tuning_level >= level["AggressiveFollow"] else default["AggressiveFollow"], 1, MAX_T_FOLLOW)
    standard_profile = toggle.custom_personalities and (self.params.get_bool("StandardPersonalityProfile") if tuning_level >= level["StandardPersonalityProfile"] else default["StandardPersonalityProfile"])
    toggle.standard_jerk_acceleration = np.clip((self.params.get("StandardJerkAcceleration") if standard_profile and tuning_level >= level["StandardJerkAcceleration"] else default["StandardJerkAcceleration"]) / 100, 0.25, 2)
    toggle.standard_jerk_deceleration = np.clip((self.params.get("StandardJerkDeceleration") if standard_profile and tuning_level >= level["StandardJerkDeceleration"] else default["StandardJerkDeceleration"]) / 100, 0.25, 2)
    toggle.standard_jerk_danger = np.clip((self.params.get("StandardJerkDanger") if standard_profile and tuning_level >= level["StandardJerkDanger"] else default["StandardJerkDanger"]) / 100, 0.25, 2)
    toggle.standard_jerk_speed = np.clip((self.params.get("StandardJerkSpeed") if standard_profile and tuning_level >= level["StandardJerkSpeed"] else default["StandardJerkSpeed"]) / 100, 0.25, 2)
    toggle.standard_jerk_speed_decrease = np.clip((self.params.get("StandardJerkSpeedDecrease") if standard_profile and tuning_level >= level["StandardJerkSpeedDecrease"] else default["StandardJerkSpeedDecrease"]) / 100, 0.25, 2)
    toggle.standard_follow = np.clip(self.params.get("StandardFollow") if standard_profile and tuning_level >= level["StandardFollow"] else default["StandardFollow"], 1, MAX_T_FOLLOW)
    relaxed_profile = toggle.custom_personalities and (self.params.get_bool("RelaxedPersonalityProfile") if tuning_level >= level["RelaxedPersonalityProfile"] else default["RelaxedPersonalityProfile"])
    toggle.relaxed_jerk_acceleration = np.clip((self.params.get("RelaxedJerkAcceleration") if relaxed_profile and tuning_level >= level["RelaxedJerkAcceleration"] else default["RelaxedJerkAcceleration"]) / 100, 0.25, 2)
    toggle.relaxed_jerk_deceleration = np.clip((self.params.get("RelaxedJerkDeceleration") if relaxed_profile and tuning_level >= level["RelaxedJerkDeceleration"] else default["RelaxedJerkDeceleration"]) / 100, 0.25, 2)
    toggle.relaxed_jerk_danger = np.clip((self.params.get("RelaxedJerkDanger") if relaxed_profile and tuning_level >= level["RelaxedJerkDanger"] else default["RelaxedJerkDanger"]) / 100, 0.25, 2)
    toggle.relaxed_jerk_speed = np.clip((self.params.get("RelaxedJerkSpeed") if relaxed_profile and tuning_level >= level["RelaxedJerkSpeed"] else default["RelaxedJerkSpeed"]) / 100, 0.25, 2)
    toggle.relaxed_jerk_speed_decrease = np.clip((self.params.get("RelaxedJerkSpeedDecrease") if relaxed_profile and tuning_level >= level["RelaxedJerkSpeedDecrease"] else default["RelaxedJerkSpeedDecrease"]) / 100, 0.25, 2)
    toggle.relaxed_follow = np.clip(self.params.get("RelaxedFollow") if relaxed_profile and tuning_level >= level["RelaxedFollow"] else default["RelaxedFollow"], 1, MAX_T_FOLLOW)
    traffic_profile = toggle.custom_personalities and (self.params.get_bool("TrafficPersonalityProfile") if tuning_level >= level["TrafficPersonalityProfile"] else default["TrafficPersonalityProfile"])
    toggle.traffic_mode_jerk_acceleration = [np.clip((self.params.get("TrafficJerkAcceleration") if traffic_profile and tuning_level >= level["TrafficJerkAcceleration"] else default["TrafficJerkAcceleration"]) / 100, 0.25, 2), toggle.aggressive_jerk_acceleration]
    toggle.traffic_mode_jerk_deceleration = [np.clip((self.params.get("TrafficJerkDeceleration") if traffic_profile and tuning_level >= level["TrafficJerkDeceleration"] else default["TrafficJerkDeceleration"]) / 100, 0.25, 2), toggle.aggressive_jerk_deceleration]
    toggle.traffic_mode_jerk_danger = [np.clip((self.params.get("TrafficJerkDanger") if traffic_profile and tuning_level >= level["TrafficJerkDanger"] else default["TrafficJerkDanger"]) / 100, 0.25, 2), toggle.aggressive_jerk_danger]
    toggle.traffic_mode_jerk_speed = [np.clip((self.params.get("TrafficJerkSpeed") if traffic_profile and tuning_level >= level["TrafficJerkSpeed"] else default["TrafficJerkSpeed"]) / 100, 0.25, 2), toggle.aggressive_jerk_speed]
    toggle.traffic_mode_jerk_speed_decrease = [np.clip((self.params.get("TrafficJerkSpeedDecrease") if traffic_profile and tuning_level >= level["TrafficJerkSpeedDecrease"] else default["TrafficJerkSpeedDecrease"]) / 100, 0.25, 2), toggle.aggressive_jerk_speed_decrease]
    toggle.traffic_mode_follow = [np.clip(self.params.get("TrafficFollow") if traffic_profile and tuning_level >= level["TrafficFollow"] else default["TrafficFollow"], 0.5, MAX_T_FOLLOW), toggle.aggressive_follow]

    custom_ui = self.params.get_bool("CustomUI") if tuning_level >= level["CustomUI"] else default["CustomUI"]
    toggle.acceleration_path = toggle.openpilot_longitudinal and custom_ui and (self.params.get_bool("AccelerationPath") if tuning_level >= level["AccelerationPath"] else default["AccelerationPath"])
    toggle.adjacent_paths = custom_ui and (self.params.get_bool("AdjacentPath") if tuning_level >= level["AdjacentPath"] else default["AdjacentPath"])
    toggle.blind_spot_path = has_bsm and custom_ui and (self.params.get_bool("BlindSpotPath") if tuning_level >= level["BlindSpotPath"] else default["BlindSpotPath"])
    toggle.compass = custom_ui and (self.params.get_bool("Compass") if tuning_level >= level["Compass"] else default["Compass"])
    toggle.pedals_on_ui = toggle.openpilot_longitudinal and (custom_ui and (self.params.get_bool("PedalsOnUI") if tuning_level >= level["PedalsOnUI"] else default["PedalsOnUI"]))
    toggle.dynamic_pedals_on_ui = toggle.pedals_on_ui and (self.params.get_bool("DynamicPedalsOnUI") if tuning_level >= level["DynamicPedalsOnUI"] else default["DynamicPedalsOnUI"])
    toggle.static_pedals_on_ui = toggle.pedals_on_ui and (self.params.get_bool("StaticPedalsOnUI") if tuning_level >= level["StaticPedalsOnUI"] else default["StaticPedalsOnUI"])
    toggle.rotating_wheel = custom_ui and (self.params.get_bool("RotatingWheel") if tuning_level >= level["RotatingWheel"] else default["RotatingWheel"])

    toggle.developer_ui = self.params.get_bool("DeveloperUI") if tuning_level >= level["DeveloperUI"] else default["DeveloperUI"]
    developer_metrics = toggle.developer_ui and (self.params.get_bool("DeveloperMetrics") if tuning_level >= level["DeveloperMetrics"] else default["DeveloperMetrics"])
    border_metrics = developer_metrics and (self.params.get_bool("BorderMetrics") if tuning_level >= level["BorderMetrics"] else default["BorderMetrics"])
    toggle.blind_spot_metrics = has_bsm and border_metrics and (self.params.get_bool("BlindSpotMetrics") if tuning_level >= level["BlindSpotMetrics"] else default["BlindSpotMetrics"])
    toggle.signal_metrics = border_metrics and (self.params.get_bool("SignalMetrics") if tuning_level >= level["SignalMetrics"] else default["SignalMetrics"])
    toggle.steering_metrics = border_metrics and (self.params.get_bool("ShowSteering") if tuning_level >= level["ShowSteering"] else default["ShowSteering"])
    toggle.show_fps = developer_metrics and (self.params.get_bool("FPSCounter") if tuning_level >= level["FPSCounter"] else default["FPSCounter"])
    toggle.adjacent_path_metrics = developer_metrics and (self.params.get_bool("AdjacentPathMetrics") if tuning_level >= level["AdjacentPathMetrics"] else default["AdjacentPathMetrics"])
    toggle.lead_metrics = developer_metrics and (self.params.get_bool("LeadInfo") if tuning_level >= level["LeadInfo"] else default["LeadInfo"])
    toggle.numerical_temp = developer_metrics and (self.params.get_bool("NumericalTemp") if tuning_level >= level["NumericalTemp"] else default["NumericalTemp"])
    toggle.fahrenheit = toggle.numerical_temp and (self.params.get_bool("Fahrenheit") if tuning_level >= level["Fahrenheit"] else default["Fahrenheit"])
    toggle.cpu_metrics = developer_metrics and (self.params.get_bool("ShowCPU") if tuning_level >= level["ShowCPU"] else default["ShowCPU"])
    toggle.gpu_metrics = developer_metrics and (self.params.get_bool("ShowGPU") if tuning_level >= level["ShowGPU"] else default["ShowGPU"])
    toggle.ip_metrics = developer_metrics and (self.params.get_bool("ShowIP") if tuning_level >= level["ShowIP"] else default["ShowIP"])
    toggle.memory_metrics = developer_metrics and (self.params.get_bool("ShowMemoryUsage") if tuning_level >= level["ShowMemoryUsage"] else default["ShowMemoryUsage"])
    toggle.storage_left_metrics = developer_metrics and (self.params.get_bool("ShowStorageLeft") if tuning_level >= level["ShowStorageLeft"] else default["ShowStorageLeft"])
    toggle.storage_used_metrics = developer_metrics and (self.params.get_bool("ShowStorageUsed") if tuning_level >= level["ShowStorageUsed"] else default["ShowStorageUsed"])
    toggle.use_si_metrics = developer_metrics and (self.params.get_bool("UseSI") if tuning_level >= level["UseSI"] else default["UseSI"])
    toggle.developer_sidebar = toggle.developer_ui and (self.params.get_bool("DeveloperSidebar") if tuning_level >= level["DeveloperSidebar"] else default["DeveloperSidebar"])
    toggle.developer_sidebar_metric1 = self.params.get("DeveloperSidebarMetric1") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric1"] else default["DeveloperSidebarMetric1"]
    toggle.developer_sidebar_metric2 = self.params.get("DeveloperSidebarMetric2") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric2"] else default["DeveloperSidebarMetric2"]
    toggle.developer_sidebar_metric3 = self.params.get("DeveloperSidebarMetric3") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric3"] else default["DeveloperSidebarMetric3"]
    toggle.developer_sidebar_metric4 = self.params.get("DeveloperSidebarMetric4") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric4"] else default["DeveloperSidebarMetric4"]
    toggle.developer_sidebar_metric5 = self.params.get("DeveloperSidebarMetric5") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric5"] else default["DeveloperSidebarMetric5"]
    toggle.developer_sidebar_metric6 = self.params.get("DeveloperSidebarMetric6") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric6"] else default["DeveloperSidebarMetric6"]
    toggle.developer_sidebar_metric7 = self.params.get("DeveloperSidebarMetric7") if toggle.developer_sidebar and tuning_level >= level["DeveloperSidebarMetric7"] else default["DeveloperSidebarMetric7"]
    developer_widgets = toggle.developer_ui and (self.params.get_bool("DeveloperWidgets") if tuning_level >= level["DeveloperWidgets"] else default["DeveloperWidgets"])
    toggle.adjacent_lead_tracking = has_radar and developer_widgets and (self.params.get_bool("AdjacentLeadsUI") if tuning_level >= level["AdjacentLeadsUI"] else default["AdjacentLeadsUI"])
    toggle.radar_tracks = has_radar and developer_widgets and (self.params.get_bool("RadarTracksUI") if tuning_level >= level["RadarTracksUI"] else default["RadarTracksUI"])
    toggle.show_stopping_point = toggle.openpilot_longitudinal and developer_widgets and (self.params.get_bool("ShowStoppingPoint") if tuning_level >= level["ShowStoppingPoint"] else default["ShowStoppingPoint"])
    toggle.show_stopping_point_metrics = toggle.show_stopping_point and (self.params.get_bool("ShowStoppingPointMetrics") if tuning_level >= level["ShowStoppingPointMetrics"] else default["ShowStoppingPointMetrics"])

    device_management = self.params.get_bool("DeviceManagement") if tuning_level >= level["DeviceManagement"] else default["DeviceManagement"]
    device_shutdown_setting = self.params.get("DeviceShutdown") if device_management and tuning_level >= level["DeviceShutdown"] else default["DeviceShutdown"]
    toggle.device_shutdown_time = (device_shutdown_setting - 3) * 3600 if device_shutdown_setting >= 4 else device_shutdown_setting * (60 * 15)
    toggle.increase_thermal_limits = device_management and (self.params.get_bool("IncreaseThermalLimits") if tuning_level >= level["IncreaseThermalLimits"] else default["IncreaseThermalLimits"])
    toggle.low_voltage_shutdown = np.clip(self.params.get("LowVoltageShutdown") if device_management and tuning_level >= level["LowVoltageShutdown"] else default["LowVoltageShutdown"], VBATT_PAUSE_CHARGING, 12.5)
    toggle.no_logging = device_management and (self.params.get_bool("NoLogging") if tuning_level >= level["NoLogging"] else default["NoLogging"]) and not self.vetting_branch or toggle.force_onroad
    toggle.no_uploads = device_management and (self.params.get_bool("NoUploads") if tuning_level >= level["NoUploads"] else default["NoUploads"]) and not self.vetting_branch or toggle.use_higher_bitrate
    toggle.no_onroad_uploads = toggle.no_uploads and (self.params.get_bool("DisableOnroadUploads") if tuning_level >= level["DisableOnroadUploads"] else default["DisableOnroadUploads"]) and not toggle.use_higher_bitrate

    distance_button_control = self.params.get("DistanceButtonControl") if tuning_level >= level["DistanceButtonControl"] else default["DistanceButtonControl"]
    toggle.experimental_mode_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press = toggle.experimental_mode_via_distance
    toggle.force_coast_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance = distance_button_control == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    distance_button_control_long = self.params.get("LongDistanceButtonControl") if tuning_level >= level["LongDistanceButtonControl"] else default["LongDistanceButtonControl"]
    toggle.experimental_mode_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_distance_long
    toggle.force_coast_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance_long = distance_button_control_long == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    distance_button_control_very_long = self.params.get("VeryLongDistanceButtonControl") if tuning_level >= level["VeryLongDistanceButtonControl"] else default["VeryLongDistanceButtonControl"]
    toggle.experimental_mode_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_distance_very_long
    toggle.force_coast_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance_very_long = distance_button_control_very_long == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    toggle.experimental_gm_tune = toggle.openpilot_longitudinal and toggle.car_make == "gm" and (self.params.get_bool("ExperimentalGMTune") if tuning_level >= level["ExperimentalGMTune"] else default["ExperimentalGMTune"])
    toggle.stoppingDecelRate = 0.3 if toggle.experimental_gm_tune else toggle.stoppingDecelRate
    toggle.vEgoStarting = 0.15 if toggle.experimental_gm_tune else toggle.vEgoStarting
    toggle.vEgoStopping = 0.15 if toggle.experimental_gm_tune else toggle.vEgoStopping

    toggle.force_fingerprint = (self.params.get_bool("ForceFingerprint") if tuning_level >= level["ForceFingerprint"] else default["ForceFingerprint"]) and toggle.car_model is not None

    toggle.holiday_themes = self.params.get_bool("HolidayThemes") if tuning_level >= level["HolidayThemes"] else default["HolidayThemes"]
    toggle.current_holiday_theme = holiday_theme if toggle.holiday_themes else "stock"

    toggle.lane_changes = self.params.get_bool("LaneChanges") if tuning_level >= level["LaneChanges"] else default["LaneChanges"]
    toggle.lane_change_delay = self.params.get("LaneChangeTime") if toggle.lane_changes and tuning_level >= level["LaneChangeTime"] else default["LaneChangeTime"]
    toggle.lane_detection_width = self.params.get("LaneDetectionWidth") * distance_conversion if toggle.lane_changes and tuning_level >= level["LaneDetectionWidth"] else default["LaneDetectionWidth"] * CV.FOOT_TO_METER
    toggle.lane_detection = toggle.lane_detection_width > 0
    toggle.minimum_lane_change_speed = self.params.get("MinimumLaneChangeSpeed") * speed_conversion if toggle.lane_changes and tuning_level >= level["MinimumLaneChangeSpeed"] else default["MinimumLaneChangeSpeed"] * CV.MPH_TO_MS
    toggle.nudgeless = toggle.lane_changes and (self.params.get_bool("NudgelessLaneChange") if tuning_level >= level["NudgelessLaneChange"] else default["NudgelessLaneChange"])
    toggle.one_lane_change = toggle.lane_changes and (self.params.get_bool("OneLaneChange") if tuning_level >= level["OneLaneChange"] else default["OneLaneChange"])

    lateral_tuning = self.params.get_bool("LateralTune") if tuning_level >= level["LateralTune"] else default["LateralTune"]
    toggle.force_torque_controller = lateral_tuning and not is_torque_car and (self.params.get_bool("ForceTorqueController") if tuning_level >= level["ForceTorqueController"] else default["ForceTorqueController"])
    toggle.use_turn_desires = lateral_tuning and (self.params.get_bool("TurnDesires") if tuning_level >= level["TurnDesires"] else default["TurnDesires"])

    lkas_button_control = (self.params.get("LKASButtonControl") if tuning_level >= level["LKASButtonControl"] else default["LKASButtonControl"]) if toggle.car_make != "subaru" else 0
    toggle.experimental_mode_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_lkas
    toggle.force_coast_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_lkas = lkas_button_control == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    toggle.lock_doors_timer = self.params.get("LockDoorsTimer") if toggle.car_make == "toyota" and tuning_level >= level["LockDoorsTimer"] else default["LockDoorsTimer"]

    longitudinal_tuning = toggle.openpilot_longitudinal and (self.params.get_bool("LongitudinalTune") if tuning_level >= level["LongitudinalTune"] else default["LongitudinalTune"])
    toggle.acceleration_profile = self.params.get("AccelerationProfile") if longitudinal_tuning and tuning_level >= level["AccelerationProfile"] else default["AccelerationProfile"]
    toggle.deceleration_profile = self.params.get("DecelerationProfile") if longitudinal_tuning and tuning_level >= level["DecelerationProfile"] else default["DecelerationProfile"]
    toggle.human_acceleration = longitudinal_tuning and (self.params.get_bool("HumanAcceleration") if tuning_level >= level["HumanAcceleration"] else default["HumanAcceleration"])
    toggle.human_following = longitudinal_tuning and (self.params.get_bool("HumanFollowing") if tuning_level >= level["HumanFollowing"] else default["HumanFollowing"])
    toggle.human_lane_changes = longitudinal_tuning and has_radar and (self.params.get_bool("HumanLaneChanges") if tuning_level >= level["HumanLaneChanges"] else default["HumanLaneChanges"])
    toggle.lead_detection_probability = np.clip((self.params.get("LeadDetectionThreshold") if longitudinal_tuning and tuning_level >= level["LeadDetectionThreshold"] else default["LeadDetectionThreshold"]) / 100, 0.25, 0.50)
    toggle.taco_tune = longitudinal_tuning and (self.params.get_bool("TacoTune") if tuning_level >= level["TacoTune"] else default["TacoTune"])

    toggle.available_models = (self.params.get("AvailableModels") or "") + f",{DEFAULT_MODEL}"
    toggle.available_model_names = (self.params.get("AvailableModelNames") or "") + f",{DEFAULT_MODEL_NAME}"
    downloaded_models = [model for model in toggle.available_models.split(",") if (MODELS_PATH / f"{model}.thneed").is_file() or all((MODELS_PATH / f"{model}_{filename}").is_file() for filename, _ in TINYGRAD_FILES)]
    model_versions = (self.params.get("AvailableModelVersions") or "") + f",{DEFAULT_MODEL_VERSION}"
    toggle.model_randomizer = self.params.get_bool("ModelRandomizer") if tuning_level >= level["ModelRandomizer"] else default["ModelRandomizer"]
    if toggle.model_randomizer:
      if not started:
        blacklisted_models = self.params.get("BlacklistedModels").split(",")
        selectable_models = [model for model in downloaded_models if model not in blacklisted_models]
        toggle.model = random.choice(selectable_models) if selectable_models else DEFAULT_MODEL
        toggle.model_name = "Mystery Model 👻"
        toggle.model_version = model_versions.split(",")[toggle.available_models.split(",").index(toggle.model)]
    else:
      model = ((self.params.get("Model") if tuning_level >= level["Model"] else default["Model"]) or DEFAULT_MODEL).removesuffix("_default")
      if model in downloaded_models:
        toggle.model = model
        toggle.model_name = dict(zip(toggle.available_models.split(","), toggle.available_model_names.split(",")))[toggle.model]
        toggle.model_version = dict(zip(toggle.available_models.split(","), model_versions.split(",")))[toggle.model]
      else:
        toggle.model = DEFAULT_MODEL
        toggle.model_name = DEFAULT_MODEL_NAME
        toggle.model_version = DEFAULT_MODEL_VERSION

    toggle.model_ui = self.params.get_bool("ModelUI") if tuning_level >= level["ModelUI"] else default["ModelUI"]
    toggle.dynamic_path_width = toggle.model_ui and (self.params.get_bool("DynamicPathWidth") if tuning_level >= level["DynamicPathWidth"] else default["DynamicPathWidth"])
    toggle.lane_line_width = self.params.get("LaneLinesWidth") * small_distance_conversion / 200 if toggle.model_ui and tuning_level >= level["LaneLinesWidth"] else default["LaneLinesWidth"] * CV.INCH_TO_CM / 200
    toggle.path_edge_width = self.params.get("PathEdgeWidth") if toggle.model_ui and tuning_level >= level["PathEdgeWidth"] else default["PathEdgeWidth"]
    toggle.path_width = self.params.get("PathWidth") * distance_conversion / 2 if toggle.model_ui and tuning_level >= level["PathWidth"] else default["PathWidth"] * CV.FOOT_TO_METER / 2
    toggle.road_edge_width = self.params.get("RoadEdgesWidth") * small_distance_conversion / 200 if toggle.model_ui and tuning_level >= level["RoadEdgesWidth"] else default["RoadEdgesWidth"] * CV.INCH_TO_CM / 200
    toggle.unlimited_road_ui_length = toggle.model_ui and (self.params.get_bool("UnlimitedLength") if tuning_level >= level["UnlimitedLength"] else default["UnlimitedLength"])

    toggle.navigation_ui = self.params.get_bool("NavigationUI") if tuning_level >= level["NavigationUI"] else default["NavigationUI"]
    toggle.road_name_ui = toggle.navigation_ui and (self.params.get_bool("RoadNameUI") if tuning_level >= level["RoadNameUI"] else default["RoadNameUI"])
    toggle.show_speed_limits = toggle.navigation_ui and (self.params.get_bool("ShowSpeedLimits") if tuning_level >= level["ShowSpeedLimits"] else default["ShowSpeedLimits"])
    toggle.speed_limit_vienna = toggle.navigation_ui and (self.params.get_bool("UseVienna") if tuning_level >= level["UseVienna"] else default["UseVienna"])

    personalize_openpilot = self.params.get_bool("PersonalizeOpenpilot") if tuning_level >= level["PersonalizeOpenpilot"] else default["PersonalizeOpenpilot"]
    toggle.color_scheme = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else self.params.get("CustomColors") if personalize_openpilot else "stock"
    toggle.distance_icons = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else self.params.get("CustomDistanceIcons") if personalize_openpilot else "stock"
    toggle.icon_pack = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else self.params.get("CustomIcons") if personalize_openpilot else "stock"
    toggle.random_themes = personalize_openpilot and (self.params.get_bool("RandomThemes") if tuning_level >= level["RandomThemes"] else default["RandomThemes"])
    toggle.signal_icons = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else self.params.get("CustomSignals") if personalize_openpilot else "stock"
    toggle.sound_pack = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else self.params.get("CustomSounds") if personalize_openpilot else "stock"
    if not toggle.random_themes:
      toggle.wheel_image = toggle.current_holiday_theme if toggle.current_holiday_theme != "stock" else self.params.get("WheelIcon") if personalize_openpilot else "stock"
    else:
      toggle.wheel_image = next((file.resolve().stem for file in (ACTIVE_THEME_PATH / "steering_wheel").glob("wheel.*")), "none")

    quality_of_life_lateral = self.params.get_bool("QOLLateral") if tuning_level >= level["QOLLateral"] else default["QOLLateral"]
    toggle.pause_lateral_below_speed = self.params.get("PauseLateralSpeed") * speed_conversion if quality_of_life_lateral and tuning_level >= level["PauseLateralSpeed"] else default["PauseLateralSpeed"] * CV.MPH_TO_MS
    toggle.pause_lateral_below_signal = toggle.pause_lateral_below_speed != 0 and (self.params.get_bool("PauseLateralOnSignal") if tuning_level >= level["PauseLateralOnSignal"] else default["PauseLateralOnSignal"])

    quality_of_life_longitudinal = self.params.get_bool("QOLLongitudinal") if tuning_level >= level["QOLLongitudinal"] else default["QOLLongitudinal"]
    toggle.cruise_increase = self.params.get("CustomCruise") if quality_of_life_longitudinal and not pcm_cruise and tuning_level >= level["CustomCruise"] else default["CustomCruise"]
    toggle.cruise_increase_long = self.params.get("CustomCruiseLong") if quality_of_life_longitudinal and not pcm_cruise and tuning_level >= level["CustomCruiseLong"] else default["CustomCruiseLong"]
    toggle.force_stops = quality_of_life_longitudinal and (self.params.get_bool("ForceStops") if tuning_level >= level["ForceStops"] else default["ForceStops"])
    toggle.increase_stopped_distance = self.params.get("IncreasedStoppedDistance") * distance_conversion if quality_of_life_longitudinal and tuning_level >= level["IncreasedStoppedDistance"] else default["IncreasedStoppedDistance"] * CV.FOOT_TO_METER
    map_gears = quality_of_life_longitudinal and (self.params.get_bool("MapGears") if tuning_level >= level["MapGears"] else default["MapGears"])
    toggle.map_acceleration = map_gears and (self.params.get_bool("MapAcceleration") if tuning_level >= level["MapAcceleration"] else default["MapAcceleration"])
    toggle.map_deceleration = map_gears and (self.params.get_bool("MapDeceleration") if tuning_level >= level["MapDeceleration"] else default["MapDeceleration"])
    toggle.reverse_cruise_increase = quality_of_life_longitudinal and toggle.car_make == "toyota" and pcm_cruise and (self.params.get_bool("ReverseCruise") if tuning_level >= level["ReverseCruise"] else default["ReverseCruise"])
    toggle.set_speed_offset = self.params.get("SetSpeedOffset") * (1 if toggle.is_metric else CV.MPH_TO_KPH) if quality_of_life_longitudinal and not pcm_cruise and tuning_level >= level["SetSpeedOffset"] else default["SetSpeedOffset"] * CV.MPH_TO_KPH
    toggle.weather_presets = quality_of_life_longitudinal and (self.params.get_bool("WeatherPresets") if tuning_level >= level["WeatherPresets"] else default["WeatherPresets"])
    toggle.increase_following_distance_low_visibility = self.params.get("IncreaseFollowingLowVisibility") if toggle.weather_presets and tuning_level >= level["IncreaseFollowingLowVisibility"] else default["IncreaseFollowingLowVisibility"]
    toggle.increase_following_distance_rain = self.params.get("IncreaseFollowingRain") if toggle.weather_presets and tuning_level >= level["IncreaseFollowingRain"] else default["IncreaseFollowingRain"]
    toggle.increase_following_distance_rain_storm = self.params.get("IncreaseFollowingRainStorm") if toggle.weather_presets and tuning_level >= level["IncreaseFollowingRainStorm"] else default["IncreaseFollowingRainStorm"]
    toggle.increase_following_distance_snow = self.params.get("IncreaseFollowingSnow") if toggle.weather_presets and tuning_level >= level["IncreaseFollowingSnow"] else default["IncreaseFollowingSnow"]
    toggle.increase_stopped_distance_low_visibility = self.params.get("IncreasedStoppedDistanceLowVisibility") * distance_conversion if toggle.weather_presets and tuning_level >= level["IncreasedStoppedDistanceLowVisibility"] else default["IncreasedStoppedDistanceLowVisibility"] * CV.FOOT_TO_METER
    toggle.increase_stopped_distance_rain = self.params.get("IncreasedStoppedDistanceRain") * distance_conversion if toggle.weather_presets and tuning_level >= level["IncreasedStoppedDistanceRain"] else default["IncreasedStoppedDistanceRain"] * CV.FOOT_TO_METER
    toggle.increase_stopped_distance_rain_storm = self.params.get("IncreasedStoppedDistanceRainStorm") * distance_conversion if toggle.weather_presets and tuning_level >= level["IncreasedStoppedDistanceRainStorm"] else default["IncreasedStoppedDistanceRainStorm"] * CV.FOOT_TO_METER
    toggle.increase_stopped_distance_snow = self.params.get("IncreasedStoppedDistanceSnow") * distance_conversion if toggle.weather_presets and tuning_level >= level["IncreasedStoppedDistanceSnow"] else default["IncreasedStoppedDistanceSnow"] * CV.FOOT_TO_METER
    toggle.reduce_acceleration_low_visibility = (self.params.get("ReduceAccelerationLowVisibility") if toggle.weather_presets and tuning_level >= level["ReduceAccelerationLowVisibility"] else default["ReduceAccelerationLowVisibility"]) / 100
    toggle.reduce_acceleration_rain = (self.params.get("ReduceAccelerationRain") if toggle.weather_presets and tuning_level >= level["ReduceAccelerationRain"] else default["ReduceAccelerationRain"]) / 100
    toggle.reduce_acceleration_rain_storm = (self.params.get("ReduceAccelerationRainStorm") if toggle.weather_presets and tuning_level >= level["ReduceAccelerationRainStorm"] else default["ReduceAccelerationRainStorm"]) / 100
    toggle.reduce_acceleration_snow = (self.params.get("ReduceAccelerationSnow") if toggle.weather_presets and tuning_level >= level["ReduceAccelerationSnow"] else default["ReduceAccelerationSnow"]) / 100
    toggle.reduce_lateral_acceleration_low_visibility = (self.params.get("ReduceLateralAccelerationLowVisibility") if toggle.weather_presets and tuning_level >= level["ReduceLateralAccelerationLowVisibility"] else default["ReduceLateralAccelerationLowVisibility"]) / 100
    toggle.reduce_lateral_acceleration_rain = (self.params.get("ReduceLateralAccelerationRain") if toggle.weather_presets and tuning_level >= level["ReduceLateralAccelerationRain"] else default["ReduceLateralAccelerationRain"]) / 100
    toggle.reduce_lateral_acceleration_rain_storm = (self.params.get("ReduceLateralAccelerationRainStorm") if toggle.weather_presets and tuning_level >= level["ReduceLateralAccelerationRainStorm"] else default["ReduceLateralAccelerationRainStorm"]) / 100
    toggle.reduce_lateral_acceleration_snow = (self.params.get("ReduceLateralAccelerationSnow") if toggle.weather_presets and tuning_level >= level["ReduceLateralAccelerationSnow"] else default["ReduceLateralAccelerationSnow"]) / 100

    quality_of_life_visuals = self.params.get_bool("QOLVisuals") if tuning_level >= level["QOLVisuals"] else default["QOLVisuals"]
    toggle.camera_view = self.params.get("CameraView") if quality_of_life_visuals and tuning_level >= level["CameraView"] else default["CameraView"]
    toggle.driver_camera_in_reverse = quality_of_life_visuals and (self.params.get_bool("DriverCamera") if tuning_level >= level["DriverCamera"] else default["DriverCamera"])
    toggle.onroad_distance_button = toggle.openpilot_longitudinal and quality_of_life_visuals and (self.params.get_bool("OnroadDistanceButton") if tuning_level >= level["OnroadDistanceButton"] else default["OnroadDistanceButton"])
    toggle.stopped_timer = quality_of_life_visuals and (self.params.get_bool("StoppedTimer") if tuning_level >= level["StoppedTimer"] else default["StoppedTimer"])

    toggle.rainbow_path = self.params.get_bool("RainbowPath") if tuning_level >= level["RainbowPath"] else default["RainbowPath"]

    toggle.random_events = self.params.get_bool("RandomEvents") if tuning_level >= level["RandomEvents"] else default["RandomEvents"]

    screen_management = self.params.get_bool("ScreenManagement") if tuning_level >= level["ScreenManagement"] else default["ScreenManagement"]
    toggle.screen_brightness = self.params.get("ScreenBrightness") if screen_management and tuning_level >= level["ScreenBrightness"] else default["ScreenBrightness"]
    toggle.screen_brightness_onroad = self.params.get("ScreenBrightnessOnroad") if screen_management and tuning_level >= level["ScreenBrightnessOnroad"] else default["ScreenBrightnessOnroad"]
    toggle.screen_recorder = screen_management and (self.params.get_bool("ScreenRecorder") if tuning_level >= level["ScreenRecorder"] else default["ScreenRecorder"])
    toggle.screen_timeout = self.params.get("ScreenTimeout") if screen_management and tuning_level >= level["ScreenTimeout"] else default["ScreenTimeout"]
    toggle.screen_timeout_onroad = self.params.get("ScreenTimeoutOnroad") if screen_management and tuning_level >= level["ScreenTimeoutOnroad"] else default["ScreenTimeoutOnroad"]
    toggle.standby_mode = screen_management and (self.params.get_bool("StandbyMode") if tuning_level >= level["StandbyMode"] else default["StandbyMode"])

    toggle.sng_hack = toggle.openpilot_longitudinal and toggle.car_make == "toyota" and not toggle.has_pedal and not has_sng and (self.params.get_bool("SNGHack") if tuning_level >= level["SNGHack"] else default["SNGHack"])

    toggle.speed_limit_controller = toggle.openpilot_longitudinal and (self.params.get_bool("SpeedLimitController") if tuning_level >= level["SpeedLimitController"] else default["SpeedLimitController"])
    toggle.force_mph_dashboard = toggle.speed_limit_controller and (self.params.get_bool("ForceMPHDashboard") if tuning_level >= level["ForceMPHDashboard"] else default["ForceMPHDashboard"])
    toggle.map_speed_lookahead_higher = self.params.get("SLCLookaheadHigher") if toggle.speed_limit_controller and tuning_level >= level["SLCLookaheadHigher"] else default["SLCLookaheadHigher"]
    toggle.map_speed_lookahead_lower = self.params.get("SLCLookaheadLower") if toggle.speed_limit_controller and tuning_level >= level["SLCLookaheadLower"] else default["SLCLookaheadLower"]
    toggle.set_speed_limit = toggle.speed_limit_controller and (self.params.get_bool("SetSpeedLimit") if tuning_level >= level["SetSpeedLimit"] else default["SetSpeedLimit"])
    toggle.show_speed_limit_offset = toggle.speed_limit_controller and (self.params.get_bool("ShowSLCOffset") if tuning_level >= level["ShowSLCOffset"] else default["ShowSLCOffset"])
    slc_fallback_method = self.params.get("SLCFallback") if toggle.speed_limit_controller and tuning_level >= level["SLCFallback"] else default["SLCFallback"]
    toggle.slc_fallback_experimental_mode = slc_fallback_method == 1
    toggle.slc_fallback_previous_speed_limit = slc_fallback_method == 2
    toggle.slc_fallback_set_speed = slc_fallback_method == 0
    toggle.slc_mapbox_filler = (toggle.show_speed_limits or toggle.speed_limit_controller) and self.params.get("MapboxSecretKey") is not None and (self.params.get_bool("SLCMapboxFiller") if tuning_level >= level["SLCMapboxFiller"] else default["SLCMapboxFiller"])
    toggle.speed_limit_confirmation = toggle.speed_limit_controller and (self.params.get_bool("SLCConfirmation") if tuning_level >= level["SLCConfirmation"] else default["SLCConfirmation"])
    toggle.speed_limit_confirmation_higher = toggle.speed_limit_confirmation and (self.params.get_bool("SLCConfirmationHigher") if tuning_level >= level["SLCConfirmationHigher"] else default["SLCConfirmationHigher"])
    toggle.speed_limit_confirmation_lower = toggle.speed_limit_confirmation and (self.params.get_bool("SLCConfirmationLower") if tuning_level >= level["SLCConfirmationLower"] else default["SLCConfirmationLower"])
    slc_override_method = self.params.get("SLCOverride") if toggle.speed_limit_controller and tuning_level >= level["SLCOverride"] else default["SLCOverride"]
    toggle.speed_limit_controller_override_manual = slc_override_method == 1
    toggle.speed_limit_controller_override_set_speed = slc_override_method == 2
    toggle.speed_limit_offset1 = (self.params.get("Offset1") * speed_conversion if tuning_level >= level["Offset1"] else default["Offset1"] * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset2 = (self.params.get("Offset2") * speed_conversion if tuning_level >= level["Offset2"] else default["Offset2"] * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset3 = (self.params.get("Offset3") * speed_conversion if tuning_level >= level["Offset3"] else default["Offset3"] * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset4 = (self.params.get("Offset4") * speed_conversion if tuning_level >= level["Offset4"] else default["Offset4"] * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset5 = (self.params.get("Offset5") * speed_conversion if tuning_level >= level["Offset5"] else default["Offset5"] * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset6 = (self.params.get("Offset6") * speed_conversion if tuning_level >= level["Offset6"] else default["Offset6"] * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_offset7 = (self.params.get("Offset7") * speed_conversion if tuning_level >= level["Offset7"] else default["Offset7"] * CV.MPH_TO_MS) if toggle.speed_limit_controller else 0
    toggle.speed_limit_priority1 = self.params.get("SLCPriority1") if toggle.speed_limit_controller and tuning_level >= level["SLCPriority1"] else default["SLCPriority1"]
    toggle.speed_limit_priority2 = self.params.get("SLCPriority2") if toggle.speed_limit_controller and tuning_level >= level["SLCPriority2"] else default["SLCPriority2"]
    toggle.speed_limit_priority_highest = toggle.speed_limit_priority1 == "Highest"
    toggle.speed_limit_priority_lowest = toggle.speed_limit_priority1 == "Lowest"
    toggle.speed_limit_sources = toggle.speed_limit_controller and (self.params.get_bool("SpeedLimitSources") if tuning_level >= level["SpeedLimitSources"] else default["SpeedLimitSources"])

    toggle.speed_limit_filler = self.params.get_bool("SpeedLimitFiller") if tuning_level >= level["SpeedLimitFiller"] else default["SpeedLimitFiller"]

    toggle.startup_alert_top = self.params.get("StartupMessageTop") if tuning_level >= level["StartupMessageTop"] else default["StartupMessageTop"]
    toggle.startup_alert_bottom = self.params.get("StartupMessageBottom") if tuning_level >= level["StartupMessageBottom"] else default["StartupMessageBottom"]

    toggle.taco_tune_hack = taco_hacks_allowed and (self.params.get_bool("TacoTuneHacks") if tuning_level >= level["TacoTuneHacks"] else default["TacoTuneHacks"])

    toggle.tethering_config = self.params.get("TetheringEnabled") if tuning_level >= level["TetheringEnabled"] else default["TetheringEnabled"]

    toyota_doors = toggle.car_make == "toyota" and (self.params.get_bool("ToyotaDoors") if tuning_level >= level["ToyotaDoors"] else default["ToyotaDoors"])
    toggle.lock_doors = toyota_doors and (self.params.get_bool("LockDoors") if tuning_level >= level["LockDoors"] else default["LockDoors"])
    toggle.unlock_doors = toyota_doors and (self.params.get_bool("UnlockDoors") if tuning_level >= level["UnlockDoors"] else default["UnlockDoors"])

    toggle.volt_sng = toggle.car_model == "CHEVROLET_VOLT" and (self.params.get_bool("VoltSNG") if tuning_level >= level["VoltSNG"] else default["VoltSNG"])

    self.params_memory.put("FrogPilotToggles", toggle.__dict__)
    self.params_memory.remove("FrogPilotTogglesUpdated")
