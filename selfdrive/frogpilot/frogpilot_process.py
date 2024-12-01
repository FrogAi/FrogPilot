import datetime
import subprocess
import threading
import time

import openpilot.system.sentry as sentry

from pathlib import Path

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, config_realtime_process
from openpilot.common.time import system_time_valid
from openpilot.system.hardware import HARDWARE

from openpilot.selfdrive.frogpilot.assets.model_manager import ModelManager, MODEL_DOWNLOAD_PARAM
from openpilot.selfdrive.frogpilot.assets.theme_manager import ThemeManager
from openpilot.selfdrive.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_tracking import FrogPilotTracking
from openpilot.selfdrive.frogpilot.frogpilot_functions import backup_toggles
from openpilot.selfdrive.frogpilot.frogpilot_utilities import is_url_pingable
from openpilot.selfdrive.frogpilot.frogpilot_variables import FrogPilotVariables, get_frogpilot_toggles, params, params_memory
from openpilot.selfdrive.frogpilot.navigation.mapd import update_mapd

locks = {
  "backup_toggles": threading.Lock(),
  "download_all_models": threading.Lock(),
  "download_model": threading.Lock(),
  "download_theme": threading.Lock(),
  "update_checks": threading.Lock(),
  "update_mapd": threading.Lock(),
  "update_models": threading.Lock(),
  "update_themes": threading.Lock()
}

running_threads = {}

def run_thread_with_lock(name, target, args=()):
  if not running_threads.get(name, threading.Thread()).is_alive():
    with locks[name]:
      thread = threading.Thread(target=target, args=args)
      thread.start()
      running_threads[name] = thread

def automatic_update_check():
  subprocess.run(["pkill", "-SIGUSR1", "-f", "system.updated.updated"], check=False)
  while params.get("UpdaterState", encoding="utf8") != "idle":
    time.sleep(60)

  if not params.get_bool("UpdaterFetchAvailable"):
    return

  subprocess.run(["pkill", "-SIGHUP", "-f", "system.updated.updated"], check=False)
  while not params.get_bool("UpdateAvailable"):
    time.sleep(60)

  while params.get_bool("IsOnroad"):
    time.sleep(60)

  HARDWARE.reboot()

def check_assets(model_manager, theme_manager, frogpilot_toggles):
  if params_memory.get_bool("DownloadAllModels"):
    run_thread_with_lock("download_all_models", model_manager.download_all_models)

  model_to_download = params_memory.get(MODEL_DOWNLOAD_PARAM, encoding='utf-8')
  if model_to_download is not None:
    run_thread_with_lock("download_model", model_manager.download_model, (model_to_download,))

  assets = [
    ("ColorToDownload", "colors"),
    ("DistanceIconToDownload", "distance_icons"),
    ("IconToDownload", "icons"),
    ("SignalToDownload", "signals"),
    ("SoundToDownload", "sounds"),
    ("WheelToDownload", "steering_wheels")
  ]

  for param, asset_type in assets:
    asset_to_download = params_memory.get(param, encoding='utf-8')
    if asset_to_download is not None:
      run_thread_with_lock("download_theme", theme_manager.download_theme, (asset_type, asset_to_download, param))

def update_checks(model_manager, now, theme_manager, frogpilot_toggles):
  if not (is_url_pingable("https://github.com") or is_url_pingable("https://gitlab.com")):
    return

  if frogpilot_toggles.automatic_updates and not params_memory.get_bool("ManualUpdateInitiated"):
    automatic_update_check()

  update_maps(now)

  run_thread_with_lock("update_mapd", update_mapd)
  run_thread_with_lock("update_models", model_manager.update_models)
  run_thread_with_lock("update_themes", theme_manager.update_themes, (frogpilot_toggles,))

def update_maps(now):
  maps_selected = params.get("MapsSelected", encoding='utf8')
  if maps_selected is None:
    return

  day = now.day
  is_first = day == 1
  is_Sunday = now.weekday() == 6
  schedule = params.get_int("PreferredSchedule")

  maps_downloaded = Path("/data/media/0/osm/offline").exists()
  if maps_downloaded and (schedule == 0 or (schedule == 1 and not is_Sunday) or (schedule == 2 and not is_first)):
    return

  suffix = "th" if 4 <= day <= 20 or 24 <= day <= 30 else ["st", "nd", "rd"][day % 10 - 1]
  todays_date = now.strftime(f"%B {day}{suffix}, %Y")

  if params.get("LastMapsUpdate", encoding='utf-8') == todays_date:
    return

  if params.get("OSMDownloadProgress", encoding='utf-8') is None:
    params_memory.put("OSMDownloadLocations", maps_selected)
    params.put("LastMapsUpdate", todays_date)

