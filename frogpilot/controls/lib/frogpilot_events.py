#!/usr/bin/env python3
from openpilot.selfdrive.selfdrived.events import FROGPILOT_EVENT_NAME, Events

class FrogPilotEvents:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.events = Events(frogpilot=True)

    self.startup_seen = False

    self.played_events = set()

  def update(self, sm, frogpilot_toggles):
    current_alert = sm["selfdriveState"].alertType
    current_frogpilot_alert = sm["frogpilotSelfdriveState"].alertType

    alerts_empty = all(sm[state].alertText1 == "" and sm[state].alertText2 == "" for state in ["selfdriveState", "frogpilotSelfdriveState"])

    self.events.clear()

    acceleration = sm["carControl"].actuators.accel

    self.startup_seen |= sm["frogpilotSelfdriveState"].alertText1 == frogpilot_toggles.startup_alert_top and sm["frogpilotSelfdriveState"].alertText2 == frogpilot_toggles.startup_alert_bottom

    self.played_events.update(FROGPILOT_EVENT_NAME[event] for event in self.events.names)
