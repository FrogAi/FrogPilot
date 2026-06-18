#!/usr/bin/env python3
import json
import math
import random
import tomllib

from functools import cache
from pathlib import Path
from types import SimpleNamespace

import cereal.messaging as messaging

from cereal import car, custom, log
from opendbc.car import gen_empty_fingerprint
from opendbc.car.car_helpers import interfaces
from opendbc.car.gm.values import GMFlags
from opendbc.car.hyundai.values import HyundaiFlags
from opendbc.car.interfaces import TORQUE_SUBSTITUTE_PATH, CarInterfaceBase, GearShifter
from opendbc.car.mock.values import CAR as MOCK
from opendbc.car.subaru.values import SubaruFlags
from opendbc.car.toyota.values import ToyotaFrogPilotFlags
from openpilot.common.basedir import BASEDIR
from openpilot.common.constants import CV
from openpilot.common.params import Params
from openpilot.selfdrive.controls.lib.latcontrol_torque import KP
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.system.hardware import HARDWARE
from openpilot.system.hardware.power_monitoring import VBATT_PAUSE_CHARGING
from openpilot.system.version import get_build_metadata

CITY_SPEED_LIMIT = 25                     # 55mph is typically the minimum speed for highways
CRUISING_SPEED = 5                        # Roughly the speed cars go when not touching the gas while in drive
DECEL_TIME_MARGIN = 2.0                   # Seconds reserved for deceleration preparation
DEFAULT_LATERAL_ACCELERATION = 2.0        # m/s^2, typical lateral acceleration when taking curves
DISPLAY_MENU_TIMER = 350                  # The length of time the following distance menu appears on some GM vehicles to prevent things getting out of sync
EARTH_RADIUS = 6378137                    # Radius of the Earth in meters
MAPD_ARCHIVE_SIZE_DEGREES = 2             # Size of mapd's downloadable archive cells in degrees
MAPD_MAX_MAP_AGE_DAYS = 28                # Maximum age of downloaded archive cells before refreshing
MAX_ACCELERATION = 4.0                    # ISO 15622:2018
MAX_T_FOLLOW = 3.0                        # Maximum allowed following duration. Larger values risk losing track of the lead but may be increased as models improve
MINIMUM_LATERAL_ACCELERATION = 1.3        # m/s^2, typical minimum lateral acceleration when taking curves
MINIMUM_PLANNED_SPEED = 2.0               # m/s, model path points predicted slower than this are stop approaches, not curves
PLANNER_TIME = ModelConstants.T_IDXS[-1]  # Length of time the model projects out for
SLOWDOWN_PERCENTAGE = 0.50                # Treat an end-of-horizon speed drop below 50% of the current speed as a stop hint
THRESHOLD = 1 - 1 / math.e                # Requires the condition to be true for ~1 second

NON_DRIVING_GEARS = [GearShifter.neutral, GearShifter.park, GearShifter.reverse, GearShifter.unknown]

FROGPILOT_API = "https://api.frogpilot.com"

RESOURCES_REPO = "FrogAi/FrogPilot-Resources"

ACTIVE_THEME_PATH = Path(BASEDIR) / "frogpilot/assets/active_theme"
METADATAS_PATH = Path(BASEDIR) / "frogpilot/assets/model_metadata"
MODELS_PATH = Path("/data/models")
RANDOM_EVENTS_PATH = Path(BASEDIR) / "frogpilot/assets/random_events"
STOCK_THEME_PATH = Path(BASEDIR) / "frogpilot/assets/stock_theme"
THEME_COLORS_PATH = (ACTIVE_THEME_PATH / "colors/colors.json")
THEME_SAVE_PATH = Path("/data/themes")

ERROR_LOGS_PATH = Path("/data/error_logs")
SCREEN_RECORDINGS_PATH = Path("/data/media/screen_recordings")
SENTRY_QUEUE_PATH = Path("/data/sentry_queue")
VIDEO_CACHE_PATH = Path("/data/video_cache")

BACKUP_PATH = Path("/cache/on_backup")
FROGPILOT_BACKUPS = Path("/data/backups")
TOGGLE_BACKUPS = Path("/data/toggle_backups")

FROGS_GO_MOO_PATH = Path("/persist/frogsgomoo.py")

HD_LOGS_PATH = Path("/data/media/0/realdata_HD")
HD_PATH = Path("/cache/use_HD")

KONIK_LOGS_PATH = Path("/data/media/0/realdata_konik")
KONIK_PATH = Path("/cache/use_konik")

MAPS_PATH = Path("/data/media/0/osm/offline")

NNFF_MODELS_PATH = Path(BASEDIR) / "frogpilot/assets/nnff_models"

BUTTON_FUNCTIONS = {
  "NOTHING": 0,
  "PERSONALITY_PROFILE": 1,
  "FORCE_COAST": 2,
  "PAUSE_LATERAL": 3,
  "PAUSE_LONGITUDINAL": 4,
  "EXPERIMENTAL_MODE": 5,
  "TRAFFIC_MODE": 6
}

CURVE_SPEED_PROFILES = {
  "GENTLE": 0,
  "STANDARD": 1,
  "SPORT": 2,
  "AUTO": 3
}

DEVELOPER_SIDEBAR_METRICS = {
  "NONE": 0,
  "ACCELERATION_CURRENT": 1,
  "ACCELERATION_MAX": 2,
  "AUTOTUNE_ACTUATOR_DELAY": 3,
  "AUTOTUNE_FRICTION": 4,
  "AUTOTUNE_LATERAL_ACCELERATION": 5,
  "AUTOTUNE_STEER_RATIO": 6,
  "AUTOTUNE_STIFFNESS_FACTOR": 7,
  "ENGAGEMENT_LATERAL": 8,
  "ENGAGEMENT_LONGITUDINAL": 9,
  "LATERAL_STEERING_ANGLE": 10,
  "LATERAL_TORQUE_USED": 11,
  "LONGITUDINAL_ACTUATOR_ACCELERATION": 12,
  "LONGITUDINAL_MPC_JERK_ACCELERATION": 13,
  "LONGITUDINAL_MPC_JERK_DANGER_ZONE": 14,
  "LONGITUDINAL_MPC_JERK_SPEED_CONTROL": 15,
}

