import pyray as rl

from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.frogpilot.common import frogpilot_paths, frogpilot_status
from openpilot.frogpilot.ui.mici.frogpilot_theme import theme, WHEEL_SIZE
from openpilot.selfdrive.ui.ui_state import ui_state, UIStatus
from openpilot.system.ui.lib.application import gui_app
from openpilot.system.ui.widgets import Widget

ICON_SIZE = 44
MARGIN_BOTTOM = 14
MARGIN_SIDE = 24
SHADOW_ALPHA = 0.5
SLOT_GAP = 8
TEXT_ALPHA = 0.9

CE_ICONS = {
  frogpilot_status.CE_STATUS["CURVATURE"]: "curve_icon",
  frogpilot_status.CE_STATUS["LEAD"]: "lead_icon",
  frogpilot_status.CE_STATUS["SPEED"]: "conditional_speed_icon",
  frogpilot_status.CE_STATUS["SPEED_LIMIT"]: "conditional_speed_icon",
  frogpilot_status.CE_STATUS["STOP_LIGHT"]: "light_icon",
  frogpilot_status.CE_STATUS["USER_DISABLED"]: "chill_mode_icon",
  frogpilot_status.CE_STATUS["USER_OVERRIDDEN"]: "experimental_mode_icon",
}


class FrogPilotWidgets(Widget):
  def __init__(self):
    super().__init__()

    self._can_draw = False
    self._max_speed_showing = False

    self._ce_icon = ""

    self._bottom_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)
    self._ce_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)

  def set_can_draw(self, can_draw, max_speed_showing):
    self._can_draw = can_draw and ui_state.started
    self._max_speed_showing = max_speed_showing

  def _render(self, rect):
    self._bottom_filter.update(float(self._can_draw and ui_state.status != UIStatus.DISENGAGED))

    self._render_conditional_status(rect)

  def _render_conditional_status(self, rect):
    status = ui_state.sm["frogpilotPlan"].ceStatus
    car_state = ui_state.sm["carState"]
    icon = None

    if status == frogpilot_status.CE_STATUS["SIGNAL"]:
      if car_state.leftBlinker:
        icon = "conditional_turn_left_icon"
      elif car_state.rightBlinker:
        icon = "conditional_turn_right_icon"
    else:
      icon = CE_ICONS.get(status)

    visible = bool(icon is not None and ui_state.frogpilot_toggles.get("cem_status"))

    if visible and self._ce_filter.x < frogpilot_status.MIN_VISIBLE_ALPHA:
      self._ce_icon = icon

    alpha = self._ce_filter.update(float(visible and icon == self._ce_icon)) * self._bottom_filter.x

    if alpha >= frogpilot_status.MIN_VISIBLE_ALPHA:
      x, y = self._right_slot(rect, 0)
      self._draw_icon(self._ce_icon, x, y, alpha)

  @staticmethod
  def _slide_slot(slot_filter, visibility_filter, slot):
    if visibility_filter.x < frogpilot_status.MIN_VISIBLE_ALPHA:
      slot_filter.x = slot

    return slot_filter.update(slot)

  @staticmethod
  def _left_slot(rect, index):
    x = rect.x + MARGIN_SIDE
    y = rect.y + rect.height - MARGIN_BOTTOM - WHEEL_SIZE - (index + 1) * (ICON_SIZE + SLOT_GAP)

    return x, y

  @staticmethod
  def _right_slot(rect, index):
    x = rect.x + rect.width - MARGIN_SIDE - ICON_SIZE
    y = rect.y + rect.height - MARGIN_BOTTOM - ICON_SIZE - index * (ICON_SIZE + SLOT_GAP)

    return x, y

  def _draw_icon(self, name, x, y, alpha):
    self._draw_shadow(x + ICON_SIZE / 2, y + ICON_SIZE / 2, ICON_SIZE, ICON_SIZE, alpha)

    texture = gui_app.texture(str(frogpilot_paths.WIDGET_ICONS_PATH / f"{name}.png"), ICON_SIZE, ICON_SIZE)
    rl.draw_texture(texture, int(x), int(y), rl.Color(255, 255, 255, int(255 * TEXT_ALPHA * alpha)))

  @staticmethod
  def _draw_shadow(center_x, center_y, radius_x, radius_y, alpha):
    texture = gui_app.texture(str(frogpilot_paths.WIDGET_ICONS_PATH / "shadow.png"), 128, 128)
    source = rl.Rectangle(0, 0, texture.width, texture.height)
    target = rl.Rectangle(center_x - radius_x, center_y - radius_y, radius_x * 2, radius_y * 2)
    rl.draw_texture_pro(texture, source, target, rl.Vector2(0, 0), 0, rl.Color(0, 0, 0, int(255 * SHADOW_ALPHA * alpha)))
