#!/usr/bin/env python3
import datetime
import os
import signal
import subprocess
import sys
import threading
import time
import traceback

from cereal import log
import cereal.messaging as messaging
import openpilot.system.sentry as sentry
from openpilot.common.params import Params, ParamKeyType
from openpilot.common.text_window import TextWindow
from openpilot.system.hardware import HARDWARE, PC
from openpilot.system.manager.helpers import unblock_stdout, write_onroad_params, save_bootlog
from openpilot.system.manager.process import ensure_running
from openpilot.system.manager.process_config import managed_processes
from openpilot.system.athena.registration import register, UNREGISTERED_DONGLE_ID
from openpilot.common.swaglog import cloudlog, add_file_handler
from openpilot.common.time import system_time_valid
from openpilot.system.version import get_build_metadata, terms_version, training_version

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import FrogPilotFunctions


def frogpilot_boot_functions(frogpilot_functions):
  while not system_time_valid():
    print("Waiting for system time to become valid...")
    time.sleep(1)

  try:
    frogpilot_functions.backup_frogpilot()
  except subprocess.CalledProcessError as e:
    print(f"Failed to backup FrogPilot. Error: {e}")
    return

  try:
    frogpilot_functions.backup_toggles()
  except subprocess.CalledProcessError as e:
    print(f"Failed to backup toggles. Error: {e}")
    return

