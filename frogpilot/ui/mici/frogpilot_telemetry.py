import pyray as rl

from openpilot.frogpilot.ui.mici.frogpilot_ui_events import send_ui_request, ui_request_publisher
from openpilot.selfdrive.ui.mici.widgets.button import BigToggle
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.lib.application import FontWeight, gui_app
from openpilot.system.ui.lib.multilang import tr
from openpilot.system.ui.mici_setup import TermsHeader, TermsPage
from openpilot.system.ui.widgets.label import UnifiedLabel


class TelemetryPage(TermsPage):
  def __init__(self, answered_callback):
    super().__init__(lambda: self._answer(True), back_callback=lambda: self._answer(False), back_text=tr("no thanks"), continue_text=tr("yes, share data"))

    self._pm = ui_request_publisher()
    self._title_header = TermsHeader(tr("help improve frogpilot?"), gui_app.texture("icons_mici/setup/green_info.png", 60, 60))
    self._description_label = UnifiedLabel(tr("Your drives make FrogPilot better for everyone, with better steering and braking, support for more cars, " +
                                              "and smarter speed limits.\n\nSharing sends filtered driving logs, short road camera clips, " +
                                              "learned speed limits, and usage stats. Turn it off anytime in settings. " +
                                              "Learn more at frogpilot.com/privacy."), 36, FontWeight.ROMAN)

    self._answered_callback = answered_callback

  def _answer(self, share):
    ui_state.params.put_bool("FrogPilotTelemetry", share)
    ui_state.params.put_bool("FrogPilotTelemetryConfirmed", True)

    send_ui_request(self._pm, "updateToggles")

    self._answered_callback()

  @property
  def _content_height(self):
    return self._description_label.rect.y + self._description_label.rect.height - self._scroll_panel.get_offset()

  def _render_content(self, scroll_offset):
    self._title_header.set_position(self._rect.x + 16, self._rect.y + 12 + scroll_offset)
    self._title_header.render()

    self._description_label.render(rl.Rectangle(
      self._rect.x + 16,
      self._title_header.rect.y + self._title_header.rect.height + self.ITEM_SPACING,
      self._rect.width - 100,
      self._description_label.get_content_height(int(self._rect.width - 100)),
    ))

  def _render(self, rect):
    super()._render(rect)
    return -1


class ShareDataToggle(BigToggle):
  def __init__(self):
    super().__init__(tr("share frogpilot data"))

    self._label.set_font_size(self._get_label_font_size())

    self._question_open = False

    self._pm = ui_request_publisher()

    self._refresh()

    ui_state.add_offroad_transition_callback(self._offroad_transition)

  def show_event(self):
    super().show_event()
    self._refresh()

  def _offroad_transition(self):
    if self._question_open and ui_state.is_onroad():
      self._close_question()

  def _refresh(self):
    self.set_checked(ui_state.params.get_bool("FrogPilotTelemetry") and ui_state.params.get_bool("FrogPilotTelemetryConfirmed"))
    self.set_value(ui_state.params.get("FrogPilotDongleId") or "")

  def _handle_mouse_release(self, mouse_pos):
    if self._checked:
      ui_state.params.put_bool("FrogPilotTelemetry", False)
      send_ui_request(self._pm, "updateToggles")
      self._refresh()
    elif ui_state.is_offroad():
      self._question_open = True
      gui_app.set_modal_overlay(TelemetryPage(self._close_question))

  def _close_question(self):
    self._question_open = False
    gui_app.set_modal_overlay(None)
    self._refresh()
