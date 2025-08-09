#!/usr/bin/env python3
import datetime
import json
import time

import openpilot.system.sentry as sentry

from cereal import messaging
from openpilot.common.realtime import Priority, config_realtime_process
from openpilot.common.time import system_time_valid

from openpilot.frogpilot.assets.model_manager import ModelManager, MODEL_DOWNLOAD_ALL_PARAM, MODEL_DOWNLOAD_PARAM
from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common.frogpilot_functions import backup_toggles
from openpilot.frogpilot.common.frogpilot_utilities import flash_panda, is_url_pingable, lock_doors, run_thread_with_lock, update_maps, update_openpilot
from openpilot.frogpilot.common.frogpilot_variables import ERROR_LOGS_PATH, FrogPilotVariables, get_frogpilot_toggles, params, params_cache, params_memory
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.frogpilot.controls.lib.frogpilot_tracking import FrogPilotTracking

def assets_checks(model_manager, theme_manager):
  if params_memory.get_bool(MODEL_DOWNLOAD_ALL_PARAM):
    run_thread_with_lock("download_all_models", model_manager.download_all_models)
  else:
    model_to_download = params_memory.get(MODEL_DOWNLOAD_PARAM, encoding="utf-8")
    if model_to_download:
      run_thread_with_lock("download_model", model_manager.download_model, (model_to_download,))

  if params_memory.get_bool("FlashPanda"):
    run_thread_with_lock("flash_panda", flash_panda)

  report_data = json.loads(params_memory.get("IssueReported", encoding="utf-8") or "{}")
  if report_data:
    sentry.capture_report(report_data["DiscordUser"], report_data["Issue"], vars(get_frogpilot_toggles()))
    params_memory.remove("IssueReported")

  assets = [
    ("ColorToDownload", "colors"),
    ("DistanceIconToDownload", "distance_icons"),
    ("IconToDownload", "icons"),
    ("SignalToDownload", "signals"),
    ("SoundToDownload", "sounds"),
    ("WheelToDownload", "steering_wheels")
  ]

  for param, asset_type in assets:
    asset_to_download = params_memory.get(param, encoding="utf-8")
    if asset_to_download:
      run_thread_with_lock("download_theme", theme_manager.download_theme, (asset_type, asset_to_download, param))

def update_checks(manually_updated, model_manager, now, sm, theme_manager, frogpilot_toggles, boot_run=False):
  while not (is_url_pingable("https://github.com") or is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  model_manager.update_models(boot_run)
  theme_manager.update_themes(frogpilot_toggles, boot_run)

  run_thread_with_lock("update_maps", update_maps, (now,))

  if frogpilot_toggles.automatic_updates and not manually_updated:
    run_thread_with_lock("update_openpilot", update_openpilot)

  time.sleep(1)

def frogpilot_thread():
  config_realtime_process(5, Priority.CTRL_LOW)

  error_log = ERROR_LOGS_PATH / "error.txt"
  if error_log.is_file():
    error_log.unlink()

  frogpilot_variables = FrogPilotVariables()
  model_manager = ModelManager()
  theme_manager = ThemeManager()

  assets_checked = False
  run_update_checks = False
  started_previously = False
  time_validated = False
  toggles_updated = False

  frogpilot_toggles = get_frogpilot_toggles()

  toggles_last_updated = datetime.datetime.now()

  pm = messaging.PubMaster(["frogpilotPlan"])
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState", "driverMonitoringState",
                            "liveLocationKalman", "managerState", "modelV2", "pandaStates", "radarState",
                            "frogpilotCarState", "frogpilotNavigation"],
                            poll="modelV2", ignore_avg_freq=["radarState"])

  while True:
    sm.update()

    now = datetime.datetime.now()

    started = sm["deviceState"].started

    if not started and started_previously:
      run_update_checks = True

      frogpilot_variables.update(theme_manager.holiday_theme, started)
      frogpilot_toggles = get_frogpilot_toggles()

      if frogpilot_toggles.lock_doors_timer:
        run_thread_with_lock("lock_doors", lock_doors, (frogpilot_toggles.lock_doors_timer, sm), report=False)

      if frogpilot_toggles.random_themes:
        theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=True)

      params_memory.put_bool("IsOnroad", False)

    elif started and not started_previously:
      frogpilot_planner = FrogPilotPlanner()
      frogpilot_tracking = FrogPilotTracking()

      if error_log.is_file():
        error_log.unlink()

      params_memory.put_bool("IsOnroad", True)

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(sm, frogpilot_toggles)
      frogpilot_planner.publish(sm, pm, theme_manager.theme_updated, toggles_updated)

      frogpilot_tracking.update(sm)
    elif not started and toggles_updated:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      frogpilot_plan_send.frogpilotPlan.themeUpdated = theme_manager.theme_updated
      frogpilot_plan_send.frogpilotPlan.togglesUpdated = toggles_updated
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    if now.second % 2 == 0:
      if not assets_checked:
        assets_checks(model_manager, theme_manager)

        assets_checked = True
    else:
      assets_checked = False

    if params_memory.get_bool("FrogPilotTogglesUpdated") or theme_manager.theme_updated:
      previous_holiday_themes = frogpilot_toggles.holiday_themes
      previous_random_themes = frogpilot_toggles.random_themes

      frogpilot_variables.update(theme_manager.holiday_theme, started)
      frogpilot_toggles = get_frogpilot_toggles()

      randomize_theme = frogpilot_toggles.holiday_themes != previous_holiday_themes
      randomize_theme |= frogpilot_toggles.random_themes != previous_random_themes

      theme_manager.theme_updated = False
      theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=randomize_theme)

      if time_validated:
        run_thread_with_lock("backup_toggles", backup_toggles, (params_cache,), report=False)

      toggles_last_updated = now

    toggles_updated = (now - toggles_last_updated).total_seconds() <= 1

    manually_updated = params_memory.get_bool("ManualUpdateInitiated")

    run_update_checks |= manually_updated
    run_update_checks |= now.second == 0 and (now.minute % 60 == 0 or (now.minute % 5 == 0 and frogpilot_toggles.frogs_go_moo))
    run_update_checks &= time_validated

    if run_update_checks:
      theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      run_thread_with_lock("update_checks", update_checks, (manually_updated, model_manager, now, sm, theme_manager, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()
      if not time_validated:
        continue

      theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      run_thread_with_lock("update_checks", update_checks, (manually_updated, model_manager, now, sm, theme_manager, frogpilot_toggles, True))

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
