#!/usr/bin/env python3
import datetime
import os
import signal
import subprocess
import sys
import threading
import traceback
from pathlib import Path
from typing import List, Tuple, Union

from cereal import log
import cereal.messaging as messaging
import openpilot.selfdrive.sentry as sentry
from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params, ParamKeyType
from openpilot.common.text_window import TextWindow
from openpilot.system.hardware import HARDWARE, PC
from openpilot.selfdrive.manager.helpers import unblock_stdout, write_onroad_params, save_bootlog
from openpilot.selfdrive.manager.process import ensure_running
from openpilot.selfdrive.manager.process_config import managed_processes
from openpilot.selfdrive.athena.registration import register, UNREGISTERED_DONGLE_ID
from openpilot.common.swaglog import cloudlog, add_file_handler
from openpilot.system.version import is_dirty, get_commit, get_version, get_origin, get_short_branch, \
                           get_normalized_origin, terms_version, training_version, \
                           is_tested_branch, is_release_branch, get_commit_date

from openpilot.selfdrive.frogpilot.functions.frogpilot_functions import DEFAULT_MODEL


def manager_init() -> None:
  save_bootlog()

  # Clear the error log on boot to prevent old errors from hanging around
  if os.path.isfile(os.path.join(sentry.CRASHES_DIR, 'error.txt')):
    os.remove(os.path.join(sentry.CRASHES_DIR, 'error.txt'))

  params = Params()
  params_storage = Params("/persist/comma/params")
  params.clear_all(ParamKeyType.CLEAR_ON_MANAGER_START)
  params.clear_all(ParamKeyType.CLEAR_ON_ONROAD_TRANSITION)
  params.clear_all(ParamKeyType.CLEAR_ON_OFFROAD_TRANSITION)
  if is_release_branch():
    params.clear_all(ParamKeyType.DEVELOPMENT_ONLY)

  ############### Remove this after the April 26th update ###############

  previous_speed_limit = params.get_float("PreviousSpeedLimit")
  if previous_speed_limit >= 50:
    params.put_float("PreviousSpeedLimit", previous_speed_limit / 100)

  for priority_key in ["SLCPriority", "SLCPriority1", "SLCPriority2", "SLCPriority3"]:
    priority_value = params.get(priority_key)
    if isinstance(priority_value, int):
      params.remove(priority_key)

  attributes = ["AggressiveFollow", "StandardFollow", "RelaxedFollow", "AggressiveJerk", "StandardJerk", "RelaxedJerk"]
  values = {attr: params.get_float(attr) for attr in attributes}
  if any(value > 5 for value in values.values()):
    for attr, value in values.items():
      params.put_float(attr, value / 10)

  if params.get_bool("SilentMode"):
    attributes = ["DisengageVolume", "EngageVolume", "PromptVolume", "PromptDistractedVolume", "RefuseVolume", "WarningSoftVolume", "WarningImmediateVolume"]
    for attr in attributes:
      params.put_float(attr, 0)
    params.put_bool("SilentMode", False)

  #######################################################################

  # Check if the currently selected model still exists
  current_model = params.get("Model", encoding='utf-8')

  if current_model != DEFAULT_MODEL:
    models_folder = os.path.join(BASEDIR, 'selfdrive/modeld/models/models')
    model_exists = current_model in [os.path.splitext(file)[0] for file in os.listdir(models_folder)]

    if not model_exists:
      params.remove("Model")

  default_params: List[Tuple[str, Union[str, bytes]]] = [
    ("CompletedTrainingVersion", "0"),
    ("DisengageOnAccelerator", "0"),
    ("GsmMetered", "1"),
    ("HasAcceptedTerms", "0"),
    ("LanguageSetting", "main_en"),
    ("OpenpilotEnabledToggle", "1"),
    ("UpdaterAvailableBranches", ""),
    ("LongitudinalPersonality", str(log.LongitudinalPersonality.standard)),

    # Default FrogPilot parameters
    ("AccelerationPath", "1"),
    ("AccelerationProfile", "2"),
    ("AdjacentPath", "0"),
    ("AdjacentPathMetrics", "0"),
    ("AdjustablePersonalities", "1"),
    ("AggressiveAcceleration", "1"),
    ("AggressiveFollow", "1.25"),
    ("AggressiveJerk", "0.5"),
    ("AlertVolumeControl", "0"),
    ("AlwaysOnLateral", "1"),
    ("AlwaysOnLateralMain", "0"),
    ("BlindSpotPath", "1"),
    ("CameraView", "1"),
    ("CarMake", ""),
    ("CarModel", ""),
    ("CECurves", "1"),
    ("CENavigation", "1"),
    ("CENavigationIntersections", "1"),
    ("CENavigationLead", "1"),
    ("CENavigationTurns", "1"),
    ("CESignal", "1"),
    ("CESlowerLead", "0"),
    ("CESpeed", "0"),
    ("CESpeedLead", "0"),
    ("CEStopLights", "1"),
    ("CEStopLightsLead", "0"),
    ("Compass", "0"),
    ("ConditionalExperimental", "1"),
    ("CrosstrekTorque", "1"),
    ("CurveSensitivity", "100"),
    ("CustomAlerts", "1"),
    ("CustomColors", "1"),
    ("CustomIcons", "1"),
    ("CustomPersonalities", "1"),
    ("CustomSignals", "1"),
    ("CustomSounds", "1"),
    ("CustomTheme", "1"),
    ("CustomUI", "1"),
    ("CydiaTune", "0"),
    ("DecelerationProfile", "1"),
    ("DeviceShutdown", "9"),
    ("DisableMTSCSmoothing", "0"),
    ("DisableOnroadUploads", "0"),
    ("DisableOpenpilotLongitudinal", "0"),
    ("DisableVTSCSmoothing", "0"),
    ("DisengageVolume", "100"),
    ("DragonPilotTune", "0"),
    ("DriverCamera", "0"),
    ("DriveStats", "1"),
    ("DynamicPathWidth", "0"),
    ("EngageVolume", "100"),
    ("EVTable", "1"),
    ("ExperimentalModeActivation", "1"),
    ("ExperimentalModeViaDistance", "0"),
    ("ExperimentalModeViaLKAS", "0"),
    ("ExperimentalModeViaScreen", "1"),
    ("Fahrenheit", "0"),
    ("FireTheBabysitter", "0"),
    ("ForceAutoTune", "0"),
    ("ForceFingerprint", "0"),
    ("ForceMPHDashboard", "0"),
    ("FPSCounter", "0"),
    ("FrogPilotDrives", "0"),
    ("FrogPilotKilometers", "0"),
    ("FrogPilotMinutes", "0"),
    ("FrogsGoMooTune", "1"),
    ("FullMap", "0"),
    ("GasRegenCmd", "0"),
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
    ("HigherBitrate", "0"),
    ("HolidayThemes", "1"),
    ("LaneChangeTime", "0"),
    ("LaneDetection", "1"),
    ("LaneDetectionWidth", "60"),
    ("LaneLinesWidth", "4"),
    ("LateralTune", "1"),
    ("LeadDepartingAlert", "0"),
    ("LeadDetectionThreshold", "25"),
    ("LeadInfo", "0"),
    ("LockDoors", "0"),
    ("LongitudinalTune", "1"),
    ("LongPitch", "1"),
    ("LoudBlindspotAlert", "0"),
    ("LowerVolt", "1"),
    ("MapsSelected", ""),
    ("MapStyle", "0"),
    ("MTSCAggressiveness", "100"),
    ("MTSCCurvatureCheck", "0"),
    ("MTSCLimit", "0"),
    ("Model", DEFAULT_MODEL),
    ("ModelUI", "1"),
    ("MTSCEnabled", "1"),
    ("MuteOverheated", "0"),
    ("NavChill", "0"),
    ("NNFF", "1"),
    ("NoLogging", "0"),
    ("NoUploads", "0"),
    ("NudgelessLaneChange", "1"),
    ("NumericalTemp", "0"),
    ("OfflineMode", "0"),
    ("Offset1", "5"),
    ("Offset2", "5"),
    ("Offset3", "5"),
    ("Offset4", "10"),
    ("OneLaneChange", "1"),
    ("PathEdgeWidth", "20"),
    ("PathWidth", "61"),
    ("PauseLateralOnSignal", "0"),
    ("PedalsOnUI", "1"),
    ("PersonalitiesViaScreen", "1"),
    ("PersonalitiesViaWheel", "1"),
    ("PreferredSchedule", "0"),
    ("PromptVolume", "100"),
    ("PromptDistractedVolume", "100"),
    ("QOLControls", "1"),
    ("QOLVisuals", "1"),
    ("RandomEvents", "0"),
    ("RefuseVolume", "100"),
    ("RelaxedFollow", "1.75"),
    ("RelaxedJerk", "1.0"),
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
    ("ShowCPU", "0"),
    ("ShowGPU", "0"),
    ("ShowIP", "0"),
    ("ShowMemoryUsage", "0"),
    ("ShowSLCOffset", "0"),
    ("ShowSLCOffsetUI", "1"),
    ("ShowStorageLeft", "0"),
    ("ShowStorageUsed", "0"),
    ("Sidebar", "0"),
    ("SLCConfirmation", "1"),
    ("SLCConfirmationLower", "1"),
    ("SLCConfirmationHigher", "1"),
    ("SLCFallback", "2"),
    ("SLCOverride", "1"),
    ("SLCPriority1", "Dashboard"),
    ("SLCPriority2", "Offline Maps"),
    ("SLCPriority3", "Navigation"),
    ("SmoothBraking", "1"),
    ("SNGHack", "1"),
    ("SpeedLimitChangedAlert", "1"),
    ("SpeedLimitController", "1"),
    ("StandardFollow", "1.45"),
    ("StandardJerk", "1.0"),
    ("StandbyMode", "0"),
    ("SteerRatio", "0"),
    ("StockTune", "0"),
    ("StoppingDistance", "0"),
    ("TurnAggressiveness", "100"),
    ("TurnDesires", "0"),
    ("UnlimitedLength", "1"),
    ("UpdateSchedule", "0"),
    ("UseLateralJerk", "0"),
    ("UseSI", "0"),
    ("UseVienna", "0"),
    ("VisionTurnControl", "1"),
    ("WarningSoftVolume", "100"),
    ("WarningImmediateVolume", "100"),
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
      if params_storage.get(k) is None:
        params.put(k, v)
      else:
        params.put(k, params_storage.get(k))
    else:
      params_storage.put(k, params.get(k))

  # Create folders needed for msgq
  try:
    os.mkdir("/dev/shm")
  except FileExistsError:
    pass
  except PermissionError:
    print("WARNING: failed to make /dev/shm")

  # set version params
  params.put("Version", get_version())
  params.put("TermsVersion", terms_version)
  params.put("TrainingVersion", training_version)
  params.put("GitCommit", get_commit())
  params.put("GitCommitDate", get_commit_date())
  params.put("GitBranch", get_short_branch())
  params.put("GitRemote", get_origin())
  params.put_bool("IsTestedBranch", is_tested_branch())
  params.put_bool("IsReleaseBranch", is_release_branch())

  # set dongle id
  reg_res = register(show_spinner=True)
  if reg_res:
    dongle_id = reg_res
  else:
    serial = params.get("HardwareSerial")
    raise Exception(f"Registration failed for device {serial}")
  os.environ['DONGLE_ID'] = dongle_id  # Needed for swaglog
  os.environ['GIT_ORIGIN'] = get_normalized_origin() # Needed for swaglog
  os.environ['GIT_BRANCH'] = get_short_branch() # Needed for swaglog
  os.environ['GIT_COMMIT'] = get_commit() # Needed for swaglog

  if not is_dirty():
    os.environ['CLEAN'] = '1'

  # init logging
  sentry.init(sentry.SentryProject.SELFDRIVE)
  cloudlog.bind_global(dongle_id=dongle_id,
                       version=get_version(),
                       origin=get_normalized_origin(),
                       branch=get_short_branch(),
                       commit=get_commit(),
                       dirty=is_dirty(),
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


def update_frogpilot_params(params, params_memory):
  keys = ["DisableOnroadUploads", "FireTheBabysitter", "NoLogging", "NoUploads", "RoadNameUI"]
  for key in keys:
    params_memory.put_bool(key, params.get_bool(key))

def manager_thread() -> None:
  cloudlog.bind(daemon="manager")
  cloudlog.info("manager start")
  cloudlog.info({"environ": os.environ})

  params = Params()
  params_memory = Params("/dev/shm/params")
  params_storage = Params("/persist/comma/params")

  update_frogpilot_params(params, params_memory)

  ignore: List[str] = []
  if params.get("DongleId", encoding='utf8') in (None, UNREGISTERED_DONGLE_ID):
    ignore += ["manage_athenad", "uploader"]
  if os.getenv("NOBOARD") is not None:
    ignore.append("pandad")
  ignore += [x for x in os.getenv("BLOCK", "").split(",") if len(x) > 0]

  sm = messaging.SubMaster(['deviceState', 'carParams'], poll='deviceState')
  pm = messaging.PubMaster(['managerState'])

  write_onroad_params(False, params)
  ensure_running(managed_processes.values(), False, params=params, params_memory=params_memory, CP=sm['carParams'], not_run=ignore)

  started_prev = False

  while True:
    sm.update(1000)

    started = sm['deviceState'].started

    if started and not started_prev:
      params.clear_all(ParamKeyType.CLEAR_ON_ONROAD_TRANSITION)
    elif not started and started_prev:
      params.clear_all(ParamKeyType.CLEAR_ON_OFFROAD_TRANSITION)

      # Clear the error log on offroad transition to prevent old errors from hanging around
      if os.path.isfile(os.path.join(sentry.CRASHES_DIR, 'error.txt')):
        os.remove(os.path.join(sentry.CRASHES_DIR, 'error.txt'))

    # update onroad params, which drives boardd's safety setter thread
    if started != started_prev:
      write_onroad_params(started, params)

    started_prev = started

    ensure_running(managed_processes.values(), started, params=params, params_memory=params_memory, CP=sm['carParams'], not_run=ignore)

    running = ' '.join("%s%s\u001b[0m" % ("\u001b[32m" if p.proc.is_alive() else "\u001b[31m", p.name)
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

    if params_memory.get_bool("FrogPilotTogglesUpdated"):
      update_frogpilot_params(params, params_memory)

def backup_openpilot():
  # Configure the auto backup generator
  backup_dir_path = '/data/backups'
  Path(backup_dir_path).mkdir(parents=True, exist_ok=True)

  # Sort backups by creation time and only keep the 5 newest auto generated ones
  auto_backups = sorted([f for f in os.listdir(backup_dir_path) if f.endswith("_auto")],
                        key=lambda x: os.path.getmtime(os.path.join(backup_dir_path, x)))
  for old_backup in auto_backups:
    subprocess.run(['sudo', 'rm', '-rf', os.path.join(backup_dir_path, old_backup)], check=True)
    print(f"Deleted oldest backup to maintain limit: {old_backup}")

  # Generate the backup folder name from the current git commit and branch name
  branch = get_short_branch()
  commit = get_commit_date()[12:-16]
  backup_folder_name = f"{branch}_{commit}_auto"

  # Check if the backup folder already exists
  backup_path = os.path.join(backup_dir_path, backup_folder_name)

  if os.path.exists(backup_path):
    print(f"Backup folder {backup_folder_name} already exists. Skipping backup.")
    return

  # Create the backup directory and copy openpilot to it
  Path(backup_path).mkdir(parents=True, exist_ok=True)
  subprocess.run(['sudo', 'cp', '-a', '/data/openpilot/.', backup_path + '/'], check=True)
  print(f"Successfully backed up openpilot to {backup_folder_name}.")

def main() -> None:
  # Create the long term param storage folder
  try:
    # Attempt to remount /persist as read-write
    subprocess.run(['sudo', 'mount', '-o', 'remount,rw', '/persist'], check=True)
    print("Successfully remounted /persist as read-write.")
  except subprocess.CalledProcessError as e:
    print(f"Failed to remount /persist. Error: {e}")

  # Backup the current version of openpilot
  try:
    backup_thread = threading.Thread(target=backup_openpilot)
    backup_thread.start()
  except subprocess.CalledProcessError as e:
    print(f"Failed to backup openpilot. Error: {e}")

  manager_init()
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
    HARDWARE.uninstall()
  elif params.get_bool("DoReboot"):
    cloudlog.warning("reboot")
    HARDWARE.reboot()
  elif params.get_bool("DoShutdown"):
    cloudlog.warning("shutdown")
    HARDWARE.shutdown()


if __name__ == "__main__":
  unblock_stdout()

  try:
    main()
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
