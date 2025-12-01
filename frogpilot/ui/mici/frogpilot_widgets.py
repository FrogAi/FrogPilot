import time

from functools import cache

import numpy as np
import pyray as rl

from cereal import car
from openpilot.common.constants import CV
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.frogpilot.common import frogpilot_paths, frogpilot_status
from openpilot.frogpilot.ui.mici.frogpilot_speed_limit import SIGN_MARGIN, US_WIDTH, VIENNA_DIAMETER
from openpilot.frogpilot.ui.mici.frogpilot_theme import theme, WHEEL_SIZE
from openpilot.selfdrive.ui.ui_state import ui_state, UIStatus
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.lib.multilang import tr
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.widgets import Widget

CSC_HOLD_TIME = 1.0
CSC_ICON_OFFSET = 25
CSC_ICON_SIZE = 64
CSC_ICON_TOP = 24
ICON_SIZE = 44
LABEL_FONT_SIZE = 36
LABEL_LEFT = 25
MARGIN_BOTTOM = 14
MARGIN_SIDE = 24
ROAD_NAME_CENTER_Y = 34
ROAD_NAME_LEFT = 88
ROAD_NAME_MAX_FONT_SIZE = 36
ROAD_NAME_MIN_FONT_SIZE = 18
ROAD_NAME_SHADOW_RADIUS_Y = 44
ROAD_NAME_SHADOW_SPREAD = 36
SET_SPEED_FONT_SIZE = 112
SET_SPEED_LABEL_TOP = 109
SET_SPEED_LEFT = 17
SET_SPEED_SHADOW_RADIUS = 81
SET_SPEED_TOP = -4
SHADOW_ALPHA = 0.5
SLOT_GAP = 8
STOPPED_COLOR_TIMES = (60, 150, 300)
STOPPED_COLORS = ((30, 215, 96), (255, 214, 10), (255, 59, 48))
STOPPED_FONT_SIZE = 96
STOPPED_LABEL_TOP = 96
STOPPED_SHADOW_CENTER_Y = 72
STOPPED_SHADOW_RADIUS_Y = 90
STOPPED_SHADOW_SPREAD = 50
STOPPED_TIMER_DELAY = 60
STOPPED_TOP = 4
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

ROAD_SUFFIXES = {
  "Avenue": "Ave",
  "Boulevard": "Blvd",
  "Drive": "Dr",
  "Highway": "Hwy",
  "Lane": "Ln",
  "Parkway": "Pkwy",
  "Road": "Rd",
  "Street": "St",
}


@cache
def road_name_text(name):
  font = gui_app.font(FontWeight.SEMI_BOLD)
  missing_glyph = rl.get_glyph_index(font, ord("?"))

  if any(character != "?" and rl.get_glyph_index(font, ord(character)) == missing_glyph for character in name):
    return ""

  words = name.split(" ")
  words[-1] = ROAD_SUFFIXES.get(words[-1], words[-1])

  return " ".join(words)


def stopped_color(seconds):
  return tuple(int(np.interp(seconds, STOPPED_COLOR_TIMES, channel)) for channel in zip(*STOPPED_COLORS, strict=True))


