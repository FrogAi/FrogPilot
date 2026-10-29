#!/usr/bin/env python3
import datetime
import json
import os
import threading
import time

from cereal import messaging
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, set_core_affinity
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import PC

from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common import frogpilot_api, frogpilot_backups, frogpilot_functions, frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.frogpilot.controls.lib.speed_limit_filler import upload_speed_limits
from openpilot.frogpilot.system.frogpilot_stats import send_stats
from openpilot.frogpilot.system.frogpilot_tracking import FrogPilotTracking
from openpilot.frogpilot.system.model_manager import ModelManager
from openpilot.frogpilot.system.reporting import capture_report
from openpilot.frogpilot.system.speed_limit_reader_models import SpeedLimitReaderModels

class FrogPilotRequests:
  def __init__(self, cancel_maps_download, model_manager, theme_manager, thread_manager):
    self.cancel_maps_download = cancel_maps_download
    self.model_manager = model_manager
    self.theme_manager = theme_manager
    self.thread_manager = thread_manager

    self.the_pond_request_sock = messaging.sub_sock("thePondRequest")
    self.ui_request_sock = messaging.sub_sock("frogpilotUIRequest")

    self.downloading_models = False
    self.downloading_themes = False
    self.flashed_panda = False

    self.door_command = ""
    self.settings_backup = ""

    self.pending = {}
    self.request_times = {}

    self.issue_report = "{}"

    self.failed_report = None

  def take(self, kind):
    pending = self.pending.pop(kind, None)
    if pending is None:
      return None

    self.request_times[kind], request = pending
    return request

  def flash_panda(self):
    self.flashed_panda = frogpilot_utilities.flash_panda()

  def run_settings_backup(self, function, params, argument):
    try:
      function(params, argument)
      self.settings_backup = "succeeded"
    except Exception:
      self.settings_backup = "failed"
      raise

  def send_door_command(self, lock):
    if frogpilot_utilities.send_door_command(lock):
      self.door_command = "locked" if lock else "unlocked"
    else:
      self.door_command = "failed"

  def publish_issue_report(self, issue_report):
    self.issue_report = json.dumps(issue_report)

  def send_report(self, report, frogpilot_toggles, api):
    try:
      self.failed_report = capture_report(report, frogpilot_toggles, api, self.publish_issue_report)
    except Exception:
      self.failed_report = report
      self.publish_issue_report({"status": "failed"})
      raise

  def update(self, now, time_validated, sm, params, frogpilot_toggles, api):
    for msg in messaging.drain_sock(self.ui_request_sock):
      self.pending[msg.frogpilotUIRequest.which()] = (msg.logMonoTime, msg.frogpilotUIRequest)

    for msg in messaging.drain_sock(self.the_pond_request_sock):
      self.pending[msg.thePondRequest.request.which()] = (msg.logMonoTime, msg.thePondRequest.request)

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
        self.thread_manager.run_in_thread(self.theme_manager.download_themes, (themes,))

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
          model_ids = list(request.downloadModels)
          finished_message = "Downloaded!"
        else:
          self.take("downloadAllModels")
          model_ids = self.model_manager.missing_models()
          finished_message = "All models downloaded!"

        self.model_manager.download_state.cancelled = False
        self.model_manager.download_state.progress = "Downloading..."
        self.thread_manager.run_in_thread(self.model_manager.download_models, (model_ids, finished_message))

    if "downloadAllModels" not in self.pending and "downloadModels" not in self.pending and self.take("cancelModelDownload") is not None:
      self.model_manager.download_state.cancelled = True

    if sm["deviceState"].networkMetered and self.model_manager.downloading_automatically:
      self.model_manager.download_state.cancelled = True

    if not self.thread_manager.is_thread_alive("flash_panda") and self.take("flashPanda") is not None:
      self.flashed_panda = False
      self.thread_manager.run_in_thread(self.flash_panda)

    if time_validated and not self.thread_manager.is_thread_alive("send_report"):
      request = self.take("issueReport")
      if request is not None:
        self.issue_report = json.dumps({"status": "pending"})
        self.thread_manager.run_in_thread(self.send_report, (json.loads(request.issueReport), dict(vars(frogpilot_toggles)), api))
      elif self.take("retryIssueReport") is not None and self.failed_report is not None:
        self.issue_report = json.dumps({"status": "pending"})
        self.thread_manager.run_in_thread(self.send_report, (self.failed_report, dict(vars(frogpilot_toggles)), api))

    if not self.thread_manager.is_thread_alive("update_maps") and self.take("downloadMaps") is not None:
      self.thread_manager.run_in_thread(frogpilot_functions.update_maps, (now, params, self.cancel_maps_download, sm, True))

    if self.take("cancelMapsDownload") is not None:
      self.take("downloadMaps")
      self.cancel_maps_download.set()

    if not self.thread_manager.is_thread_alive("send_door_command"):
      request = self.take("lockDoors")
      if request is not None:
        if sm["deviceState"].started or self.thread_manager.is_thread_alive("lock_doors"):
          self.door_command = "busy"
        else:
          self.door_command = "pending"
          self.thread_manager.run_in_thread(self.send_door_command, (request.lockDoors,))

    if not self.thread_manager.is_thread_alive("run_settings_backup"):
      request = self.take("createSettingsBackup")
      if request is not None:
        self.settings_backup = "pending"
        self.thread_manager.run_in_thread(self.run_settings_backup, (frogpilot_backups.create_settings_backup, params, request.createSettingsBackup))
      else:
        request = self.take("restoreSettingsBackup")
        if request is not None:
          self.settings_backup = "pending"
          self.thread_manager.run_in_thread(self.run_settings_backup, (frogpilot_backups.restore_settings_backup, params,
                                                                      frogpilot_paths.TOGGLE_BACKUPS / request.restoreSettingsBackup))

    if self.take("refreshTheme") is not None:
      self.theme_manager.previous_asset_mappings = {}
      self.theme_manager.theme_update_count += 1
      self.theme_manager.theme_updated = True

    return self.take("updateToggles") is not None, self.take("updateChecks") is not None

  def publish(self, pm, stats_saved_count, toggles_json):
    flashing_panda = self.thread_manager.is_thread_alive("flash_panda")

    frogpilot_process_state_send = messaging.new_message("frogpilotProcessState")
    frogpilotProcessState = frogpilot_process_state_send.frogpilotProcessState

    frogpilotProcessState.doorCommand = self.door_command
    frogpilotProcessState.doorCommandRequestTime = self.request_times.get("lockDoors", 0)
    frogpilotProcessState.downloadingMaps = self.thread_manager.is_thread_alive("update_maps")
    frogpilotProcessState.downloadingModels = self.thread_manager.is_thread_alive("download_models") or self.model_manager.downloading_automatically
    frogpilotProcessState.downloadMapsRequestTime = self.request_times.get("downloadMaps", 0)
    frogpilotProcessState.downloadThemeRequestTime = self.request_times.get("downloadTheme", 0)
    frogpilotProcessState.flashedPanda = self.flashed_panda
    frogpilotProcessState.flashingPanda = flashing_panda
    frogpilotProcessState.flashPandaRequestTime = self.request_times.get("flashPanda", 0)
    frogpilotProcessState.frogpilotToggles = toggles_json
    frogpilotProcessState.issueReport = self.issue_report
    frogpilotProcessState.issueReportRequestTime = max(self.request_times.get("issueReport", 0), self.request_times.get("retryIssueReport", 0))
    frogpilotProcessState.modelDownloadProgress = self.model_manager.download_state.progress
    frogpilotProcessState.modelDownloadRequestTime = max(self.request_times.get("downloadAllModels", 0), self.request_times.get("downloadModels", 0))
    frogpilotProcessState.settingsBackup = self.settings_backup
    frogpilotProcessState.settingsBackupRequestTime = max(self.request_times.get("createSettingsBackup", 0), self.request_times.get("restoreSettingsBackup", 0))
    frogpilotProcessState.statsSavedCount = stats_saved_count
    frogpilotProcessState.themeDownloadCount = self.theme_manager.download_count
    frogpilotProcessState.themeDownloadFailedCount = self.theme_manager.download_failed_count
    frogpilotProcessState.themeDownloadProgress = self.theme_manager.download_state.progress
    frogpilotProcessState.themeDownloadSuccessCount = self.theme_manager.download_success_count

    pm.send("frogpilotProcessState", frogpilot_process_state_send)

