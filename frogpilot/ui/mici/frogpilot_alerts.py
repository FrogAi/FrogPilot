import random

import pyray as rl

from cereal import log
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.frogpilot.common import frogpilot_paths, frogpilot_status
from openpilot.frogpilot.ui.mici.frogpilot_camera_view import frogpilot_camera_view
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.lib.application import gui_app
from openpilot.system.ui.widgets import Widget


AlertSize = log.SelfdriveState.AlertSize
AlertStatus = log.SelfdriveState.AlertStatus

FERG_CHANCE = 0.5
FERG_HEIGHT = 240
FERG_WIDTH = 360

REVERSE_ALERT_TYPE = "reverseGear/permanent"

HIDDEN_ALERT_TYPES = {
  "calibrationIncomplete/permanent",
  "laneChange/warning",
  "noLaneAvailable/warning",
  "personalityChanged/warning",
  "preLaneChangeLeft/warning",
  "preLaneChangeRight/warning",
  "turningLeft/warning",
  "turningRight/warning",
}


class FrogPilotAlerts(Widget):
  def __init__(self):
    super().__init__()

    self._ferg_enabled = False
    self._ferg_showing = False

    self._ferg_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)

    ui_state.add_offroad_transition_callback(self._draw_ferg_chance)

  def _draw_ferg_chance(self):
    self._ferg_enabled = random.random() < FERG_CHANCE

  def alert_fields(self, ss):
    sm = ui_state.sm
    fpss = sm["frogpilotSelfdriveState"]

    fields = None

    frogpilot_alert = sm.recv_frame["frogpilotSelfdriveState"] >= ui_state.started_frame and fpss.alertSize.raw != 0

    if frogpilot_alert and (ss.alertSize == 0 or fpss.hasPriorityAlert):
      fields = {
        "alert_type": fpss.alertType,
        "frogpilot": True,
        "size": fpss.alertSize.raw,
        "status": fpss.alertStatus.raw,
        "text1": fpss.alertText1,
        "text2": fpss.alertText2,
        "visual_alert": ss.alertHudVisual,
      }

    if fields is None:
      alert_type = ss.alertType
    else:
      alert_type = fields["alert_type"]

    if alert_type in HIDDEN_ALERT_TYPES and ui_state.frogpilot_toggles.get("hide_alerts"):
      fields = {}

    if alert_type == REVERSE_ALERT_TYPE and frogpilot_camera_view.driver_view_active:
      fields = {}

    stock_alert = sm.recv_frame["selfdriveState"] >= ui_state.started_frame and ss.alertSize != 0 and not fpss.hasPriorityAlert

    if frogpilot_paths.ERROR_LOG_PATH.exists() and not stock_alert and fpss.alertSize.raw != AlertSize.full:
      random_events = ui_state.frogpilot_toggles.get("random_events")

      if random_events and self._ferg_enabled:
        self._ferg_showing = True

        fields = {}
      else:
        fields = self._crash_fields(ss, random_events)

    return fields

  @staticmethod
  def _crash_fields(ss, random_events):
    if random_events:
      text1 = "Crashed 💩"
      alert_type = "openpilotCrashedRandomEvent"
    else:
      text1 = "Crashed"
      alert_type = "openpilotCrashed"

    return {
      "alert_type": alert_type,
      "frogpilot": True,
      "size": AlertSize.mid,
      "status": AlertStatus.critical,
      "text1": text1,
      "text2": "Post the Error Log in Discord",
      "visual_alert": ss.alertHudVisual,
    }

  def _render(self, rect):
    alpha = self._ferg_filter.update(float(self._ferg_showing and ui_state.started))

    self._ferg_showing = False

    if alpha < frogpilot_status.MIN_VISIBLE_ALPHA:
      return

    texture = gui_app.texture(str(frogpilot_paths.WIDGET_ICONS_PATH / "ferg.png"), FERG_WIDTH, FERG_HEIGHT)
    x = int(rect.x + (rect.width - FERG_WIDTH) / 2)
    y = int(rect.y + (rect.height - FERG_HEIGHT) / 2)

    rl.draw_texture(texture, x, y, rl.Color(255, 255, 255, int(255 * alpha)))


frogpilot_alerts = FrogPilotAlerts()
