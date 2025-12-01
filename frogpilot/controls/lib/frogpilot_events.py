#!/usr/bin/env python3
import numpy as np

from openpilot.common.realtime import DT_MDL
from openpilot.selfdrive.selfdrived.events import ET, EVENT_NAME, FROGPILOT_EVENT_NAME, EventName, FrogPilotEventName, Events

from openpilot.frogpilot.common import frogpilot_variables

GREEN_LIGHT_TIME = 0.35

class FrogPilotEvents:
  def __init__(self, FrogPilotPlanner, error_log):
    self.frogpilot_planner = FrogPilotPlanner

    self.events = Events(frogpilot=True)

    self.startup_seen = False
    self.stopped_for_light = False

    self.green_light_timer = 0
    self.max_acceleration = 0
    self.tracked_lead_distance = 0

    self.played_events = set()

    self.error_log = error_log

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

    if self.frogpilot_planner.frogpilot_vcruise.forcing_stop:
      self.events.add(FrogPilotEventName.forcingStop)

    if not self.frogpilot_planner.lead_relevant and sm["carState"].standstill and sm["carState"].gearShifter not in frogpilot_variables.NON_DRIVING_GEARS:
      self.green_light_timer = self.green_light_timer + DT_MDL if np.interp(6, sm["modelV2"].velocity.t, sm["modelV2"].velocity.x) >= frogpilot_variables.CRUISING_SPEED else 0

      if self.green_light_timer >= GREEN_LIGHT_TIME and self.stopped_for_light and frogpilot_toggles.green_light_alert:
        self.events.add(FrogPilotEventName.greenLight)

      self.stopped_for_light = self.frogpilot_planner.frogpilot_cem.stop_light_detected
    else:
      self.green_light_timer = 0
      self.stopped_for_light = False

    if "holidayActive" not in self.played_events and self.startup_seen and alerts_empty and len(self.events) == 0 and frogpilot_toggles.current_holiday_theme != "stock":
      self.events.add(FrogPilotEventName.holidayActive)

    if not sm["carState"].standstill:
      self.tracked_lead_distance = self.frogpilot_planner.lead_one.dRel if self.frogpilot_planner.lead_one.status else 0
    elif self.frogpilot_planner.lead_one.status and self.tracked_lead_distance != 0 and sm["carState"].gearShifter not in frogpilot_variables.NON_DRIVING_GEARS:
      lead_departing = self.frogpilot_planner.lead_one.dRel - self.tracked_lead_distance >= 1
      lead_departing &= self.frogpilot_planner.lead_one.vLead >= frogpilot_variables.LEAD_DEPARTURE_SPEED

      if lead_departing and frogpilot_toggles.lead_departing_alert:
        self.events.add(FrogPilotEventName.leadDeparting)
    else:
      self.tracked_lead_distance = 0

    if "nnffLoaded" not in self.played_events and self.startup_seen and alerts_empty and len(self.events) == 0 and self.frogpilot_planner.params.get("NNFFModelName") is not None and frogpilot_toggles.nnff:
      self.events.add(FrogPilotEventName.nnffLoaded)

    if self.error_log.is_file():
      self.events.add(FrogPilotEventName.openpilotCrashed)

    if sm["frogpilotCarState"].pedalInterceptorNoBrake:
      if sm["carControl"].enabled:
        self.events.add(FrogPilotEventName.pedalInterceptorNoBrake)
      else:
        self.events.add(FrogPilotEventName.pedalInterceptorNoBrakeNoEntry)

    self.startup_seen |= sm["frogpilotSelfdriveState"].alertText1 == frogpilot_toggles.startup_alert_top and sm["frogpilotSelfdriveState"].alertText2 == frogpilot_toggles.startup_alert_bottom

    self.played_events.update(FROGPILOT_EVENT_NAME[event] for event in self.events.names)