def manager_init(frogpilot_functions) -> None:
  frogpilot_boot = threading.Thread(target=frogpilot_boot_functions, args=(frogpilot_functions,))
  frogpilot_boot.start()

  save_bootlog()

  build_metadata = get_build_metadata()

  params = Params()
  params_storage = Params("/persist/params")
  params_tracking = Params("/persist/tracking")
  params.clear_all(ParamKeyType.CLEAR_ON_MANAGER_START)
  params.clear_all(ParamKeyType.CLEAR_ON_ONROAD_TRANSITION)
  params.clear_all(ParamKeyType.CLEAR_ON_OFFROAD_TRANSITION)
  if build_metadata.release_channel:
    params.clear_all(ParamKeyType.DEVELOPMENT_ONLY)

  frogpilot_functions.convert_params(params, params_storage, params_tracking)

  default_params: list[tuple[str, str | bytes]] = [
    ("CarParamsPersistent", ""),
    ("CompletedTrainingVersion", "0"),
    ("DisengageOnAccelerator", "0"),
    ("ExperimentalLongitudinalEnabled", "0"),
    ("GithubSshKeys", ""),
    ("GithubUsername", ""),
    ("GsmApn", ""),
    ("GsmMetered", "1"),
    ("GsmRoaming", "1"),
    ("HasAcceptedTerms", "0"),
    ("IsLdwEnabled", "0"),
    ("IsMetric", "0"),
    ("LanguageSetting", "main_en"),
    ("NavSettingLeftSide", "0"),
    ("NavSettingTime24h", "0"),
    ("OpenpilotEnabledToggle", "1"),
    ("RecordFront", "0"),
    ("SshEnabled", "0"),
    ("LongitudinalPersonality", str(log.LongitudinalPersonality.standard)),

    # Default FrogPilot parameters
    ("AccelerationPath", "1"),
    ("AccelerationProfile", "3"),
    ("AdjacentPath", "0"),
    ("AdjacentPathMetrics", "0"),
    ("AggressiveAcceleration", "1"),
    ("AggressiveFollow", "1.25"),
    ("AggressiveJerkAcceleration", "50"),
    ("AggressiveJerkDanger", "100"),
    ("AggressiveJerkSpeed", "50"),
    ("AggressivePersonalityProfile", "1"),
    ("AlertVolumeControl", "0"),
    ("AlwaysOnLateral", "1"),
    ("AlwaysOnLateralLKAS", "0"),
    ("AlwaysOnLateralMain", "1"),
    ("AMapKey1", ""),
    ("AMapKey2", ""),
    ("AutomaticUpdates", "1"),
    ("BlindSpotMetrics", "0"),
    ("BlindSpotPath", "1"),
    ("BorderMetrics", "1"),
    ("CameraView", "2"),
    ("CarMake", ""),
    ("CarModel", ""),
    ("CECurves", "1"),
    ("CELead", "1"),
    ("CENavigation", "1"),
    ("CENavigationIntersections", "1"),
    ("CENavigationLead", "1"),
    ("CENavigationTurns", "1"),
    ("CESignal", "1"),
    ("CESlowerLead", "1"),
    ("CESpeed", "0"),
    ("CESpeedLead", "0"),
    ("CEStopLights", "1"),
    ("CEStopLightsLessSensitive", "0"),
    ("CEStoppedLead", "1"),
    ("ClusterOffset", "1.015"),
    ("Compass", "0"),
    ("ConditionalExperimental", "1"),
    ("CrosstrekTorque", "1"),
    ("CurveSensitivity", "100"),
    ("CustomAlerts", "1"),
    ("CustomColors", "1"),
    ("CustomCruise", "1"),
    ("CustomCruiseLong", "5"),
    ("CustomIcons", "1"),
    ("CustomPaths", "1"),
    ("CustomPersonalities", "0"),
    ("CustomSignals", "1"),
    ("CustomSounds", "1"),
    ("CustomTheme", "1"),
    ("CustomUI", "1"),
    ("CydiaTune", "0"),
    ("DecelerationProfile", "1"),
    ("DeveloperUI", "0"),
    ("DeviceManagement", "1"),
    ("DeviceShutdown", "9"),
    ("DisableMTSCSmoothing", "0"),
    ("DisableOnroadUploads", "0"),
    ("DisableOpenpilotLongitudinal", "0"),
    ("DisableVTSCSmoothing", "0"),
    ("DisengageVolume", "100"),
    ("DragonPilotTune", "0"),
    ("DriverCamera", "0"),
    ("DrivingPersonalities", "0"),
    ("DynamicPathWidth", "0"),
    ("DynamicPedalsOnUI", "1"),
    ("EngageVolume", "100"),
    ("ExperimentalModeActivation", "1"),
    ("ExperimentalModeViaDistance", "1"),
    ("ExperimentalModeViaLKAS", "1"),
    ("ExperimentalModeViaTap", "0"),
    ("Fahrenheit", "0"),
    ("ForceAutoTune", "1"),
    ("ForceFingerprint", "0"),
    ("ForceMPHDashboard", "0"),
    ("ForceStandstill", "0"),
    ("ForceStops", "0"),
    ("FPSCounter", "1"),
    ("FrogsGoMooTune", "1"),
    ("FullMap", "0"),
    ("GasRegenCmd", "1"),
    ("GMapKey", ""),
    ("GoatScream", "1"),
    ("GreenLightAlert", "0"),
    ("HideAlerts", "0"),
    ("HideAOLStatusBar", "0"),
    ("HideCEMStatusBar", "0"),
    ("HideLeadMarker", "0"),
    ("HideMapIcon", "0"),
    ("HideMaxSpeed", "0"),
    ("HideSpeed", "0"),
    ("HideSpeedUI", "0"),
    ("HideUIElements", "0"),
    ("HolidayThemes", "1"),
    ("IncreaseThermalLimits", "0"),
    ("JerkInfo", "1"),
    ("LaneChangeCustomizations", "1"),
    ("LaneChangeTime", "0"),
    ("LaneDetectionWidth", "90"),
    ("LaneLinesWidth", "4"),
    ("LateralMetrics", "1"),
    ("LateralTune", "1"),
    ("LeadDepartingAlert", "0"),
    ("LeadDetectionThreshold", "35"),
    ("LeadInfo", "1"),
    ("LockDoors", "1"),
    ("LongitudinalMetrics", "1"),
    ("LongitudinalTune", "1"),
    ("LongPitch", "1"),
    ("LoudBlindspotAlert", "0"),
    ("LowVoltageShutdown", "11.8"),
    ("MapAcceleration", "0"),
    ("MapDeceleration", "0"),
    ("MapsSelected", ""),
    ("MapboxPublicKey", ""),
    ("MapboxSecretKey", ""),
    ("MapStyle", "10"),
    ("MinimumLaneChangeSpeed", "20"),
    ("MTSCAggressiveness", "100"),
    ("MTSCCurvatureCheck", "0"),
    ("Model", DEFAULT_MODEL),
    ("ModelName", DEFAULT_MODEL_NAME),
    ("ModelSelector", "0"),
    ("ModelUI", "1"),
    ("MTSCEnabled", "1"),
    ("NNFF", "1"),
    ("NNFFLite", "1"),
    ("NoLogging", "0"),
    ("NoUploads", "0"),
    ("NudgelessLaneChange", "1"),
    ("NumericalTemp", "1"),
    ("OfflineMode", "1"),
    ("Offset1", "5"),
    ("Offset2", "5"),
    ("Offset3", "5"),
    ("Offset4", "10"),
    ("OneLaneChange", "1"),
    ("OnroadDistanceButton", "0"),
    ("PathEdgeWidth", "20"),
    ("PathWidth", "61"),
    ("PauseAOLOnBrake", "0"),
    ("PauseLateralOnSignal", "0"),
    ("PedalsOnUI", "1"),
    ("PreferredSchedule", "0"),
    ("PromptDistractedVolume", "100"),
    ("PromptVolume", "100"),
    ("QOLControls", "1"),
    ("QOLVisuals", "1"),
    ("RandomEvents", "0"),
    ("RefuseVolume", "100"),
    ("RelaxedFollow", "1.75"),
    ("RelaxedJerkAcceleration", "100"),
    ("RelaxedJerkDanger", "100"),
    ("RelaxedJerkSpeed", "100"),
    ("RelaxedPersonalityProfile", "1"),
    ("ReverseCruise", "0"),
    ("ReverseCruiseUI", "1"),
    ("RoadEdgesWidth", "2"),
    ("RoadNameUI", "1"),
    ("RotatingWheel", "1"),
    ("ScreenBrightness", "101"),
    ("ScreenBrightnessOnroad", "101"),
    ("ScreenManagement", "1"),
    ("ScreenRecorder", "1"),
    ("ScreenTimeout", "30"),
    ("ScreenTimeoutOnroad", "30"),
    ("SearchInput", "0"),
    ("SetSpeedLimit", "0"),
    ("SetSpeedOffset", "0"),
    ("ShowCPU", "1"),
    ("ShowGPU", "0"),
    ("ShowIP", "0"),
    ("ShowMemoryUsage", "1"),
    ("ShowSLCOffset", "1"),
    ("ShowSLCOffsetUI", "1"),
    ("ShowSteering", "1"),
    ("ShowStoppingPoint", "0"),
    ("ShowStorageLeft", "0"),
    ("ShowStorageUsed", "0"),
    ("Sidebar", "0"),
    ("SidebarMetrics", "1"),
    ("SignalMetrics", "0"),
    ("SLCConfirmation", "1"),
    ("SLCConfirmationLower", "1"),
    ("SLCConfirmationHigher", "1"),
    ("SLCFallback", "2"),
    ("SLCLookaheadHigher", "5"),
    ("SLCLookaheadLower", "5"),
    ("SLCOverride", "1"),
    ("SLCPriority1", "Dashboard"),
    ("SLCPriority2", "Offline Maps"),
    ("SLCPriority3", "Navigation"),
    ("SmoothBraking", "1"),
    ("SNGHack", "1"),
    ("SpeedLimitChangedAlert", "1"),
    ("SpeedLimitController", "1"),
    ("StandardFollow", "1.45"),
    ("StandardJerkAcceleration", "100"),
    ("StandardJerkDanger", "100"),
    ("StandardJerkSpeed", "100"),
    ("StandardPersonalityProfile", "1"),
    ("StandbyMode", "0"),
    ("StaticPedalsOnUI", "0"),
    ("SteerRatio", ""),
    ("SteerRatioStock", ""),
    ("StockTune", "0"),
    ("StoppingDistance", "3"),
    ("TacoTune", "0"),
    ("ToyotaDoors", "0"),
    ("TrafficFollow", "0.5"),
    ("TrafficJerkAcceleration", "50"),
    ("TrafficJerkDanger", "100"),
    ("TrafficJerkSpeed", "50"),
    ("TrafficMode", "0"),
    ("TrafficPersonalityProfile", "1"),
    ("TuningInfo", "1"),
    ("TurnAggressiveness", "100"),
    ("TurnDesires", "0"),
    ("UnlimitedLength", "1"),
    ("UnlockDoors", "1"),
    ("UseSI", "1"),
    ("UseVienna", "0"),
    ("VisionTurnControl", "1"),
    ("WarningImmediateVolume", "100"),
    ("WarningSoftVolume", "100"),
    ("WheelIcon", "3"),
    ("WheelSpeed", "0")
  ]
  if not PC:
    default_params.append(("LastUpdateTime", datetime.datetime.utcnow().isoformat().encode('utf8')))

  if params.get_bool("RecordFrontLock"):
    params.put_bool("RecordFront", True)

  # set unset params
  for k, v in default_params:
    if params.get(k) is None:
      params.put(k, v)

  # Create folders needed for msgq
  try:
    os.mkdir("/dev/shm")
  except FileExistsError:
    pass
  except PermissionError:
    print("WARNING: failed to make /dev/shm")

  # set version params
  params.put("Version", build_metadata.openpilot.version)
  params.put("TermsVersion", terms_version)
  params.put("TrainingVersion", training_version)
  params.put("GitCommit", build_metadata.openpilot.git_commit)
  params.put("GitCommitDate", build_metadata.openpilot.git_commit_date)
  params.put("GitBranch", build_metadata.channel)
  params.put("GitRemote", build_metadata.openpilot.git_origin)
  params.put_bool("IsTestedBranch", build_metadata.tested_channel)
  params.put_bool("IsReleaseBranch", build_metadata.release_channel)

  # set dongle id
  reg_res = register(show_spinner=True)
  if reg_res:
    dongle_id = reg_res
  else:
    serial = params.get("HardwareSerial")
    raise Exception(f"Registration failed for device {serial}")
  os.environ['DONGLE_ID'] = dongle_id  # Needed for swaglog
  os.environ['GIT_ORIGIN'] = build_metadata.openpilot.git_normalized_origin # Needed for swaglog
  os.environ['GIT_BRANCH'] = build_metadata.channel # Needed for swaglog
  os.environ['GIT_COMMIT'] = build_metadata.openpilot.git_commit # Needed for swaglog

  if not build_metadata.openpilot.is_dirty:
    os.environ['CLEAN'] = '1'

  # init logging
  sentry.init(sentry.SentryProject.SELFDRIVE)
  cloudlog.bind_global(dongle_id=dongle_id,
                       version=build_metadata.openpilot.version,
                       origin=build_metadata.openpilot.git_normalized_origin,
                       branch=build_metadata.channel,
                       commit=build_metadata.openpilot.git_commit,
                       dirty=build_metadata.openpilot.is_dirty,
                       device=HARDWARE.get_device_type())

  # preimport all processes
  for p in managed_processes.values():
    p.prepare()


