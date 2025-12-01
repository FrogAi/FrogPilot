#!/usr/bin/env python3
import datetime
import os
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, set_core_affinity
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import PC

from openpilot.frogpilot.common import frogpilot_api

def transition_offroad(frogpilot_planner, thread_manager, time_validated, params, frogpilot_toggles, api):

def transition_onroad():
  if not PC:
    os.sched_setscheduler(0, os.SCHED_FIFO, os.sched_param(Priority.CTRL_LOW))
  set_core_affinity([5])

def update_checks(now, thread_manager, sm, params, frogpilot_toggles, boot_run=False):
  while not (frogpilot_utilities.is_url_pingable("https://github.com") or frogpilot_utilities.is_url_pingable("https://gitlab.com")):
    time.sleep(60)

def frogpilot_thread():
  params = Params()

  api = frogpilot_api.FrogPilotAPI(params)
  api.register_device()

  run_update_checks = False
  started_previously = False
  time_validated = False

  previous_update_checks_period = None

  while True:
    sm.update()

    now = datetime.datetime.now(datetime.UTC)

    started = sm["deviceState"].started

    if not started and started_previously:
      transition_offroad(frogpilot_planner, thread_manager, time_validated, params, frogpilot_toggles, api)

      run_update_checks = True
    elif started and not started_previously:
      transition_onroad()

    if started and sm.updated["modelV2"]:
    elif not started:

    started_previously = started

    update_checks_period = now.hour

    run_update_checks |= update_checks_period != previous_update_checks_period
    run_update_checks &= time_validated

    previous_update_checks_period = update_checks_period

    if run_update_checks:
      thread_manager.run_in_thread(update_checks, (now, thread_manager, sm, params, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()

      if time_validated:
        thread_manager.run_in_thread(update_checks, (now, thread_manager, sm, params, frogpilot_toggles, True))

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
