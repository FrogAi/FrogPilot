#!/usr/bin/env python3
import json
import math
import random

from functools import cache, lru_cache
from pathlib import Path
from types import SimpleNamespace

import cereal.messaging as messaging

from cereal import car, custom, log
from opendbc.car import gen_empty_fingerprint
from opendbc.car.car_helpers import interfaces
from opendbc.car.gm.values import GMOPGMFlags
from opendbc.car.hyundai.values import HyundaiFlags
from opendbc.car.interfaces import CarInterfaceBase, GearShifter
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
DEFAULT_LATERAL_ACCELERATION = 2.0        # m/s^2, baseline lateral acceleration when taking curves
DISPLAY_MENU_TIMER = 350                  # The length of time the following distance menu appears on some GM vehicles
EARTH_RADIUS = 6378137                    # Radius of the Earth in meters
JERK_MAX_RAW = 200                        # Maximum jerk percentage
JERK_MIN_RAW = 25                         # Minimum jerk percentage
LEAD_DETECTION_MAX_RAW = 50               # Maximum lead detection threshold percentage
LEAD_DETECTION_MIN_RAW = 25               # Minimum lead detection threshold percentage
LOW_VOLTAGE_SHUTDOWN_MAX = 12.5           # Maximum voltage threshold for low-voltage shutdown
MAX_ACCELERATION = 4.0                    # ISO 15622:2018
MAX_T_FOLLOW = 3.0                        # Maximum allowed following duration. Larger values risk losing track of the lead but may be increased as models improve
MINIMUM_LATERAL_ACCELERATION = 1.3        # m/s^2, minimum lateral acceleration when taking curves
STEERING_TUNE_MAX_FACTOR = 1.5            # Upper bound multiplier for manual steering-tune overrides
STEERING_TUNE_MIN_FACTOR = 0.5            # Lower bound multiplier for manual steering-tune overrides
TRAFFIC_FOLLOW_MIN = 0.5                  # Minimum following distance for Traffic Mode in seconds
PLANNER_TIME = ModelConstants.T_IDXS[-1]  # Length of time the model projects out for
THRESHOLD = 1 - 1 / math.e                # Requires the condition to be true for 0.5 seconds
UNOFFICIAL_DONGLE_ID = "UnofficialDevice" # Banned/unofficial device: passes the registration gate so it can drive, but the manager keeps comma uplinks (athenad/uploader) off

AOL_BORDER_COLOR = (0x0A, 0xBA, 0xB5, 0xF1)

NON_DRIVING_GEARS = [GearShifter.neutral, GearShifter.park, GearShifter.reverse, GearShifter.unknown]

FROGPILOT_API = "https://frogpilot.com/api"

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

BACKUP_PATH = Path("/cache/on_backup")
FROGPILOT_BACKUPS = Path("/data/backups")
TOGGLE_BACKUPS = Path("/data/toggle_backups")

MAPS_PATH = Path("/data/media/0/osm/offline")

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
  "AvailableModelNames",
  "AvailableModels",
  "CalibratedLateralAcceleration",
  "CalibrationProgress",
  "CarBatteryCapacity",
  "CarParamsPersistent",
  "CurvatureData",
  "ExperimentalLongitudinalEnabled",
  "FrogPilotCarParamsPersistent",
  "KonikMinutes",
  "LastMapsUpdate",
  "LastUpdateTime",
  "MapBoxRequests",
  "ModelDrivesAndScores",
  "openpilotMinutes",
  "OverpassRequests",
  "PandaSignatures",
  "PreviousSpeedLimit",
  "SpeedLimits",
  "SpeedLimitsFiltered",
  "ThemesDownloaded",
  "UpdateFailedCount",
  "UpdaterAvailableBranches",
  "UpdaterCurrentDescription",
  "UpdaterCurrentReleaseNotes",
  "UpdaterFetchAvailable",
  "UpdaterTargetBranch",
  "UptimeOffroad"
}

TUNING_LEVELS = {
  "MINIMAL": 0,
  "STANDARD": 1,
  "ADVANCED": 2,
  "DEVELOPER": 3
}

def get_frogpilot_toggles(sm=None):
  if sm is not None:
    toggles = sm["frogpilotUI"].frogpilotToggles
    if toggles:
      return process_frogpilot_toggles(toggles)
  return FrogPilotVariables().frogpilot_toggles

@lru_cache(maxsize=1)
def process_frogpilot_toggles(toggles_json):
  return SimpleNamespace(**json.loads(toggles_json))