def manager_cleanup() -> None:
  # send signals to kill all procs
  for p in managed_processes.values():
    p.stop(block=False)

  # ensure all are killed
  for p in managed_processes.values():
    p.stop(block=True)

  cloudlog.info("everything is dead")


def manager_thread() -> None:
  cloudlog.bind(daemon="manager")
  cloudlog.info("manager start")
  cloudlog.info({"environ": os.environ})

  params = Params()
  params_memory = Params("/dev/shm/params")

  ignore: list[str] = []
  if params.get("DongleId", encoding='utf8') in (None, UNREGISTERED_DONGLE_ID):
    ignore += ["manage_athenad", "uploader"]
  if os.getenv("NOBOARD") is not None:
    ignore.append("pandad")
  ignore += [x for x in os.getenv("BLOCK", "").split(",") if len(x) > 0]

  sm = messaging.SubMaster(['deviceState', 'carParams'], poll='deviceState')
  pm = messaging.PubMaster(['managerState'])

  write_onroad_params(False, params)
  ensure_running(managed_processes.values(), False, params=params, CP=sm['carParams'], not_run=ignore)

  started_prev = False

  while True:
    sm.update(1000)

    started = sm['deviceState'].started

    if started and not started_prev:
      params.clear_all(ParamKeyType.CLEAR_ON_ONROAD_TRANSITION)

      error_log = os.path.join(sentry.CRASHES_DIR, 'error.txt')
      if os.path.isfile(error_log):
        os.remove(error_log)

    elif not started and started_prev:
      params.clear_all(ParamKeyType.CLEAR_ON_OFFROAD_TRANSITION)
      params_memory.clear_all(ParamKeyType.CLEAR_ON_OFFROAD_TRANSITION)

    # update onroad params, which drives pandad's safety setter thread
    if started != started_prev:
      write_onroad_params(started, params)

    started_prev = started

    ensure_running(managed_processes.values(), started, params=params, CP=sm['carParams'], not_run=ignore)

    running = ' '.join("{}{}\u001b[0m".format("\u001b[32m" if p.proc.is_alive() else "\u001b[31m", p.name)
                       for p in managed_processes.values() if p.proc)
    print(running)
    cloudlog.debug(running)

    # send managerState
    msg = messaging.new_message('managerState', valid=True)
    msg.managerState.processes = [p.get_process_state_msg() for p in managed_processes.values()]
    pm.send('managerState', msg)

    # Exit main loop when uninstall/shutdown/reboot is needed
    shutdown = False
    for param in ("DoUninstall", "DoShutdown", "DoReboot"):
      if params.get_bool(param):
        shutdown = True
        params.put("LastManagerExitReason", f"{param} {datetime.datetime.now()}")
        cloudlog.warning(f"Shutting down manager - {param} set")

    if shutdown:
      break


