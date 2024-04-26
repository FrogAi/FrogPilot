import datetime
import os
import time

import cereal.messaging as messaging

from cereal import car, log
from openpilot.common.params import Params
from openpilot.common.realtime import DT_MDL, Priority, config_realtime_process
from openpilot.common.time import system_time_valid
from openpilot.system.hardware import HARDWARE

from openpilot.selfdrive.frogpilot.controls.frogpilot_planner import FrogPilotPlanner
from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import FrogPilotFunctions

NetworkType = log.DeviceState.NetworkType

def frogpilot_thread():
  config_realtime_process(5, Priority.CTRL_LOW)

  params = Params()
  params_memory = Params("/dev/shm/params")

  frogpilot_functions = FrogPilotFunctions()

  CP = None

  time_validated = system_time_valid()

  pm = messaging.PubMaster(['frogpilotPlan'])
  sm = messaging.SubMaster(['carState', 'controlsState', 'deviceState', 'frogpilotCarControl', 'frogpilotNavigation',
                            'frogpilotPlan', 'liveLocationKalman', 'longitudinalPlan', 'modelV2', 'radarState'],
                           poll='modelV2', ignore_avg_freq=['radarState'])

  while True:
    sm.update()

    deviceState = sm['deviceState']
    started = deviceState.started

    if started:
      if CP is None:
        with car.CarParams.from_bytes(params.get("CarParams", block=True)) as msg:
          CP = msg
          frogpilot_planner = FrogPilotPlanner(CP)
          frogpilot_planner.update_frogpilot_params()

      if sm.updated['modelV2']:
        frogpilot_planner.update(sm['carState'], sm['controlsState'], sm['frogpilotCarControl'], sm['frogpilotNavigation'],
                                  sm['liveLocationKalman'], sm['modelV2'], sm['radarState'])
        frogpilot_planner.publish(sm, pm)

    if params_memory.get_bool("FrogPilotTogglesUpdated"):
      if started:
        frogpilot_planner.update_frogpilot_params()

    if not time_validated:
      time_validated = system_time_valid()
      if not time_validated:
        continue

    time.sleep(DT_MDL)

def main():
  frogpilot_thread()

if __name__ == "__main__":
  main()
