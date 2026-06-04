#!/usr/bin/env python3
import datetime
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import DT_MDL, Priority, Ratekeeper, config_realtime_process
from openpilot.common.time_helpers import system_time_valid

from openpilot.frogpilot.common import frogpilot_utilities
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.frogpilot.system.frogpilot_stats import send_stats
from openpilot.frogpilot.system.frogpilot_tracking import FrogPilotTracking


ASSET_CHECK_RATE = (1 / DT_MDL)

def check_assets(thread_manager):
  pass

def transition_offroad(gps_position, thread_manager, time_validated, sm, params):
  if time_validated:
    thread_manager.run_with_lock(send_stats, (gps_position, params))

def transition_onroad():
  pass

def update_checks(now, thread_manager, params, boot_run=False):
  while not (frogpilot_utilities.is_url_pingable("https://github.com") or frogpilot_utilities.is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  time.sleep(1)

def frogpilot_thread():
  rate_keeper = Ratekeeper(1 / DT_MDL, None)

  config_realtime_process(5, Priority.CTRL_LOW)

  pm = messaging.PubMaster(["frogpilotPlan"])
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState", "driverMonitoringState",
                            "frogpilotCarState", "frogpilotModelV2", "frogpilotSelfdriveState", "frogpilotUI",
                            "gpsLocation", "gpsLocationExternal", "liveParameters", "managerState",
                            "modelV2", "onroadEvents", "pandaStates", "radarState", "selfdriveState"],
                            poll="modelV2")

  params = Params(return_defaults=True)

  thread_manager = frogpilot_utilities.ThreadManager()

  run_update_checks = False
  started_previously = False
  time_validated = False

  frogpilot_planner = FrogPilotPlanner()

  while True:
    sm.update()

    now = datetime.datetime.now(datetime.UTC)

    started = sm["deviceState"].started

    if not started and started_previously:
      transition_offroad(frogpilot_planner.gps_position, thread_manager, time_validated, sm, params)

      run_update_checks = True
    elif started and not started_previously:
      frogpilot_planner = FrogPilotPlanner()
      frogpilot_tracking = FrogPilotTracking(frogpilot_planner)

      transition_onroad()

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(now, time_validated, sm)
      frogpilot_planner.publish(sm, pm)

      frogpilot_tracking.update(now, time_validated, sm)
    elif not started:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    if rate_keeper.frame % ASSET_CHECK_RATE == 0:
      check_assets(thread_manager)

    run_update_checks |= now.second == 0 and (now.minute % 60 == 0)
    run_update_checks &= time_validated

    if run_update_checks:
      thread_manager.run_with_lock(update_checks, (now, thread_manager, params))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()
      if not time_validated:
        continue

      thread_manager.run_with_lock(send_stats, (frogpilot_planner.gps_position, params))
      thread_manager.run_with_lock(update_checks, (now, thread_manager, params, True))

    rate_keeper.keep_time()

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
