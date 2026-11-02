from openpilot.selfdrive.ui.ui_state import ui_state


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

    return fields


frogpilot_alerts = FrogPilotAlerts()
