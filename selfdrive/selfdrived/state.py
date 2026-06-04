from cereal import log
from openpilot.selfdrive.selfdrived.events import Events, ET
from openpilot.common.realtime import DT_CTRL

from openpilot.frogpilot.common import frogpilot_utilities

State = log.SelfdriveState.OpenpilotState

SOFT_DISABLE_TIME = 3  # seconds
ACTIVE_STATES = (State.enabled, State.softDisabling, State.overriding)
ENABLED_STATES = (State.preEnabled, *ACTIVE_STATES)

class StateMachine:
  def __init__(self):
    self.current_alert_types = [ET.PERMANENT]
    self.state = State.disabled
    self.soft_disable_timer = 0

  def update(self, events: Events, frogpilot_events: Events):
    # decrement the soft disable timer at every step, as it's reset on
    # entrance in SOFT_DISABLING state
    self.soft_disable_timer = max(0, self.soft_disable_timer - 1)

    self.current_alert_types = [ET.PERMANENT]

    # ENABLED, SOFT DISABLING, PRE ENABLING, OVERRIDING
    if self.state != State.disabled:
      # user and immediate disable always have priority in a non-disabled state
      if frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.USER_DISABLE):
        self.state = State.disabled
        self.current_alert_types.append(ET.USER_DISABLE)

      elif frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.IMMEDIATE_DISABLE):
        self.state = State.disabled
        self.current_alert_types.append(ET.IMMEDIATE_DISABLE)

      else:
        # ENABLED
        if self.state == State.enabled:
          if frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.SOFT_DISABLE):
            self.state = State.softDisabling
            self.soft_disable_timer = int(SOFT_DISABLE_TIME / DT_CTRL)
            self.current_alert_types.append(ET.SOFT_DISABLE)

          elif frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.OVERRIDE_LATERAL, ET.OVERRIDE_LONGITUDINAL):
            self.state = State.overriding
            self.current_alert_types += [ET.OVERRIDE_LATERAL, ET.OVERRIDE_LONGITUDINAL]

        # SOFT DISABLING
        elif self.state == State.softDisabling:
          if not frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.SOFT_DISABLE):
            # no more soft disabling condition, so go back to ENABLED
            self.state = State.enabled

          elif self.soft_disable_timer > 0:
            self.current_alert_types.append(ET.SOFT_DISABLE)

          elif self.soft_disable_timer <= 0:
            self.state = State.disabled

        # PRE ENABLING
        elif self.state == State.preEnabled:
          if not frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.PRE_ENABLE):
            self.state = State.enabled
          else:
            self.current_alert_types.append(ET.PRE_ENABLE)

        # OVERRIDING
        elif self.state == State.overriding:
          if frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.SOFT_DISABLE):
            self.state = State.softDisabling
            self.soft_disable_timer = int(SOFT_DISABLE_TIME / DT_CTRL)
            self.current_alert_types.append(ET.SOFT_DISABLE)
          elif not frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.OVERRIDE_LATERAL, ET.OVERRIDE_LONGITUDINAL):
            self.state = State.enabled
          else:
            self.current_alert_types += [ET.OVERRIDE_LATERAL, ET.OVERRIDE_LONGITUDINAL]

    # DISABLED
    elif self.state == State.disabled:
      if frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.ENABLE):
        if frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.NO_ENTRY):
          self.current_alert_types.append(ET.NO_ENTRY)

        else:
          if frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.PRE_ENABLE):
            self.state = State.preEnabled
          elif frogpilot_utilities.contains_event_type(events, frogpilot_events, ET.OVERRIDE_LATERAL, ET.OVERRIDE_LONGITUDINAL):
            self.state = State.overriding
          else:
            self.state = State.enabled
          self.current_alert_types.append(ET.ENABLE)

    # Check if openpilot is engaged and actuators are enabled
    enabled = self.state in ENABLED_STATES
    active = self.state in ACTIVE_STATES
    if active:
      self.current_alert_types.append(ET.WARNING)
    return enabled, active
