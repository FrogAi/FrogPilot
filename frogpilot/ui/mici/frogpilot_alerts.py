from cereal import log
from openpilot.frogpilot.common import frogpilot_paths
from openpilot.selfdrive.ui.ui_state import ui_state


AlertSize = log.SelfdriveState.AlertSize
AlertStatus = log.SelfdriveState.AlertStatus

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


class FrogPilotAlerts:
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

    stock_alert = sm.recv_frame["selfdriveState"] >= ui_state.started_frame and ss.alertSize != 0 and not fpss.hasPriorityAlert

    if frogpilot_paths.ERROR_LOG_PATH.exists() and not stock_alert and fpss.alertSize.raw != AlertSize.full:
      fields = self._crash_fields(ss, ui_state.frogpilot_toggles.get("random_events"))

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


frogpilot_alerts = FrogPilotAlerts()