class FrogPilotVariables:
  def __init__(self):
    self.params = Params(return_defaults=True)

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

    toggle.tuning_level = self.params.get("TuningLevel") if self.params.get_bool("TuningLevelConfirmed") else TUNING_LEVELS["ADVANCED"]

    self.update()

  def get_value(self, key, cast=bool, condition=True, conversion=None, default=None, min=None, max=None):
    if not condition or (self.frogpilot_toggles.tuning_level < self.tuning_levels.get(key, 0)):
      if default is not None:
        return default
      return False if cast is bool else self.default_values.get(key)

    value = self.params.get(key)
    if value is None and default is not None:
      value = default

    if value is not None and cast is not None:
      try:
        needs_cast = not isinstance(value, cast)
      except TypeError:
        needs_cast = True

      if needs_cast:
        try:
          value = cast(value)
        except (TypeError, ValueError):
          value = self.default_values.get(key)
          if default is not None:
            value = default
          if value is not None:
            value = cast(value)
    elif default is not None:
      value = default

    if conversion is not None and isinstance(value, (int, float)):
      value *= conversion

    if min is not None and value < min:
      value = min
    elif max is not None and value > max:
      value = max

    return value

  def update(self, started=False):
    toggle = self.frogpilot_toggles
    toggle.tuning_level = self.params.get("TuningLevel") if self.params.get_bool("TuningLevelConfirmed") else TUNING_LEVELS["ADVANCED"]

    msg_bytes = self.params.get("CarParams" if started else "CarParamsPersistent", block=started)
    if msg_bytes:
      CP = messaging.log_from_bytes(msg_bytes, car.CarParams)
    else:
      CP = interfaces[MOCK.MOCK].get_params(MOCK.MOCK, gen_empty_fingerprint(), [], False, False, False, toggle).as_reader()

    toggle.is_torque_car = CP.lateralTuning.which() == "torque"
    if not toggle.is_torque_car:
      CP_builder = CP.as_builder()
      CarInterfaceBase.configure_torque_tune(MOCK.MOCK, CP_builder.lateralTuning)
      CP = CP_builder.as_reader()

    fpmsg_bytes = self.params.get("FrogPilotCarParams" if started else "FrogPilotCarParamsPersistent", block=started)
    if fpmsg_bytes:
      FPCP = messaging.log_from_bytes(fpmsg_bytes, custom.FrogPilotCarParams)
    else:
      FPCP = interfaces[MOCK.MOCK].get_frogpilot_params(MOCK.MOCK, gen_empty_fingerprint(), [], CP, toggle)

    toggle.alpha_longitudinal = CP.alphaLongitudinalAvailable
    toggle.car_make = CP.brand
    toggle.car_model = CP.carFingerprint
    toggle.default_friction = CP.lateralTuning.torque.friction
    toggle.has_bsm = CP.enableBsm
    toggle.has_cc_long = toggle.car_make == "gm" and bool(CP.flags & GMOPGMFlags.CC_LONG.value)
    toggle.has_dash_speed_limits = toggle.car_make in ("toyota")
    toggle.has_pedal = CP.enableGasInterceptorDEPRECATED
    toggle.has_radar = not CP.radarUnavailable
    toggle.has_sdsu = toggle.car_make == "toyota" and bool(FPCP.flags & ToyotaFrogPilotFlags.SMART_DSU.value)
    toggle.has_sng = CP.autoResumeSng
    toggle.has_zss = toggle.car_make == "toyota" and bool(FPCP.flags & ToyotaFrogPilotFlags.ZSS.value)
    toggle.is_angle_car = CP.steerControlType == car.CarParams.SteerControlType.angle
    toggle.is_tsk = toggle.car_make == "toyota" and CP.secOcRequired
    toggle.lat_accel_factor = CP.lateralTuning.torque.latAccelFactor
    toggle.lkas_allowed_for_aol = toggle.car_make == "hyundai" and bool(CP.flags & HyundaiFlags.CANFD or CP.flags & HyundaiFlags.HAS_LDA_BUTTON)
    toggle.longitudinal_actuator_delay = CP.longitudinalActuatorDelay
    toggle.openpilot_longitudinal = CP.openpilotLongitudinalControl and not toggle.disable_openpilot_long
    toggle.pcm_cruise = CP.pcmCruise
    prohibited_main_aol = not toggle.openpilot_longitudinal and toggle.car_make == "hyundai" and bool(CP.flags & HyundaiFlags.CANFD or CP.flags & HyundaiFlags.HAS_LDA_BUTTON)
    toggle.start_accel = CP.startAccel
    toggle.steer_actuator_delay = CP.steerActuatorDelay
    toggle.steer_kp = KP
    toggle.steer_ratio = CP.steerRatio
    toggle.stop_accel = CP.stopAccel
    toggle.stoppingDecelRate = CP.stoppingDecelRate
    toggle.vEgoStarting = CP.vEgoStarting
    toggle.vEgoStopping = CP.vEgoStopping

    msg_bytes = self.params.get("LiveTorqueParameters")
    if msg_bytes:
      LTP = messaging.log_from_bytes(msg_bytes, log.LiveTorqueParametersData)
    else:
      pass

    toggle.is_metric = self.params.get_bool("IsMetric")

    advanced_custom_ui = self.get_value("AdvancedCustomUI")
    toggle.hide_alerts = self.get_value("HideAlerts", condition=advanced_custom_ui)
    toggle.hide_lead_marker = self.get_value("HideLeadMarker", condition=advanced_custom_ui and toggle.openpilot_longitudinal)
    toggle.hide_max_speed = self.get_value("HideMaxSpeed", condition=advanced_custom_ui)
    toggle.hide_speed = self.get_value("HideSpeed", condition=advanced_custom_ui)
    toggle.hide_speed_limit = self.get_value("HideSpeedLimit", condition=advanced_custom_ui)
    toggle.use_wheel_speed = self.get_value("WheelSpeed", condition=advanced_custom_ui)

    advanced_lateral_tuning = self.get_value("AdvancedLateralTune")
    toggle.steerActuatorDelay = self.get_value("SteerDelay", cast=float, condition=advanced_lateral_tuning, default=toggle.steer_actuator_delay, min=0.01, max=1.0)
    toggle.use_custom_steerActuatorDelay = bool(round(toggle.steerActuatorDelay, 2) != round(toggle.steer_actuator_delay, 2))
    toggle.friction = self.get_value("SteerFriction", cast=float, condition=advanced_lateral_tuning and toggle.is_torque_car, default=toggle.default_friction, min=0, max=1)
    toggle.use_custom_friction = bool(round(toggle.friction, 2) != round(toggle.default_friction, 2)) and toggle.is_torque_car
    toggle.steerKp = [[0], [self.get_value("SteerKP", cast=float, condition=advanced_lateral_tuning and toggle.is_torque_car and not toggle.is_angle_car, default=toggle.steer_kp, min=toggle.steer_kp * STEERING_TUNE_MIN_FACTOR, max=toggle.steer_kp * STEERING_TUNE_MAX_FACTOR)]]
    toggle.latAccelFactor = self.get_value("SteerLatAccel", cast=float, condition=advanced_lateral_tuning and toggle.is_torque_car, default=toggle.lat_accel_factor, min=toggle.lat_accel_factor * STEERING_TUNE_MIN_FACTOR, max=toggle.lat_accel_factor * STEERING_TUNE_MAX_FACTOR)
    toggle.use_custom_latAccelFactor = bool(round(toggle.latAccelFactor, 2) != round(toggle.lat_accel_factor, 2)) and toggle.is_torque_car
    toggle.steerRatio = self.get_value("SteerRatio", cast=float, condition=advanced_lateral_tuning, default=toggle.steer_ratio, min=toggle.steer_ratio * STEERING_TUNE_MIN_FACTOR, max=toggle.steer_ratio * STEERING_TUNE_MAX_FACTOR)
    toggle.use_custom_steerRatio = bool(round(toggle.steerRatio, 2) != round(toggle.steer_ratio, 2))

    advanced_longitudinal_tuning = toggle.openpilot_longitudinal and self.get_value("AdvancedLongitudinalTune")
    toggle.longitudinalActuatorDelay = self.get_value("LongitudinalActuatorDelay", cast=float, condition=advanced_longitudinal_tuning, default=toggle.longitudinal_actuator_delay, min=0, max=1)
    toggle.startAccel = self.get_value("StartAccel", cast=float, condition=advanced_longitudinal_tuning, default=toggle.start_accel, min=0, max=MAX_ACCELERATION)
    toggle.stopAccel = self.get_value("StopAccel", cast=float, condition=advanced_longitudinal_tuning, default=toggle.stop_accel, min=-MAX_ACCELERATION, max=0)
    toggle.stoppingDecelRate = self.get_value("StoppingDecelRate", cast=float, condition=advanced_longitudinal_tuning, default=toggle.stoppingDecelRate, min=0.001, max=1)
    toggle.vEgoStarting = self.get_value("VEgoStarting", cast=float, condition=advanced_longitudinal_tuning, default=toggle.vEgoStarting, min=0.01, max=1)
    toggle.vEgoStopping = self.get_value("VEgoStopping", cast=float, condition=advanced_longitudinal_tuning, default=toggle.vEgoStopping, min=0.01, max=1)

    toggle.alert_volume_controller = self.get_value("AlertVolumeControl")
    toggle.disengage_volume = self.get_value("DisengageVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.engage_volume = self.get_value("EngageVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.prompt_volume = self.get_value("PromptVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.promptDistracted_volume = self.get_value("PromptDistractedVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.refuse_volume = self.get_value("RefuseVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.warningSoft_volume = self.get_value("WarningSoftVolume", cast=float, condition=toggle.alert_volume_controller)
    toggle.warningImmediate_volume = max(self.get_value("WarningImmediateVolume", cast=float, condition=toggle.alert_volume_controller, default=25), 25)

    toggle.always_on_lateral = self.get_value("AlwaysOnLateral")
    toggle.always_on_lateral_lkas = toggle.always_on_lateral and toggle.lkas_allowed_for_aol and self.get_value("AlwaysOnLateralLKAS")
    toggle.always_on_lateral_main = toggle.always_on_lateral and not prohibited_main_aol and not toggle.always_on_lateral_lkas
    toggle.always_on_lateral_pause_speed = self.get_value("PauseAOLOnBrake", cast=float, condition=toggle.always_on_lateral)

    toggle.automatic_updates = self.get_value("AutomaticUpdates", condition=(self.release_branch or self.vetting_branch), default=True) and not BACKUP_PATH.is_file()

    car_model = self.params.get("CarModel")
    toggle.force_fingerprint = self.get_value("ForceFingerprint", condition=car_model != self.default_values["CarModel"])
    if toggle.force_fingerprint:
      toggle.car_model = car_model

    toggle.cluster_offset = self.get_value("ClusterOffset", cast=float, condition=toggle.car_make == "toyota")

    toggle.conditional_experimental_mode = toggle.openpilot_longitudinal and self.get_value("ConditionalExperimental")
    toggle.conditional_curves = self.get_value("CECurves", condition=toggle.conditional_experimental_mode)
    toggle.conditional_curves_lead = self.get_value("CECurvesLead", condition=toggle.conditional_curves)
    toggle.conditional_lead = self.get_value("CELead", condition=toggle.conditional_experimental_mode)
    toggle.conditional_slower_lead = self.get_value("CESlowerLead", condition=toggle.conditional_lead)
    toggle.conditional_stopped_lead = self.get_value("CEStoppedLead", condition=toggle.conditional_lead)
    toggle.conditional_limit = self.get_value("CESpeed", cast=float, condition=toggle.conditional_experimental_mode)
    toggle.conditional_limit_lead = self.get_value("CESpeedLead", cast=float, condition=toggle.conditional_experimental_mode)
    toggle.conditional_model_stop_time = self.get_value("CEModelStopTime", cast=float, condition=toggle.conditional_experimental_mode and self.get_value("CEStopLights"))
    toggle.conditional_signal = self.get_value("CESignalSpeed", cast=float, condition=toggle.conditional_experimental_mode)
    toggle.conditional_signal_lane_detection = self.get_value("CESignalLaneDetection", condition=toggle.conditional_signal != 0)
    toggle.cem_status = self.get_value("ShowCEMStatus", condition=toggle.conditional_experimental_mode)

    toggle.curve_speed_controller = toggle.openpilot_longitudinal and self.get_value("CurveSpeedController")
    toggle.csc_status = self.get_value("ShowCSCStatus", condition=toggle.curve_speed_controller)

    custom_alerts = self.get_value("CustomAlerts")
    toggle.goat_scream_alert = self.get_value("GoatScream", condition=custom_alerts)
    toggle.green_light_alert = self.get_value("GreenLightAlert", condition=custom_alerts)
    toggle.lead_departing_alert = self.get_value("LeadDepartingAlert", condition=custom_alerts)
    toggle.loud_blindspot_alert = self.get_value("LoudBlindspotAlert", condition=custom_alerts and toggle.has_bsm)
    toggle.speed_limit_changed_alert = self.get_value("SpeedLimitChangedAlert", condition=custom_alerts)

    toggle.custom_personalities = toggle.openpilot_longitudinal and self.get_value("CustomPersonalities")
    toggle.aggressive_jerk_acceleration = self.get_value("AggressiveJerkAcceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.aggressive_jerk_deceleration = self.get_value("AggressiveJerkDeceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.aggressive_jerk_danger = self.get_value("AggressiveJerkDanger", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.aggressive_jerk_speed = self.get_value("AggressiveJerkSpeed", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.aggressive_jerk_speed_decrease = self.get_value("AggressiveJerkSpeedDecrease", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.aggressive_follow = self.get_value("AggressiveFollow", cast=float, condition=toggle.custom_personalities, min=1, max=MAX_T_FOLLOW)
    toggle.standard_jerk_acceleration = self.get_value("StandardJerkAcceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.standard_jerk_deceleration = self.get_value("StandardJerkDeceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.standard_jerk_danger = self.get_value("StandardJerkDanger", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.standard_jerk_speed = self.get_value("StandardJerkSpeed", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.standard_jerk_speed_decrease = self.get_value("StandardJerkSpeedDecrease", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.standard_follow = self.get_value("StandardFollow", cast=float, condition=toggle.custom_personalities, min=1, max=MAX_T_FOLLOW)
    toggle.relaxed_jerk_acceleration = self.get_value("RelaxedJerkAcceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.relaxed_jerk_deceleration = self.get_value("RelaxedJerkDeceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.relaxed_jerk_danger = self.get_value("RelaxedJerkDanger", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.relaxed_jerk_speed = self.get_value("RelaxedJerkSpeed", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.relaxed_jerk_speed_decrease = self.get_value("RelaxedJerkSpeedDecrease", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01)
    toggle.relaxed_follow = self.get_value("RelaxedFollow", cast=float, condition=toggle.custom_personalities, min=1, max=MAX_T_FOLLOW)
    toggle.traffic_mode_jerk_acceleration = [self.get_value("TrafficJerkAcceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01), toggle.aggressive_jerk_acceleration]
    toggle.traffic_mode_jerk_deceleration = [self.get_value("TrafficJerkDeceleration", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01), toggle.aggressive_jerk_deceleration]
    toggle.traffic_mode_jerk_danger = [self.get_value("TrafficJerkDanger", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01), toggle.aggressive_jerk_danger]
    toggle.traffic_mode_jerk_speed = [self.get_value("TrafficJerkSpeed", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01), toggle.aggressive_jerk_speed]
    toggle.traffic_mode_jerk_speed_decrease = [self.get_value("TrafficJerkSpeedDecrease", cast=float, condition=toggle.custom_personalities, conversion=0.01, min=JERK_MIN_RAW * 0.01, max=JERK_MAX_RAW * 0.01), toggle.aggressive_jerk_speed_decrease]
    toggle.traffic_mode_follow = [self.get_value("TrafficFollow", cast=float, condition=toggle.custom_personalities, min=TRAFFIC_FOLLOW_MIN, max=MAX_T_FOLLOW), toggle.aggressive_follow]

    custom_themes = self.get_value("CustomThemes")
    toggle.sound_pack = self.get_value("SoundPack", cast=None, condition=custom_themes, default="stock")

    custom_ui = self.get_value("CustomUI")
    toggle.acceleration_path = toggle.openpilot_longitudinal and (self.get_value("AccelerationPath", condition=custom_ui))
    toggle.adjacent_paths = self.get_value("AdjacentPath", condition=custom_ui)
    toggle.blind_spot_path = toggle.has_bsm and self.get_value("BlindSpotPath", condition=custom_ui)
    toggle.compass = self.get_value("Compass", condition=custom_ui)
    toggle.pedals_on_ui = self.get_value("PedalsOnUI", condition=custom_ui and toggle.openpilot_longitudinal)
    toggle.dynamic_pedals_on_ui = self.get_value("DynamicPedalsOnUI", condition=toggle.pedals_on_ui)
    toggle.static_pedals_on_ui = self.get_value("StaticPedalsOnUI", condition=toggle.pedals_on_ui)
    toggle.rotating_wheel = self.get_value("RotatingWheel", condition=custom_ui)

    toggle.developer_ui = self.get_value("DeveloperUI")
    developer_metrics = self.get_value("DeveloperMetrics", condition=toggle.developer_ui)
    border_metrics = self.get_value("BorderMetrics", condition=developer_metrics)
    toggle.blind_spot_metrics = toggle.has_bsm and self.get_value("BlindSpotMetrics", condition=border_metrics)
    toggle.signal_metrics = self.get_value("SignalMetrics", condition=border_metrics)
    toggle.steering_metrics = self.get_value("ShowSteering", condition=border_metrics)
    toggle.show_fps = self.get_value("FPSCounter", condition=developer_metrics)
    toggle.adjacent_path_metrics = self.get_value("AdjacentPathMetrics", condition=developer_metrics)
    toggle.lead_info = self.get_value("LeadInfo", condition=developer_metrics)
    toggle.numerical_temp = self.get_value("NumericalTemp", condition=developer_metrics)
    toggle.fahrenheit = self.get_value("Fahrenheit", condition=toggle.numerical_temp)
    toggle.cpu_metrics = self.get_value("ShowCPU", condition=developer_metrics)
    toggle.gpu_metrics = self.get_value("ShowGPU", condition=developer_metrics)
    toggle.ip_metrics = self.get_value("ShowIP", condition=developer_metrics)
    toggle.memory_metrics = self.get_value("ShowMemoryUsage", condition=developer_metrics)
    toggle.storage_left_metrics = self.get_value("ShowStorageLeft", condition=developer_metrics)
    toggle.storage_used_metrics = self.get_value("ShowStorageUsed", condition=developer_metrics)
    toggle.use_si_metrics = self.get_value("UseSI", condition=developer_metrics)
    toggle.developer_sidebar = self.get_value("DeveloperSidebar", condition=toggle.developer_ui)
    developer_widgets = self.get_value("DeveloperWidgets", condition=toggle.developer_ui)
    toggle.adjacent_lead_tracking = toggle.has_radar and (self.get_value("AdjacentLeadsUI", condition=developer_widgets))
    toggle.radar_tracks = toggle.has_radar and (self.get_value("RadarTracksUI", condition=developer_widgets))
    toggle.show_stopping_point = toggle.openpilot_longitudinal and (self.get_value("ShowStoppingPoint", condition=developer_widgets))
    toggle.show_stopping_point_metrics = self.get_value("ShowStoppingPointMetrics", condition=toggle.show_stopping_point)

    device_management = self.get_value("DeviceManagement")
    toggle.increase_thermal_limits = self.get_value("IncreaseThermalLimits", condition=device_management)
    toggle.low_voltage_shutdown = self.get_value("LowVoltageShutdown", cast=float, condition=device_management, min=VBATT_PAUSE_CHARGING, max=LOW_VOLTAGE_SHUTDOWN_MAX)
    toggle.no_logging = self.get_value("NoLogging", condition=device_management and not self.vetting_branch)
    toggle.no_uploads = self.get_value("NoUploads", condition=device_management and not self.vetting_branch)
    toggle.no_onroad_uploads = self.get_value("DisableOnroadUploads", condition=toggle.no_uploads)

    distance_button_control = self.get_value("DistanceButtonControl", cast=int)
    toggle.experimental_mode_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press = toggle.experimental_mode_via_distance
    toggle.force_coast_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance = distance_button_control == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance = toggle.openpilot_longitudinal and distance_button_control == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    distance_button_control_long = self.get_value("LongDistanceButtonControl", cast=int)
    toggle.experimental_mode_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_distance_long
    toggle.force_coast_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance_long = distance_button_control_long == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance_long = toggle.openpilot_longitudinal and distance_button_control_long == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    distance_button_control_very_long = self.get_value("VeryLongDistanceButtonControl", cast=int)
    toggle.experimental_mode_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_distance_very_long
    toggle.force_coast_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_distance_very_long = distance_button_control_very_long == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_distance_very_long = toggle.openpilot_longitudinal and distance_button_control_very_long == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    toggle.has_mapbox_key = self.params.get("MapboxSecretKey") is not None

    toggle.lane_changes = self.get_value("LaneChanges")
    toggle.lane_change_delay = self.get_value("LaneChangeTime", cast=float, condition=toggle.lane_changes)
    toggle.lane_detection_width = self.get_value("LaneDetectionWidth", cast=float, condition=toggle.lane_changes)
    toggle.minimum_lane_change_speed = self.get_value("MinimumLaneChangeSpeed", cast=float, condition=toggle.lane_changes)
    toggle.nudgeless = self.get_value("NudgelessLaneChange", condition=toggle.lane_changes)
    toggle.one_lane_change = self.get_value("OneLaneChange", condition=toggle.lane_changes)

    lateral_tuning = self.get_value("LateralTune")
    toggle.use_turn_desires = self.get_value("TurnDesires", condition=lateral_tuning)

    lkas_button_control = self.get_value("LKASButtonControl", cast=int, condition=toggle.car_make != "subaru")
    toggle.experimental_mode_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["EXPERIMENTAL_MODE"]
    toggle.experimental_mode_via_press |= toggle.experimental_mode_via_lkas
    toggle.force_coast_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["FORCE_COAST"]
    toggle.pause_lateral_via_lkas = lkas_button_control == BUTTON_FUNCTIONS["PAUSE_LATERAL"]
    toggle.pause_longitudinal_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["PAUSE_LONGITUDINAL"]
    toggle.personality_profile_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["PERSONALITY_PROFILE"]
    toggle.traffic_mode_via_lkas = toggle.openpilot_longitudinal and lkas_button_control == BUTTON_FUNCTIONS["TRAFFIC_MODE"]

    toggle.lock_doors_timer = self.get_value("LockDoorsTimer", cast=int, condition=(toggle.car_make == "toyota"))

    longitudinal_tuning = toggle.openpilot_longitudinal and self.get_value("LongitudinalTune")
    toggle.acceleration_profile = self.get_value("AccelerationProfile", cast=int, condition=longitudinal_tuning)
    toggle.deceleration_profile = self.get_value("DecelerationProfile", cast=int, condition=longitudinal_tuning)
    toggle.human_acceleration = self.get_value("HumanAcceleration", condition=longitudinal_tuning)
    toggle.human_following = self.get_value("HumanFollowing", condition=longitudinal_tuning)
    toggle.human_lane_changes = toggle.has_radar and self.get_value("HumanLaneChanges", condition=longitudinal_tuning)
    toggle.lead_detection_probability = self.get_value("LeadDetectionThreshold", cast=float, condition=longitudinal_tuning, conversion=0.01, min=LEAD_DETECTION_MIN_RAW * 0.01, max=LEAD_DETECTION_MAX_RAW * 0.01)
    toggle.taco_tune = self.get_value("TacoTune", condition=longitudinal_tuning)

    toggle.model_name = self.get_value("DrivingModelName", cast=str, default="")

    toggle.model_ui = self.get_value("ModelUI")
    toggle.dynamic_path_width = self.get_value("DynamicPathWidth", condition=toggle.model_ui)
    toggle.lane_line_width = self.get_value("LaneLinesWidth", cast=float, condition=toggle.model_ui)
    toggle.path_edge_width = self.get_value("PathEdgeWidth", cast=float, condition=toggle.model_ui)
    toggle.path_width = self.get_value("PathWidth", cast=float, condition=toggle.model_ui)
    toggle.road_edge_width = self.get_value("RoadEdgesWidth", cast=float, condition=toggle.model_ui)

    navigation_ui = self.get_value("NavigationUI")
    toggle.road_name_ui = self.get_value("RoadNameUI", condition=navigation_ui)
    toggle.show_speed_limits = self.get_value("ShowSpeedLimits", condition=navigation_ui)
    toggle.speed_limit_vienna = self.get_value("UseVienna", condition=navigation_ui)

    quality_of_life_lateral = self.get_value("QOLLateral")
    toggle.pause_lateral_below_speed = self.get_value("PauseLateralSpeed", cast=float, condition=quality_of_life_lateral)
    toggle.pause_lateral_below_signal = self.get_value("PauseLateralOnSignal", condition=toggle.pause_lateral_below_speed != 0)

    quality_of_life_longitudinal = toggle.openpilot_longitudinal and self.get_value("QOLLongitudinal")
    toggle.cruise_increase = self.get_value("CruiseButtonIncrement", cast=float, condition=(quality_of_life_longitudinal and not toggle.pcm_cruise))
    toggle.cruise_increase_long = self.get_value("CruiseButtonIncrementLong", cast=float, condition=(quality_of_life_longitudinal and not toggle.pcm_cruise))
    toggle.force_stops = self.get_value("ForceStops", condition=quality_of_life_longitudinal)
    toggle.increase_stopped_distance = self.get_value("IncreasedStoppedDistance", cast=float, condition=quality_of_life_longitudinal)
    map_gears = self.get_value("MapGears", condition=quality_of_life_longitudinal)
    toggle.map_acceleration = self.get_value("MapAcceleration", condition=map_gears)
    toggle.map_deceleration = self.get_value("MapDeceleration", condition=map_gears)
    toggle.reverse_cruise_increase = self.get_value("ReverseCruise", condition=quality_of_life_longitudinal and toggle.car_make == "toyota" and toggle.pcm_cruise)
    toggle.set_speed_offset = self.get_value("SetSpeedOffset", cast=float, condition=(quality_of_life_longitudinal and not toggle.pcm_cruise), conversion=CV.MS_TO_KPH)
    toggle.weather_presets = self.get_value("WeatherPresets", condition=quality_of_life_longitudinal)
    toggle.increase_following_distance_low_visibility = self.get_value("IncreaseFollowingLowVisibility", cast=float, condition=toggle.weather_presets)
    toggle.increase_following_distance_rain = self.get_value("IncreaseFollowingRain", cast=float, condition=toggle.weather_presets)
    toggle.increase_following_distance_rain_storm = self.get_value("IncreaseFollowingRainStorm", cast=float, condition=toggle.weather_presets)
    toggle.increase_following_distance_snow = self.get_value("IncreaseFollowingSnow", cast=float, condition=toggle.weather_presets)
    toggle.increase_stopped_distance_low_visibility = self.get_value("IncreasedStoppedDistanceLowVisibility", cast=float, condition=toggle.weather_presets)
    toggle.increase_stopped_distance_rain = self.get_value("IncreasedStoppedDistanceRain", cast=float, condition=toggle.weather_presets)
    toggle.increase_stopped_distance_rain_storm = self.get_value("IncreasedStoppedDistanceRainStorm", cast=float, condition=toggle.weather_presets)
    toggle.increase_stopped_distance_snow = self.get_value("IncreasedStoppedDistanceSnow", cast=float, condition=toggle.weather_presets)
    toggle.reduce_acceleration_low_visibility = self.get_value("ReduceAccelerationLowVisibility", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_acceleration_rain = self.get_value("ReduceAccelerationRain", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_acceleration_rain_storm = self.get_value("ReduceAccelerationRainStorm", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_acceleration_snow = self.get_value("ReduceAccelerationSnow", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_low_visibility = self.get_value("ReduceLateralAccelerationLowVisibility", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_rain = self.get_value("ReduceLateralAccelerationRain", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_rain_storm = self.get_value("ReduceLateralAccelerationRainStorm", cast=float, condition=toggle.weather_presets, conversion=0.01)
    toggle.reduce_lateral_acceleration_snow = self.get_value("ReduceLateralAccelerationSnow", cast=float, condition=toggle.weather_presets, conversion=0.01)

    quality_of_life_visuals = self.get_value("QOLVisuals")
    toggle.camera_view = self.get_value("CameraView", cast=int, condition=quality_of_life_visuals)
    toggle.driver_camera_in_reverse = self.get_value("DriverCamera", condition=quality_of_life_visuals)
    toggle.onroad_distance_button = toggle.openpilot_longitudinal and (self.get_value("OnroadDistanceButton", condition=quality_of_life_visuals))
    toggle.stopped_timer = self.get_value("StoppedTimer", condition=quality_of_life_visuals)

    toggle.rainbow_path = self.get_value("RainbowPath")

    toggle.random_events = self.get_value("RandomEvents")

    screen_management = self.get_value("ScreenManagement")
    toggle.screen_brightness = max(self.get_value("ScreenBrightness", cast=float, condition=screen_management), 1)
    toggle.screen_brightness_onroad = self.get_value("ScreenBrightnessOnroad", cast=float, condition=(screen_management))
    toggle.screen_recorder = self.get_value("ScreenRecorder", condition=screen_management)
    toggle.screen_timeout = self.get_value("ScreenTimeout", cast=float, condition=screen_management)
    toggle.screen_timeout_onroad = self.get_value("ScreenTimeoutOnroad", cast=float, condition=screen_management)
    toggle.standby_mode = self.get_value("StandbyMode", condition=screen_management)

    toggle.sng_hack = self.get_value("SNGHack", condition=toggle.openpilot_longitudinal and toggle.car_make == "toyota" and not toggle.has_pedal and not toggle.has_sng)

    toggle.speed_limit_controller = toggle.openpilot_longitudinal and self.get_value("SpeedLimitController")
    toggle.map_speed_lookahead_higher = self.get_value("SLCLookaheadHigher", cast=float, condition=toggle.speed_limit_controller)
    toggle.map_speed_lookahead_lower = self.get_value("SLCLookaheadLower", cast=float, condition=toggle.speed_limit_controller)
    toggle.set_speed_limit = self.get_value("SetSpeedLimit", condition=toggle.speed_limit_controller)
    toggle.show_speed_limit_offset = self.get_value("ShowSLCOffset", condition=toggle.speed_limit_controller)
    slc_fallback_method = self.get_value("SLCFallback", cast=int, condition=toggle.speed_limit_controller)
    toggle.slc_fallback_experimental_mode = slc_fallback_method == 1
    toggle.slc_fallback_previous_speed_limit = slc_fallback_method == 2
    toggle.slc_fallback_set_speed = slc_fallback_method == 0
    toggle.slc_mapbox_filler = self.get_value("SLCMapboxFiller", condition=(toggle.show_speed_limits or toggle.speed_limit_controller) and toggle.has_mapbox_key)
    speed_limit_confirmation = self.get_value("SLCConfirmation", condition=toggle.speed_limit_controller)
    toggle.speed_limit_confirmation_higher = self.get_value("SLCConfirmationHigher", condition=speed_limit_confirmation)
    toggle.speed_limit_confirmation_lower = self.get_value("SLCConfirmationLower", condition=speed_limit_confirmation)
    slc_override_method = self.get_value("SLCOverride", cast=int, condition=toggle.speed_limit_controller)
    toggle.speed_limit_controller_override_manual = slc_override_method == 1
    toggle.speed_limit_controller_override_set_speed = slc_override_method == 2
    toggle.speed_limit_offset1 = self.get_value("Offset1", cast=float, condition=toggle.speed_limit_controller)
    toggle.speed_limit_offset2 = self.get_value("Offset2", cast=float, condition=toggle.speed_limit_controller)
    toggle.speed_limit_offset3 = self.get_value("Offset3", cast=float, condition=toggle.speed_limit_controller)
    toggle.speed_limit_offset4 = self.get_value("Offset4", cast=float, condition=toggle.speed_limit_controller)
    toggle.speed_limit_offset5 = self.get_value("Offset5", cast=float, condition=toggle.speed_limit_controller)
    toggle.speed_limit_offset6 = self.get_value("Offset6", cast=float, condition=toggle.speed_limit_controller)
    toggle.speed_limit_offset7 = self.get_value("Offset7", cast=float, condition=toggle.speed_limit_controller)
    toggle.speed_limit_priority1 = self.get_value("SLCPriority1", cast=None, condition=toggle.speed_limit_controller)
    toggle.speed_limit_priority2 = self.get_value("SLCPriority2", cast=None, condition=toggle.speed_limit_controller)
    toggle.speed_limit_priority_highest = toggle.speed_limit_priority1 == "Highest"
    toggle.speed_limit_priority_lowest = toggle.speed_limit_priority1 == "Lowest"
    toggle.speed_limit_sources = self.get_value("SpeedLimitSources", condition=toggle.speed_limit_controller)

    toggle.speed_limit_filler = self.get_value("SpeedLimitFiller")

    toggle.startup_alert_top = self.get_value("StartupMessageTop", cast=str, default="")
    toggle.startup_alert_bottom = self.get_value("StartupMessageBottom", cast=str, default="")

    toggle.tethering_config = self.get_value("TetheringMode", cast=int)

    toyota_doors = self.get_value("ToyotaDoors", condition=toggle.car_make == "toyota")
    toggle.lock_doors = self.get_value("LockDoors", condition=toyota_doors)
    toggle.unlock_doors = self.get_value("UnlockDoors", condition=toyota_doors)

    toggle.volt_sng = self.get_value("VoltSNG", condition=toggle.car_model == "CHEVROLET_VOLT")

    return self.frogpilot_toggles