DEVICE_SHUTDOWN_TIMES = {
  0: 300,      # 5 mins
  1: 900,      # 15 mins
  2: 1800,     # 30 mins
  3: 2700,     # 45 mins
  4: 3600,     # 1 hour
  5: 7200,     # 2 hours
  6: 10800,    # 3 hours
  7: 14400,    # 4 hours
  8: 18000,    # 5 hours
  9: 21600,    # 6 hours
  10: 25200,   # 7 hours
  11: 28800,   # 8 hours
  12: 32400,   # 9 hours
  13: 36000,   # 10 hours
  14: 39600,   # 11 hours
  15: 43200,   # 12 hours
  16: 46800,   # 13 hours
  17: 50400,   # 14 hours
  18: 54000,   # 15 hours
  19: 57600,   # 16 hours
  20: 61200,   # 17 hours
  21: 64800,   # 18 hours
  22: 68400,   # 19 hours
  23: 72000,   # 20 hours
  24: 75600,   # 21 hours
  25: 79200,   # 22 hours
  26: 82800,   # 23 hours
  27: 86400,   # 24 hours
  28: 90000,   # 25 hours
  29: 93600,   # 26 hours
  30: 97200,   # 27 hours
  31: 100800,  # 28 hours
  32: 104400,  # 29 hours
  33: 108000,  # 30 hours
}

EXCLUDED_KEYS = {
  "ApiCache_DriveStats",
  "AthenadRecentlyViewedRoutes",
  "AthenadUploadQueue",
  "AvailableModelNames",
  "AvailableModels",
  "CalibratedLateralAcceleration",
  "CalibrationParams",
  "CalibrationProgress",
  "CarBatteryCapacity",
  "CarParamsPersistent",
  "CurvatureData",
  "ExperimentalLongitudinalEnabled",
  "FrogPilotCarParamsPersistent",
  "FrogPilotStats",
  "GitDiff",
  "KonikMinutes",
  "LastAthenaPingTime",
  "LastOffroadStatusPacket",
  "LastUpdateRouteCount",
  "LastUpdateTime",
  "LastUpdateUptimeOnroad",
  "LiveDelay",
  "LiveParameters",
  "LiveTorqueParameters",
  "LocationFilterInitialState",
  "MapBoxRequests",
  "MaxLateralAcceleration",
  "MinimumBackupSize",
  "ModelDrivesAndScores",
  "NetworkMetered",
  "openpilotMinutes",
  "PandaSignatures",
  "PreviousSpeedLimit",
  "SpeedLimits",
  "SpeedLimitsFiltered",
  "Timezone",
  "UpdateFailedCount",
  "UpdaterAvailableBranches",
  "UpdaterCurrentDescription",
  "UpdaterCurrentReleaseNotes",
  "UpdaterFetchAvailable",
  "UpdaterNewDescription",
  "UpdaterNewReleaseNotes",
  "UpdaterTargetBranch",
  "UptimeOffroad",
  "UptimeOnroad"
}

TUNING_LEVELS = {
  "MINIMAL": 0,
  "STANDARD": 1,
  "ADVANCED": 2,
  "DEVELOPER": 3
}

@cache
def get_nnff_model_files():
  return [file.stem for file in NNFF_MODELS_PATH.iterdir() if file.is_file()]

@cache
def get_nnff_substitutes():
  substitutes = {}
  with open(TORQUE_SUBSTITUTE_PATH, "rb") as f:
    substitutes_data = tomllib.load(f)
    substitutes = {key: value for key, value in substitutes_data.items()}
  return substitutes

def nnff_supported(car_fingerprint):
  from openpilot.frogpilot.controls.lib.neural_network_feedforward import get_nn_model_path
  return get_nn_model_path(car_fingerprint, "") is not None

def get_frogpilot_toggles(sm=messaging.SubMaster(["frogpilotPlan"])):
  return process_frogpilot_toggles(sm["frogpilotPlan"].frogpilotToggles)

@cache
def process_frogpilot_toggles(toggles):
  if toggles:
    return SimpleNamespace(**json.loads(toggles))
  return FrogPilotVariables().frogpilot_toggles

def update_frogpilot_toggles():
  if not hasattr(update_frogpilot_toggles, "_params_memory"):
    update_frogpilot_toggles._params_memory = Params(memory=True)

  update_frogpilot_toggles._params_memory.put_bool("FrogPilotTogglesUpdated", True)

