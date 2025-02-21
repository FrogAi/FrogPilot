#!/usr/bin/env python3
from cereal import car
from openpilot.common.params import Params
from openpilot.common.realtime import Priority, config_realtime_process
from openpilot.common.swaglog import cloudlog
from openpilot.selfdrive.controls.lib.longitudinal_planner import LongitudinalPlanner
import cereal.messaging as messaging

from openpilot.selfdrive.frogpilot.frogpilot_variables import get_frogpilot_toggles

def publish_ui_plan(sm, pm, longitudinal_planner):
  ui_send = messaging.new_message('uiPlan')
  ui_send.valid = sm.all_checks(service_list=['carState', 'controlsState', 'modelV2'])
  uiPlan = ui_send.uiPlan
  uiPlan.frameId = sm['modelV2'].frameId
  uiPlan.position.x = list(sm['modelV2'].position.x)
  uiPlan.position.y = list(sm['modelV2'].position.y)
  uiPlan.position.z = list(sm['modelV2'].position.z)
  uiPlan.accel = longitudinal_planner.a_desired_trajectory_full.tolist()
  pm.send('uiPlan', ui_send)

def plannerd_thread():
  config_realtime_process(5, Priority.CTRL_LOW)

  cloudlog.info("plannerd is waiting for CarParams")
  params = Params()
  with car.CarParams.from_bytes(params.get("CarParams", block=True)) as msg:
    CP = msg
  cloudlog.info("plannerd got CarParams: %s", CP.carName)

  longitudinal_planner = LongitudinalPlanner(CP)
  pm = messaging.PubMaster(['longitudinalPlan', 'uiPlan'])
  sm = messaging.SubMaster(['carControl', 'carState', 'controlsState', 'liveParameters', 'radarState', 'modelV2',
                            'frogpilotCarState', 'frogpilotPlan'],
                           poll='modelV2', ignore_avg_freq=['radarState'])

  # FrogPilot variables
  frogpilot_toggles = get_frogpilot_toggles()

  classic_model = frogpilot_toggles.classic_model
  radarless_model = frogpilot_toggles.radarless_model

  while True:
    sm.update()
    if sm.updated['modelV2']:
      longitudinal_planner.update(radarless_model, sm, frogpilot_toggles)
      longitudinal_planner.publish(classic_model, sm, pm, frogpilot_toggles)
      publish_ui_plan(sm, pm, longitudinal_planner)

    # Update FrogPilot parameters
    if sm['frogpilotPlan'].togglesUpdated:
      frogpilot_toggles = get_frogpilot_toggles()

def main():
  plannerd_thread()


if __name__ == "__main__":
  main()
