#!/usr/bin/env python3
import datetime
import json
import os
import time

from cereal import car, messaging
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, set_core_affinity
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import PC

from openpilot.frogpilot.assets.theme_manager import ThemeManager
from openpilot.frogpilot.common import frogpilot_api, frogpilot_backups, frogpilot_functions, frogpilot_paths, frogpilot_utilities, frogpilot_variables
from openpilot.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.frogpilot.system.frogpilot_stats import send_stats
from openpilot.frogpilot.system.frogpilot_tracking import FrogPilotTracking

class FrogPilotRequests:
  def __init__(self, theme_manager, thread_manager):
    self.theme_manager = theme_manager
    self.thread_manager = thread_manager

    self.the_pond_request_sock = messaging.sub_sock("thePondRequest")
    self.ui_request_sock = messaging.sub_sock("frogpilotUIRequest")

    self.downloading_themes = False

    self.settings_backup = ""

    self.pending = {}
    self.request_times = {}

  def take(self, kind):
    pending = self.pending.pop(kind, None)
    if pending is None:
      return None

    self.request_times[kind], request = pending
    return request

  def run_settings_backup(self, function, params, argument):
    try:
      function(params, argument)
      self.settings_backup = "succeeded"
    except Exception:
      self.settings_backup = "failed"
      raise

  def update(self, params):
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

    return self.take("updateToggles") is not None, self.take("updateChecks") is not None

  def publish(self, pm, stats_saved_count, toggles_json):
    frogpilot_process_state_send = messaging.new_message("frogpilotProcessState")
    frogpilotProcessState = frogpilot_process_state_send.frogpilotProcessState

    frogpilotProcessState.downloadThemeRequestTime = self.request_times.get("downloadTheme", 0)
    frogpilotProcessState.frogpilotToggles = toggles_json
    frogpilotProcessState.settingsBackup = self.settings_backup
    frogpilotProcessState.settingsBackupRequestTime = max(self.request_times.get("createSettingsBackup", 0), self.request_times.get("restoreSettingsBackup", 0))
    frogpilotProcessState.statsSavedCount = stats_saved_count
    frogpilotProcessState.themeDownloadCount = self.theme_manager.download_count
    frogpilotProcessState.themeDownloadFailedCount = self.theme_manager.download_failed_count
    frogpilotProcessState.themeDownloadProgress = self.theme_manager.download_state.progress
    frogpilotProcessState.themeDownloadSuccessCount = self.theme_manager.download_success_count

    pm.send("frogpilotProcessState", frogpilot_process_state_send)

def transition_offroad(frogpilot_planner, thread_manager, time_validated, params, frogpilot_toggles, api):
  if frogpilot_planner.last_gps_position is not None:
    params.put("LastGPSPosition", json.dumps(frogpilot_planner.last_gps_position))

  if time_validated:
    thread_manager.run_in_thread(send_stats, (params, frogpilot_toggles, api))

def transition_onroad():
  if not PC:
    os.sched_setscheduler(0, os.SCHED_FIFO, os.sched_param(Priority.CTRL_LOW))
  set_core_affinity([5])

  frogpilot_paths.ERROR_LOG_PATH.unlink(missing_ok=True)

