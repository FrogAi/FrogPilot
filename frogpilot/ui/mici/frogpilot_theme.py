import hashlib
import os
import subprocess
import threading

from collections import Counter
from functools import cache
from xml.etree import ElementTree

import pyray as rl

from PIL import Image, ImageSequence

from cereal import log
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.realtime import set_core_affinity
from openpilot.frogpilot.common import frogpilot_paths, frogpilot_status
from openpilot.selfdrive.ui.ui_state import ui_state, UIStatus
from openpilot.system.hardware import PC
from openpilot.system.ui.lib.application import gui_app

AlertSize = log.SelfdriveState.AlertSize

BIG_UI_WIDTH = 2160
BOOKMARK_SIZE = 180
MAIN_COLOR_MIN_ALPHA = 200
MAIN_COLOR_MIN_BRIGHTNESS = 70
MAIN_COLOR_MIN_SATURATION = 48
PATH_ALPHAS = (170, 120, 10)
SIGNAL_BOTTOM_OFFSET = 26
STATIC_SIGNAL_HEIGHT = 91
STATIC_SIGNAL_MARGIN_X = 2
STATIC_SIGNAL_MARGIN_Y = 5
STATIC_SIGNAL_WIDTH = 100
TRADITIONAL_SIGNAL_ALERT_SIZES = (AlertSize.none, AlertSize.small)
WHEEL_SIZE = 50