def transition_offroad(frogpilot_planner, theme_manager, thread_manager, time_validated, params, frogpilot_toggles, api):
  frogpilot_planner.frogpilot_vcruise.slc.close()
  frogpilot_planner.frogpilot_weather.close()

  frogpilot_planner.speed_limit_filler.save()

  if frogpilot_planner.last_gps_position is not None:
    params.put("LastGPSPosition", json.dumps(frogpilot_planner.last_gps_position))

  if frogpilot_toggles.lock_doors_timer != 0:
    thread_manager.run_in_thread(frogpilot_utilities.lock_doors, (frogpilot_toggles.lock_doors_timer, params), report=False)

  theme_manager.restore_wheel_image()

  if frogpilot_toggles.random_themes:
    theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=True)

  if time_validated:
    thread_manager.run_in_thread(send_stats, (params, frogpilot_toggles, api))

def transition_onroad():
  if not PC:
    os.sched_setscheduler(0, os.SCHED_FIFO, os.sched_param(Priority.CTRL_LOW))
  set_core_affinity([5])

  frogpilot_paths.ERROR_LOG_PATH.unlink(missing_ok=True)

def update_checks(now, model_manager, speed_limit_reader_models, theme_manager, thread_manager, sm, params, cancel_maps_download, frogpilot_toggles, boot_run=False):
  while not (frogpilot_utilities.is_url_pingable("https://github.com") or frogpilot_utilities.is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  thread_manager.run_in_thread(frogpilot_functions.update_maps, (now, params, cancel_maps_download, sm))

  if not sm["deviceState"].networkMetered and frogpilot_toggles.automatic_updates:
    thread_manager.run_in_thread(frogpilot_functions.update_openpilot, (thread_manager, params))

  if not sm["deviceState"].networkMetered:
    with theme_manager.download_lock:
      theme_manager.update_themes(boot_run)

  with model_manager.download_lock:
    model_manager.update_models(not sm["deviceState"].networkMetered and frogpilot_toggles.automatically_download_models)

  if frogpilot_toggles.speed_limit_controller or frogpilot_toggles.speed_limit_filler:
    speed_limit_reader_models.update(sm)

def update_toggles(variables, started, model_manager, theme_manager, thread_manager, time_validated, params):
  frogpilot_toggles = variables.frogpilot_toggles

  previous_holiday_themes = frogpilot_toggles.holiday_themes
  previous_random_themes = frogpilot_toggles.random_themes

  model_manager.models_updated = False
  theme_manager.theme_updated = False

  if not started and params.get_bool("ModelRandomizer"):
    model_manager.randomize_model(new_drive=False)

  variables.update(theme_manager.holiday_theme, started)

  if not started:
    frogpilot_functions.sync_flag_file(frogpilot_paths.HD_PATH, frogpilot_toggles.use_higher_bitrate)
    frogpilot_functions.sync_flag_file(frogpilot_paths.KONIK_PATH, frogpilot_toggles.use_konik_server)

  randomize_theme = frogpilot_toggles.holiday_themes != previous_holiday_themes
  randomize_theme |= frogpilot_toggles.random_themes != previous_random_themes

  theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=randomize_theme)

  if time_validated:
    thread_manager.run_in_thread(frogpilot_backups.backup_toggles, (params,))