class FrogPilotWidgets(Widget):
  def __init__(self):
    super().__init__()

    self._can_draw = False
    self._csc_flipped = False
    self._csc_training = False
    self._max_speed_showing = False

    self._stopped_seconds = 0

    self._csc_speed = 0.0
    self._csc_time = -CSC_HOLD_TIME
    self._stopped_start = None

    self._ce_icon = ""
    self._road_name = ""

    self._bottom_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)
    self._ce_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)
    self._csc_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)
    self._road_name_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)
    self._stopped_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)

    self._font_display = gui_app.font(FontWeight.DISPLAY)
    self._font_semi_bold = gui_app.font(FontWeight.SEMI_BOLD)

  def set_can_draw(self, can_draw, max_speed_showing):
    self._can_draw = can_draw and ui_state.started
    self._max_speed_showing = max_speed_showing

  def drawing_top_slot(self):
    return self._csc_filter.x >= frogpilot_status.MIN_VISIBLE_ALPHA or self._stopped_filter.x >= frogpilot_status.MIN_VISIBLE_ALPHA

  def _top_slot_free(self):
    return not self._max_speed_showing and not self.drawing_top_slot()

  def _render(self, rect):
    self._bottom_filter.update(float(self._can_draw and ui_state.status != UIStatus.DISENGAGED and not theme.signal_crosses_bottom()))

    self._render_conditional_status(rect)
    self._render_curve_speed(rect)
    self._render_road_name(rect)
    self._render_stopped_timer(rect)

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

  def _render_curve_speed(self, rect):
    plan = ui_state.sm["frogpilotPlan"]
    now = rl.get_time()

    controlling = plan.cscControllingSpeed and ui_state.sm["selfdriveState"].enabled and not ui_state.sm["carState"].standstill

    if (controlling or plan.cscTraining) and ui_state.frogpilot_toggles.get("csc_status"):
      self._csc_flipped = plan.roadCurvature >= 0
      self._csc_time = now
      self._csc_training = plan.cscTraining

      if controlling:
        self._csc_speed = plan.cscSpeed

    alpha = self._csc_filter.update(float(self._can_draw and not self._max_speed_showing and now - self._csc_time < CSC_HOLD_TIME))

    if alpha < frogpilot_status.MIN_VISIBLE_ALPHA:
      return

    self._draw_shadow(rect.x + SET_SPEED_SHADOW_RADIUS, rect.y + SET_SPEED_SHADOW_RADIUS, SET_SPEED_SHADOW_RADIUS, SET_SPEED_SHADOW_RADIUS, alpha)

    if self._csc_training:
      self._draw_curve_sign(rect, alpha)
      self._draw_text(self._font_semi_bold, tr("LEARNING"), LABEL_FONT_SIZE, rect.x + LABEL_LEFT, rect.y + SET_SPEED_LABEL_TOP, alpha)
    else:
      if ui_state.is_metric:
        speed = self._csc_speed * CV.MS_TO_KPH
      else:
        speed = self._csc_speed * CV.MS_TO_MPH

      self._draw_text(self._font_display, str(round(speed)), SET_SPEED_FONT_SIZE, rect.x + SET_SPEED_LEFT, rect.y + SET_SPEED_TOP, alpha)
      self._draw_text(self._font_semi_bold, tr("CURVE"), LABEL_FONT_SIZE, rect.x + LABEL_LEFT, rect.y + SET_SPEED_LABEL_TOP, alpha)

  def _draw_curve_sign(self, rect, alpha):
    texture = gui_app.texture(str(frogpilot_paths.WIDGET_ICONS_PATH / "curve_speed.png"), CSC_ICON_SIZE, CSC_ICON_SIZE)

    if self._csc_flipped:
      source_width = -texture.width
    else:
      source_width = texture.width

    source = rl.Rectangle(0, 0, source_width, texture.height)
    target = rl.Rectangle(rect.x + CSC_ICON_OFFSET, rect.y + CSC_ICON_TOP, CSC_ICON_SIZE, CSC_ICON_SIZE)
    rl.draw_texture_pro(texture, source, target, rl.Vector2(0, 0), 0, rl.Color(255, 255, 255, int(255 * TEXT_ALPHA * alpha)))

  def _render_road_name(self, rect):
    toggles = ui_state.frogpilot_toggles

    road_name = ""
    if toggles.get("road_name_ui"):
      road_name = road_name_text(ui_state.sm["mapdOut"].roadName)

    if road_name and self._road_name_filter.x < frogpilot_status.MIN_VISIBLE_ALPHA:
      self._road_name = road_name

    visible = road_name != "" and road_name == self._road_name and self._top_slot_free()
    visible = visible and not ui_state.sm["frogpilotPlan"].speedLimitChanged and not theme.static_signal_on_left()
    alpha = self._road_name_filter.update(float(self._can_draw and visible))

    if alpha < frogpilot_status.MIN_VISIBLE_ALPHA:
      return

    if toggles.get("speed_limit_vienna"):
      sign_width = VIENNA_DIAMETER
    else:
      sign_width = US_WIDTH

    left = rect.x + ROAD_NAME_LEFT
    right = rect.x + rect.width - SIGN_MARGIN - sign_width - SIGN_MARGIN

    text, font_size = self._fit_road_name(right - left)
    size = measure_text_cached(self._font_semi_bold, text, font_size)
    center_x = (left + right) / 2
    center_y = rect.y + ROAD_NAME_CENTER_Y

    self._draw_shadow(center_x, center_y, size.x / 2 + ROAD_NAME_SHADOW_SPREAD, ROAD_NAME_SHADOW_RADIUS_Y, alpha)
    self._draw_text(self._font_semi_bold, text, font_size, center_x - size.x / 2, center_y - size.y / 2, alpha)

  def _fit_road_name(self, width):
    text = self._road_name
    full_width = measure_text_cached(self._font_semi_bold, text, ROAD_NAME_MAX_FONT_SIZE).x
    font_size = max(min(int(ROAD_NAME_MAX_FONT_SIZE * width / full_width), ROAD_NAME_MAX_FONT_SIZE), ROAD_NAME_MIN_FONT_SIZE)

    if measure_text_cached(self._font_semi_bold, text, font_size).x <= width:
      return text, font_size

    while measure_text_cached(self._font_semi_bold, text + "...", font_size).x > width:
      text = text[:-1]

    return text.rstrip() + "...", font_size

  def _render_stopped_timer(self, rect):
    car_state = ui_state.sm["carState"]
    now = time.monotonic()

    stopped = bool(car_state.standstill and car_state.gearShifter != car.CarState.GearShifter.reverse and ui_state.frogpilot_toggles.get("stopped_timer"))

    if stopped:
      if self._stopped_start is None:
        self._stopped_start = now

      self._stopped_seconds = int(now - max(self._stopped_start, ui_state.started_time))
    else:
      self._stopped_start = None

    visible = stopped and self._stopped_seconds >= 1 and now - ui_state.started_time >= STOPPED_TIMER_DELAY
    alpha = self._stopped_filter.update(float(self._can_draw and not self._max_speed_showing and visible))

    if alpha < frogpilot_status.MIN_VISIBLE_ALPHA:
      return

    text = f"{self._stopped_seconds // 60}:{self._stopped_seconds % 60:02d}"
    width = measure_text_cached(self._font_display, text, STOPPED_FONT_SIZE).x

    self._draw_shadow(rect.x + SET_SPEED_LEFT + width / 2, rect.y + STOPPED_SHADOW_CENTER_Y, width / 2 + STOPPED_SHADOW_SPREAD, STOPPED_SHADOW_RADIUS_Y, alpha)
    self._draw_text(self._font_display, text, STOPPED_FONT_SIZE, rect.x + SET_SPEED_LEFT, rect.y + STOPPED_TOP, alpha, stopped_color(self._stopped_seconds))
    self._draw_text(self._font_semi_bold, tr("STOPPED"), LABEL_FONT_SIZE, rect.x + LABEL_LEFT, rect.y + STOPPED_LABEL_TOP, alpha)

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
  def _draw_text(font, text, size, x, y, alpha, color=(255, 255, 255)):
    rl.draw_text_ex(font, text, rl.Vector2(x, y), size, 0, rl.Color(*color, int(255 * TEXT_ALPHA * alpha)))

  @staticmethod
  def _draw_shadow(center_x, center_y, radius_x, radius_y, alpha):
    texture = gui_app.texture(str(frogpilot_paths.WIDGET_ICONS_PATH / "shadow.png"), 128, 128)
    source = rl.Rectangle(0, 0, texture.width, texture.height)
    target = rl.Rectangle(center_x - radius_x, center_y - radius_y, radius_x * 2, radius_y * 2)
    rl.draw_texture_pro(texture, source, target, rl.Vector2(0, 0), 0, rl.Color(0, 0, 0, int(255 * SHADOW_ALPHA * alpha)))
