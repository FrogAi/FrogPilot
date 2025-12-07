#!/usr/bin/env python3
import datetime
import gc
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, config_realtime_process
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common import frogpilot_api

def transition_offroad(frogpilot_planner, theme_manager, thread_manager, time_validated, params, frogpilot_toggles, api):
  gc.enable()
  gc.collect()

def transition_onroad():
  config_realtime_process(5, Priority.CTRL_LOW)

def update_checks(now, theme_manager, thread_manager, sm, params, cancel_maps_download, frogpilot_toggles, boot_run=False):
  while not (is_url_pingable("https://github.com") or is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  time.sleep(1)

def frogpilot_thread():
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState",
                            "gpsLocation", "gpsLocationExternal", "liveParameters", "modelV2",
                            "radarState", "selfdriveState"],
                            poll="modelV2")

  params = Params(return_defaults=True)

  api = frogpilot_api.FrogPilotAPI(params)
  api.register_device(get_build_metadata())

  run_update_checks = False
  started_previously = False
  time_validated = False

  while True:
    sm.update()

    now = datetime.datetime.now(datetime.UTC)

    started = sm["deviceState"].started

    if not started and started_previously:
      transition_offroad(frogpilot_planner, theme_manager, thread_manager, time_validated, params, frogpilot_toggles, api)

      run_update_checks = True
    elif started and not started_previously:
      transition_onroad()

    if started and sm.updated["modelV2"]:
    elif not started:

    started_previously = started

    run_update_checks |= now.second == 0 and now.minute == 0
    run_update_checks &= time_validated

    if run_update_checks:
      thread_manager.run_with_lock(update_checks, (now, theme_manager, thread_manager, sm, params, cancel_maps_download, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()

      if time_validated:
        thread_manager.run_with_lock(update_checks, (now, theme_manager, thread_manager, sm, params, cancel_maps_download, frogpilot_toggles, True))

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
