#!/usr/bin/env python3
import datetime
import json
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import DT_MDL, Priority, Ratekeeper, config_realtime_process
from openpilot.common.time_helpers import system_time_valid

from openpilot.frogpilot.assets.theme_manager import THEME_COMPONENT_PARAMS, ThemeManager
from openpilot.frogpilot.common.frogpilot_backups import backup_toggles
from openpilot.frogpilot.common.frogpilot_functions import capture_report, update_maps, update_openpilot
from openpilot.frogpilot.common.frogpilot_utilities import ThreadManager, flash_panda, is_url_pingable, lock_doors
from openpilot.frogpilot.common.frogpilot_variables import ERROR_LOGS_PATH, FrogPilotVariables
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.frogpilot.system.frogpilot_stats import send_stats
from openpilot.frogpilot.system.frogpilot_tracking import FrogPilotTracking

ASSET_CHECK_RATE = (1 / DT_MDL)

def check_assets(now, theme_manager, thread_manager, sm, params, params_memory, frogpilot_toggles):
  for asset_type, asset_param in THEME_COMPONENT_PARAMS.items():
    asset_to_download = params_memory.get(asset_param)
    if asset_to_download:
      thread_manager.run_with_lock(theme_manager.download_theme, (asset_type, asset_to_download, asset_param, frogpilot_toggles))

  if params_memory.get_bool("FlashPanda"):
    thread_manager.run_with_lock(flash_panda, (params_memory))

  report_data = params_memory.get("IssueReported")
  if report_data:
    thread_manager.run_with_lock(capture_report, (report_data["DiscordUser"], report_data["Issue"], params, dict(vars(frogpilot_toggles))))
    params_memory.remove("IssueReported")

  if params_memory.get_bool("DownloadMaps"):
    thread_manager.run_with_lock(update_maps, (now, params, params_memory, sm, True))

def transition_offroad(frogpilot_planner, theme_manager, thread_manager, time_validated, sm, params, frogpilot_toggles):
  frogpilot_planner.frogpilot_vcruise.slc.close()

  params.put("LastGPSPosition", json.dumps(frogpilot_planner.gps_position))

  if frogpilot_toggles.lock_doors_timer != 0:
    thread_manager.run_with_lock(lock_doors, (frogpilot_toggles.lock_doors_timer, sm, params), report=False)

  if frogpilot_toggles.random_themes:
    theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=True)

  if time_validated:
    thread_manager.run_with_lock(send_stats, (params, frogpilot_toggles))

def transition_onroad(error_log):
  if error_log.is_file():
    error_log.unlink()

def update_checks(now, theme_manager, thread_manager, sm, params, params_memory, frogpilot_toggles, boot_run=False):
  while not (is_url_pingable("https://github.com") or is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  if not sm["deviceState"].networkMetered:
    theme_manager.update_themes(frogpilot_toggles, boot_run)

  thread_manager.run_with_lock(update_maps, (now, params, params_memory, sm))

  if frogpilot_toggles.automatic_updates:
    thread_manager.run_with_lock(update_openpilot, (thread_manager, params))

  time.sleep(1)

def update_toggles(frogpilot_variables, started, theme_manager, thread_manager, time_validated, params, frogpilot_toggles):
  previous_holiday_themes = frogpilot_toggles.holiday_themes
  previous_random_themes = frogpilot_toggles.random_themes

  frogpilot_variables.update(theme_manager.holiday_theme, started)
  frogpilot_toggles = frogpilot_variables.frogpilot_toggles

  randomize_theme = frogpilot_toggles.holiday_themes != previous_holiday_themes
  randomize_theme |= frogpilot_toggles.random_themes != previous_random_themes

  theme_manager.theme_updated = False
  theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=randomize_theme)

  if time_validated:
    thread_manager.run_with_lock(backup_toggles, (params))

  return frogpilot_toggles

def frogpilot_thread():
  rate_keeper = Ratekeeper(1 / DT_MDL, None)

  config_realtime_process(5, Priority.CTRL_LOW)

  pm = messaging.PubMaster(["frogpilotPlan"])
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState", "driverMonitoringState",
                            "gpsLocation", "gpsLocationExternal", "liveParameters", "liveTorqueParameters", "managerState", "modelV2",
                            "onroadEvents", "pandaStates", "radarState", "selfdriveState", "frogpilotCarState",
                            "frogpilotSelfdriveState", "frogpilotModelV2", "frogpilotOnroadEvents", "mapdOut"],
                            poll="modelV2")

  params = Params(return_defaults=True)
  params_memory = Params(memory=True)

  frogpilot_variables = FrogPilotVariables()
  theme_manager = ThemeManager(params, params_memory)
  thread_manager = ThreadManager()

  frogpilot_toggles = frogpilot_variables.frogpilot_toggles

  run_update_checks = False
  started_previously = False
  time_validated = False

  error_log = ERROR_LOGS_PATH / "error.txt"
  if error_log.is_file():
    error_log.unlink()

  while True:
    sm.update()

    now = datetime.datetime.now(datetime.UTC)

    started = sm["deviceState"].started

    if not started and started_previously:
      frogpilot_toggles = update_toggles(frogpilot_variables, started, theme_manager, thread_manager, time_validated, params, frogpilot_toggles)
      transition_offroad(frogpilot_planner, theme_manager, thread_manager, time_validated, sm, params, frogpilot_toggles)

      run_update_checks = True
    elif started and not started_previously:
      frogpilot_planner = FrogPilotPlanner(error_log, theme_manager)
      frogpilot_tracking = FrogPilotTracking(frogpilot_planner, frogpilot_toggles)

      transition_onroad(error_log)

    if theme_manager.theme_updated:
      frogpilot_variables.update(theme_manager.holiday_theme, started)
      frogpilot_toggles = frogpilot_variables.frogpilot_toggles

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(now, time_validated, sm, frogpilot_toggles)
      frogpilot_planner.publish(theme_manager.theme_updated, sm, pm, frogpilot_toggles)

      frogpilot_tracking.update(now, time_validated, sm, frogpilot_toggles)
    elif not started:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      frogpilot_plan_send.frogpilotPlan.frogpilotToggles = json.dumps(vars(frogpilot_toggles))
      frogpilot_plan_send.frogpilotPlan.themeUpdated = theme_manager.theme_updated
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    if rate_keeper.frame % ASSET_CHECK_RATE == 0:
      check_assets(now, theme_manager, thread_manager, sm, params, params_memory, frogpilot_toggles)

    if params_memory.get_bool("FrogPilotTogglesUpdated") or theme_manager.theme_updated:
      frogpilot_toggles = update_toggles(frogpilot_variables, started, theme_manager, thread_manager, time_validated, params, frogpilot_toggles)

    run_update_checks |= params_memory.get_bool("ManualUpdateInitiated")
    run_update_checks |= now.second == 0 and (now.minute % 60 == 0 or (now.minute % 5 == 0 and frogpilot_variables.frogs_go_moo))
    run_update_checks &= time_validated

    if run_update_checks:
      theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      thread_manager.run_with_lock(update_checks, (now, theme_manager, thread_manager, sm, params, params_memory, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()

      if time_validated:
        theme_manager.update_active_theme(time_validated, frogpilot_toggles)

        thread_manager.run_with_lock(backup_toggles, (params, True))
        thread_manager.run_with_lock(send_stats, (params, frogpilot_toggles))
        thread_manager.run_with_lock(update_checks, (now, theme_manager, thread_manager, sm, params, params_memory, frogpilot_toggles, True))

    rate_keeper.keep_time()

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
