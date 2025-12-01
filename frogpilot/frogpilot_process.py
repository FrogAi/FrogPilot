#!/usr/bin/env python3
import datetime
import gc
import json
import threading
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, config_realtime_process
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common import frogpilot_api, frogpilot_backups, frogpilot_functions, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.frogpilot.system.frogpilot_stats import send_stats
from openpilot.frogpilot.system.frogpilot_tracking import FrogPilotTracking
from openpilot.frogpilot.system.model_manager import ModelManager
from openpilot.frogpilot.system.reporting import capture_report

class FrogPilotRequests:
  def __init__(self, cancel_maps_download, model_manager, theme_manager, thread_manager):
    self.cancel_maps_download = cancel_maps_download
    self.model_manager = model_manager
    self.theme_manager = theme_manager
    self.thread_manager = thread_manager

    self.ui_request_sock = messaging.sub_sock("frogpilotUIRequest")

    self.downloading_models = False
    self.downloading_themes = False
    self.flashed_panda = False

    self.issue_report = "{}"

    self.pending = {}
    self.request_times = {}

  def take(self, kind):
    msg = self.pending.pop(kind, None)
    if msg is None:
      return None

    self.request_times[kind] = msg.logMonoTime
    return msg.frogpilotUIRequest

  def flash_panda(self):
    self.flashed_panda = frogpilot_utilities.flash_panda()

  def send_report(self, report, frogpilot_toggles, api):
    try:
      self.issue_report = json.dumps(capture_report(report, frogpilot_toggles, api))
    except Exception:
      self.issue_report = json.dumps({**report, "status": "failed"})
      raise

  def update(self, now, time_validated, sm, params, frogpilot_toggles, api):
    for msg in messaging.drain_sock(self.ui_request_sock):
      self.pending[msg.frogpilotUIRequest.which()] = msg

    if not self.thread_manager.is_thread_alive("download_themes"):
      if self.downloading_themes:
        self.theme_manager.download_state.cancelled = False
        self.theme_manager.download_lock.release()
        self.downloading_themes = False

      if "downloadTheme" in self.pending and self.theme_manager.download_lock.acquire(blocking=False):
        self.downloading_themes = True

        request = self.take("downloadTheme")
        themes = [(theme.component, theme.theme) for theme in request.downloadTheme]

        self.theme_manager.download_count = len(themes)
        self.theme_manager.download_failed_count = 0
        self.theme_manager.download_success_count = 0
        self.theme_manager.download_state.progress = "Downloading..."
        self.thread_manager.run_with_lock(self.theme_manager.download_themes, (themes,))

    if "downloadTheme" not in self.pending and self.take("cancelThemeDownload") is not None:
      if self.thread_manager.is_thread_alive("download_themes"):
        self.theme_manager.download_state.cancelled = True

    if not self.thread_manager.is_thread_alive("download_models"):
      if self.downloading_models:
        self.model_manager.download_lock.release()
        self.downloading_models = False

      if ("downloadAllModels" in self.pending or "downloadModels" in self.pending) and self.model_manager.download_lock.acquire(blocking=False):
        self.downloading_models = True

        request = self.take("downloadModels")
        if request is not None:
          model_ids, finished_message = list(request.downloadModels), "Downloaded!"
        else:
          self.take("downloadAllModels")
          model_ids, finished_message = self.model_manager.missing_models(), "All models downloaded!"

        self.model_manager.download_state.cancelled = False
        self.model_manager.download_state.progress = "Downloading..."
        self.thread_manager.run_with_lock(self.model_manager.download_models, (model_ids, finished_message))

    if "downloadAllModels" not in self.pending and "downloadModels" not in self.pending and self.take("cancelModelDownload") is not None:
      self.model_manager.download_state.cancelled = True

    if sm["deviceState"].networkMetered and self.model_manager.downloading_automatically:
      self.model_manager.download_state.cancelled = True

    if not self.thread_manager.is_thread_alive("flash_panda") and self.take("flashPanda") is not None:
      self.flashed_panda = False
      self.thread_manager.run_with_lock(self.flash_panda)

    if time_validated and not self.thread_manager.is_thread_alive("send_report"):
      request = self.take("issueReport")
      if request is not None:
        self.issue_report = json.dumps({"status": "pending"})
        self.thread_manager.run_with_lock(self.send_report, (json.loads(request.issueReport), dict(vars(frogpilot_toggles)), api))

    if not self.thread_manager.is_thread_alive("update_maps") and self.take("downloadMaps") is not None:
      self.thread_manager.run_with_lock(frogpilot_functions.update_maps, (now, params, self.cancel_maps_download, sm, True))

    if self.take("cancelMapsDownload") is not None:
      self.take("downloadMaps")
      self.cancel_maps_download.set()

    return self.take("updateToggles") is not None, self.take("updateChecks") is not None

  def publish(self, pm, stats_saved_count):
    flashing_panda = self.thread_manager.is_thread_alive("flash_panda")

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

  frogpilot_planner.frogpilot_vcruise.slc.close()

  if frogpilot_planner.last_gps_position is not None:
    params.put("LastGPSPosition", json.dumps(frogpilot_planner.last_gps_position))

  if frogpilot_toggles.lock_doors_timer != 0:
    thread_manager.run_with_lock(frogpilot_utilities.lock_doors, (frogpilot_toggles.lock_doors_timer, params), report=False)

  theme_manager.restore_wheel_image()

  if frogpilot_toggles.random_themes:
    theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=True)

  if time_validated:
    thread_manager.run_with_lock(send_stats, (params, frogpilot_toggles, api))

