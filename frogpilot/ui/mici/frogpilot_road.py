import numpy as np
import pyray as rl

from openpilot.frogpilot.ui.mici.frogpilot_theme import hex_to_color, PATH_ALPHAS
from openpilot.selfdrive.ui.ui_state import ui_state

ACCELERATION_PATH_MIN_ACCEL = 0.25

BAND_STOPS = [0.0, 0.5, 1.0]


class FrogPilotRoad:
  def path_gradient_enabled(self):
    return ui_state.frogpilot_toggles.get("acceleration_path", False)

  def path_gradient_color(self, stock_color, gradient_point, acceleration, experimental_mode):
    toggles = ui_state.frogpilot_toggles
    steady = abs(acceleration) < ACCELERATION_PATH_MIN_ACCEL

    if steady and not experimental_mode and toggles.get("color_scheme", "stock") != "stock":
      path_color = hex_to_color(toggles["path_color"])

      return rl.Color(path_color.r, path_color.g, path_color.b, int(np.interp(gradient_point, BAND_STOPS, PATH_ALPHAS)))

    return stock_color
