import colorsys

import numpy as np
import pyray as rl

from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.frogpilot.common import frogpilot_status
from openpilot.frogpilot.ui.mici.frogpilot_theme import hex_to_color, PATH_ALPHAS
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.lib.application import gui_app
from openpilot.system.ui.lib.shader_polygon import draw_polygon, Gradient

ACCELERATION_PATH_MIN_ACCEL = 0.25

BAND_ALPHAS = (0.4, 0.35, 0.0)
BAND_STOPS = [0.0, 0.5, 1.0]
BLIND_SPOT_PATH_HUE = 0.0


def hsla_to_color(hue, saturation, lightness, alpha):
  red, green, blue = colorsys.hls_to_rgb(hue, lightness, saturation)

  return rl.Color(int(red * 255), int(green * 255), int(blue * 255), int(alpha * 255))


def band_gradient(colors):
  return Gradient(start=(0.0, 1.0), end=(0.0, 0.0), colors=colors, stops=BAND_STOPS)


def solid_band_gradient(color, strength=1.0):
  return band_gradient([rl.Color(color.r, color.g, color.b, int(255 * alpha * strength)) for alpha in BAND_ALPHAS])


class FrogPilotRoad:
  def __init__(self):
    self._blind_spot_filters = {side: FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps) for side in (True, False)}

    self._adjacent_paths = []

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

  def path_gradient_enabled(self):
    return ui_state.frogpilot_toggles.get("acceleration_path", False)

  def path_gradient_color(self, stock_color, gradient_point, acceleration, experimental_mode):
    toggles = ui_state.frogpilot_toggles
    steady = abs(acceleration) < ACCELERATION_PATH_MIN_ACCEL

    if steady and not experimental_mode and toggles.get("color_scheme", "stock") != "stock":
      path_color = hex_to_color(toggles["path_color"])

      return rl.Color(path_color.r, path_color.g, path_color.b, int(np.interp(gradient_point, BAND_STOPS, PATH_ALPHAS)))

    return stock_color
