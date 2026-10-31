#!/usr/bin/env python3
import datetime
import json
import time

from pathlib import Path

from cereal import messaging
from openpilot.common.realtime import DT_MDL, Priority, set_realtime_priority
from openpilot.common.time import system_time_valid

from openpilot.frogpilot.system.model_manager import ModelManager
from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common.frogpilot_api import FrogPilotAPI
from openpilot.frogpilot.selfdrive.car.toyota.door_control import lock_doors, send_door_command
from openpilot.frogpilot.selfdrive.navd.map_scheduler import update_maps
from openpilot.frogpilot.selfdrive.pandad.panda_flashing import flash_panda
from openpilot.frogpilot.system.reporting import capture_report
from openpilot.frogpilot.system.speed_limit_reader_models import SpeedLimitReaderModels
from openpilot.frogpilot.system.updated.update_helpers import update_openpilot
from openpilot.frogpilot.common import frogpilot_backups, frogpilot_functions, frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.frogpilot.controls.lib.speed_limit_filler import upload_speed_limits
from openpilot.frogpilot.system.frogpilot_stats import send_stats
from openpilot.frogpilot.system.frogpilot_tracking import FrogPilotTracking

ASSET_CHECK_RATE = (1 / DT_MDL)

class FrogPilotRequests:
  def __init__(self, model_manager, theme_manager):
    self.model_manager = model_manager
    self.theme_manager = theme_manager

    self.the_pond_request_sock = messaging.sub_sock("thePondRequest")
    self.ui_request_sock = messaging.sub_sock("frogpilotUIRequest")

    self.downloading_models = False
    self.downloading_themes = False
    self.flashed_panda = False

    self.account_link_request_time = 0
    self.report_finished_time = 0.0

    self.account_link = "{}"
    self.account_url = ""
    self.door_command = ""
    self.issue_report = "{}"

    self.failed_report = None
    self.pending = {}
    self.request_times = {}

  def take(self, kind):
    pending = self.pending.pop(kind, None)
    if pending is None:
      return None

    self.request_times[kind], request = pending
    return request

  def flash_panda(self):
    self.flashed_panda = flash_panda()

  def send_door_command(self, lock):
    if send_door_command(lock):
      if lock:
        self.door_command = "locked"
      else:
        self.door_command = "unlocked"
    else:
      self.door_command = "failed"

  def update_account_link(self, frogpilot_api, unlink, request_time):
    if unlink:
      frogpilot_api.unlink_account()

    account_link = frogpilot_api.account_link()
    if account_link:
      self.account_link = json.dumps(account_link)
    self.account_link_request_time = request_time

  def publish_issue_report(self, issue_report):
    self.issue_report = json.dumps(issue_report)

  def send_report(self, report, frogpilot_toggles, frogpilot_api):
    try:
      self.failed_report = capture_report(report, frogpilot_toggles, frogpilot_api, self.publish_issue_report)
    except Exception:
      self.failed_report = report
      self.publish_issue_report({"status": "failed"})
      raise
    finally:
      self.report_finished_time = time.monotonic()

  def start_report(self, report, frogpilot_toggles, frogpilot_api):
    self.issue_report = json.dumps({"status": "pending"})
    frogpilot_utilities.run_thread_with_lock("send_report", self.send_report, (report, dict(vars(frogpilot_toggles)), frogpilot_api))

  def update(self, time_validated, sm, frogpilot_toggles, frogpilot_api):
    for msg in messaging.drain_sock(self.ui_request_sock):
      self.pending[msg.frogpilotUIRequest.which()] = (msg.logMonoTime, msg.frogpilotUIRequest)

    for msg in messaging.drain_sock(self.the_pond_request_sock):
      self.pending[msg.thePondRequest.request.which()] = (msg.logMonoTime, msg.thePondRequest.request)

    if not frogpilot_utilities.is_thread_alive("download_models"):
      if self.downloading_models:
        self.model_manager.download_lock.release()
        self.downloading_models = False

      if ("downloadAllModels" in self.pending or "downloadModels" in self.pending) and self.model_manager.download_lock.acquire(blocking=False):
        self.downloading_models = True

        request = self.take("downloadModels")
        if request is not None:
          models = list(request.downloadModels)
          finished_message = "Downloaded!"
        else:
          self.take("downloadAllModels")
          models = self.model_manager.missing_models()
          finished_message = "All models downloaded!"

        self.model_manager.download_state.cancelled = False
        self.model_manager.download_state.progress = "Downloading..."
        frogpilot_utilities.run_thread_with_lock("download_models", self.model_manager.download_models, (models, finished_message))

    if "downloadAllModels" not in self.pending and "downloadModels" not in self.pending and self.take("cancelModelDownload") is not None:
      self.model_manager.download_state.cancelled = True

    if sm["deviceState"].networkMetered and self.model_manager.downloading_automatically:
      self.model_manager.download_state.cancelled = True

    if not frogpilot_utilities.is_thread_alive("flash_panda") and self.take("flashPanda") is not None:
      self.flashed_panda = False
      frogpilot_utilities.run_thread_with_lock("flash_panda", self.flash_panda)

    if not frogpilot_utilities.is_thread_alive("download_themes"):
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
        frogpilot_utilities.run_thread_with_lock("download_themes", self.theme_manager.download_themes, (themes,))

    if "downloadTheme" not in self.pending and self.take("cancelThemeDownload") is not None:
      if frogpilot_utilities.is_thread_alive("download_themes"):
        self.theme_manager.download_state.cancelled = True

    if self.take("refreshTheme") is not None:
      self.theme_manager.previous_asset_mappings = {}
      self.theme_manager.theme_update_count += 1
      self.theme_manager.theme_updated = True

    if not frogpilot_utilities.is_thread_alive("send_door_command"):
      request = self.take("lockDoors")
      if request is not None:
        if sm["deviceState"].started or frogpilot_utilities.is_thread_alive("lock_doors"):
          self.door_command = "busy"
        else:
          self.door_command = "pending"
          frogpilot_utilities.run_thread_with_lock("send_door_command", self.send_door_command, (request.lockDoors,))

    if time_validated and not frogpilot_utilities.is_thread_alive("send_report"):
      report_wait_over = time.monotonic() - self.report_finished_time >= frogpilot_variables.REPORT_WAIT_SECONDS

      request = self.take("issueReport")
      if request is not None:
        self.start_report(json.loads(request.issueReport), frogpilot_toggles, frogpilot_api)
      elif self.failed_report is not None and report_wait_over:
        if self.failed_report.get("MeteredConfirmed") or not sm["deviceState"].networkMetered:
          self.start_report(self.failed_report, frogpilot_toggles, frogpilot_api)
      elif self.failed_report is None and report_wait_over:
        self.issue_report = "{}"

    if not frogpilot_utilities.is_thread_alive("update_account_link"):
      refresh = self.take("accountLink") is not None
      unlink = self.take("unlinkAccount") is not None
      if refresh or unlink:
        request_time = max(self.request_times.get("accountLink", 0), self.request_times.get("unlinkAccount", 0))
        frogpilot_utilities.run_thread_with_lock("update_account_link", self.update_account_link, (frogpilot_api, unlink, request_time))

    request = self.take("accountUrl")
    if request is not None:
      self.account_url = frogpilot_api.account_url(request.accountUrl)

    return self.take("updateToggles") is not None, self.take("updateChecks") is not None

  def publish(self, pm, stats_saved_count):
    frogpilot_account_state_send = messaging.new_message("frogpilotAccountState")
    frogpilotAccountState = frogpilot_account_state_send.frogpilotAccountState

    frogpilotAccountState.link = self.account_link
    frogpilotAccountState.linkRequestTime = self.account_link_request_time
    frogpilotAccountState.url = self.account_url
    frogpilotAccountState.urlRequestTime = self.request_times.get("accountUrl", 0)

    pm.send("frogpilotAccountState", frogpilot_account_state_send)

    frogpilot_process_state_send = messaging.new_message("frogpilotProcessState")
    frogpilotProcessState = frogpilot_process_state_send.frogpilotProcessState

    frogpilotProcessState.doorCommand = self.door_command
    frogpilotProcessState.doorCommandRequestTime = self.request_times.get("lockDoors", 0)
    frogpilotProcessState.downloadingModels = frogpilot_utilities.is_thread_alive("download_models") or self.model_manager.downloading_automatically
    frogpilotProcessState.downloadThemeRequestTime = self.request_times.get("downloadTheme", 0)
    frogpilotProcessState.flashedPanda = self.flashed_panda
    frogpilotProcessState.flashingPanda = frogpilot_utilities.is_thread_alive("flash_panda")
    frogpilotProcessState.flashPandaRequestTime = self.request_times.get("flashPanda", 0)
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