def frogpilot_thread():
  pm = messaging.PubMaster(["frogpilotPlan", "frogpilotProcessState"])
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState",
                            "frogpilotCarParams", "frogpilotCarState", "frogpilotDeviceState", "frogpilotModelV2",
                            "frogpilotSelfdriveState", "frogpilotSignReading", "gpsLocation", "gpsLocationExternal",
                            "liveParameters", "longitudinalPlan", "mapdOut", "modelV2", "radarState", "selfdriveState"],
                            poll="modelV2")

  params = Params(return_defaults=True)

  api = frogpilot_api.FrogPilotAPI(params)
  api.register_device()
  variables = frogpilot_variables.FrogPilotVariables()
  model_manager = ModelManager(params)
  speed_limit_reader_models = SpeedLimitReaderModels(params)
  theme_manager = ThemeManager(params)
  thread_manager = frogpilot_utilities.ThreadManager()

  cancel_maps_download = threading.Event()
  frogpilot_requests = FrogPilotRequests(cancel_maps_download, model_manager, theme_manager, thread_manager)

  frogpilot_toggles = variables.frogpilot_toggles

  run_update_checks = False
  share_speed_limits = True
  started_previously = False
  time_validated = False
  waiting_for_car_params = False

  force_onroad_cleared_count = 0
  stats_saved_count = 0

  previous_update_checks_period = None

  frogpilot_paths.ERROR_LOG_PATH.unlink(missing_ok=True)

  while True:
    sm.update()

    now = datetime.datetime.now(datetime.UTC)

    started = sm["deviceState"].started

    if not started and started_previously:
      frogpilot_tracking.save_stats(blocking=True)
      stats_saved_count += 1

      if frogpilot_toggles.model_randomizer:
        model_manager.randomize_model(new_drive=True)

      update_toggles(variables, started, model_manager, theme_manager, thread_manager, time_validated, params)
      transition_offroad(frogpilot_planner, theme_manager, thread_manager, time_validated, params, frogpilot_toggles, api)

      run_update_checks = True
      share_speed_limits = True
    elif started and not started_previously:
      frogpilot_planner = FrogPilotPlanner(theme_manager, api)
      frogpilot_tracking = FrogPilotTracking(frogpilot_planner, frogpilot_toggles)

      transition_onroad()

      waiting_for_car_params = True

    if model_manager.models_updated or theme_manager.theme_updated:
      update_toggles(variables, started, model_manager, theme_manager, thread_manager, time_validated, params)

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(now, time_validated, sm, frogpilot_toggles)
      frogpilot_planner.publish(theme_manager.theme_update_count, sm, pm, frogpilot_toggles)

      frogpilot_tracking.update(now, time_validated, sm)
    elif not started:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      frogpilot_plan_send.frogpilotPlan.themeUpdateCount = theme_manager.theme_update_count
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    if share_speed_limits and not started and frogpilot_utilities.is_unmetered_network(sm["deviceState"]):
      if frogpilot_toggles.speed_limit_filler and frogpilot_toggles.frogpilot_telemetry and not frogpilot_toggles.uploads_disabled:
        thread_manager.run_in_thread(upload_speed_limits, (params, api))

      share_speed_limits = False

    toggles_updated, update_checks_requested = frogpilot_requests.update(now, time_validated, sm, params, frogpilot_toggles, api)
    frogpilot_requests.publish(pm, stats_saved_count, variables.toggles_json)

    toggles_updated |= sm["frogpilotDeviceState"].forceOnroadClearedCount > force_onroad_cleared_count
    toggles_updated |= waiting_for_car_params and sm.updated["frogpilotCarParams"]

    force_onroad_cleared_count = sm["frogpilotDeviceState"].forceOnroadClearedCount
    waiting_for_car_params &= not sm.updated["frogpilotCarParams"]

    if toggles_updated:
      update_toggles(variables, started, model_manager, theme_manager, thread_manager, time_validated, params)

    update_checks_period = now.minute // 5 if variables.frogs_go_moo else now.hour

    run_update_checks |= update_checks_requested
    run_update_checks |= update_checks_period != previous_update_checks_period
    run_update_checks &= time_validated

    previous_update_checks_period = update_checks_period

    if run_update_checks:
      theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      thread_manager.run_in_thread(update_checks, (now, model_manager, speed_limit_reader_models, theme_manager, thread_manager, sm, params, cancel_maps_download, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()

      if time_validated:
        theme_manager.update_active_theme(time_validated, frogpilot_toggles)

        thread_manager.run_in_thread(frogpilot_backups.backup_toggles, (params, True))
        thread_manager.run_in_thread(send_stats, (params, frogpilot_toggles, api))
        thread_manager.run_in_thread(update_checks, (now, model_manager, speed_limit_reader_models, theme_manager, thread_manager, sm, params, cancel_maps_download, frogpilot_toggles, True))

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