class FrogPilotVariables:
  def __init__(self):
    self.params = Params(return_defaults=True)
    self.params_memory = Params(memory=True)

    self.frogpilot_toggles = SimpleNamespace()
    toggle = self.frogpilot_toggles

    self.default_values = {key.decode(): self.params.get_default_value(key) for key in self.params.all_keys()}
    self.tuning_levels = {key.decode(): self.params.get_tuning_level(key) for key in self.params.all_keys()}

    branch = get_build_metadata().channel
    self.development_branch = branch == "FrogPilot-Development"
    self.release_branch = branch == "FrogPilot"
    self.staging_branch = branch == "FrogPilot-Staging"
    self.testing_branch = branch == "FrogPilot-Testing"
    self.vetting_branch = branch == "FrogPilot-Vetting"

    self.frogs_go_moo = FROGS_GO_MOO_PATH.is_file()
    toggle.block_user = (self.development_branch or branch == "MAKE-PRS-HERE" or self.vetting_branch) and not self.frogs_go_moo

    toggle.tuning_level = self.params.get("TuningLevel") if self.params.get_bool("TuningLevelConfirmed") else TUNING_LEVELS["ADVANCED"]

    device_management = self.get_value("DeviceManagement")

    toggle.use_higher_bitrate = device_management
    toggle.use_higher_bitrate &= self.get_value("HigherBitrate")
    toggle.use_higher_bitrate &= self.get_value("NoUploads")
    toggle.use_higher_bitrate &= not self.get_value("DisableOnroadUploads")
    toggle.use_higher_bitrate &= not self.vetting_branch
    toggle.use_higher_bitrate |= self.development_branch

    if not HD_PATH.is_file() and toggle.use_higher_bitrate:
      HD_PATH.touch()
      HARDWARE.reboot()
    elif HD_PATH.is_file() and not toggle.use_higher_bitrate:
      HD_PATH.unlink()
      HARDWARE.reboot()

    toggle.use_konik_server = device_management
    toggle.use_konik_server &= self.get_value("UseKonikServer")
    toggle.use_konik_server |= Path("/data/openpilot/not_vetted").is_file()

    if not KONIK_PATH.is_file() and toggle.use_konik_server:
      KONIK_PATH.touch()
      HARDWARE.reboot()
    elif KONIK_PATH.is_file() and not toggle.use_konik_server:
      KONIK_PATH.unlink()
      HARDWARE.reboot()

    stock_colors_json = (STOCK_THEME_PATH / "colors/colors.json")
    self.stock_colors = json.loads(stock_colors_json.read_text()) if stock_colors_json.is_file() else {}

    self.update()

  def get_color(self, key, theme_colors):
    for source in (theme_colors, self.stock_colors):
      color = source.get(key)
      if isinstance(color, dict):
        return f"#{color.get('alpha', 255):02X}{color.get('red', 255):02X}{color.get('green', 255):02X}{color.get('blue', 255):02X}"
    return "#FFFFFFFF"

  def get_value(self, key, cast=bool, condition=True, conversion=None, default=None, min=None, max=None):
    if not condition or (self.frogpilot_toggles.tuning_level < self.tuning_levels.get(key, 0)):
      if default is not None:
        return default
      return False if cast is bool else self.default_values.get(key)

    if cast is bool:
      value = self.params.get_bool(key)
    else:
      value = self.params.get(key)

    if value is not None:
      if cast is not bool and cast is not None:
        try:
          value = cast(value)
        except (TypeError, ValueError):
          value = self.default_values.get(key)
    elif default is not None:
      value = default

    if conversion is not None and isinstance(value, (int, float)):
      value *= conversion

    if min is not None and value < min:
      value = min
    elif max is not None and value > max:
      value = max

    return value

  def update(self, holiday_theme="stock", started=False):
    self.params_memory.remove("FrogPilotTogglesUpdated")

    toggle = self.frogpilot_toggles
    toggle.tuning_level = self.params.get("TuningLevel") if self.params.get_bool("TuningLevelConfirmed") else TUNING_LEVELS["ADVANCED"]

    msg_bytes = self.params.get("CarParams" if started else "CarParamsPersistent", block=started)
    if msg_bytes:
      CP = messaging.log_from_bytes(msg_bytes, car.CarParams)
    else:
      CP = interfaces[MOCK.MOCK].get_params(MOCK.MOCK, gen_empty_fingerprint(), [], False, False, False, toggle).as_reader()

    is_torque_car = CP.lateralTuning.which() == "torque"
    if not is_torque_car:
      CP_builder = CP.as_builder()
      CarInterfaceBase.configure_torque_tune(MOCK.MOCK, CP_builder.lateralTuning)
      CP = CP_builder.as_reader()

    fpmsg_bytes = self.params.get("FrogPilotCarParams" if started else "FrogPilotCarParamsPersistent", block=started)
    if fpmsg_bytes:
      FPCP = messaging.log_from_bytes(fpmsg_bytes, custom.FrogPilotCarParams)
    else:
      FPCP = interfaces[MOCK.MOCK].get_frogpilot_params(MOCK.MOCK, gen_empty_fingerprint(), [], CP, toggle)

    selected_car_model = self.params.get("CarModel")
    toggle.force_fingerprint = self.get_value("ForceFingerprint", condition=selected_car_model != self.default_values["CarModel"])

    alpha_longitudinal = CP.alphaLongitudinalAvailable
    toggle.car_make = CP.brand
    toggle.car_model = selected_car_model if toggle.force_fingerprint else CP.carFingerprint
    toggle.disable_openpilot_long = self.get_value("DisableOpenpilotLongitudinal", condition=not alpha_longitudinal)
    friction = CP.lateralTuning.torque.friction
    has_bsm = CP.enableBsm
    toggle.has_cc_long = toggle.car_make == "gm" and bool(CP.flags & GMFlags.CC_LONG.value)
    toggle.has_nnff = nnff_supported(toggle.car_model)
    toggle.has_pedal = CP.enableGasInterceptorDEPRECATED
    has_radar = not CP.radarUnavailable
    toggle.has_sdsu = toggle.car_make == "toyota" and bool(FPCP.flags & ToyotaFrogPilotFlags.SMART_DSU.value)
    has_sng = CP.autoResumeSng
    toggle.has_zss = toggle.car_make == "toyota" and bool(FPCP.flags & ToyotaFrogPilotFlags.ZSS.value)
    hyundai_canfd = toggle.car_make == "hyundai" and CP.safetyConfigs[-1].safetyModel == car.CarParams.SafetyModel.hyundaiCanfd
    is_angle_car = CP.steerControlType == car.CarParams.SteerControlType.angle
    latAccelFactor = CP.lateralTuning.torque.latAccelFactor
    toggle.lkas_allowed_for_aol = toggle.car_make == "hyundai" and bool(CP.flags & HyundaiFlags.CANFD or CP.flags & HyundaiFlags.HAS_LDA_BUTTON)
    longitudinalActuatorDelay = CP.longitudinalActuatorDelay
    toggle.maxLateralAccel = CP.maxLateralAccel
    toggle.openpilot_longitudinal = CP.openpilotLongitudinalControl and not toggle.disable_openpilot_long
    pcm_cruise = CP.pcmCruise
    prohibited_main_aol = not toggle.openpilot_longitudinal and toggle.car_make == "hyundai" and bool(CP.flags & HyundaiFlags.CANFD or CP.flags & HyundaiFlags.HAS_LDA_BUTTON)
    startAccel = CP.startAccel
    stopAccel = CP.stopAccel
    steerActuatorDelay = CP.steerActuatorDelay
    steerKp = KP
    steerRatio = CP.steerRatio
    toggle.stoppingDecelRate = CP.stoppingDecelRate
    toggle.vEgoStarting = CP.vEgoStarting
    toggle.vEgoStopping = CP.vEgoStopping

    msg_bytes = self.params.get("LiveTorqueParameters")
    if msg_bytes:
      LTP = messaging.log_from_bytes(msg_bytes, log.Event).liveTorqueParameters
      has_auto_tune = LTP.useParams
      toggle.liveValid = LTP.liveValid
    else:
      has_auto_tune = False
      toggle.liveValid = False

    toggle.debug_mode = self.params.get_bool("DebugMode")
    toggle.force_offroad = self.params.get_bool("ForceOffroad")
    toggle.force_onroad = self.params.get_bool("ForceOnroad")

    toggle.is_metric = self.params.get_bool("IsMetric")
    distance_conversion = 1 if toggle.is_metric else CV.FOOT_TO_METER
    small_distance_conversion = 1 if toggle.is_metric else CV.INCH_TO_CM
    speed_conversion = CV.KPH_TO_MS if toggle.is_metric else CV.MPH_TO_MS

    advanced_custom_ui = self.get_value("AdvancedCustomUI")
    toggle.hide_alerts = self.get_value("HideAlerts", condition=advanced_custom_ui and not toggle.debug_mode)
    toggle.hide_lead_marker = self.get_value("HideLeadMarker", condition=advanced_custom_ui and toggle.openpilot_longitudinal and not toggle.debug_mode)
    toggle.hide_max_speed = self.get_value("HideMaxSpeed", condition=advanced_custom_ui and not toggle.debug_mode)
    toggle.hide_speed = self.get_value("HideSpeed", condition=advanced_custom_ui and not toggle.debug_mode)
    toggle.hide_speed_limit = self.get_value("HideSpeedLimit", condition=advanced_custom_ui and not toggle.debug_mode)
    toggle.use_wheel_speed = self.get_value("WheelSpeed", condition=advanced_custom_ui)

    advanced_lateral_tuning = self.get_value("AdvancedLateralTune")
    toggle.force_auto_tune = self.get_value("ForceAutoTune", condition=advanced_lateral_tuning and not has_auto_tune and is_torque_car and not is_angle_car)
    toggle.force_auto_tune_off = self.get_value("ForceAutoTuneOff", condition=advanced_lateral_tuning and has_auto_tune and is_torque_car and not is_angle_car)
    toggle.force_torque_controller = self.get_value("ForceTorqueController", condition=advanced_lateral_tuning)
    toggle.steerActuatorDelay = self.get_value("SteerDelay", cast=float, condition=advanced_lateral_tuning and self.params.get("SteerDelay") != 0, default=steerActuatorDelay, min=0.01, max=1.0)
    toggle.use_custom_steerActuatorDelay = bool(round(toggle.steerActuatorDelay, 2) != round(steerActuatorDelay, 2))
    toggle.friction = self.get_value("SteerFriction", cast=float, condition=advanced_lateral_tuning and self.params.get("SteerFriction") != 0, default=friction, min=0.01, max=1)
    toggle.use_custom_friction = (bool(round(toggle.friction, 2) != round(friction, 2)) and is_torque_car and not has_auto_tune and not toggle.force_auto_tune) or toggle.force_auto_tune_off
    toggle.steerKp = [[0], [self.get_value("SteerKP", cast=float, condition=advanced_lateral_tuning and is_torque_car and not is_angle_car and self.params.get("SteerKP") != 0, default=steerKp, min=steerKp * 0.5, max=steerKp * 1.5)]]
    toggle.latAccelFactor = self.get_value("SteerLatAccel", cast=float, condition=advanced_lateral_tuning and self.params.get("SteerLatAccel") != 0, default=latAccelFactor, min=latAccelFactor * 0.5, max=latAccelFactor * 1.5)
    toggle.use_custom_latAccelFactor = (bool(round(toggle.latAccelFactor, 2) != round(latAccelFactor, 2)) and is_torque_car and not has_auto_tune and not toggle.force_auto_tune) or toggle.force_auto_tune_off
    toggle.steerRatio = self.get_value("SteerRatio", cast=float, condition=advanced_lateral_tuning and self.params.get("SteerRatio") != 0, default=steerRatio, min=steerRatio * 0.5, max=steerRatio * 1.5)
    toggle.use_custom_steerRatio = (bool(round(toggle.steerRatio, 2) != round(steerRatio, 2)) and not toggle.force_auto_tune) or toggle.force_auto_tune_off

    advanced_longitudinal_tuning = toggle.openpilot_longitudinal and self.get_value("AdvancedLongitudinalTune")
    toggle.longitudinalActuatorDelay = self.get_value("LongitudinalActuatorDelay", cast=float, condition=advanced_longitudinal_tuning and self.params.get("LongitudinalActuatorDelay") != 0, default=longitudinalActuatorDelay, min=0, max=1)
    toggle.max_desired_acceleration = self.get_value("MaxDesiredAcceleration", cast=float, condition=advanced_longitudinal_tuning and self.params.get("MaxDesiredAcceleration") != 0, min=0.1, max=MAX_ACCELERATION)
    toggle.startAccel = self.get_value("StartAccel", cast=float, condition=advanced_longitudinal_tuning and self.params.get("StartAccel") != 0, default=startAccel, min=0, max=MAX_ACCELERATION)
    toggle.stopAccel = self.get_value("StopAccel", cast=float, condition=advanced_longitudinal_tuning and self.params.get("StopAccel") != 0, default=stopAccel, min=-MAX_ACCELERATION, max=0)
    toggle.stoppingDecelRate = self.get_value("StoppingDecelRate", cast=float, condition=advanced_longitudinal_tuning and self.params.get("StoppingDecelRate") != 0, default=toggle.stoppingDecelRate, min=0.001, max=12)
    toggle.vEgoStarting = self.get_value("VEgoStarting", cast=float, condition=advanced_longitudinal_tuning and self.params.get("VEgoStarting") != 0, default=toggle.vEgoStarting, min=0.01, max=1)
    toggle.vEgoStopping = self.get_value("VEgoStopping", cast=float, condition=advanced_longitudinal_tuning and self.params.get("VEgoStopping") != 0, default=toggle.vEgoStopping, min=0.01, max=1)

    toggle.alert_volume_controller = self.get_value("AlertVolumeControl")
    toggle.disengage_volume = self.get_value("DisengageVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.engage_volume = self.get_value("EngageVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.prompt_volume = self.get_value("PromptVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.promptDistracted_volume = self.get_value("PromptDistractedVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.refuse_volume = self.get_value("RefuseVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.warningSoft_volume = max(self.get_value("WarningSoftVolume", cast=float, condition=toggle.alert_volume_controller, default=25), 25)
    toggle.warningImmediate_volume = max(self.get_value("WarningImmediateVolume", cast=float, condition=toggle.alert_volume_controller, default=25), 25)

    toggle.always_on_lateral = self.get_value("AlwaysOnLateral")
    toggle.always_on_lateral_lkas = toggle.always_on_lateral and toggle.lkas_allowed_for_aol and self.get_value("AlwaysOnLateralLKAS")
    toggle.always_on_lateral_main = toggle.always_on_lateral and not prohibited_main_aol and not toggle.always_on_lateral_lkas
    toggle.always_on_lateral_pause_speed = self.get_value("PauseAOLOnBrake", cast=float, condition=toggle.always_on_lateral, conversion=speed_conversion)

    toggle.automatic_updates = self.get_value("AutomaticUpdates", condition=(self.release_branch or self.vetting_branch or self.frogs_go_moo), default=True) and not BACKUP_PATH.is_file()

    toggle.cluster_offset = self.get_value("ClusterOffset", cast=float, condition=toggle.car_make == "toyota")

    toggle.conditional_experimental_mode = toggle.openpilot_longitudinal and self.get_value("ConditionalExperimental")
    toggle.conditional_curves = self.get_value("CECurves", condition=toggle.conditional_experimental_mode)
    toggle.conditional_curves_lead = self.get_value("CECurvesLead", condition=toggle.conditional_curves)
    toggle.conditional_lead = self.get_value("CELead", condition=toggle.conditional_experimental_mode)
    toggle.conditional_slower_lead = self.get_value("CESlowerLead", condition=toggle.conditional_lead)
    toggle.conditional_stopped_lead = self.get_value("CEStoppedLead", condition=toggle.conditional_lead)
    toggle.conditional_limit = self.get_value("CESpeed", cast=float, condition=toggle.conditional_experimental_mode, conversion=speed_conversion)
    toggle.conditional_limit_lead = self.get_value("CESpeedLead", cast=float, condition=toggle.conditional_experimental_mode, conversion=speed_conversion)
    if toggle.conditional_experimental_mode and self.get_value("CEStopLights"):
      toggle.conditional_model_stop_time = self.get_value("CEModelStopTime", cast=float)
    else:
      toggle.conditional_model_stop_time = 0
    toggle.conditional_signal = self.get_value("CESignalSpeed", cast=float, condition=toggle.conditional_experimental_mode, conversion=speed_conversion)
    toggle.conditional_signal_lane_detection = self.get_value("CESignalLaneDetection", condition=toggle.conditional_signal != 0)
    toggle.cem_status = self.get_value("ShowCEMStatus", condition=toggle.conditional_experimental_mode) or toggle.debug_mode

    toggle.curve_speed_controller = toggle.openpilot_longitudinal and self.get_value("CurveSpeedController")
    toggle.curve_speed_profile = self.get_value("CurveSpeedProfile", cast=int, condition=toggle.curve_speed_controller)
    toggle.csc_status = self.get_value("ShowCSCStatus", condition=toggle.curve_speed_controller) or toggle.debug_mode

    custom_alerts = self.get_value("CustomAlerts")
    toggle.goat_scream_alert = self.get_value("GoatScream", condition=custom_alerts)
    toggle.green_light_alert = self.get_value("GreenLightAlert", condition=custom_alerts)
    toggle.lead_departing_alert = self.get_value("LeadDepartingAlert", condition=custom_alerts)
    toggle.loud_blindspot_alert = self.get_value("LoudBlindspotAlert", condition=custom_alerts and has_bsm)
    toggle.speed_limit_changed_alert = self.get_value("SpeedLimitChangedAlert", condition=custom_alerts)

    toggle.custom_personalities = toggle.openpilot_longitudinal and self.get_value("CustomPersonalities")
    toggle.aggressive_jerk_acceleration = self.get_value("AggressiveJerkAcceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.aggressive_jerk_deceleration = self.get_value("AggressiveJerkDeceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.aggressive_jerk_danger = self.get_value("AggressiveJerkDanger", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.aggressive_jerk_speed = self.get_value("AggressiveJerkSpeed", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.aggressive_jerk_speed_decrease = self.get_value("AggressiveJerkSpeedDecrease", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.aggressive_follow = self.get_value("AggressiveFollow", cast=float, condition=toggle.custom_personalities, min=1, max=MAX_T_FOLLOW)
    toggle.standard_jerk_acceleration = self.get_value("StandardJerkAcceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.standard_jerk_deceleration = self.get_value("StandardJerkDeceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.standard_jerk_danger = self.get_value("StandardJerkDanger", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.standard_jerk_speed = self.get_value("StandardJerkSpeed", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.standard_jerk_speed_decrease = self.get_value("StandardJerkSpeedDecrease", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.standard_follow = self.get_value("StandardFollow", cast=float, condition=toggle.custom_personalities, min=1, max=MAX_T_FOLLOW)
    toggle.relaxed_jerk_acceleration = self.get_value("RelaxedJerkAcceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.relaxed_jerk_deceleration = self.get_value("RelaxedJerkDeceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.relaxed_jerk_danger = self.get_value("RelaxedJerkDanger", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.relaxed_jerk_speed = self.get_value("RelaxedJerkSpeed", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.relaxed_jerk_speed_decrease = self.get_value("RelaxedJerkSpeedDecrease", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=0.25, max=2.0)
    toggle.relaxed_follow = self.get_value("RelaxedFollow", cast=float, condition=toggle.custom_personalities, min=1, max=MAX_T_FOLLOW)

    custom_themes = self.get_value("CustomThemes")
    toggle.color_scheme = self.get_value("ColorScheme", cast=None, condition=custom_themes, default="stock")
    theme_colors = json.loads(THEME_COLORS_PATH.read_text()) if THEME_COLORS_PATH.is_file() else {}
    toggle.lane_lines_color = self.get_color("LaneLines", theme_colors)
    toggle.lead_marker_color = self.get_color("LeadMarker", theme_colors)
    toggle.path_color = self.get_color("Path", theme_colors)
    toggle.path_edges_color = self.get_color("PathEdge", theme_colors)
    toggle.sidebar_color1 = self.get_color("Sidebar1", theme_colors)
    toggle.sidebar_color2 = self.get_color("Sidebar2", theme_colors)
    toggle.sidebar_color3 = self.get_color("Sidebar3", theme_colors)
    toggle.distance_icons = self.get_value("DistanceIconPack", cast=None, condition=custom_themes, default="stock")
    toggle.icon_pack = self.get_value("IconPack", cast=None, condition=custom_themes, default="stock")
    toggle.signal_icons = self.get_value("SignalAnimation", cast=None, condition=custom_themes, default="stock")
    toggle.sound_pack = self.get_value("SoundPack", cast=None, condition=custom_themes, default="stock")
    toggle.random_themes = self.get_value("RandomThemes", condition=custom_themes)
    toggle.random_themes_holidays = self.get_value("RandomThemesHolidays", condition=toggle.random_themes)
    if toggle.random_themes:
      toggle.wheel_image = random.choice([file.stem for file in (THEME_SAVE_PATH / "steering_wheels").iterdir() if file.is_file()] or ["stock"]) if (THEME_SAVE_PATH / "steering_wheels").exists() else "stock"
    else:
      toggle.wheel_image = self.get_value("WheelIcon", cast=None, condition=custom_themes, default="stock")

    custom_ui = self.get_value("CustomUI")
    toggle.acceleration_path = toggle.openpilot_longitudinal and (self.get_value("AccelerationPath", condition=custom_ui) or toggle.debug_mode)
    toggle.adjacent_paths = self.get_value("AdjacentPath", condition=custom_ui)
    toggle.blind_spot_path = has_bsm and self.get_value("BlindSpotPath", condition=custom_ui)
    toggle.compass = self.get_value("Compass", condition=custom_ui)
    toggle.onroad_distance_button = toggle.openpilot_longitudinal and (self.get_value("OnroadDistanceButton", condition=custom_ui) or toggle.debug_mode)
    toggle.pedals_on_ui = self.get_value("PedalsOnUI", condition=custom_ui and toggle.openpilot_longitudinal)
    toggle.dynamic_pedals_on_ui = self.get_value("DynamicPedalsOnUI", condition=toggle.pedals_on_ui)
    toggle.static_pedals_on_ui = self.get_value("StaticPedalsOnUI", condition=toggle.pedals_on_ui)
    toggle.rotating_wheel = self.get_value("RotatingWheel", condition=custom_ui)

    toggle.developer_ui = self.get_value("DeveloperUI")
    developer_metrics = self.get_value("DeveloperMetrics", condition=toggle.developer_ui)
    border_metrics = self.get_value("BorderMetrics", condition=developer_metrics)
    toggle.blind_spot_metrics = has_bsm and self.get_value("BlindSpotMetrics", condition=border_metrics)
    toggle.signal_metrics = self.get_value("SignalMetrics", condition=border_metrics) or toggle.debug_mode
    toggle.steering_metrics = self.get_value("ShowSteering", condition=border_metrics) or toggle.debug_mode
    toggle.show_fps = self.get_value("FPSCounter", condition=developer_metrics) or toggle.debug_mode
    toggle.adjacent_path_metrics = self.get_value("AdjacentPathMetrics", condition=developer_metrics) or toggle.debug_mode
    toggle.lead_info = self.get_value("LeadInfo", condition=developer_metrics) or toggle.debug_mode
    toggle.numerical_temp = self.get_value("NumericalTemp", condition=developer_metrics) or toggle.debug_mode
    toggle.fahrenheit = self.get_value("Fahrenheit", condition=toggle.numerical_temp and not toggle.debug_mode)
    toggle.cpu_metrics = self.get_value("ShowCPU", condition=developer_metrics) or toggle.debug_mode
    toggle.gpu_metrics = self.get_value("ShowGPU", condition=developer_metrics and not toggle.debug_mode)
    toggle.ip_metrics = self.get_value("ShowIP", condition=developer_metrics)
    toggle.memory_metrics = self.get_value("ShowMemoryUsage", condition=developer_metrics) or toggle.debug_mode
    toggle.storage_left_metrics = self.get_value("ShowStorageLeft", condition=developer_metrics and not toggle.debug_mode)
    toggle.storage_used_metrics = self.get_value("ShowStorageUsed", condition=developer_metrics and not toggle.debug_mode)
    toggle.use_si_metrics = self.get_value("UseSI", condition=developer_metrics) or toggle.debug_mode
    toggle.developer_sidebar = self.get_value("DeveloperSidebar", condition=toggle.developer_ui) or toggle.debug_mode
    toggle.developer_sidebar_metric1 = self.get_value("DeveloperSidebarMetric1", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LONGITUDINAL_ACTUATOR_ACCELERATION"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric2 = self.get_value("DeveloperSidebarMetric2", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["ACCELERATION_CURRENT"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric3 = self.get_value("DeveloperSidebarMetric3", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LATERAL_STEERING_ANGLE"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric4 = self.get_value("DeveloperSidebarMetric4", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LATERAL_TORQUE_USED"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric5 = self.get_value("DeveloperSidebarMetric5", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LONGITUDINAL_MPC_JERK_ACCELERATION"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric6 = self.get_value("DeveloperSidebarMetric6", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LONGITUDINAL_MPC_JERK_DANGER_ZONE"] if toggle.debug_mode else None)
    toggle.developer_sidebar_metric7 = self.get_value("DeveloperSidebarMetric7", cast=None, condition=toggle.developer_sidebar, default=DEVELOPER_SIDEBAR_METRICS["LONGITUDINAL_MPC_JERK_SPEED_CONTROL"] if toggle.debug_mode else None)
    developer_widgets = self.get_value("DeveloperWidgets", condition=toggle.developer_ui)
    toggle.adjacent_lead_tracking = has_radar and (self.get_value("AdjacentLeadsUI", condition=developer_widgets) or toggle.debug_mode)
    toggle.radar_tracks = has_radar and (self.get_value("RadarTracksUI", condition=developer_widgets) or toggle.debug_mode)
    toggle.show_stopping_point = toggle.openpilot_longitudinal and (self.get_value("ShowStoppingPoint", condition=developer_widgets) or toggle.debug_mode)
    toggle.show_stopping_point_metrics = self.get_value("ShowStoppingPointMetrics", condition=toggle.show_stopping_point) or toggle.debug_mode

    device_management = self.get_value("DeviceManagement")
    toggle.device_shutdown_time = DEVICE_SHUTDOWN_TIMES.get(self.get_value("DeviceShutdown", cast=int, condition=device_management))
    toggle.increase_thermal_limits = self.get_value("IncreaseThermalLimits", condition=device_management)
    toggle.low_voltage_shutdown = self.get_value("LowVoltageShutdown", cast=float, condition=device_management, min=VBATT_PAUSE_CHARGING, max=12.5)
    toggle.no_logging = self.get_value("NoLogging", condition=device_management and not self.vetting_branch) or toggle.force_onroad
    toggle.no_uploads = self.get_value("NoUploads", condition=device_management and not self.vetting_branch) or toggle.use_higher_bitrate
    toggle.no_onroad_uploads = self.get_value("DisableOnroadUploads", condition=toggle.no_uploads and not toggle.use_higher_bitrate)

    distance_button_control = self.get_value("DistanceButtonControl", cast=float)
    toggle.experimental_mode_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press = toggle.experimental_mode_via_distance
    toggle.force_coast_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance = distance_button_control == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    distance_button_control_long = self.get_value("LongDistanceButtonControl", cast=float)
    toggle.experimental_mode_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_distance_long
    toggle.force_coast_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance_long = distance_button_control_long == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    distance_button_control_very_long = self.get_value("VeryLongDistanceButtonControl", cast=float)
    toggle.experimental_mode_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_distance_very_long
    toggle.force_coast_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance_very_long = distance_button_control_very_long == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    toggle.frogpilot_telemetry = self.get_value("FrogPilotTelemetry")

    toggle.frogsgomoo_tweak = self.get_value("FrogsGoMoosTweak", condition=toggle.openpilot_longitudinal and toggle.car_make == "toyota")
    toggle.stoppingDecelRate = 0.01 if toggle.frogsgomoo_tweak else toggle.stoppingDecelRate
    toggle.vEgoStarting = 0.1 if toggle.frogsgomoo_tweak else toggle.vEgoStarting
    toggle.vEgoStopping = 0.5 if toggle.frogsgomoo_tweak else toggle.vEgoStopping

    toggle.holiday_themes = self.get_value("HolidayThemes")
    toggle.current_holiday_theme = holiday_theme if toggle.holiday_themes else "stock"
    if toggle.current_holiday_theme != "stock":
      toggle.color_scheme = toggle.current_holiday_theme
      toggle.distance_icons = toggle.current_holiday_theme
      toggle.icon_pack = toggle.current_holiday_theme
      toggle.signal_icons = toggle.current_holiday_theme
      toggle.sound_pack = toggle.current_holiday_theme
      toggle.wheel_image = toggle.current_holiday_theme

    toggle.lane_changes = self.get_value("LaneChanges")
    toggle.lane_change_delay = self.get_value("LaneChangeTime", cast=float, condition=toggle.lane_changes)
    toggle.lane_detection_width = self.get_value("LaneDetectionWidth", cast=float, condition=toggle.lane_changes, conversion=distance_conversion)
    toggle.minimum_lane_change_speed = self.get_value("MinimumLaneChangeSpeed", cast=float, condition=toggle.lane_changes, conversion=speed_conversion)
    toggle.nudgeless = self.get_value("NudgelessLaneChange", condition=toggle.lane_changes)
    toggle.one_lane_change = self.get_value("OneLaneChange", condition=toggle.lane_changes)

    lateral_tuning = self.get_value("LateralTune")
    toggle.nnff = self.get_value("NNFF", condition=lateral_tuning and toggle.has_nnff and not is_angle_car)
    toggle.nnff_lite = self.get_value("NNFFLite", condition=not toggle.nnff and lateral_tuning and not is_angle_car)
    toggle.use_turn_desires = self.get_value("TurnDesires", condition=lateral_tuning)

    lkas_button_control = self.get_value("LKASButtonControl", cast=float, condition=toggle.car_make != "subaru" and not toggle.always_on_lateral_lkas)
    toggle.experimental_mode_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_lkas
    toggle.force_coast_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_lkas = lkas_button_control == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    toggle.lock_doors_timer = self.get_value("LockDoorsTimer", cast=float, condition=(toggle.car_make == "toyota"))

    longitudinal_tuning = toggle.openpilot_longitudinal and self.get_value("LongitudinalTune")
    toggle.acceleration_profile = self.get_value("AccelerationProfile", cast=float, condition=longitudinal_tuning)
    toggle.deceleration_profile = self.get_value("DecelerationProfile", cast=float, condition=longitudinal_tuning)
    toggle.human_acceleration = self.get_value("HumanAcceleration", condition=longitudinal_tuning)
    toggle.human_following = self.get_value("HumanFollowing", condition=longitudinal_tuning)
    toggle.human_lane_changes = has_radar and self.get_value("HumanLaneChanges", condition=longitudinal_tuning)
    toggle.lead_detection_probability = self.get_value("LeadDetectionThreshold", cast=float, condition=longitudinal_tuning, conversion=0.01, min=0.25, max=0.5)
    toggle.taco_tune = self.get_value("TacoTune", condition=longitudinal_tuning)

    toggle.model = self.default_values["DrivingModel"]
    toggle.model_name = self.default_values["DrivingModelName"]
    toggle.model_version = self.default_values["DrivingModelVersion"]

    toggle.model_ui = self.get_value("ModelUI")
    toggle.dynamic_path_width = self.get_value("DynamicPathWidth", condition=toggle.model_ui and not toggle.debug_mode)
    toggle.lane_line_width = self.get_value("LaneLinesWidth", cast=float, condition=toggle.model_ui and not toggle.debug_mode, conversion=small_distance_conversion / 200)
    toggle.path_edge_width = self.get_value("PathEdgeWidth", cast=float, condition=toggle.model_ui and not toggle.debug_mode)
    toggle.path_width = self.get_value("PathWidth", cast=float, condition=toggle.model_ui and not toggle.debug_mode, conversion=distance_conversion / 2)
    toggle.road_edge_width = self.get_value("RoadEdgesWidth", cast=float, condition=toggle.model_ui and not toggle.debug_mode, conversion=small_distance_conversion / 200)

    navigation_ui = self.get_value("NavigationUI")
    toggle.road_name_ui = self.get_value("RoadNameUI", condition=navigation_ui) or toggle.debug_mode
    toggle.show_speed_limits = self.get_value("ShowSpeedLimits", condition=navigation_ui) or toggle.debug_mode
    toggle.speed_limit_vienna = self.get_value("UseVienna", condition=navigation_ui)

    quality_of_life_lateral = self.get_value("QOLLateral")
    toggle.pause_lateral_below_speed = self.get_value("PauseLateralSpeed", cast=float, condition=quality_of_life_lateral, conversion=speed_conversion)
    toggle.pause_lateral_below_signal = self.get_value("PauseLateralOnSignal", condition=toggle.pause_lateral_below_speed != 0)

    quality_of_life_longitudinal = toggle.openpilot_longitudinal and self.get_value("QOLLongitudinal")
    toggle.cruise_increase = self.get_value("CustomCruise", cast=float, condition=(quality_of_life_longitudinal and not pcm_cruise))
    toggle.cruise_increase_long = self.get_value("CustomCruiseLong", cast=float, condition=(quality_of_life_longitudinal and not pcm_cruise))
    toggle.force_stops = self.get_value("ForceStops", condition=quality_of_life_longitudinal)
    toggle.increase_stopped_distance = self.get_value("IncreasedStoppedDistance", cast=float, condition=quality_of_life_longitudinal, conversion=distance_conversion)
    map_gears = self.get_value("MapGears", condition=quality_of_life_longitudinal and (toggle.car_make in {"gm", "toyota"} or hyundai_canfd) and not CP.secOcRequired)
    toggle.map_acceleration = self.get_value("MapAcceleration", condition=map_gears)
    toggle.map_deceleration = self.get_value("MapDeceleration", condition=map_gears)
    toggle.reverse_cruise_increase = self.get_value("ReverseCruise", condition=quality_of_life_longitudinal and toggle.car_make == "toyota" and pcm_cruise)
    toggle.set_speed_offset = self.get_value("SetSpeedOffset", cast=float, condition=(quality_of_life_longitudinal and not pcm_cruise), conversion=(1 if toggle.is_metric else CV.MPH_TO_KPH))
    toggle.weather_presets = self.get_value("WeatherPresets", condition=quality_of_life_longitudinal)
    toggle.increase_following_distance_low_visibility = self.get_value("IncreaseFollowingLowVisibility", cast=float, condition=toggle.weather_presets)
    toggle.increase_following_distance_rain = self.get_value("IncreaseFollowingRain", cast=float, condition=toggle.weather_presets)
    toggle.increase_following_distance_rain_storm = self.get_value("IncreaseFollowingRainStorm", cast=float, condition=toggle.weather_presets)
    toggle.increase_following_distance_snow = self.get_value("IncreaseFollowingSnow", cast=float, condition=toggle.weather_presets)
    toggle.increase_stopped_distance_low_visibility = self.get_value("IncreasedStoppedDistanceLowVisibility", cast=float, condition=toggle.weather_presets, conversion=distance_conversion)
    toggle.increase_stopped_distance_rain = self.get_value("IncreasedStoppedDistanceRain", cast=float, condition=toggle.weather_presets, conversion=distance_conversion)
    toggle.increase_stopped_distance_rain_storm = self.get_value("IncreasedStoppedDistanceRainStorm", cast=float, condition=toggle.weather_presets, conversion=distance_conversion)
    toggle.increase_stopped_distance_snow = self.get_value("IncreasedStoppedDistanceSnow", cast=float, condition=toggle.weather_presets, conversion=distance_conversion)
    toggle.reduce_acceleration_low_visibility = self.get_value("ReduceAccelerationLowVisibility", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_acceleration_rain = self.get_value("ReduceAccelerationRain", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_acceleration_rain_storm = self.get_value("ReduceAccelerationRainStorm", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_acceleration_snow = self.get_value("ReduceAccelerationSnow", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_low_visibility = self.get_value("ReduceLateralAccelerationLowVisibility", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_rain = self.get_value("ReduceLateralAccelerationRain", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_rain_storm = self.get_value("ReduceLateralAccelerationRainStorm", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_snow = self.get_value("ReduceLateralAccelerationSnow", cast=float, condition=toggle.weather_presets, conversion=0.01)

    quality_of_life_visuals = self.get_value("QOLVisuals")
    toggle.camera_view = self.get_value("CameraView", cast=float, condition=quality_of_life_visuals and not toggle.debug_mode)
    toggle.driver_camera_in_reverse = self.get_value("DriverCamera", condition=quality_of_life_visuals)
    toggle.stopped_timer = self.get_value("StoppedTimer", condition=quality_of_life_visuals)

    toggle.rainbow_path = self.get_value("RainbowPath", condition=not toggle.debug_mode)

    toggle.random_events = self.get_value("RandomEvents")

    screen_management = self.get_value("ScreenManagement")
    toggle.screen_brightness = max(self.get_value("ScreenBrightness", cast=float, condition=screen_management), 1)
    toggle.screen_brightness_onroad = self.get_value("ScreenBrightnessOnroad", cast=float, condition=(screen_management and not toggle.force_onroad))
    toggle.screen_recorder = self.get_value("ScreenRecorder", condition=screen_management) or toggle.debug_mode
    toggle.screen_timeout = self.get_value("ScreenTimeout", cast=float, condition=screen_management)
    toggle.screen_timeout_onroad = self.get_value("ScreenTimeoutOnroad", cast=float, condition=screen_management)
    toggle.standby_mode = self.get_value("StandbyMode", condition=screen_management)

    toggle.sng_hack = self.get_value("SNGHack", condition=toggle.openpilot_longitudinal and toggle.car_make == "toyota" and not toggle.has_pedal and not has_sng)

    toggle.speed_limit_controller = toggle.openpilot_longitudinal and self.get_value("SpeedLimitController")
    toggle.map_speed_lookahead_higher = self.get_value("SLCLookaheadHigher", cast=float, condition=toggle.speed_limit_controller)
    toggle.map_speed_lookahead_lower = self.get_value("SLCLookaheadLower", cast=float, condition=toggle.speed_limit_controller)
    toggle.set_speed_limit = self.get_value("SetSpeedLimit", condition=toggle.speed_limit_controller)
    toggle.show_speed_limit_offset = self.get_value("ShowSLCOffset", condition=toggle.speed_limit_controller) or toggle.debug_mode
    slc_fallback_method = self.get_value("SLCFallback", cast=float, condition=toggle.speed_limit_controller)
    toggle.slc_fallback_experimental_mode = toggle.speed_limit_controller and slc_fallback_method == 1
    toggle.slc_fallback_previous_speed_limit = toggle.speed_limit_controller and slc_fallback_method == 2
    toggle.slc_mapbox_filler = self.get_value("SLCMapboxFiller", condition=(toggle.show_speed_limits or toggle.speed_limit_controller) and bool(self.params.get("MapboxPublicKey")))
    speed_limit_confirmation = self.get_value("SLCConfirmation", condition=toggle.speed_limit_controller)
    toggle.speed_limit_confirmation_higher = self.get_value("SLCConfirmationHigher", condition=speed_limit_confirmation)
    toggle.speed_limit_confirmation_lower = self.get_value("SLCConfirmationLower", condition=speed_limit_confirmation)
    slc_override_method = self.get_value("SLCOverride", cast=float, condition=toggle.speed_limit_controller)
    toggle.speed_limit_controller_override_manual = slc_override_method == 1
    toggle.speed_limit_controller_override_set_speed = slc_override_method == 2
    toggle.speed_limit_offset1 = self.get_value("Offset1", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset2 = self.get_value("Offset2", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset3 = self.get_value("Offset3", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset4 = self.get_value("Offset4", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset5 = self.get_value("Offset5", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset6 = self.get_value("Offset6", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_offset7 = self.get_value("Offset7", cast=float, condition=toggle.speed_limit_controller, conversion=speed_conversion)
    toggle.speed_limit_priority1 = self.get_value("SLCPriority1", cast=None, condition=toggle.speed_limit_controller)
    toggle.speed_limit_priority2 = self.get_value("SLCPriority2", cast=None, condition=toggle.speed_limit_controller)
    toggle.speed_limit_priority_highest = toggle.speed_limit_priority1 == "Highest"
    toggle.speed_limit_priority_lowest = toggle.speed_limit_priority1 == "Lowest"
    toggle.speed_limit_sources = self.get_value("SpeedLimitSources", condition=toggle.speed_limit_controller) or toggle.debug_mode

    toggle.speed_limit_filler = self.get_value("SpeedLimitFiller")

    toggle.startup_alert_top = self.get_value("StartupMessageTop", cast=str, default="")
    toggle.startup_alert_bottom = self.get_value("StartupMessageBottom", cast=str, default="")

    toggle.subaru_sng = self.get_value("SubaruSNG", condition=toggle.car_make == "subaru" and not (CP.flags & SubaruFlags.GLOBAL_GEN2 or CP.flags & SubaruFlags.HYBRID))

    toggle.tethering_config = self.get_value("TetheringEnabled", cast=float)

    toggle.toyota_dsu_bypass = self.get_value("ToyotaDSUBypass", condition=toggle.car_make == "toyota" and not toggle.has_sdsu)

    toyota_doors = self.get_value("ToyotaDoors", condition=toggle.car_make == "toyota")
    toggle.lock_doors = self.get_value("LockDoors", condition=toyota_doors)
    toggle.unlock_doors = self.get_value("UnlockDoors", condition=toyota_doors)

    toggle.volt_sng = self.get_value("VoltSNG", condition=toggle.car_model == "CHEVROLET_VOLT")

    process_frogpilot_toggles.cache_clear()
