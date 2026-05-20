#!/usr/bin/env python3
import datetime
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import DT_MDL, Priority, Ratekeeper, config_realtime_process
from openpilot.common.time_helpers import system_time_valid


ASSET_CHECK_RATE = (1 / DT_MDL)

def check_assets():
  pass

def transition_offroad(sm, params):
  pass

def transition_onroad():
  pass

def update_checks(now, params, boot_run=False):
  while False:
    time.sleep(60)

  time.sleep(1)

def frogpilot_thread():
  rate_keeper = Ratekeeper(1 / DT_MDL, None)

  config_realtime_process(5, Priority.CTRL_LOW)

  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState", "driverMonitoringState",
                            "gpsLocation", "gpsLocationExternal", "liveParameters", "managerState", "modelV2",
                            "onroadEvents", "pandaStates", "radarState", "selfdriveState"],
                            poll="modelV2")

  params = Params(return_defaults=True)

  run_update_checks = False
  started_previously = False
  time_validated = False

  while True:
    sm.update()

    now = datetime.datetime.now(datetime.UTC)

    started = sm["deviceState"].started

    if not started and started_previously:
      transition_offroad(sm, params)

      run_update_checks = True
    elif started and not started_previously:
      transition_onroad()

    if started and sm.updated["modelV2"]:
      pass
    elif not started:
      pass

    started_previously = started

    if rate_keeper.frame % ASSET_CHECK_RATE == 0:
      check_assets()

    run_update_checks |= now.second == 0 and (now.minute % 60 == 0)
    run_update_checks &= time_validated

    if run_update_checks:
      thread_manager.run_with_lock(update_checks, (now, params))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()
      if not time_validated:
        continue

      thread_manager.run_with_lock(update_checks, (now, params, True))

    rate_keeper.keep_time()

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