def serve_settings_request(key, function):
  path = Path(frogpilot_variables.params_memory.get(key, encoding="utf-8"))

  try:
    function(path)
  except Exception:
    frogpilot_variables.params_memory.put_bool(key, False)
    raise

  frogpilot_variables.params_memory.remove(key)

def assets_checks():
  if frogpilot_variables.params_memory.get("BackupSettings") not in (None, b"0"):
    frogpilot_utilities.run_thread_with_lock("backup_settings", serve_settings_request, ("BackupSettings", frogpilot_functions.backup_settings))

  if frogpilot_variables.params_memory.get("RestoreSettings") not in (None, b"0"):
    frogpilot_utilities.run_thread_with_lock("restore_settings", serve_settings_request, ("RestoreSettings", frogpilot_functions.restore_settings))

def update_checks(model_manager, now, speed_limit_reader_models, theme_manager, sm, frogpilot_toggles, boot_run=False):
  while not (frogpilot_utilities.is_url_pingable("https://github.com") or frogpilot_utilities.is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  if not sm["deviceState"].networkMetered and frogpilot_toggles.automatic_updates:
    frogpilot_utilities.run_thread_with_lock("update_openpilot", update_openpilot)

  with model_manager.download_lock:
    model_manager.update_models(not sm["deviceState"].networkMetered and frogpilot_toggles.automatically_download_models, sm["deviceState"].started)

  if frogpilot_toggles.show_speed_limits or frogpilot_toggles.speed_limit_controller or frogpilot_toggles.speed_limit_filler:
    speed_limit_reader_models.update(sm)

  if not sm["deviceState"].networkMetered:
    with theme_manager.download_lock:
      theme_manager.update_themes(boot_run)

  if not sm["deviceState"].networkMetered:
    frogpilot_utilities.run_thread_with_lock("update_maps", update_maps, (now,))

def update_toggles(variables, started, model_manager, theme_manager, time_validated):
  frogpilot_toggles = variables.frogpilot_toggles

  previous_holiday_themes = frogpilot_toggles.holiday_themes
  previous_random_themes = frogpilot_toggles.random_themes

  model_manager.models_updated = False
  theme_manager.theme_updated = False

  if not started and frogpilot_variables.params.get_bool("ModelRandomizer"):
    model_manager.randomize_model(new_drive=False)

  variables.update(theme_manager.holiday_theme, started)

  if not started:
    frogpilot_functions.sync_flag_file(frogpilot_paths.HD_PATH, frogpilot_toggles.use_higher_bitrate)
    frogpilot_functions.sync_flag_file(frogpilot_paths.KONIK_PATH, frogpilot_toggles.use_konik_server)

  randomize_theme = frogpilot_toggles.holiday_themes != previous_holiday_themes
  randomize_theme |= frogpilot_toggles.random_themes != previous_random_themes

  theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=randomize_theme)

  if time_validated:
    frogpilot_utilities.run_thread_with_lock("backup_toggles", frogpilot_backups.backup_toggles, (frogpilot_variables.params_cache,), report=False)

