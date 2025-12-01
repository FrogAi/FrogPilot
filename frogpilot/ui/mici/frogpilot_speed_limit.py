import math

import pyray as rl

from openpilot.common.constants import CV
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.frogpilot.common import frogpilot_paths, frogpilot_status
from openpilot.frogpilot.ui.mici.frogpilot_theme import theme
from openpilot.frogpilot.ui.mici.frogpilot_ui_events import send_ui_event, ui_event_publisher
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.lib.multilang import tr
from openpilot.system.ui.lib.scroll_panel2 import MIN_DRAG_PIXELS
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.widgets import Widget

LABEL_FONT_SIZE = 24
LABEL_TOP_OFFSET = 8
OFFSET_FONT_SIZE = 24
OVERRIDDEN_TEXT_STRENGTH = 0.25
PENDING_PULSE_MIN = 0.55
SIGN_MARGIN = 12

US_HEIGHT = 90
US_HEIGHT_WITH_OFFSET = 110
US_NUMBER_FONT_SIZE = 44
US_NUMBER_FONT_SIZE_LONG = 36
US_NUMBER_TOP_OFFSET = 31
US_OFFSET_TOP_OFFSET = 78
US_WIDTH = 80

VIENNA_DIAMETER = 84
VIENNA_NUMBER_FONT_SIZE = 40
VIENNA_NUMBER_FONT_SIZE_SMALL = 36
VIENNA_NUMBER_TOP_OFFSET = 11
VIENNA_OFFSET_TOP_OFFSET = 44

BLACK = rl.Color(0, 0, 0, 255)
RED = rl.Color(255, 59, 48, 255)
WHITE = rl.Color(255, 255, 255, 255)


def faded(color, strength):
  return rl.Color(color.r, color.g, color.b, int(color.a * strength))


def speed_text(speed):
  if ui_state.is_metric:
    converted_speed = speed * CV.MS_TO_KPH
  else:
    converted_speed = speed * CV.MS_TO_MPH

  if converted_speed <= 1:
    return "–"

  return str(round(converted_speed))


def offset_text(offset):
  if ui_state.is_metric:
    converted_offset = round(offset * CV.MS_TO_KPH)
  else:
    converted_offset = round(offset * CV.MS_TO_MPH)

  if converted_offset == 0:
    return "–"

  if converted_offset > 0:
    return f"+{converted_offset}"

  return str(converted_offset)