def update_checks(now, theme_manager, thread_manager, sm, params, frogpilot_toggles, boot_run=False):
  while not (frogpilot_utilities.is_url_pingable("https://github.com") or frogpilot_utilities.is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  if not sm["deviceState"].networkMetered and frogpilot_toggles.automatic_updates:
    thread_manager.run_in_thread(frogpilot_functions.update_openpilot, (thread_manager, params))

  if not sm["deviceState"].networkMetered:
    with theme_manager.download_lock:
      theme_manager.update_themes(boot_run)

def update_stock_tuning_values(params, car_params_key):
  msg_bytes = params.get(car_params_key)
  if msg_bytes is None:
    return

  CP = messaging.log_from_bytes(msg_bytes, car.CarParams)

  if CP.lateralTuning.which() == "torque":
    friction = CP.lateralTuning.torque.friction
    lat_accel_factor = CP.lateralTuning.torque.latAccelFactor
    steer_kp = 1.0
  else:
    friction = 0.0
    lat_accel_factor = 0.0
    steer_kp = 0.0

  stock_values = {
    "LongitudinalActuatorDelay": CP.longitudinalActuatorDelay,
    "StartAccel": CP.startAccel,
    "SteerDelay": CP.steerActuatorDelay,
    "SteerFriction": friction,
    "SteerKP": steer_kp,
    "SteerLatAccel": lat_accel_factor,
    "SteerRatio": CP.steerRatio,
    "StopAccel": CP.stopAccel,
    "StoppingDecelRate": CP.stoppingDecelRate,
    "VEgoStarting": CP.vEgoStarting,
    "VEgoStopping": CP.vEgoStopping,
  }

  for key, stock_value in stock_values.items():
    previous_stock_value = params.get(f"{key}Stock")

    if stock_value != previous_stock_value and (stock_value != 0 or key == "StartAccel"):
      if round(params.get(key), 6) == round(previous_stock_value, 6):
        params.put(key, stock_value)

      params.put(f"{key}Stock", stock_value)

def update_toggles(variables, started, theme_manager, thread_manager, time_validated, params):
  frogpilot_toggles = variables.frogpilot_toggles

  theme_manager.theme_updated = False

  variables.update(started=started)

  theme_manager.update_active_theme(frogpilot_toggles)

  if time_validated:
    thread_manager.run_in_thread(frogpilot_backups.backup_toggles, (params,))

def frogpilot_thread():
  pm = messaging.PubMaster(["frogpilotAccountState", "frogpilotPlan", "frogpilotProcessState"])
  sm = messaging.SubMaster(["carControl", "carState", "controlsState", "deviceState",
                            "frogpilotCarParams", "frogpilotCarState", "frogpilotDeviceState", "frogpilotModelV2",
                            "frogpilotSelfdriveState", "frogpilotSignReading", "gpsLocation", "gpsLocationExternal",
                            "liveParameters", "longitudinalPlan", "mapdOut", "modelV2", "radarState", "selfdriveState"],
                            poll="modelV2")

  params = Params(return_defaults=True)

  update_stock_tuning_values(params, "CarParamsPersistent")

  api = frogpilot_api.FrogPilotAPI(params)
  api.register_device()
  variables = frogpilot_variables.FrogPilotVariables()
  theme_manager = ThemeManager(params)
  thread_manager = frogpilot_utilities.ThreadManager()

  frogpilot_requests = FrogPilotRequests(theme_manager, thread_manager)

  frogpilot_toggles = variables.frogpilot_toggles

  run_update_checks = False
  started_previously = False
  time_validated = False
  waiting_for_car_params = False

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

      update_toggles(variables, started, theme_manager, thread_manager, time_validated, params)
      transition_offroad(frogpilot_planner, thread_manager, time_validated, params, frogpilot_toggles, api)

      run_update_checks = True
    elif started and not started_previously:
      frogpilot_planner = FrogPilotPlanner()
      frogpilot_tracking = FrogPilotTracking(frogpilot_planner, frogpilot_toggles)

      transition_onroad()

      waiting_for_car_params = True

    if theme_manager.theme_updated:
      update_toggles(variables, started, theme_manager, thread_manager, time_validated, params)

    if started and sm.updated["modelV2"]:
      frogpilot_planner.update(now, time_validated, sm, frogpilot_toggles)
      frogpilot_planner.publish(theme_manager.theme_update_count, sm, pm, frogpilot_toggles)

      frogpilot_tracking.update(now, time_validated, sm)
    elif not started:
      frogpilot_plan_send = messaging.new_message("frogpilotPlan")
      frogpilot_plan_send.frogpilotPlan.themeUpdateCount = theme_manager.theme_update_count
      pm.send("frogpilotPlan", frogpilot_plan_send)

    started_previously = started

    toggles_updated, update_checks_requested = frogpilot_requests.update(params)
    frogpilot_requests.publish(pm, stats_saved_count, variables.toggles_json)

    if waiting_for_car_params and sm.updated["frogpilotCarParams"]:
      update_stock_tuning_values(params, "CarParams")

      toggles_updated = True

    waiting_for_car_params &= not sm.updated["frogpilotCarParams"]

    if toggles_updated:
      update_toggles(variables, started, theme_manager, thread_manager, time_validated, params)

    update_checks_period = now.hour

    run_update_checks |= update_checks_requested
    run_update_checks |= update_checks_period != previous_update_checks_period
    run_update_checks &= time_validated

    previous_update_checks_period = update_checks_period

    if run_update_checks:
      thread_manager.run_in_thread(update_checks, (now, theme_manager, thread_manager, sm, params, frogpilot_toggles))

      run_update_checks = False
    elif not time_validated:
      time_validated = system_time_valid()

      if time_validated:
        thread_manager.run_in_thread(frogpilot_backups.backup_toggles, (params, True))
        thread_manager.run_in_thread(send_stats, (params, frogpilot_toggles, api))
        thread_manager.run_in_thread(update_checks, (now, theme_manager, thread_manager, sm, params, frogpilot_toggles, True))

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
