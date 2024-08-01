import datetime
import os
import threading

from cereal import log, messaging
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, config_realtime_process
from openpilot.common.time import system_time_valid
from openpilot.system.hardware import HARDWARE

from openpilot.selfdrive.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import backup_toggles, is_url_pingable
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_variables import FrogPilotVariables

OFFLINE = log.DeviceState.NetworkType.none

locks = {
  "backup_toggles": threading.Lock(),
  "time_checks": threading.Lock(),
  "update_frogpilot_params": threading.Lock(),
}

running_threads = {}

def run_thread_with_lock(name, lock, target, args):
  if name not in running_threads or not running_threads[name].is_alive():
    with lock:
      thread = threading.Thread(target=target, args=args)
      thread.start()
      running_threads[name] = thread

def automatic_update_check(started, params):
  update_available = params.get_bool("UpdaterFetchAvailable")
  update_ready = params.get_bool("UpdateAvailable")
  update_state_idle = params.get("UpdaterState", encoding='utf8') == "idle"

  if update_ready and not started:
    HARDWARE.reboot()
  elif update_available:
    os.system("pkill -SIGHUP -f system.updated.updated")
  elif update_state_idle:
    os.system("pkill -SIGUSR1 -f system.updated.updated")

def time_checks(automatic_updates, deviceState, now, started, params, params_memory):
  if deviceState.networkType == OFFLINE:
    return

  if not is_url_pingable("https://github.com"):
    return

  screen_off = deviceState.screenBrightnessPercent == 0
  if automatic_updates and screen_off:
    automatic_update_check(started, params)

def frogpilot_thread():
  config_realtime_process(5, Priority.CTRL_LOW)

  frogpilot_toggles = FrogPilotVariables.toggles

  params = Params()
  params_memory = Params("/dev/shm/params")
  params_storage = Params("/persist/params")

  frogpilot_planner = FrogPilotPlanner()

  run_time_checks = False
  started_previously = False
  time_validated = system_time_valid()
  update_toggles = False

  pm = messaging.PubMaster(['frogpilotPlan'])
  sm = messaging.SubMaster(['carState', 'controlsState', 'deviceState', 'frogpilotCarControl',
                            'frogpilotCarState', 'frogpilotNavigation', 'modelV2', 'radarState'],
                            poll='modelV2', ignore_avg_freq=['radarState'])

  while True:
    sm.update()

    now = datetime.datetime.now()
    deviceState = sm['deviceState']
    started = deviceState.started

    if not started and started_previously:
      frogpilot_planner = FrogPilotPlanner()

    if started and sm.updated['modelV2']:
      frogpilot_planner.update(sm['carState'], sm['controlsState'], sm['frogpilotCarControl'], sm['frogpilotCarState'],
                               sm['frogpilotNavigation'], sm['modelV2'], sm['radarState'], frogpilot_toggles)
      frogpilot_planner.publish(sm, pm, frogpilot_toggles)

    if FrogPilotVariables.toggles_updated:
      update_toggles = True
    elif update_toggles:
      run_thread_with_lock("update_frogpilot_params", locks["update_frogpilot_params"], FrogPilotVariables.update_frogpilot_params, (started,))

      if time_validated and not started:
        run_thread_with_lock("backup_toggles", locks["backup_toggles"], backup_toggles, (params, params_storage))

      update_toggles = False

    started_previously = started

    if now.second == 0:
      run_time_checks = True
    elif run_time_checks or not time_validated:
      run_thread_with_lock("time_checks", locks["time_checks"], time_checks, (frogpilot_toggles.automatic_updates, deviceState, now, started, params, params_memory))
      run_time_checks = False

      if not time_validated:
        time_validated = system_time_valid()
        if not time_validated:
          continue

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
