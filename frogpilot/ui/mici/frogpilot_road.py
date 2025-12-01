import colorsys

import numpy as np
import pyray as rl

from openpilot.common.constants import CV
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.frogpilot.common import frogpilot_paths, frogpilot_status
from openpilot.frogpilot.ui.mici.frogpilot_theme import hex_to_color, PATH_ALPHAS
from openpilot.selfdrive.ui.ui_state import ui_state, UIStatus
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.lib.multilang import tr
from openpilot.system.ui.lib.shader_polygon import draw_polygon, Gradient
from openpilot.system.ui.lib.text_measure import measure_text_cached

ACCELERATION_PATH_MIN_ACCEL = 0.25
ALWAYS_ON_LATERAL_COLOR = rl.Color(10, 186, 181, 255)
ALWAYS_ON_LATERAL_PATH_WIDTH_FACTOR = 0.75
CEM_DISABLED_COLOR = rl.Color(255, 255, 0, 255)
DYNAMIC_PATH_WIDTH_FACTOR = 0.5
EXPERIMENTAL_MODE_COLOR = rl.Color(218, 111, 37, 255)
STOP_DISTANCE_FONT_SIZE = 36
STOP_DISTANCE_MARGIN = 8
STOP_SIGN_HEIGHT = 64
STOP_SIGN_MARGIN = 4
STOP_SPEED = 1.0

BAND_ALPHAS = (0.4, 0.35, 0.0)
BAND_STOPS = [0.0, 0.5, 1.0]
BLIND_SPOT_PATH_HUE = 0.0


def hsla_to_color(hue, saturation, lightness, alpha):
  red, green, blue = colorsys.hls_to_rgb(hue, lightness, saturation)

  return rl.Color(int(red * 255), int(green * 255), int(blue * 255), int(alpha * 255))


STOCK_PATH_EDGE_COLORS = [
  hsla_to_color(148 / 360, 0.94, 0.41, 0.4),
  hsla_to_color(112 / 360, 1.0, 0.54, 0.35),
  hsla_to_color(112 / 360, 1.0, 0.54, 0.0),
]


def band_gradient(colors):
  return Gradient(start=(0.0, 1.0), end=(0.0, 0.0), colors=colors, stops=BAND_STOPS)


def solid_band_gradient(color, strength=1.0):
  return band_gradient([rl.Color(color.r, color.g, color.b, int(255 * alpha * strength)) for alpha in BAND_ALPHAS])