def main(frogpilot_functions) -> None:
  frogpilot_functions.setup_frogpilot()

  manager_init(frogpilot_functions)
  if os.getenv("PREPAREONLY") is not None:
    return

  # SystemExit on sigterm
  signal.signal(signal.SIGTERM, lambda signum, frame: sys.exit(1))

  try:
    manager_thread()
  except Exception:
    traceback.print_exc()
    sentry.capture_exception()
  finally:
    manager_cleanup()

  params = Params()
  if params.get_bool("DoUninstall"):
    cloudlog.warning("uninstalling")
    frogpilot_functions.uninstall_frogpilot()
  elif params.get_bool("DoReboot"):
    cloudlog.warning("reboot")
    HARDWARE.reboot()
  elif params.get_bool("DoShutdown"):
    cloudlog.warning("shutdown")
    HARDWARE.shutdown()


if __name__ == "__main__":
  unblock_stdout()

  try:
    main(FrogPilotFunctions())
  except KeyboardInterrupt:
    print("got CTRL-C, exiting")
  except Exception:
    add_file_handler(cloudlog)
    cloudlog.exception("Manager failed to start")

    try:
      managed_processes['ui'].stop()
    except Exception:
      pass

    # Show last 3 lines of traceback
    error = traceback.format_exc(-3)
    error = "Manager failed to start\n\n" + error
    with TextWindow(error) as t:
      t.wait_for_exit()

    raise

  # manual exit because we are forked
  sys.exit(0)
