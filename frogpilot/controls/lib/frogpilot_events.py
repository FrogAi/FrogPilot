#!/usr/bin/env python3
import random

from openpilot.selfdrive.selfdrived.events import ET, FROGPILOT_EVENT_NAME, EventName, FrogPilotEventName, Events

class FrogPilotEvents:
  def __init__(self, FrogPilotPlanner, error_log, ThemeManager):
    self.frogpilot_planner = FrogPilotPlanner

    self.events = Events(frogpilot=True)

    self.startup_seen = False

    self.played_events = set()

  def update(self, v_cruise, sm, frogpilot_toggles):
    self.event_names = {event.name for event in sm["onroadEvents"]}
    self.frogpilot_event_names = {event.name for event in sm["frogpilotOnroadEvents"]}

    alerts_empty = all(sm[state].alertText1 == "" and sm[state].alertText2 == "" for state in ["selfdriveState", "frogpilotSelfdriveState"])

    self.events.clear()

    acceleration = sm["carState"].aEgo

    if not sm["carState"].gasPressed:
      self.max_acceleration = max(acceleration, self.max_acceleration)
    else:
      self.max_acceleration = 0

    self.startup_seen |= sm["frogpilotSelfdriveState"].alertText1 == frogpilot_toggles.startup_alert_top and sm["frogpilotSelfdriveState"].alertText2 == frogpilot_toggles.startup_alert_bottom

    self.played_events.update(FROGPILOT_EVENT_NAME[event] for event in self.events.names)
