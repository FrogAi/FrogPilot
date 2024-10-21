import datetime
import os
import threading
import time

import openpilot.system.sentry as sentry

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, config_realtime_process
from openpilot.common.time import system_time_valid
from openpilot.system.hardware import HARDWARE

from openpilot.selfdrive.frogpilot.assets.model_manager import ModelManager
from openpilot.selfdrive.frogpilot.assets.theme_manager import ThemeManager
from openpilot.selfdrive.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_tracking import FrogPilotTracking
from openpilot.selfdrive.frogpilot.frogpilot_functions import backup_toggles
from openpilot.selfdrive.frogpilot.frogpilot_utilities import is_url_pingable
from openpilot.selfdrive.frogpilot.frogpilot_variables import FrogPilotVariables, get_frogpilot_toggles, params, params_memory

locks = {
  "backup_toggles": threading.Lock(),
  "download_all_models": threading.Lock(),
  "download_model": threading.Lock(),
  "download_theme": threading.Lock(),
  "update_active_theme": threading.Lock(),
  "update_checks": threading.Lock(),
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


def automatic_update_check(frogs_go_moo, started):
  update_available = params.get_bool("UpdaterFetchAvailable")
  update_ready = params.get_bool("UpdateAvailable")
  update_state_idle = params.get("UpdaterState", encoding='utf8') == "idle"

  if update_ready and not started:
    if not frogs_go_moo:
      os.system("pkill -SIGUSR1 -f system.updated.updated")
      time.sleep(30)
      if params.get_bool("UpdaterFetchAvailable"):
        os.system("pkill -SIGHUP -f system.updated.updated")
      time.sleep(300)
    HARDWARE.reboot()
  elif update_available:
    os.system("pkill -SIGUSR1 -f system.updated.updated")
    time.sleep(30)
    os.system("pkill -SIGHUP -f system.updated.updated")
  elif update_state_idle:
    os.system("pkill -SIGUSR1 -f system.updated.updated")


def check_assets(model_manager, theme_manager):
  if params_memory.get_bool("DownloadAllModels"):
    run_thread_with_lock("download_all_models", model_manager.download_all_models)

  model_to_download = params_memory.get("ModelToDownload", encoding='utf-8')
  if model_to_download is not None:
    run_thread_with_lock("download_model", model_manager.download_model, (model_to_download,))

  if params_memory.get_bool("UpdateTheme"):
    run_thread_with_lock("update_active_theme", theme_manager.update_active_theme)
    params_memory.remove("UpdateTheme");

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


def update_checks(automatic_updates, frogs_go_moo, model_manager, now, screen_off, started, theme_manager, time_validated):
  if not (is_url_pingable("https://github.com") or is_url_pingable("https://gitlab.com")):
    return

  if automatic_updates and (screen_off or frogs_go_moo):
    automatic_update_check(frogs_go_moo, started)

  if time_validated:
    update_maps(now)

  run_thread_with_lock("update_models", model_manager.update_models)


def update_maps(now):
  maps_selected = params.get("MapsSelected", encoding='utf8')
  if maps_selected is None:
    return

  day = now.day
  is_first = day == 1
  is_Sunday = now.weekday() == 6
  schedule = params.get_int("PreferredSchedule")

  maps_downloaded = os.path.exists('/data/media/0/osm/offline')
  if maps_downloaded and (schedule == 0 or (schedule == 1 and not is_Sunday) or (schedule == 2 and not is_first)):
    return

  suffix = "th" if 4 <= day <= 20 or 24 <= day <= 30 else ["st", "nd", "rd"][day % 10 - 1]
  todays_date = now.strftime(f"%B {day}{suffix}, %Y")

  if params.get("LastMapsUpdate", encoding='utf-8') == todays_date:
    return

  if params.get("OSMDownloadProgress", encoding='utf-8') is None:
    params_memory.put("OSMDownloadLocations", maps_selected)
    params.put_nonblocking("LastMapsUpdate", todays_date)


def frogpilot_thread():
  config_realtime_process(5, Priority.CTRL_LOW)

  params_storage = Params("/persist/params")

  frogpilot_planner = FrogPilotPlanner()
  frogpilot_tracking = FrogPilotTracking()
  frogpilot_variables = FrogPilotVariables()
  model_manager = ModelManager()
  theme_manager = ThemeManager()

  run_update_checks = False
  started_previously = False
  time_validated = False
  toggles_updated = False

  frogpilot_toggles = get_frogpilot_toggles()

  radarless_model = frogpilot_toggles.radarless_model

  toggles_last_updated = None

  error_log = os.path.join(sentry.CRASHES_DIR, 'error.txt')
  if os.path.isfile(error_log):
    os.remove(error_log)

  pm = messaging.PubMaster(['frogpilotPlan'])
  sm = messaging.SubMaster(['carState', 'controlsState', 'deviceState', 'modelV2', 'radarState',
                            'frogpilotCarControl', 'frogpilotCarState', 'frogpilotNavigation'],
                            poll='modelV2', ignore_avg_freq=['radarState'])

  while True:
    sm.update()

    now = datetime.datetime.now()
    deviceState = sm['deviceState']
    screen_off = deviceState.screenBrightnessPercent == 0
    started = deviceState.started

    if params_memory.get_bool("FrogPilotTogglesUpdated"):
      frogpilot_variables.update(started)
      frogpilot_toggles = get_frogpilot_toggles()
      if time_validated:
        run_thread_with_lock("backup_toggles", backup_toggles, (params, params_storage))
      toggles_last_updated = now

    if toggles_last_updated and (now - toggles_last_updated).total_seconds() <= 1:
      toggles_updated = True
    else:
      toggles_last_updated = None
      toggles_updated = False

    if not started and started_previously:
      frogpilot_planner = FrogPilotPlanner()
      frogpilot_tracking = FrogPilotTracking()
    elif started and not started_previously:
      radarless_model = frogpilot_toggles.radarless_model

      if os.path.isfile(error_log):
        os.remove(error_log)

    if started and sm.updated['modelV2']:
      frogpilot_planner.update(sm['carState'], sm['controlsState'], sm['frogpilotCarControl'], sm['frogpilotCarState'],
                               sm['frogpilotNavigation'], sm['modelV2'], radarless_model, sm['radarState'], frogpilot_toggles)
      frogpilot_planner.publish(sm, pm, frogpilot_toggles, toggles_updated)

      frogpilot_tracking.update(sm['carState'], sm['controlsState'], sm['frogpilotCarControl'])
    elif not started:
      frogpilot_plan_send = messaging.new_message('frogpilotPlan')
      frogpilot_plan_send.frogpilotPlan.togglesUpdated = toggles_updated
      pm.send('frogpilotPlan', frogpilot_plan_send)

    started_previously = started

    if now.second % 2 == 0:
      check_assets(model_manager, theme_manager)

    if params_memory.get_bool("ManualUpdateInitiated"):
      run_thread_with_lock("update_checks", update_checks, (False, frogpilot_toggles.frogs_go_moo, model_manager, now, screen_off, started, theme_manager, time_validated))
      run_thread_with_lock("update_themes", theme_manager.update_themes())
    elif now.second == 0:
      run_update_checks = not screen_off and not started or now.minute % 15 == 0 or frogpilot_toggles.frogs_go_moo
    elif run_update_checks:
      run_thread_with_lock("update_checks", update_checks, (frogpilot_toggles.automatic_updates, frogpilot_toggles.frogs_go_moo, model_manager, now, screen_off, started, theme_manager, time_validated))

      if time_validated:
        theme_manager.update_holiday()

      run_update_checks = False

    elif not time_validated:
      time_validated = system_time_valid()
      if not time_validated:
        continue
      theme_manager.update_holiday()
      run_thread_with_lock("update_models", model_manager.update_models, (True,))
      run_thread_with_lock("update_themes", theme_manager.update_themes, (True,))

def main():
  frogpilot_thread()


if __name__ == "__main__":
  main()
