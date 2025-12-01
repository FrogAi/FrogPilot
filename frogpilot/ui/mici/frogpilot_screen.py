import time

from cereal import custom, log

AUTO_BRIGHTNESS = 101
SCREEN_OFF = 0
WAKE_BRIGHTNESS = 5

SELFDRIVE_STATE_TIMEOUT = 5
SELFDRIVE_UNRESPONSIVE_TIMEOUT = 10

WAKE_ALERT_STATUSES = (custom.FrogPilotSelfdriveState.AlertStatus.userPrompt, custom.FrogPilotSelfdriveState.AlertStatus.critical)


class FrogPilotScreen:
  def __init__(self):
    self.keep_awake = False
    self.screen_off = False

    self._status = None

  def brightness(self, ui_state, brightness, auto_offroad, interacting):
    toggles = ui_state.frogpilot_toggles

    if ui_state.started:
      onroad_brightness = toggles.get("screen_brightness_onroad", AUTO_BRIGHTNESS)
      if onroad_brightness != AUTO_BRIGHTNESS:
        return int(max(WAKE_BRIGHTNESS, onroad_brightness) if interacting else onroad_brightness)
    elif auto_offroad:
      offroad_brightness = toggles.get("screen_brightness", AUTO_BRIGHTNESS)
      if offroad_brightness != AUTO_BRIGHTNESS:
        return int(offroad_brightness)
    return brightness

  def interactive_timeout(self, ui_state):
    toggles = ui_state.frogpilot_toggles
    if not toggles.get("screen_management"):
      return None
    return int(toggles["screen_timeout_onroad" if ui_state.ignition else "screen_timeout"])

  def update(self, ui_state):
    sm = ui_state.sm
    toggles = ui_state.frogpilot_toggles

    self.screen_off = toggles.get("screen_brightness_onroad") == SCREEN_OFF

    status = ui_state.status
    status_changed = status != self._status and status != type(status).OVERRIDE
    self._status = status

    ss = sm["selfdriveState"]
    alert = ss.alertSize != log.SelfdriveState.AlertSize.none or sm["frogpilotSelfdriveState"].alertStatus in WAKE_ALERT_STATUSES

    ss_missing = time.monotonic() - sm.recv_time["selfdriveState"]
    unresponsive = (sm.recv_frame["selfdriveState"] >= ui_state.started_frame and ss.enabled and not toggles.get("force_onroad") and
                    SELFDRIVE_STATE_TIMEOUT < ss_missing < SELFDRIVE_STATE_TIMEOUT + SELFDRIVE_UNRESPONSIVE_TIMEOUT)

    self.keep_awake = ui_state.started and self.screen_off and (alert or status_changed or unresponsive)
