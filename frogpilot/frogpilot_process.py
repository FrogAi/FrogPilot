#!/usr/bin/env python3
import datetime
import json
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import DT_MDL, Priority, Ratekeeper, config_realtime_process
from openpilot.common.time_helpers import system_time_valid

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.frogpilot.system.frogpilot_stats import send_stats
from openpilot.frogpilot.system.frogpilot_tracking import FrogPilotTracking


ASSET_CHECK_RATE = (1 / DT_MDL)

def check_assets(thread_manager, frogpilot_toggles):
  pass

def transition_offroad(gps_position, thread_manager, time_validated, sm, params, frogpilot_toggles):
  if time_validated:
    thread_manager.run_with_lock(send_stats, (gps_position, params, frogpilot_toggles))

def transition_onroad():
  pass

def update_checks(now, thread_manager, params, frogpilot_toggles, boot_run=False):
  while not (frogpilot_utilities.is_url_pingable("https://github.com") or frogpilot_utilities.is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  time.sleep(1)

def frogpilot_thread():
  rate_keeper = Ratekeeper(1 / DT_MDL, None)

  config_realtime_process(5, Priority.CTRL_LOW)

  pm = messaging.PubMaster(["frogpilotPlan", "frogpilotUI"])
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState", "driverMonitoringState",
                            "frogpilotCarState", "frogpilotModelV2", "frogpilotSelfdriveState",
                            "gpsLocation", "gpsLocationExternal", "liveParameters", "managerState",
                            "modelV2", "onroadEvents", "pandaStates", "radarState", "selfdriveState"],
                            poll="modelV2")

  params = Params(return_defaults=True)

  thread_manager = frogpilot_utilities.ThreadManager()

  frogpilot_variables_instance = frogpilot_variables.FrogPilotVariables()
  frogpilot_toggles = frogpilot_variables_instance.frogpilot_toggles
  frogpilot_toggles_json = json.dumps(vars(frogpilot_toggles))

  run_update_checks = False
  started_previously = False
  time_validated = False

  frogpilot_planner = FrogPilotPlanner()

  while True:
    sm.update()

    now = datetime.datetime.now(datetime.UTC)

    started = sm["deviceState"].started

    if started != started_previously:
      frogpilot_variables_instance.update(started)
      frogpilot_toggles = frogpilot_variables_instance.frogpilot_toggles
      frogpilot_toggles_json = json.dumps(vars(frogpilot_toggles))

    frogpilot_ui_send = messaging.new_message("frogpilotUI")
    frogpilot_ui_send.valid = True
    frogpilot_ui_send.frogpilotUI.frogpilotToggles = frogpilot_toggles_json
    pm.send("frogpilotUI", frogpilot_ui_send)

    if not started and started_previously:
      transition_offroad(frogpilot_planner.gps_position, thread_manager, time_validated, sm, params, frogpilot_toggles)

      run_update_checks = True
    elif started and not started_previously:
      frogpilot_planner = FrogPilotPlanner()
      frogpilot_tracking = FrogPilotTracking(frogpilot_planner, frogpilot_toggles)

      transition_onroad()

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(now, time_validated, sm, frogpilot_toggles)
      frogpilot_planner.publish(sm, pm, frogpilot_toggles)

      frogpilot_tracking.update(now, time_validated, sm, frogpilot_toggles)
    elif not started:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    if rate_keeper.frame % ASSET_CHECK_RATE == 0:
      check_assets(thread_manager, frogpilot_toggles)

    run_update_checks |= now.second == 0 and (now.minute % 60 == 0)
    run_update_checks &= time_validated

    if run_update_checks:
      thread_manager.run_with_lock(update_checks, (now, thread_manager, params, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()
      if not time_validated:
        continue

      thread_manager.run_with_lock(send_stats, (frogpilot_planner.gps_position, params, frogpilot_toggles))
      thread_manager.run_with_lock(update_checks, (now, thread_manager, params, frogpilot_toggles, True))

    rate_keeper.keep_time()

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