@cache
def hex_to_color(hex_color):
  value = int(hex_color[1:], 16)

  return rl.Color((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, (value >> 24) & 0xFF)


def find_image(folder, name):
  for extension in frogpilot_paths.THEME_IMAGE_EXTENSIONS:
    path = folder / f"{name}{extension}"

    if path.exists():
      return path

  return None


def render_svg(source, folder, size):
  if size is None:
    size_arguments = []
  else:
    size_arguments = ["-w", str(size), "-h", str(size)]

  tree = ElementTree.parse(source)
  parent = next((group for group in tree.iter() if any("data-ms" in child.attrib for child in group)), None)

  if parent is None:
    subprocess.run([str(frogpilot_paths.RESVG_PATH), *size_arguments, str(source), str(folder / "frame_000_0.png")], check=True)
    return

  frames = [child for child in parent if "data-ms" in child.attrib]

  for frame in frames:
    parent.remove(frame)

  if parent.get("data-loop") == "false":
    frames = frames[-1:]

  frame_source = folder / "source.svg"

  for index, frame in enumerate(frames):
    parent.append(frame)
    tree.write(frame_source)

    frame_path = folder / f"frame_{index:03d}_{frame.get('data-ms')}.png"
    subprocess.run([str(frogpilot_paths.RESVG_PATH), *size_arguments, str(frame_source), str(frame_path)], check=True)

    parent.remove(frame)

  frame_source.unlink()


def render_image(source, size=None):
  source_stat = source.stat()
  render_key = f"{source.resolve()}_{source_stat.st_mtime_ns}_{size}"
  render_folder = frogpilot_paths.THEME_RENDERS_PATH / hashlib.sha1(render_key.encode()).hexdigest()

  if not render_folder.exists():
    temporary_folder = render_folder.with_name(f"{render_folder.name}_temporary")
    temporary_folder.mkdir(parents=True, exist_ok=True)

    if source.suffix == ".svg":
      render_svg(source, temporary_folder, size)
    else:
      with Image.open(source) as image:
        for index, frame in enumerate(ImageSequence.Iterator(image)):
          frame.convert("RGBA").save(temporary_folder / f"frame_{index:03d}_{int(frame.info.get('duration', 0))}.png")

    temporary_folder.rename(render_folder)

  return sorted(render_folder.glob("frame_*.png"))


def mirror_image(path):
  mirrored = path.with_name(f"mirrored_{path.name}")

  if not mirrored.exists():
    temporary = path.with_name(f"temporary_{path.name}")

    with Image.open(path) as image:
      image.convert("RGBA").transpose(Image.FLIP_LEFT_RIGHT).save(temporary)

    temporary.replace(mirrored)

  return mirrored


def frame_duration(frame):
  return int(frame.stem.rsplit("_", 1)[1]) / 1000


def main_color(path):
  counts = Counter()

  with Image.open(path) as image:
    for red, green, blue, alpha in image.convert("RGBA").resize((64, 64)).getdata():
      brightest = max(red, green, blue)

      if alpha > MAIN_COLOR_MIN_ALPHA and brightest > MAIN_COLOR_MIN_BRIGHTNESS and brightest - min(red, green, blue) > MAIN_COLOR_MIN_SATURATION:
        counts[(red // 8 * 8, green // 8 * 8, blue // 8 * 8)] += 1

  if not counts:
    return None

  red, green, blue = counts.most_common(1)[0][0]

  return rl.Color(red, green, blue, int(255 * 0.9))


class ThemeAssets:
  def __init__(self):
    self.signal_is_gif = False
    self.wheel_hidden = False

    self.signal_interval = 0.0

    self.signal_style = None

    self.blindspot_left = []
    self.blindspot_right = []
    self.bookmark = []
    self.signal_left = []
    self.signal_right = []
    self.wheel = []

    self.signal_size = None

    self.settings_color = None


def load_signals(assets, folder):
  if not folder.is_dir():
    return

  for path in sorted(folder.iterdir()):
    if path.suffix in frogpilot_paths.THEME_IMAGE_EXTENSIONS:
      frames = render_image(path)

      if "blindspot" in path.name.lower():
        assets.blindspot_left = frames[:1]
        assets.blindspot_right = [mirror_image(frame) for frame in frames[:1]]
      else:
        assets.signal_is_gif |= path.suffix == ".gif"
        assets.signal_left += frames
        assets.signal_right += [mirror_image(frame) for frame in frames]
    else:
      parts = path.name.split("_")

      if len(parts) == 2 and parts[1].isdigit() and int(parts[1]) > 0:
        assets.signal_style = parts[0]
        assets.signal_interval = int(parts[1]) / 1000

  if assets.signal_left:
    with Image.open(assets.signal_left[0]) as image:
      assets.signal_size = image.size
  else:
    assets.signal_style = None


def load_assets():
  assets = ThemeAssets()

  wheel_folder = frogpilot_paths.ACTIVE_THEME_PATH / "steering_wheel"
  wheel = find_image(wheel_folder, "wheel")
  stock_wheel = frogpilot_paths.STOCK_THEME_PATH / "steering_wheel/wheel.svg"

  if wheel is None:
    assets.wheel_hidden = wheel_folder.is_dir()
  elif wheel.resolve() != stock_wheel.resolve():
    assets.wheel = render_image(wheel, WHEEL_SIZE)

  icons_folder = frogpilot_paths.ACTIVE_THEME_PATH / "icons"

  if icons_folder.resolve() != (frogpilot_paths.STOCK_THEME_PATH / "icons").resolve():
    bookmark = find_image(icons_folder, "button_flag")
    if bookmark is not None:
      assets.bookmark = render_image(bookmark, BOOKMARK_SIZE)

    settings = find_image(icons_folder, "button_settings")
    if settings is not None:
      assets.settings_color = main_color(render_image(settings)[0])

  load_signals(assets, frogpilot_paths.ACTIVE_THEME_PATH / "signals")

  return assets


def animated_frame(frames):
  durations = [frame_duration(frame) for frame in frames]
  total = sum(durations)

  if total == 0:
    return frames[0]

  position = rl.get_time() % total

  for frame, duration in zip(frames, durations, strict=True):
    if position < duration:
      return frame

    position -= duration

  return frames[-1]


class FrogPilotTheme:
  def __init__(self):
    self._signal_on = False

    self._signal_start_time = 0.0

    self._signal_side = None

    self._update_counts = None

    self._signal_alpha_filter = FirstOrderFilter(0.0, 0.1, 1 / gui_app.target_fps)
    self._assets = ThemeAssets()
    self._load_thread = None

  def update(self):
    frogpilot_plan = ui_state.sm["frogpilotPlan"]
    update_counts = (frogpilot_plan.themeUpdateCount, frogpilot_plan.wheelImageUpdateCount)

    if update_counts == self._update_counts:
      return

    if self._load_thread is not None and self._load_thread.is_alive():
      return

    self._update_counts = update_counts
    self._load_thread = threading.Thread(target=self._load, daemon=True)
    self._load_thread.start()

  def _load(self):
    set_core_affinity([0, 1, 2, 3])
    if not PC:
      os.sched_setscheduler(0, os.SCHED_OTHER, os.sched_param(0))

    self._assets = load_assets()

  def throttle_colors(self, stock_colors):
    toggles = ui_state.frogpilot_toggles

    if toggles.get("color_scheme", "stock") == "stock":
      return stock_colors

    path_color = hex_to_color(toggles["path_color"])

    return [rl.Color(path_color.r, path_color.g, path_color.b, alpha) for alpha in PATH_ALPHAS]

  def lane_lines_color(self):
    toggles = ui_state.frogpilot_toggles

    if ui_state.status != UIStatus.ENGAGED or toggles.get("color_scheme", "stock") == "stock":
      return None

    return hex_to_color(toggles["lane_lines_color"])

  def wheel_texture(self, stock_texture):
    assets = self._assets

    if not assets.wheel:
      return stock_texture

    return gui_app.texture(str(animated_frame(assets.wheel)), WHEEL_SIZE, WHEEL_SIZE)

  def wheel_gives_way(self):
    return self._assets.wheel_hidden or self.signal_crosses_bottom()

  def signal_crosses_bottom(self):
    assets = self._assets

    return assets.signal_style == "traditional" and self._signal_visible(assets)

  def bookmark_texture(self, stock_texture):
    assets = self._assets

    if not assets.bookmark:
      return stock_texture

    return gui_app.texture(str(animated_frame(assets.bookmark)), BOOKMARK_SIZE, BOOKMARK_SIZE)

  def settings_tint(self, stock_tint):
    settings_color = self._assets.settings_color

    if settings_color is None:
      return stock_tint

    return settings_color

  def alert_signal_texture(self, left, stock_texture):
    assets = self._assets

    if assets.signal_style != "static":
      return stock_texture

    if left:
      frames = assets.signal_left
    else:
      frames = assets.signal_right

    return gui_app.texture(str(frames[self._signal_index(assets)]), stock_texture.width, stock_texture.height)

  def draw_turn_signal(self, rect):
    assets = self._assets

    if assets.signal_style is None:
      return

    car_state = ui_state.sm["carState"]
    signal_on = car_state.leftBlinker or car_state.rightBlinker

    if signal_on and not self._signal_on:
      self._signal_start_time = rl.get_time()

    self._signal_on = signal_on

    if car_state.leftBlinker:
      self._signal_side = "left"
    elif car_state.rightBlinker:
      self._signal_side = "right"

    alpha = self._signal_alpha_filter.update(float(self._signal_visible(assets)))

    if alpha < frogpilot_status.MIN_VISIBLE_ALPHA or self._signal_side is None:
      return

    left = self._signal_side == "left"
    index = self._signal_index(assets)

    if left:
      frames = assets.signal_left
      blindspot = assets.blindspot_left
      blindspot_active = car_state.leftBlindspot and bool(blindspot)
    else:
      frames = assets.signal_right
      blindspot = assets.blindspot_right
      blindspot_active = car_state.rightBlindspot and bool(blindspot)

    if blindspot_active:
      frame = blindspot[0]
    else:
      frame = frames[index]

    if assets.signal_style == "static":
      texture = gui_app.texture(str(frame), STATIC_SIGNAL_WIDTH, STATIC_SIGNAL_HEIGHT)
      y = rect.y + STATIC_SIGNAL_MARGIN_Y

      if left:
        x = rect.x + STATIC_SIGNAL_MARGIN_X
      else:
        x = rect.x + rect.width - STATIC_SIGNAL_MARGIN_X - texture.width
    else:
      scale = rect.width / BIG_UI_WIDTH
      width = round(assets.signal_size[0] * scale)
      height = round(assets.signal_size[1] * scale)

      texture = gui_app.texture(str(frame), width, height)
      x, y = self._signal_position(assets, rect, left, index, width, height, blindspot_active)

    rl.draw_texture(texture, int(x), int(y), rl.Color(255, 255, 255, int(255 * 0.9 * alpha)))

  def _signal_visible(self, assets):
    car_state = ui_state.sm["carState"]

    if not car_state.leftBlinker and not car_state.rightBlinker:
      return False

    alert_size = ui_state.sm["selfdriveState"].alertSize

    if assets.signal_style == "static":
      return alert_size == AlertSize.none

    return alert_size in TRADITIONAL_SIGNAL_ALERT_SIZES

  def _signal_index(self, assets):
    return int((rl.get_time() - self._signal_start_time) / assets.signal_interval) % len(assets.signal_left)

  def _signal_position(self, assets, rect, left, index, width, height, blindspot_active):
    y = rect.y + rect.height - height - SIGNAL_BOTTOM_OFFSET

    if blindspot_active:
      if left:
        x = rect.x + rect.width - width
      else:
        x = rect.x

      return x, y

    if assets.signal_is_gif and assets.signal_style == "traditional":
      movement = (rect.width + width * 2) / len(assets.signal_left)

      if left:
        x = rect.x + rect.width - index * movement + width
      else:
        x = rect.x + index * movement - width

      return x, y

    if left:
      x = rect.x + rect.width - (index + 1) * width
    else:
      x = rect.x + index * width

    return x, y


theme = FrogPilotTheme()
