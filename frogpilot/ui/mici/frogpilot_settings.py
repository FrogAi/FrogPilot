import pyray as rl

from openpilot.frogpilot.common import frogpilot_network
from openpilot.selfdrive.ui.mici.widgets.button import BigButton
from openpilot.selfdrive.ui.mici.widgets.dialog import BigDialog
from openpilot.selfdrive.ui.mici.widgets.pairing_dialog import PairingDialog
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.lib.application import FontWeight, gui_app
from openpilot.system.ui.lib.multilang import tr
from openpilot.system.ui.widgets import NavWidget
from openpilot.system.ui.widgets.label import MiciLabel

ADDRESS_BOTTOM_OFFSET = 42
ADDRESS_FONT_SIZE = 26
CODE_BOTTOM_OFFSET = 50
CODE_FONT_SIZE = 32
CODE_ROW_HEIGHT = 42
QR_LEFT_MARGIN = 8
QR_TEXT_MARGIN = 24
TITLE_FONT_SIZE = 48
TITLE_LINE_HEIGHT = 44
TITLE_TOP_OFFSET = 12


class ThePondDialog(PairingDialog):
  def __init__(self, ip_address):
    super().__init__()

    self._ip_address = ip_address

    main_color = rl.Color(255, 255, 255, int(255 * 0.9))

    self._address_label = MiciLabel(ip_address, ADDRESS_FONT_SIZE, font_weight=FontWeight.ROMAN, color=rl.Color(255, 255, 255, int(255 * 0.6)))
    self._code_label = None
    self._title_label = MiciLabel(tr("frogpilot settings"), TITLE_FONT_SIZE, font_weight=FontWeight.BOLD, color=main_color,
                                  line_height=TITLE_LINE_HEIGHT, wrap_text=True)

    if ui_state.params.get_bool("ThePondPairing"):
      code = ui_state.params.get("ThePondPairingCode")
      self._code_label = MiciLabel(tr("code {}").format(code), CODE_FONT_SIZE, font_weight=FontWeight.SEMI_BOLD, color=main_color)

  def _get_pairing_url(self):
    return f"http://{self._ip_address}"

  def _update_state(self):
    NavWidget._update_state(self)

    if ui_state.started and not ui_state.sm["carState"].standstill:
      self._playing_dismiss_animation = True

  def _render(self, rect):
    self._check_qr_refresh()
    self._render_qr_code()

    label_x = self._rect.x + QR_LEFT_MARGIN + self._rect.height + QR_TEXT_MARGIN
    label_width = int(self._rect.width - label_x)
    bottom = self._rect.y + self._rect.height

    self._title_label.set_width(label_width)
    self._title_label.set_position(label_x, self._rect.y + TITLE_TOP_OFFSET)
    self._title_label.render()

    address_y = bottom - ADDRESS_BOTTOM_OFFSET

    if self._code_label is not None:
      address_y -= CODE_ROW_HEIGHT

      self._code_label.set_width(label_width)
      self._code_label.set_position(label_x, bottom - CODE_BOTTOM_OFFSET)
      self._code_label.render()

    self._address_label.set_width(label_width)
    self._address_label.set_position(label_x, address_y)
    self._address_label.render()

    return -1


class FrogPilotBigButton(BigButton):
  def __init__(self):
    super().__init__("frogpilot", tr("open the pond"))

  def _handle_mouse_release(self, mouse_pos):
    super()._handle_mouse_release(mouse_pos)

    ip_address = frogpilot_network.get_ip_address(frogpilot_network.IP_ADDRESS_INTERFACES)

    if ip_address is None:
      gui_app.set_modal_overlay(BigDialog(tr("Please connect to Wi-Fi to open The Pond"), ""))
    else:
      gui_app.set_modal_overlay(ThePondDialog(ip_address))
