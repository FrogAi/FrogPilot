import datetime
import json
import os
import pickle
import threading
import time

from types import SimpleNamespace

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
from openpilot.selfdrive.frogpilot.frogpilot_variables import FrogPilotVariables

locks = {
  "backup_toggles": threading.Lock(),
  "download_all_models": threading.Lock(),
  "download_model": threading.Lock(),
  "download_theme": threading.Lock(),
  "toggle_updates": threading.Lock(),
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

def automatic_update_check(started, params):
  update_available = params.get_bool("UpdaterFetchAvailable")
  update_ready = params.get_bool("UpdateAvailable")
  update_state_idle = params.get("UpdaterState", encoding='utf8') == "idle"

  if update_ready and not started:
    os.system("pkill -SIGUSR1 -f system.updated.updated")
    time.sleep(30)
    os.system("pkill -SIGHUP -f system.updated.updated")
    time.sleep(300)
    HARDWARE.reboot()
  elif update_available:
    os.system("pkill -SIGUSR1 -f system.updated.updated")
    time.sleep(30)
    os.system("pkill -SIGHUP -f system.updated.updated")
  elif update_state_idle:
    os.system("pkill -SIGUSR1 -f system.updated.updated")

def check_assets(model_manager, theme_manager, params, params_memory):
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

def toggle_updates(frogpilot_variables, started, time_validated, params, params_storage, pm):
  frogpilot_variables.update_frogpilot_params(started)
  frogpilot_variables.publish_frogpilot_params(pm)

  if time_validated:
    run_thread_with_lock("backup_toggles", backup_toggles, (params, params_storage))

def update_checks(automatic_updates, model_manager, now, screen_off, started, theme_manager, time_validated, params, params_memory):
  if not is_url_pingable("https://github.com"):
    return

  if automatic_updates and screen_off:
    automatic_update_check(started, params)

  if time_validated:
    update_maps(now, params, params_memory)

  with locks["update_models"]:
    model_manager.update_models()

  with locks["update_themes"]:
    theme_manager.update_themes()

def update_maps(now, params, params_memory):
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

  params = Params()
  params_memory = Params("/dev/shm/params")
  params_storage = Params("/persist/params")

  frogpilot_planner = FrogPilotPlanner()
  frogpilot_tracking = FrogPilotTracking()
  frogpilot_variables = FrogPilotVariables()
  model_manager = ModelManager()
  theme_manager = ThemeManager()

  theme_manager.update_active_theme()

  run_update_checks = False
  started_previously = False
  time_validated = False

  frogs_go_moo = params.get("DongleId", encoding='utf-8') == "FrogsGoMoo"

  pm = messaging.PubMaster(['frogpilotPlan', 'frogpilotToggles'])
  sm = messaging.SubMaster(['carState', 'controlsState', 'deviceState', 'frogpilotCarControl',
                            'frogpilotCarState', 'frogpilotNavigation', 'frogpilotToggles',
                            'modelV2', 'radarState'],
                            poll='modelV2', ignore_avg_freq=['radarState'])

  frogpilot_variables.update_frogpilot_params(False)
  frogpilot_variables.publish_frogpilot_params(pm)

  frogpilot_toggles = pickle.loads(params.get("FrogPilotToggles", block=True))

  radarless_model = frogpilot_toggles.radarless_model

  while True:
    sm.update()

    now = datetime.datetime.now()
    deviceState = sm['deviceState']
    screen_off = deviceState.screenBrightnessPercent == 0
    started = deviceState.started

    if params_memory.get_bool("FrogPilotTogglesUpdated"):
      run_thread_with_lock("toggle_updates", toggle_updates, (frogpilot_variables, started, time_validated, params, params_storage, pm))

    if sm.updated['frogpilotToggles']:
      frogpilot_toggles = SimpleNamespace(**json.loads(sm['frogpilotToggles'].frogpilotToggles[0]))

    if not started and started_previously:
      frogpilot_planner = FrogPilotPlanner()
      frogpilot_tracking = FrogPilotTracking()

    if started and sm.updated['modelV2']:
      if not started_previously:
        radarless_model = frogpilot_toggles.radarless_model

      frogpilot_planner.update(sm['carState'], sm['controlsState'], sm['frogpilotCarControl'], sm['frogpilotCarState'],
                               sm['frogpilotNavigation'], sm['modelV2'], radarless_model, sm['radarState'], frogpilot_toggles)
      frogpilot_planner.publish(sm, pm, frogpilot_toggles)

      frogpilot_tracking.update(sm['carState'])

    started_previously = started

    check_assets(model_manager, theme_manager, params, params_memory)

    if params_memory.get_bool("ManualUpdateInitiated"):
      run_thread_with_lock("update_checks", update_checks, (False, model_manager, now, screen_off, started, theme_manager, time_validated, params, params_memory))
    elif now.second == 0:
      run_update_checks = not screen_off and not started or now.minute % 15 == 0 or frogs_go_moo
    elif run_update_checks or not time_validated:
      run_thread_with_lock("update_checks", update_checks, (frogpilot_toggles.automatic_updates, model_manager, now, screen_off, started, theme_manager, time_validated, params, params_memory))
      run_update_checks = False

      if not time_validated:
        time_validated = system_time_valid()
        if not time_validated:
          continue
        run_thread_with_lock("update_models", model_manager.update_models, (True,))
        run_thread_with_lock("update_themes", theme_manager.update_themes, (True,))

      theme_manager.update_holiday()

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
