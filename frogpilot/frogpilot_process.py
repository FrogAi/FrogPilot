#!/usr/bin/env python3
import datetime
import gc
import json
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, config_realtime_process
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common import frogpilot_api
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner

class FrogPilotRequests:
  def __init__(self, cancel_maps_download, theme_manager, thread_manager):
    self.thread_manager = thread_manager

    self.ui_request_sock = messaging.sub_sock("frogpilotUIRequest")

    self.pending = {}
    self.request_times = {}

  def take(self, kind):
    msg = self.pending.pop(kind, None)
    if msg is None:
      return None

    self.request_times[kind] = msg.logMonoTime
    return msg.frogpilotUIRequest

  def update(self, now, time_validated, sm, params, frogpilot_toggles, api):
    for msg in messaging.drain_sock(self.ui_request_sock):
      self.pending[msg.frogpilotUIRequest.which()] = msg

    return self.take("updateToggles") is not None, self.take("updateChecks") is not None

  def publish(self, pm):
    frogpilot_process_state_send = messaging.new_message("frogpilotProcessState")
    frogpilotProcessState = frogpilot_process_state_send.frogpilotProcessState

    frogpilotProcessState.downloadMapsRequestTime = self.request_times.get("downloadMaps", 0)
    frogpilotProcessState.downloadThemeRequestTime = self.request_times.get("downloadTheme", 0)
    frogpilotProcessState.downloadingMaps = self.thread_manager.is_thread_alive("update_maps")
    frogpilotProcessState.downloadingModels = self.thread_manager.is_thread_alive("download_models") or self.model_manager.downloading_automatically
    frogpilotProcessState.flashPandaRequestTime = self.request_times.get("flashPanda", 0)
    frogpilotProcessState.flashedPanda = self.flashed_panda
    frogpilotProcessState.flashingPanda = flashing_panda
    frogpilotProcessState.issueReport = self.issue_report
    frogpilotProcessState.issueReportRequestTime = self.request_times.get("issueReport", 0)
    frogpilotProcessState.modelDownloadProgress = self.model_manager.download_state.progress
    frogpilotProcessState.modelDownloadRequestTime = max(self.request_times.get("downloadAllModels", 0), self.request_times.get("downloadModels", 0))
    frogpilotProcessState.statsSavedCount = stats_saved_count
    frogpilotProcessState.themeDownloadCount = self.theme_manager.download_count
    frogpilotProcessState.themeDownloadFailedCount = self.theme_manager.download_failed_count
    frogpilotProcessState.themeDownloadProgress = self.theme_manager.download_state.progress
    frogpilotProcessState.themeDownloadSuccessCount = self.theme_manager.download_success_count

    pm.send("frogpilotProcessState", frogpilot_process_state_send)

def transition_offroad(frogpilot_planner, theme_manager, thread_manager, time_validated, params, frogpilot_toggles, api):
  gc.enable()
  gc.collect()

  if frogpilot_planner.last_gps_position is not None:
    params.put("LastGPSPosition", json.dumps(frogpilot_planner.last_gps_position))

def transition_onroad():
  config_realtime_process(5, Priority.CTRL_LOW)

def update_checks(now, theme_manager, thread_manager, sm, params, cancel_maps_download, frogpilot_toggles, boot_run=False):
  while not (is_url_pingable("https://github.com") or is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  time.sleep(1)

def frogpilot_thread():
  pm = messaging.PubMaster(["frogpilotPlan", "frogpilotProcessState"])
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState",
                            "gpsLocation", "gpsLocationExternal", "liveParameters", "modelV2",
                            "radarState", "selfdriveState", "frogpilotCarParams", "frogpilotCarState", "frogpilotDeviceState",
                            "frogpilotSelfdriveState", "frogpilotModelV2", "frogpilotSignReading", "mapdOut"],
                            poll="modelV2")

  params = Params(return_defaults=True)

  api = frogpilot_api.FrogPilotAPI(params)
  api.register_device(get_build_metadata())

  frogpilot_requests = FrogPilotRequests(cancel_maps_download, theme_manager, thread_manager)

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
      frogpilot_planner = FrogPilotPlanner()

      transition_onroad()

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(now, time_validated, sm)
      frogpilot_planner.publish(sm, pm)
    elif not started:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      frogpilot_plan_send.frogpilotPlan.frogpilotToggles = variables.toggles_json
      frogpilot_plan_send.frogpilotPlan.themeUpdateCount = theme_manager.theme_update_count
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    toggles_updated, update_checks_requested = frogpilot_requests.update(now, time_validated, sm, params, frogpilot_toggles, api)
    frogpilot_requests.publish(pm)

    force_onroad_cleared_count = sm["frogpilotDeviceState"].forceOnroadClearedCount

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