def frogpilot_thread():
  config_realtime_process(5, Priority.CTRL_LOW)

  error_log = Path(sentry.CRASHES_DIR) / "error.txt"
  if error_log.is_file():
    error_log.unlink()

  params_storage = Params("/persist/params")

  frogpilot_planner = FrogPilotPlanner(error_log)
  frogpilot_tracking = FrogPilotTracking()
  frogpilot_variables = FrogPilotVariables()
  model_manager = ModelManager()
  theme_manager = ThemeManager()

  assets_checked = False
  run_update_checks = False
  started_previously = False
  theme_updated = False
  time_validated = False
  toggles_updated = False

  frogpilot_toggles = get_frogpilot_toggles()

  toggles_last_updated = datetime.datetime.now()

  pm = messaging.PubMaster(['frogpilotPlan'])
  sm = messaging.SubMaster(['carControl', 'carState', 'controlsState', 'deviceState', 'modelV2', 'radarState',
                            'frogpilotCarControl', 'frogpilotCarState', 'frogpilotNavigation'],
                            poll='modelV2', ignore_avg_freq=['radarState'])

  while True:
    sm.update()

    now = datetime.datetime.now()

    started = sm['deviceState'].started

    if params_memory.get_bool("FrogPilotTogglesUpdated") or theme_updated:
      theme_updated = theme_manager.update_active_theme(time_validated, frogpilot_toggles)

      frogpilot_variables.update(theme_manager.theme_assets["holiday_theme"], started)
      frogpilot_toggles = get_frogpilot_toggles()

      if time_validated:
        run_thread_with_lock("backup_toggles", backup_toggles, (params_storage,))

      toggles_last_updated = now
    toggles_updated = (now - toggles_last_updated).total_seconds() <= 1

    if not started and started_previously:
      frogpilot_planner = FrogPilotPlanner(error_log)
      frogpilot_tracking = FrogPilotTracking()

      run_update_checks = True

      frogpilot_variables.update(theme_manager.theme_assets["holiday_theme"], started)
      frogpilot_toggles = get_frogpilot_toggles()
    elif started and not started_previously:
      radarless_model = frogpilot_toggles.radarless_model

      if error_log.is_file():
        error_log.unlink()

    if started and sm.updated['modelV2']:
      frogpilot_planner.update(sm['carControl'], sm['carState'], sm['controlsState'], sm['frogpilotCarControl'], sm['frogpilotCarState'],
                               sm['frogpilotNavigation'], sm['modelV2'], radarless_model, sm['radarState'], frogpilot_toggles)
      frogpilot_planner.publish(sm, pm, theme_updated, toggles_updated, frogpilot_toggles)

      frogpilot_tracking.update(sm['carState'], sm['controlsState'], sm['frogpilotCarControl'])
    elif toggles_updated:
      frogpilot_plan_send = messaging.new_message('frogpilotPlan')
      frogpilot_plan_send.frogpilotPlan.themeUpdated = theme_updated
      frogpilot_plan_send.frogpilotPlan.togglesUpdated = toggles_updated
      pm.send('frogpilotPlan', frogpilot_plan_send)

    started_previously = started

    if now.second % 2 == 0:
      if not assets_checked:
        check_assets(model_manager, theme_manager, frogpilot_toggles)

        assets_checked = True
    else:
      assets_checked = False

    run_update_checks |= params_memory.get_bool("ManualUpdateInitiated")
    run_update_checks |= now.second == 0 and (now.minute % 60 == 0 or frogpilot_toggles.frogs_go_moo)
    run_update_checks &= time_validated

    if run_update_checks:
      theme_updated = theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      run_thread_with_lock("update_checks", update_checks, (model_manager, now, theme_manager, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()
      if not time_validated:
        continue

      theme_updated = theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      run_thread_with_lock("update_models", model_manager.update_models, (True,))
      run_thread_with_lock("update_themes", theme_manager.update_themes, (frogpilot_toggles, True,))

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