def transition_onroad(error_log):
  config_realtime_process(5, Priority.CTRL_LOW)

  if error_log.is_file():
    error_log.unlink()

def update_checks(now, model_manager, theme_manager, thread_manager, sm, params, cancel_maps_download, frogpilot_toggles, boot_run=False):
  while not (frogpilot_utilities.is_url_pingable("https://github.com") or frogpilot_utilities.is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  thread_manager.run_with_lock(frogpilot_functions.update_maps, (now, params, cancel_maps_download, sm))

  if frogpilot_toggles.automatic_updates:
    thread_manager.run_with_lock(frogpilot_functions.update_openpilot, (thread_manager, params))

  if not sm["deviceState"].networkMetered:
    with theme_manager.download_lock:
      theme_manager.update_themes(boot_run)

  with model_manager.download_lock:
    model_manager.update_models(not sm["deviceState"].networkMetered and frogpilot_toggles.automatically_download_models)

  time.sleep(1)

def update_toggles(variables, started, model_manager, theme_manager, thread_manager, time_validated, params, frogpilot_toggles):
  previous_holiday_themes = frogpilot_toggles.holiday_themes
  previous_random_themes = frogpilot_toggles.random_themes

  model_manager.models_updated = False
  theme_manager.theme_updated = False

  if not started and params.get_bool("ModelRandomizer"):
    model_manager.randomize_model(new_drive=False)

  variables.update(theme_manager.holiday_theme, started)

  randomize_theme = frogpilot_toggles.holiday_themes != previous_holiday_themes
  randomize_theme |= frogpilot_toggles.random_themes != previous_random_themes

  theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=randomize_theme)

  if time_validated:
    thread_manager.run_with_lock(frogpilot_backups.backup_toggles, (params,))

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
  variables = frogpilot_variables.FrogPilotVariables()
  model_manager = ModelManager(params)
  theme_manager = ThemeManager(params)
  thread_manager = frogpilot_utilities.ThreadManager()

  cancel_maps_download = threading.Event()
  frogpilot_requests = FrogPilotRequests(cancel_maps_download, model_manager, theme_manager, thread_manager)

  frogpilot_toggles = variables.frogpilot_toggles

  run_update_checks = False
  started_previously = False
  time_validated = False
  waiting_for_car_params = False

  force_onroad_cleared_count = 0
  stats_saved_count = 0

  error_log = frogpilot_variables.ERROR_LOGS_PATH / "error.txt"
  if error_log.is_file():
    error_log.unlink()

  while True:
    sm.update()

    now = datetime.datetime.now(datetime.UTC)

    started = sm["deviceState"].started

    if not started and started_previously:
      frogpilot_tracking.save_stats(blocking=True)
      stats_saved_count += 1

      if frogpilot_toggles.model_randomizer:
        model_manager.randomize_model(new_drive=True)

      update_toggles(variables, started, model_manager, theme_manager, thread_manager, time_validated, params, frogpilot_toggles)
      transition_offroad(frogpilot_planner, theme_manager, thread_manager, time_validated, params, frogpilot_toggles, api)

      run_update_checks = True
    elif started and not started_previously:
      frogpilot_planner = FrogPilotPlanner(error_log, theme_manager)
      frogpilot_tracking = FrogPilotTracking(frogpilot_planner, frogpilot_toggles)

      transition_onroad(error_log)

      waiting_for_car_params = True

    if model_manager.models_updated or theme_manager.theme_updated:
      update_toggles(variables, started, model_manager, theme_manager, thread_manager, time_validated, params, frogpilot_toggles)

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(now, time_validated, sm, frogpilot_toggles)
      frogpilot_planner.publish(theme_manager.theme_update_count, sm, pm, frogpilot_toggles, variables.toggles_json)

      frogpilot_tracking.update(now, time_validated, sm)
    elif not started:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      frogpilot_plan_send.frogpilotPlan.frogpilotToggles = variables.toggles_json
      frogpilot_plan_send.frogpilotPlan.themeUpdateCount = theme_manager.theme_update_count
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    toggles_updated, update_checks_requested = frogpilot_requests.update(now, time_validated, sm, params, frogpilot_toggles, api)
    frogpilot_requests.publish(pm, stats_saved_count)

    toggles_updated |= sm["frogpilotDeviceState"].forceOnroadClearedCount > force_onroad_cleared_count
    toggles_updated |= waiting_for_car_params and sm.updated["frogpilotCarParams"]

    force_onroad_cleared_count = sm["frogpilotDeviceState"].forceOnroadClearedCount
    waiting_for_car_params &= not sm.updated["frogpilotCarParams"]

    if toggles_updated:
      update_toggles(variables, started, model_manager, theme_manager, thread_manager, time_validated, params, frogpilot_toggles)

    run_update_checks |= update_checks_requested
    run_update_checks |= now.second == 0 and (now.minute == 0 or (now.minute % 5 == 0 and variables.frogs_go_moo))
    run_update_checks &= time_validated

    if run_update_checks:
      theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      thread_manager.run_with_lock(update_checks, (now, model_manager, theme_manager, thread_manager, sm, params, cancel_maps_download, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()

      if time_validated:
        theme_manager.update_active_theme(time_validated, frogpilot_toggles)

        thread_manager.run_with_lock(frogpilot_backups.backup_toggles, (params, True))
        thread_manager.run_with_lock(send_stats, (params, frogpilot_toggles, api))
        thread_manager.run_with_lock(update_checks, (now, model_manager, theme_manager, thread_manager, sm, params, cancel_maps_download, frogpilot_toggles, True))

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
