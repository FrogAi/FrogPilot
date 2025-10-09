#!/usr/bin/env python3
import datetime
import os
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import DT_MDL, Priority, Ratekeeper, config_realtime_process
from openpilot.common.time_helpers import system_time_valid

from openpilot.frogpilot.assets.theme_manager import THEME_COMPONENT_PARAMS, ThemeManager
from openpilot.frogpilot.common.frogpilot_functions import backup_toggles
from openpilot.frogpilot.common.frogpilot_utilities import is_url_pingable, run_thread_with_lock, update_openpilot
from openpilot.frogpilot.common.frogpilot_variables import ERROR_LOGS_PATH, FrogPilotVariables
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.frogpilot.controls.lib.frogpilot_tracking import FrogPilotTracking
from openpilot.frogpilot.system.frogpilot_stats import send_stats

ASSET_CHECK_RATE = (1 / DT_MDL)

def assets_checks(model_manager, theme_manager, params_memory, frogpilot_toggles):
  for asset_type, asset_param in THEME_COMPONENT_PARAMS.items():
    asset_to_download = params_memory.get(asset_param)
    if asset_to_download:
      run_thread_with_lock("download_theme", theme_manager.download_theme, (asset_type, asset_to_download, asset_param, frogpilot_toggles))

def update_checks(model_manager, now, theme_manager, params, params_memory, frogpilot_toggles, boot_run=False):
  while not (is_url_pingable("https://github.com") or is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  theme_manager.update_themes(frogpilot_toggles, boot_run)

  if frogpilot_toggles.automatic_updates:
    run_thread_with_lock("update_openpilot", update_openpilot, (params, params_memory))

  time.sleep(1)

def frogpilot_thread():
  rate_keeper = Ratekeeper(1 / DT_MDL, None)

  config_realtime_process(5, Priority.CTRL_LOW)

  params = Params()
  params_cache = Params(cache=True)
  params_memory = Params(memory=True)

  frogpilot_variables = FrogPilotVariables()
  theme_manager = ThemeManager(params, params_memory)

  pm = messaging.PubMaster(["frogpilotPlan"])
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState", "driverMonitoringState",
                            "gpsLocation", "gpsLocationExternal", "liveParameters", "managerState", "modelV2",
                            "onroadEvents", "pandaStates", "radarState", "selfdriveState", "frogpilotCarState",
                            "frogpilotSelfdriveState", "frogpilotModelV2", "frogpilotOnroadEvents"],
                            poll="modelV2")

  run_update_checks = False
  started_previously = False
  time_validated = False
  toggles_updated = False

  frogpilot_toggles = frogpilot_variables.frogpilot_toggles

  toggles_last_updated = datetime.datetime.now(datetime.timezone.utc)

  error_log = ERROR_LOGS_PATH / "error.txt"
  if error_log.is_file():
    error_log.unlink()

  while True:
    sm.update()

    now = datetime.datetime.now(datetime.timezone.utc)

    started = sm["deviceState"].started

    if not started and started_previously:
      run_update_checks = True

      frogpilot_variables.update(theme_manager.holiday_theme, started)
      frogpilot_toggles = frogpilot_variables.frogpilot_toggles

      if frogpilot_toggles.random_themes:
        theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=True)

      if time_validated and is_url_pingable(os.environ.get("STATS_URL", "")):
        send_stats(params)

    elif started and not started_previously:
      if error_log.is_file():
        error_log.unlink()

      frogpilot_planner = FrogPilotPlanner(error_log, theme_manager, params)
      frogpilot_tracking = FrogPilotTracking(frogpilot_planner, params, frogpilot_toggles)

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(now, time_validated, params, params_memory, sm, frogpilot_toggles)
      frogpilot_planner.publish(theme_manager.theme_updated, toggles_updated, params_memory, sm, pm, frogpilot_toggles)

      frogpilot_tracking.update(now, time_validated, params, sm, frogpilot_toggles)
    elif not started:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      frogpilot_plan_send.frogpilotPlan.themeUpdated = theme_manager.theme_updated or params_memory.get_bool("UseActiveTheme")
      frogpilot_plan_send.frogpilotPlan.togglesUpdated = toggles_updated
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    if rate_keeper.frame % ASSET_CHECK_RATE == 0:
      assets_checks(model_manager, theme_manager, params_memory, frogpilot_toggles)

    if params_memory.get_bool("FrogPilotTogglesUpdated") or theme_manager.theme_updated:
      previous_holiday_themes = frogpilot_toggles.holiday_themes
      previous_random_themes = frogpilot_toggles.random_themes

      frogpilot_variables.update(theme_manager.holiday_theme, started)
      frogpilot_toggles = frogpilot_variables.frogpilot_toggles

      randomize_theme = frogpilot_toggles.holiday_themes != previous_holiday_themes
      randomize_theme |= frogpilot_toggles.random_themes != previous_random_themes

      theme_manager.theme_updated = False
      theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=randomize_theme)

      if time_validated:
        run_thread_with_lock("backup_toggles", backup_toggles, (params, params_cache), report=False)

      toggles_last_updated = now

    toggles_updated = (now - toggles_last_updated).total_seconds() <= 1

    run_update_checks |= params_memory.get_bool("ManualUpdateInitiated")
    run_update_checks |= now.second == 0 and (now.minute % 60 == 0 or (now.minute % 5 == 0 and frogpilot_toggles.frogs_go_moo))
    run_update_checks &= time_validated

    if run_update_checks:
      theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      run_thread_with_lock("update_checks", update_checks, (model_manager, now, theme_manager, params, params_memory, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()
      if not time_validated:
        continue

      theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      run_thread_with_lock("update_checks", update_checks, (model_manager, now, theme_manager, params, params_memory, frogpilot_toggles, True))

    rate_keeper.keep_time()

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
