#!/usr/bin/env python3
from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.selfdrived.events import FROGPILOT_EVENT_NAME, AlertSize, FrogPilotEventName, Events

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_variables

ALWAYS_ON_LATERAL_WAITING_ALERT_TIME = 10

class FrogPilotEvents:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.events = Events(frogpilot=True)

    self.startup_seen = False

    self.always_on_lateral_waiting_timer = 0
    self.max_acceleration = 0

    self.played_events = set()

  def update(self, long_control_active, sm, frogpilot_toggles):
    current_alert = sm["selfdriveState"].alertType
    current_frogpilot_alert = sm["frogpilotSelfdriveState"].alertType

    alerts_empty = all(sm[state].alertText1 == "" and sm[state].alertText2 == "" for state in ["selfdriveState", "frogpilotSelfdriveState"])

    self.events.clear()

    acceleration = sm["carControl"].actuators.accel

    if long_control_active:
      self.max_acceleration = max(acceleration, self.max_acceleration)
    else:
      self.max_acceleration = 0

    if sm["frogpilotCarState"].alwaysOnLateralSteeringOff:
      self.events.add(FrogPilotEventName.alwaysOnLateralSteeringOff)

    if sm["frogpilotCarState"].alwaysOnLateralWaiting:
      waiting_alert_shown = f"{FROGPILOT_EVENT_NAME[FrogPilotEventName.alwaysOnLateralWaiting]}/" in current_frogpilot_alert
      waiting_alert_shown &= sm["frogpilotSelfdriveState"].hasPriorityAlert or sm["selfdriveState"].alertSize == AlertSize.none

      if waiting_alert_shown:
        self.always_on_lateral_waiting_timer += DT_MDL

      if not frogpilot_variables.IS_COMMA_FOUR or self.always_on_lateral_waiting_timer <= ALWAYS_ON_LATERAL_WAITING_ALERT_TIME:
        self.events.add(FrogPilotEventName.alwaysOnLateralWaiting)
    else:
      self.always_on_lateral_waiting_timer = 0

    if frogpilot_paths.ERROR_LOG_PATH.is_file():
      self.events.add(FrogPilotEventName.openpilotCrashed)

    if sm["frogpilotCarState"].pedalInterceptorNoBrake:
      if sm["carControl"].enabled:
        self.events.add(FrogPilotEventName.pedalInterceptorNoBrake)
      else:
        self.events.add(FrogPilotEventName.pedalInterceptorNoBrakeNoEntry)

    if sm["frogpilotCarState"].speedSensorsDisagree:
      self.events.add(FrogPilotEventName.speedSensorsDisagree)

    self.startup_seen |= f"{FROGPILOT_EVENT_NAME[FrogPilotEventName.customStartupAlert]}/" in current_frogpilot_alert

    self.played_events.update(FROGPILOT_EVENT_NAME[event] for event in self.events.names)
