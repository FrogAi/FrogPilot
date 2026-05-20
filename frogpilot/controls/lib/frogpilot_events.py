#!/usr/bin/env python3
from openpilot.selfdrive.selfdrived.events import ET, EVENT_NAME, FROGPILOT_EVENT_NAME, EventName, FrogPilotEventName, Events


class FrogPilotEvents:
  def __init__(self, FrogPilotPlanner):
    self.frogpilot_planner = FrogPilotPlanner

    self.events = Events(frogpilot=True)

    self.startup_seen = False

    self.max_acceleration = 0

    self.played_events = set()

  def update(self, long_control_active, sm, frogpilot_toggles):
    current_alert = sm["selfdriveState"].alertType
    current_frogpilot_alert = sm["frogpilotSelfdriveState"].alertType

    alerts_empty = all(sm[state].alertSize.raw == 0 and sm[state].alertSound.raw == 0 for state in ["selfdriveState", "frogpilotSelfdriveState"])

    self.events.clear()

    acceleration = sm["carControl"].actuators.accel

    if long_control_active:
      self.max_acceleration = max(acceleration, self.max_acceleration)
    else:
      self.max_acceleration = 0

    self.startup_seen |= current_frogpilot_alert == f"{FROGPILOT_EVENT_NAME[FrogPilotEventName.customStartupAlert]}/{ET.PERMANENT}"

    self.played_events.update(self.events.names)
