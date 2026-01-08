import time

from openpilot.frogpilot.ui.mici.frogpilot_ui_events import send_ui_event, ui_event_publisher
from openpilot.selfdrive.ui.ui_state import ui_state

LONG_PRESS_TIME = 0.5


class ExperimentalPress:
  def __init__(self):
    self.triggered = False

    self._press_time = None

    self._pm = ui_event_publisher()

  def update(self, pressed, swiped):
    if not pressed:
      self.triggered = False
      self._press_time = None
      return

    if self.triggered:
      return

    if swiped:
      self._press_time = None
      return

    now = time.monotonic()

    if self._press_time is None:
      self._press_time = now
    elif now - self._press_time >= LONG_PRESS_TIME and ui_state.started and ui_state.has_longitudinal_control:
      self.triggered = True
      self._change_mode()

  def _change_mode(self):
    toggles = ui_state.frogpilot_toggles

    if toggles.get("conditional_experimental_mode") or toggles.get("slc_fallback_experimental_mode"):
      send_ui_event(self._pm, "experimentalModePressed")
    else:
      ui_state.params.put_bool("ExperimentalMode", not ui_state.params.get_bool("ExperimentalMode"))
