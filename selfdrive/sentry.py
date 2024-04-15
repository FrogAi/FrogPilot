"""Install exception handler for process crash."""
import os
import sentry_sdk
import socket
import time
import traceback
import urllib.request
import urllib.error

from datetime import datetime
from enum import Enum
from sentry_sdk.integrations.threading import ThreadingIntegration

from openpilot.common.params import Params
from openpilot.system.hardware import HARDWARE, PC
from openpilot.common.swaglog import cloudlog
from openpilot.system.version import get_commit, get_short_branch, get_origin, get_version

CRASHES_DIR = "/data/community/crashes/"

class SentryProject(Enum):
  # python project
  SELFDRIVE = "https://5ad1714d27324c74a30f9c538bff3b8d@o4505034923769856.ingest.sentry.io/4505034930651136"
  # native project
  SELFDRIVE_NATIVE = "https://5ad1714d27324c74a30f9c538bff3b8d@o4505034923769856.ingest.sentry.io/4505034930651136"


def sentry_pinged(url="https://sentry.io", timeout=5):
  try:
    urllib.request.urlopen(url, timeout=timeout)
    return True
  except (urllib.error.URLError, socket.timeout):
    return False


def bind_user() -> None:
  sentry_sdk.set_user({"id": HARDWARE.get_serial()})


def report_tombstone(fn: str, message: str, contents: str) -> None:
  FrogPilot = "frogai" in get_origin().lower()
  if not FrogPilot or PC:
    return

  no_internet = 0
  while True:
    if sentry_pinged():
      cloudlog.error({'tombstone': message})

      with sentry_sdk.configure_scope() as scope:
        bind_user()
        scope.set_extra("tombstone_fn", fn)
        scope.set_extra("tombstone", contents)
        sentry_sdk.capture_message(message=message)
        sentry_sdk.flush()
      break
    else:
      if no_internet > 5:
        break
      no_internet += 1
      time.sleep(600)


def chunk_data(data, size):
  return [data[i:i+size] for i in range(0, len(data), size)]


def format_params(params):
  formatted_params = []
  for k, v in params.items():
    if isinstance(v, bytes):
      param_value = format(float(v), '.12g') if v.replace(b'.', b'').isdigit() else v.decode()
    elif isinstance(v, float):
      param_value = format(v, '.12g')
    else:
      param_value = v
    formatted_params.append(f"{k}: {param_value}")
  return formatted_params


def get_frogpilot_params(params, keys):
  return {key: params.get(key) or '0' for key in keys}


def set_sentry_scope(scope, chunks, label):
  scope.set_extra(label, '\n'.join(['\n'.join(chunk) for chunk in chunks]))


