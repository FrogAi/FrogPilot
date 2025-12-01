#!/usr/bin/env python3
import datetime
import json
import os
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, set_core_affinity
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import PC

from openpilot.frogpilot.common import frogpilot_api
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner

class FrogPilotRequests:
  def __init__(self):
    self.the_pond_request_sock = messaging.sub_sock("thePondRequest")
    self.ui_request_sock = messaging.sub_sock("frogpilotUIRequest")

    self.pending = {}
    self.request_times = {}

  def take(self, kind):
    pending = self.pending.pop(kind, None)
    if pending is None:
      return None

    self.request_times[kind], request = pending
    return request

  def update(self):
    for msg in messaging.drain_sock(self.ui_request_sock):
      self.pending[msg.frogpilotUIRequest.which()] = (msg.logMonoTime, msg.frogpilotUIRequest)

    for msg in messaging.drain_sock(self.the_pond_request_sock):
      self.pending[msg.thePondRequest.request.which()] = (msg.logMonoTime, msg.thePondRequest.request)

    return self.take("updateToggles") is not None, self.take("updateChecks") is not None

  def publish(self, pm):
    frogpilot_process_state_send = messaging.new_message("frogpilotProcessState")
    frogpilotProcessState = frogpilot_process_state_send.frogpilotProcessState

    pm.send("frogpilotProcessState", frogpilot_process_state_send)

def transition_offroad(frogpilot_planner, thread_manager, time_validated, params, frogpilot_toggles, api):
  if frogpilot_planner.last_gps_position is not None:
    params.put("LastGPSPosition", json.dumps(frogpilot_planner.last_gps_position))

def transition_onroad():
  if not PC:
    os.sched_setscheduler(0, os.SCHED_FIFO, os.sched_param(Priority.CTRL_LOW))
  set_core_affinity([5])

def update_checks(now, thread_manager, sm, params, frogpilot_toggles, boot_run=False):
  while not (frogpilot_utilities.is_url_pingable("https://github.com") or frogpilot_utilities.is_url_pingable("https://gitlab.com")):
    time.sleep(60)

def frogpilot_thread():
  pm = messaging.PubMaster(["frogpilotAccountState", "frogpilotPlan", "frogpilotProcessState"])
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState",
                            "frogpilotCarParams", "frogpilotCarState", "frogpilotDeviceState", "frogpilotModelV2",
                            "frogpilotSelfdriveState", "frogpilotSignReading", "gpsLocation", "gpsLocationExternal",
                            "liveParameters", "longitudinalPlan", "mapdOut", "modelV2", "radarState", "selfdriveState"],
                            poll="modelV2")

  params = Params(return_defaults=True)

  api = frogpilot_api.FrogPilotAPI(params)
  api.register_device()

  frogpilot_requests = FrogPilotRequests()

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
      frogpilot_planner = FrogPilotPlanner()

      transition_onroad()

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(now, time_validated, sm)
      frogpilot_planner.publish(sm, pm)
    elif not started:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    toggles_updated, update_checks_requested = frogpilot_requests.update()
    frogpilot_requests.publish(pm)

    update_checks_period = now.hour

    run_update_checks |= update_checks_requested
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