class FrogPilotRoad:
  def __init__(self):
    self._stopping_distance = 0.0

    self._blind_spot_filters = {side: FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps) for side in (True, False)}

    self._adjacent_paths = []
    self._path_edges = []

    self._stop_sign_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)

  def max_draw_distance(self, stock_distance):
    if not ui_state.frogpilot_toggles.get("model_ui"):
      return stock_distance

    return math.inf

  def lane_line_width(self, stock_width):
    toggles = ui_state.frogpilot_toggles

    if not toggles.get("model_ui"):
      return stock_width

    return toggles["lane_line_width"]

  def road_edge_width(self, stock_width):
    toggles = ui_state.frogpilot_toggles

    if not toggles.get("model_ui"):
      return stock_width

    return toggles["road_edge_width"]

  def path_width(self, stock_width):
    toggles = ui_state.frogpilot_toggles

    if not toggles.get("model_ui"):
      return stock_width

    if ui_state.status == UIStatus.ALWAYS_ON_LATERAL and toggles.get("dynamic_path_width"):
      return toggles["path_width"] * ALWAYS_ON_LATERAL_PATH_WIDTH_FACTOR

    if ui_state.status != UIStatus.ENGAGED and toggles.get("dynamic_path_width"):
      return toggles["path_width"] * DYNAMIC_PATH_WIDTH_FACTOR

    return toggles["path_width"]

  def inner_path_width(self, path_width):
    toggles = ui_state.frogpilot_toggles

    if not toggles.get("model_ui"):
      return path_width

    return path_width * (1 - toggles["path_edge_width"] / 100)

  def update_path_edges(self, map_line_to_polygon, path_points, path_width, inner_width, path_offset_z, max_idx):
    self._path_edges = []

    if inner_width == path_width:
      return

    half_band = (path_width - inner_width) / 2
    center_offset = inner_width + half_band

    for side_offset in (-center_offset, center_offset):
      edge_points = path_points + np.array([0, side_offset, 0], dtype=np.float32)
      self._path_edges.append(map_line_to_polygon(edge_points, half_band, path_offset_z, max_idx, allow_invert=False))

  def draw_path_edges(self, rect, experimental_mode):
    if not self._path_edges:
      return

    gradient = self._path_edge_gradient(experimental_mode)

    for path_edge in self._path_edges:
      draw_polygon(rect, path_edge, gradient=gradient)

  def update_adjacent_paths(self, map_line_to_polygon, lane_lines, max_idx):
    self._adjacent_paths = []

    for left, blind_spot_filter in self._blind_spot_filters.items():
      blind_spot_filter.update(float(self._blind_spot_active(left) and ui_state.frogpilot_toggles.get("blind_spot_path", False)))

    plan = ui_state.sm["frogpilotPlan"]

    for left, first_line, second_line, lane_width in ((True, 0, 1, plan.laneWidthLeft), (False, 2, 3, plan.laneWidthRight)):
      if lane_width != 0 and self._adjacent_path_needed(left):
        center_line = (lane_lines[first_line].raw_points + lane_lines[second_line].raw_points) / 2
        self._adjacent_paths.append((left, lane_width, map_line_to_polygon(center_line, lane_width / 2, 0.0, max_idx, allow_invert=False)))

  def draw_adjacent_paths(self, rect):
    toggles = ui_state.frogpilot_toggles

    for left, lane_width, polygon in self._adjacent_paths:
      blind_spot_strength = self._blind_spot_filters[left].x

      if toggles.get("adjacent_paths"):
        adjacent_color = hsla_to_color(self._adjacent_path_hue(lane_width), 0.75, 0.5, 1.0)
        draw_polygon(rect, polygon, gradient=solid_band_gradient(adjacent_color, 1 - blind_spot_strength))

      if blind_spot_strength > frogpilot_status.MIN_VISIBLE_ALPHA:
        draw_polygon(rect, polygon, gradient=solid_band_gradient(hsla_to_color(BLIND_SPOT_PATH_HUE, 0.75, 0.5, 1.0), blind_spot_strength))

  def _adjacent_path_needed(self, left):
    toggles = ui_state.frogpilot_toggles

    if self._blind_spot_filters[left].x > frogpilot_status.MIN_VISIBLE_ALPHA:
      return True

    return toggles.get("adjacent_paths", False) or (self._blind_spot_active(left) and toggles.get("blind_spot_path", False))

  def _blind_spot_active(self, left):
    car_state = ui_state.sm["carState"]

    if left:
      return car_state.leftBlindspot

    return car_state.rightBlindspot

  def _adjacent_path_hue(self, lane_width):
    requirement = ui_state.frogpilot_toggles["lane_detection_width"]

    if requirement > 0:
      ratio = min(lane_width / requirement, 1.0)
    else:
      ratio = 1.0

    return ratio ** 2 * 120 / 360

  def update_stopping_point(self):
    visible = False

    if ui_state.sm["frogpilotPlan"].redLight and ui_state.frogpilot_toggles.get("show_stopping_point"):
      stopping_distance = self._model_stopping_distance()

      if stopping_distance > 0:
        visible = True
        self._stopping_distance = stopping_distance

    self._stop_sign_filter.update(float(visible))

  def draw_stopping_point(self, rect, path_points):
    toggles = ui_state.frogpilot_toggles
    alpha = self._stop_sign_filter.x

    if alpha < frogpilot_status.MIN_VISIBLE_ALPHA or not path_points.size:
      return

    far_end = len(path_points) // 2
    center_x = (path_points[far_end - 1][0] + path_points[far_end][0]) / 2
    center_y = (path_points[far_end - 1][1] + path_points[far_end][1]) / 2

    color = rl.Color(255, 255, 255, int(255 * 0.9 * alpha))
    texture = gui_app.texture(str(frogpilot_paths.STOP_SIGN_PATH), STOP_SIGN_HEIGHT, STOP_SIGN_HEIGHT)

    x = center_x - texture.width / 2
    y = max(rect.y + STOP_SIGN_MARGIN, center_y - texture.height)

    rl.draw_texture(texture, int(x), int(y), color)

    if toggles.get("show_stopping_point_metrics"):
      if ui_state.is_metric or toggles.get("use_si_metrics"):
        text = f"{round(self._stopping_distance)} {tr('m')}"
      else:
        text = f"{round(self._stopping_distance * CV.METER_TO_FOOT)} {tr('ft')}"

      font = gui_app.font(FontWeight.SEMI_BOLD)
      text_size = measure_text_cached(font, text, STOP_DISTANCE_FONT_SIZE)
      text_position = rl.Vector2(x + texture.width + STOP_DISTANCE_MARGIN, y + (texture.height - text_size.y) / 2)
      rl.draw_text_ex(font, text, text_position, STOP_DISTANCE_FONT_SIZE, 0, color)

  def _model_stopping_distance(self):
    model = ui_state.sm["modelV2"]

    for velocity, position in zip(model.velocity.x, model.position.x, strict=True):
      if velocity < STOP_SPEED:
        return position

    return 0.0

  def path_gradient_enabled(self):
    return ui_state.frogpilot_toggles.get("acceleration_path", False)

  def path_gradient_color(self, stock_color, gradient_point, acceleration, experimental_mode):
    toggles = ui_state.frogpilot_toggles
    steady = abs(acceleration) < ACCELERATION_PATH_MIN_ACCEL

    if steady and not experimental_mode and toggles.get("color_scheme", "stock") != "stock":
      path_color = hex_to_color(toggles["path_color"])

      return rl.Color(path_color.r, path_color.g, path_color.b, int(np.interp(gradient_point, BAND_STOPS, PATH_ALPHAS)))

    return stock_color

  def _path_edge_gradient(self, experimental_mode):
    toggles = ui_state.frogpilot_toggles
    sm = ui_state.sm

    if ui_state.status == UIStatus.ALWAYS_ON_LATERAL:
      return solid_band_gradient(ALWAYS_ON_LATERAL_COLOR)

    if sm["selfdriveState"].enabled and sm["frogpilotPlan"].ceStatus == frogpilot_status.CE_STATUS["USER_DISABLED"]:
      return solid_band_gradient(CEM_DISABLED_COLOR)

    if experimental_mode:
      return solid_band_gradient(EXPERIMENTAL_MODE_COLOR)

    if toggles.get("color_scheme", "stock") != "stock":
      return solid_band_gradient(hex_to_color(toggles["path_edges_color"]))

    return band_gradient(STOCK_PATH_EDGE_COLORS)