class FrogPilotSpeedLimit(Widget):
  def __init__(self):
    super().__init__()

    self._can_draw = False

    self._pending_limit = 0.0
    self._press_x = 0.0

    self._current_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)
    self._pending_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)
    self._text_strength_filter = FirstOrderFilter(1.0, 0.1, 1 / gui_app.target_fps)
    self._font_bold = gui_app.font(FontWeight.BOLD)
    self._font_semi_bold = gui_app.font(FontWeight.SEMI_BOLD)
    self._pm = ui_event_publisher()
    self._drawn_pending_rect = rl.Rectangle(0, 0, 0, 0)
    self._pending_rect = rl.Rectangle(0, 0, 0, 0)

  @property
  def _hit_rect(self):
    return self._pending_rect

  def set_can_draw(self, can_draw):
    self._can_draw = can_draw

  def pending_sign_contains(self, mouse_pos):
    return rl.check_collision_point_rec(mouse_pos, self._drawn_pending_rect)

  def _handle_mouse_press(self, mouse_pos):
    self._press_x = mouse_pos.x

  def _handle_mouse_release(self, mouse_pos):
    if abs(mouse_pos.x - self._press_x) > MIN_DRAG_PIXELS:
      return

    send_ui_event(self._pm, "speedLimitAccepted")

  def _render(self, rect):
    toggles = ui_state.frogpilot_toggles
    plan = ui_state.sm["frogpilotPlan"]

    enabled = toggles.get("show_speed_limits") or toggles.get("speed_limit_controller")
    can_show = bool(self._can_draw and ui_state.started and not theme.static_signal_on_right() and enabled)
    pending = can_show and plan.speedLimitChanged

    if plan.speedLimitChanged:
      self._pending_limit = plan.unconfirmedSlcSpeedLimit

    current_alpha = self._current_filter.update(float(can_show and (plan.speedLimitChanged or not toggles.get("hide_speed_limit"))))
    pending_alpha = self._pending_filter.update(float(pending))

    right = rect.x + rect.width - SIGN_MARGIN
    vienna = toggles.get("speed_limit_vienna", False)

    if vienna:
      sign_width = VIENNA_DIAMETER
    else:
      sign_width = US_WIDTH

    if current_alpha >= frogpilot_status.MIN_VISIBLE_ALPHA:
      self._draw_current_limit(rect, right, plan, vienna, current_alpha)

    self._drawn_pending_rect = rl.Rectangle(0, 0, 0, 0)

    if pending_alpha >= frogpilot_status.MIN_VISIBLE_ALPHA:
      self._drawn_pending_rect = self._draw_pending_limit(rect, right - sign_width - SIGN_MARGIN, vienna, pending_alpha)

    if pending:
      self._pending_rect = self._drawn_pending_rect
    else:
      self._pending_rect = rl.Rectangle(0, 0, 0, 0)

  def _draw_current_limit(self, rect, right, plan, vienna, alpha):
    overridden_speed = 0.0
    if plan.slcSpeedLimit != 0:
      overridden_speed = plan.slcOverriddenSpeed

    offset_row = ui_state.frogpilot_toggles.get("show_speed_limit_offset", False)
    show_offset = overridden_speed == 0 and offset_row

    if overridden_speed != 0:
      limit = overridden_speed
      text_strength = self._text_strength_filter.update(OVERRIDDEN_TEXT_STRENGTH)
    elif show_offset:
      limit = plan.slcSpeedLimit
      text_strength = self._text_strength_filter.update(1.0)
    else:
      limit = plan.slcSpeedLimit + plan.slcSpeedLimitOffset
      text_strength = self._text_strength_filter.update(1.0)

    if show_offset:
      sign_offset = offset_text(plan.slcSpeedLimitOffset)
    else:
      sign_offset = None

    text_color = faded(BLACK, alpha * text_strength)

    if vienna:
      self._draw_vienna_sign(rect, right, speed_text(limit), offset_row, sign_offset, text_color, alpha, alpha)
    else:
      self._draw_us_sign(rect, right, tr("LIMIT"), speed_text(limit), offset_row, sign_offset, text_color, faded(BLACK, alpha), alpha)

  def _draw_pending_limit(self, rect, right, vienna, alpha):
    pulse = PENDING_PULSE_MIN + (1 - PENDING_PULSE_MIN) * (0.5 + 0.5 * math.cos(rl.get_time() * math.tau))
    limit = speed_text(self._pending_limit)
    text_color = faded(BLACK, alpha)

    if vienna:
      return self._draw_vienna_sign(rect, right, limit, False, None, text_color, alpha, alpha * pulse)

    return self._draw_us_sign(rect, right, tr("NEW"), limit, False, None, text_color, faded(RED, alpha * pulse), alpha)

  def _draw_us_sign(self, rect, right, label, limit, offset_row, sign_offset, text_color, frame_color, alpha):
    if offset_row:
      height = US_HEIGHT_WITH_OFFSET
    else:
      height = US_HEIGHT

    sign_rect = rl.Rectangle(right - US_WIDTH, rect.y + SIGN_MARGIN, US_WIDTH, height)

    self._draw_image(f"us_{height}", sign_rect, faded(WHITE, alpha))
    self._draw_image(f"us_{height}_frame", sign_rect, frame_color)

    center_x = sign_rect.x + US_WIDTH / 2

    if len(limit) >= 3:
      number_size = US_NUMBER_FONT_SIZE_LONG
    else:
      number_size = US_NUMBER_FONT_SIZE

    self._draw_centered(self._font_semi_bold, label, LABEL_FONT_SIZE, center_x, sign_rect.y + LABEL_TOP_OFFSET, text_color)
    self._draw_centered(self._font_bold, limit, number_size, center_x, sign_rect.y + US_NUMBER_TOP_OFFSET + (US_NUMBER_FONT_SIZE - number_size) / 2, text_color)

    if sign_offset is not None:
      self._draw_centered(self._font_semi_bold, sign_offset, OFFSET_FONT_SIZE, center_x, sign_rect.y + US_OFFSET_TOP_OFFSET, text_color)

    return sign_rect

  def _draw_vienna_sign(self, rect, right, limit, offset_row, sign_offset, text_color, alpha, ring_alpha):
    radius = VIENNA_DIAMETER / 2
    sign_rect = rl.Rectangle(right - VIENNA_DIAMETER, rect.y + SIGN_MARGIN, VIENNA_DIAMETER, VIENNA_DIAMETER)
    center = rl.Vector2(sign_rect.x + radius, sign_rect.y + radius)

    self._draw_image("vienna", sign_rect, faded(WHITE, alpha))
    self._draw_image("vienna_ring", sign_rect, faded(RED, ring_alpha))

    if offset_row:
      self._draw_centered(self._font_bold, limit, VIENNA_NUMBER_FONT_SIZE_SMALL, center.x, sign_rect.y + VIENNA_NUMBER_TOP_OFFSET, text_color)

      if sign_offset is not None:
        self._draw_centered(self._font_semi_bold, sign_offset, OFFSET_FONT_SIZE, center.x, sign_rect.y + VIENNA_OFFSET_TOP_OFFSET, text_color)
    else:
      if len(limit) >= 3:
        number_size = VIENNA_NUMBER_FONT_SIZE_SMALL
      else:
        number_size = VIENNA_NUMBER_FONT_SIZE

      text_height = measure_text_cached(self._font_bold, limit, number_size).y
      self._draw_centered(self._font_bold, limit, number_size, center.x, center.y - text_height / 2, text_color)

    return sign_rect

  @staticmethod
  def _draw_image(name, sign_rect, color):
    path = frogpilot_paths.SPEED_LIMIT_SIGNS_PATH / f"{name}.png"
    texture = gui_app.texture(str(path), int(sign_rect.width), int(sign_rect.height))
    rl.draw_texture(texture, int(sign_rect.x), int(sign_rect.y), color)

  @staticmethod
  def _draw_centered(font, text, size, center_x, y, color):
    width = measure_text_cached(font, text, size).x
    rl.draw_text_ex(font, text, rl.Vector2(center_x - width / 2, y), size, 0, color)