def frogpilot_thread():
  set_realtime_priority(Priority.CTRL_LOW)

  frogpilot_paths.ERROR_FILE_PATH.unlink(missing_ok=True)

  frogpilot_api = FrogPilotAPI(frogpilot_variables.params)
  variables = frogpilot_variables.FrogPilotVariables()
  model_manager = ModelManager()
  speed_limit_reader_models = SpeedLimitReaderModels(frogpilot_variables.params)
  theme_manager = ThemeManager(frogpilot_variables.params)

  frogpilot_requests = FrogPilotRequests(model_manager, theme_manager)

  frogpilot_toggles = variables.frogpilot_toggles

  pm = messaging.PubMaster(["frogpilotAccountState", "frogpilotPlan", "frogpilotProcessState"])
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState",
                            "liveLocationKalman", "liveParameters", "longitudinalPlan", "modelV2",
                            "radarState", "frogpilotCarParams", "frogpilotCarState", "frogpilotControlsState",
                            "frogpilotDeviceState", "frogpilotModelV2", "frogpilotNavigation", "frogpilotSignReading"],
                            poll="modelV2")

  run_update_checks = False
  share_speed_limits = True
  started_previously = False
  time_validated = False
  waiting_for_car_params = False

  force_onroad_cleared_count = 0
  stats_saved_count = 0

  previous_update_checks_period = None

  while True:
    sm.update()

    now = datetime.datetime.now(datetime.timezone.utc)

    started = sm["deviceState"].started

    if started and not started_previously:
      frogpilot_paths.ERROR_FILE_PATH.unlink(missing_ok=True)

      frogpilot_planner = FrogPilotPlanner(theme_manager, frogpilot_api)
      frogpilot_tracking = FrogPilotTracking(frogpilot_planner, frogpilot_toggles)

      waiting_for_car_params = True

    elif not started and started_previously:
      run_update_checks = True

      frogpilot_planner.frogpilot_vcruise.slc.close()
      frogpilot_planner.frogpilot_weather.close()

      frogpilot_planner.speed_limit_filler.save()

      share_speed_limits = True

      frogpilot_tracking.save_stats(blocking=True)
      stats_saved_count += 1

      if frogpilot_toggles.model_randomizer:
        model_manager.randomize_model(new_drive=True)

      variables.update(theme_manager.holiday_theme, started)

      frogpilot_functions.sync_flag_file(frogpilot_paths.HD_PATH, frogpilot_toggles.use_higher_bitrate)
      frogpilot_functions.sync_flag_file(frogpilot_paths.KONIK_PATH, frogpilot_toggles.use_konik_server)

      if frogpilot_toggles.lock_doors_timer:
        frogpilot_utilities.run_thread_with_lock("lock_doors", lock_doors, (frogpilot_toggles.lock_doors_timer,), report=False)

      theme_manager.restore_wheel_image()

      if frogpilot_toggles.random_themes:
        theme_manager.update_active_theme(time_validated, frogpilot_toggles, randomize_theme=True)

      if time_validated:
        frogpilot_utilities.run_thread_with_lock("send_stats", send_stats, (frogpilot_variables.params, frogpilot_toggles, frogpilot_api))

    if model_manager.models_updated or theme_manager.theme_updated:
      update_toggles(variables, started, model_manager, theme_manager, time_validated)

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(now, time_validated, sm, frogpilot_toggles)
      frogpilot_planner.publish(theme_manager.theme_update_count, sm, pm, frogpilot_toggles, variables.toggles_update_time)

      frogpilot_tracking.update(now, time_validated, sm)
    elif not started:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      frogpilot_plan_send.frogpilotPlan.themeUpdateCount = theme_manager.theme_update_count
      frogpilot_plan_send.frogpilotPlan.togglesUpdateTime = variables.toggles_update_time
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    if share_speed_limits and not started and frogpilot_utilities.on_unmetered_network(sm["deviceState"]):
      if frogpilot_toggles.speed_limit_filler and frogpilot_toggles.frogpilot_telemetry and not frogpilot_toggles.uploads_disabled:
        frogpilot_utilities.run_thread_with_lock("upload_speed_limits", upload_speed_limits, (frogpilot_variables.params, frogpilot_api))

      share_speed_limits = False

    if sm.frame % ASSET_CHECK_RATE == 0:
      assets_checks()

    toggles_updated, update_checks_requested = frogpilot_requests.update(time_validated, sm, frogpilot_toggles, frogpilot_api)
    frogpilot_requests.publish(pm, stats_saved_count)

    toggles_updated |= sm["frogpilotDeviceState"].forceOnroadClearedCount > force_onroad_cleared_count
    toggles_updated |= waiting_for_car_params and sm.updated["frogpilotCarParams"]

    force_onroad_cleared_count = sm["frogpilotDeviceState"].forceOnroadClearedCount
    waiting_for_car_params &= not sm.updated["frogpilotCarParams"]

    if toggles_updated:
      update_toggles(variables, started, model_manager, theme_manager, time_validated)

    if frogpilot_utilities.is_FrogsGoMoo():
      update_checks_period = now.minute // 5
    else:
      update_checks_period = now.hour

    run_update_checks |= update_checks_requested
    run_update_checks |= update_checks_period != previous_update_checks_period
    run_update_checks &= time_validated

    previous_update_checks_period = update_checks_period

    if run_update_checks:
      theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      frogpilot_utilities.run_thread_with_lock("update_checks", update_checks, (model_manager, now, speed_limit_reader_models, theme_manager, sm, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()
      if not time_validated:
        continue

      theme_manager.update_active_theme(time_validated, frogpilot_toggles)
      frogpilot_utilities.run_thread_with_lock("update_checks", update_checks, (model_manager, now, speed_limit_reader_models, theme_manager, sm, frogpilot_toggles, True))

      frogpilot_utilities.run_thread_with_lock("backup_toggles", frogpilot_backups.backup_toggles, (frogpilot_variables.params_cache, True), report=False)
      frogpilot_utilities.run_thread_with_lock("send_stats", send_stats, (frogpilot_variables.params, frogpilot_toggles, frogpilot_api))

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