def capture_fingerprint(params, candidate, blocked=False):
  bind_user()

  control_keys, vehicle_keys, visual_keys, other_keys, tracking_keys = [
    "AlwaysOnLateral", "AlwaysOnLateralMain", "HideAOLStatusBar", "ConditionalExperimental", "CESpeed", "CESpeedLead", "CECurves", "CECurvesLead",
    "CENavigation", "CENavigationIntersections", "CENavigationTurns", "CENavigationLead", "CESlowerLead", "CEStopLights", "CEStopLightsLead",
    "CESignal", "HideCEMStatusBar", "CustomPersonalities", "TrafficFollow", "TrafficJerk", "AggressiveFollow", "AggressiveJerk", "StandardFollow",
    "StandardJerk", "RelaxedFollow", "RelaxedJerk", "DeviceManagement", "IncreaseThermalLimits", "DeviceShutdown", "NoLogging", "NoUploads", "LowVoltageShutdown",
    "OfflineMode", "ExperimentalModeActivation", "ExperimentalModeViaLKAS", "ExperimentalModeViaTap", "ExperimentalModeViaDistance", "LateralTune",
    "ForceAutoTune", "NNFF", "NNFFLite", "SteerRatio", "TacoTune", "TurnDesires", "SteerRatio", "LongitudinalTune", "AccelerationProfile", "DecelerationProfile",
    "AggressiveAcceleration", "StoppingDistance", "LeadDetectionThreshold", "SmoothBraking", "SmoothBrakingFarLead", "SmoothBrakingJerk", "TrafficMode",
    "MTSCEnabled", "DisableMTSCSmoothing", "MTSCCurvatureCheck", "MTSCAggressiveness", "ModelSelector", "Model", "NudgelessLaneChange", "LaneChangeTime",
    "LaneDetectionWidth", "OneLaneChange", "LaneDetectionWidth", "QOLControls", "CustomCruise", "CustomCruiseLong", "DisableOnroadUploads", "HigherBitrate",
    "OnroadDistanceButton", "KaofuiIcons", "PauseLateralSpeed", "PauseLateralOnSignal", "ReverseCruise", "SetSpeedOffset", "SpeedLimitController", "Offset1",
    "Offset2", "Offset3", "Offset4", "SLCFallback", "SLCOverride", "SLCPriority", "SLCConfirmation", "SLCConfirmationLower", "SLCConfirmationHigher",
    "ForceMPHDashboard", "SLCLookaheadHigher", "SLCLookaheadLower", "SetSpeedLimit", "ShowSLCOffset", "ShowSLCOffsetUI", "UseVienna", "VisionTurnControl",
    "DisableVTSCSmoothing", "CurveSensitivity", "TurnAggressiveness",
  ], [
    "ForceFingerprint", "DisableOpenpilotLongitudinal", "EVTable", "LongPitch", "GasRegenCmd", "CrosstrekTorque", "LockDoors", "StockTune", "CydiaTune",
    "DragonPilotTune", "FrogsGoMooTune", "LockDoors", "SNGHack",
  ], [
    "AlertVolumeControl", "DisengageVolume", "EngageVolume", "PromptVolume", "PromptDistractedVolume", "RefuseVolume", "WarningSoftVolume",
    "WarningImmediateVolume", "CustomAlerts", "GreenLightAlert", "LeadDepartingAlert", "LoudBlindspotAlert", "SpeedLimitChangedAlert", "CustomUI",
    "Compass", "DeveloperUI", "ShowJerk", "LeadInfo", "ShowTuning", "UseSI", "FPSCounter", "CustomPaths", "AccelerationPath", "AdjacentPath", "BlindSpotPath",
    "AdjacentPathMetrics", "PedalsOnUI", "RoadNameUI", "WheelIcon", "RotatingWheel", "CustomTheme", "CustomColors", "CustomIcons", "CustomSignals", "CustomSounds",
    "GoatScream", "HolidayThemes", "RandomEvents", "ModelUI", "DynamicPathWidth", "HideLeadMarker", "LaneLinesWidth", "PathEdgeWidth", "PathWidth", "RoadEdgesWidth",
    "UnlimitedLength", "QOLVisuals", "BigMap", "FullMap", "CameraView", "DriverCamera", "HideSpeed", "HideSpeedUI", "MapStyle", "NumericalTemp", "Fahrenheit",
    "WheelSpeed", "ScreenManagement", "HideUIElements", "HideAlerts", "HideMapIcon", "HideMaxSpeed", "ScreenBrightness", "ScreenBrightnessOnroad", "ScreenRecorder",
    "ScreenTimeout", "ScreenTimeoutOnroad", "StandbyMode",
  ], [
    "AutomaticUpdates", "ShowCPU", "ShowGPU", "ShowIP", "ShowMemoryUsage", "ShowStorageLeft", "ShowStorageUsed", "Sidebar", "TetheringEnabled",
  ], [
    "FrogPilotDrives", "FrogPilotKilometers", "FrogPilotMinutes"
  ]

  control_params, vehicle_params, visual_params, other_params, tracking_params = map(lambda keys: get_frogpilot_params(params, keys), [control_keys, vehicle_keys, visual_keys, other_keys, tracking_keys])
  control_values, vehicle_values, visual_values, other_values, tracking_values = map(format_params, [control_params, vehicle_params, visual_params, other_params, tracking_params])
  control_chunks, vehicle_chunks, visual_chunks, other_chunks, tracking_chunks = map(lambda data: chunk_data(data, 50), [control_values, vehicle_values, visual_values, other_values, tracking_values])

  no_internet = 0
  while True:
    if sentry_pinged():
      for chunks, label in zip([control_chunks, vehicle_chunks, visual_chunks, other_chunks, tracking_chunks], ["FrogPilot Controls", "FrogPilot Vehicles", "FrogPilot Visuals", "Other Toggles", "FrogPilot Tracking"]):
        with sentry_sdk.configure_scope() as scope:
          set_sentry_scope(scope, chunks, label)
      if blocked:
        sentry_sdk.capture_message("Blocked user from using the development branch", level='error')
      else:
        sentry_sdk.capture_message("Fingerprinted %s" % candidate, level='info')
        params.put_bool("FingerprintLogged", True)
      sentry_sdk.flush()
      break
    else:
      if no_internet > 5:
        break
      no_internet += 1
      time.sleep(600)


def capture_exception(*args, **kwargs) -> None:
  save_exception(traceback.format_exc())
  cloudlog.error("crash", exc_info=kwargs.get('exc_info', 1))

  FrogPilot = "frogai" in get_origin().lower()
  if not FrogPilot or PC:
    return

  try:
    bind_user()
    sentry_sdk.capture_exception(*args, **kwargs)
    sentry_sdk.flush()  # https://github.com/getsentry/sentry-python/issues/291
  except Exception:
    cloudlog.exception("sentry exception")


def save_exception(exc_text: str) -> None:
  if not os.path.exists(CRASHES_DIR):
    os.makedirs(CRASHES_DIR)

  files = [
    os.path.join(CRASHES_DIR, datetime.now().strftime('%Y-%m-%d--%H-%M-%S.log')),
    os.path.join(CRASHES_DIR, 'error.txt')
  ]

  for file in files:
    with open(file, 'w') as f:
      if file.endswith("error.txt"):
        lines = exc_text.splitlines()[-10:]
        f.write("\n".join(lines))
      else:
        f.write(exc_text)

  print('Logged current crash to {}'.format(files))


def set_tag(key: str, value: str) -> None:
  sentry_sdk.set_tag(key, value)


def init(project: SentryProject) -> bool:
  params = Params()
  installed = params.get("InstallDate", encoding='utf-8')
  updated = params.get("Updated", encoding='utf-8')

  short_branch = get_short_branch()

  if short_branch == "FrogPilot-Development":
    env = "Development"
  elif short_branch in {"FrogPilot-Staging", "FrogPilot-Testing"}:
    env = "Staging"
  elif short_branch == "FrogPilot":
    env = "Release"
  else:
    env = short_branch

  integrations = []
  if project == SentryProject.SELFDRIVE:
    integrations.append(ThreadingIntegration(propagate_hub=True))

  sentry_sdk.init(project.value,
                  default_integrations=False,
                  release=get_version(),
                  integrations=integrations,
                  traces_sample_rate=1.0,
                  max_value_length=8192,
                  environment=env)

  sentry_sdk.set_user({"id": HARDWARE.get_serial()})
  sentry_sdk.set_tag("branch", short_branch)
  sentry_sdk.set_tag("commit", get_commit())
  sentry_sdk.set_tag("updated", updated)
  sentry_sdk.set_tag("installed", installed)
  sentry_sdk.set_tag("repo", get_origin())

  if project == SentryProject.SELFDRIVE:
    sentry_sdk.Hub.current.start_session()

  return True
