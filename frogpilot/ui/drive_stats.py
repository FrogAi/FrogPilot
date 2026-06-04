#!/usr/bin/env python3
import threading

import pyray as rl
import requests

from openpilot.common.api import api_get
from openpilot.common.constants import CV
from openpilot.common.params import Params
from openpilot.common.swaglog import cloudlog
from openpilot.selfdrive.ui.lib.api_helpers import get_token
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.athena.registration import UNREGISTERED_DONGLE_ID
from openpilot.system.ui.lib.application import FONT_SCALE, FontWeight
from openpilot.system.ui.lib.multilang import tr
from openpilot.system.ui.widgets import Widget
from openpilot.system.ui.widgets.label import gui_label


COLUMN_GAP = 10
SECTION_GAP = 20
TITLE_GAP = 20
VALUE_GAP = 10

PANEL_PADDING_BOTTOM = 30
PANEL_PADDING_TOP = 35
PANEL_PADDING_X = 50

PANEL_ROUNDNESS = 0.022

TITLE_FONT_SIZE = 50
UNIT_FONT_SIZE = 50
VALUE_FONT_SIZE = 65

PANEL_COLOR = rl.Color(51, 51, 51, 255)
FROGPILOT_GREEN = rl.Color(23, 134, 67, 255)
UNIT_COLOR = rl.Color(160, 160, 160, 255)


class DriveStatsLayout(Widget):
  def __init__(self):
    super().__init__()
    self.params = Params(return_defaults=True)

    self.is_metric = self.params.get_bool("IsMetric")
    self.use_konik_server = self.params.get_bool("UseKonikServer")

    self.session = requests.Session()

    self.stats = self.load_remote_stats()
    self.frogpilot_stats = self.load_frogpilot_stats()

    threading.Thread(target=self.fetch_drive_stats, daemon=True).start()

    ui_state.add_offroad_transition_callback(self.on_state_change)

  def on_state_change(self):
    if not ui_state.started:
      threading.Thread(target=self.fetch_drive_stats, daemon=True).start()

  def show_event(self):
    super().show_event()

    self.update_stats()

  def fetch_drive_stats(self):
    dongle_id = self.params.get("DongleId")
    if not dongle_id or dongle_id == UNREGISTERED_DONGLE_ID:
      return

    try:
      token = get_token(dongle_id)
      response = api_get(f"v1.1/devices/{dongle_id}/stats", timeout=10, access_token=token, session=self.session)
      if response.status_code != 200:
        return

      self.stats = self.parse_remote_stats(response.json())
      self.params.put("ApiCache_DriveStats", self.stats)

      self.update_minutes()
    except Exception as exception:
      cloudlog.error(f"Failed to fetch drive stats: {exception}")

  def load_remote_stats(self):
    return self.parse_remote_stats(self.params.get("ApiCache_DriveStats"))

  def load_frogpilot_stats(self):
    return self.params.get("FrogPilotStats")

  def parse_remote_bucket(self, bucket):
    if not isinstance(bucket, dict):
      return {}

    return {
      "distance": bucket.get("distance") or 0,
      "minutes": bucket.get("minutes") or 0,
      "routes": bucket.get("routes") or 0,
    }

  def parse_remote_stats(self, stats):
    if not isinstance(stats, dict):
      return {"all": {}, "week": {}}

    return {
      "all": self.parse_remote_bucket(stats.get("all")),
      "week": self.parse_remote_bucket(stats.get("week")),
    }

  def update_stats(self):
    self.is_metric = self.params.get_bool("IsMetric")
    self.use_konik_server = self.params.get_bool("UseKonikServer")

    self.stats = self.load_remote_stats()
    self.frogpilot_stats = self.load_frogpilot_stats()

    self.update_minutes()

  def get_sections(self):
    distance_unit = tr("KM") if self.is_metric else tr("Miles")
    km_to_distance = 1 if self.is_metric else CV.KPH_TO_MPH

    all_stats = self.stats.get("all", {})
    week_stats = self.stats.get("week", {})
    frogpilot_stats = self.frogpilot_stats

    def format_section(title, color, routes, km, seconds):
      return title, color, str(int(routes)), str(int(km * km_to_distance)), distance_unit, str(int(seconds / 3600))

    return (
      format_section(
        tr("ALL TIME (KONIK)") if self.use_konik_server else tr("ALL TIME"), rl.WHITE,
        all_stats.get("routes", 0), all_stats.get("distance", 0) * CV.MPH_TO_KPH, all_stats.get("minutes", 0) * 60,
      ),
      format_section(
        tr("PAST WEEK (KONIK)") if self.use_konik_server else tr("PAST WEEK"), rl.WHITE,
        week_stats.get("routes", 0), week_stats.get("distance", 0) * CV.MPH_TO_KPH, week_stats.get("minutes", 0) * 60,
      ),
      format_section(
        tr("FROGPILOT"), FROGPILOT_GREEN,
        frogpilot_stats.get("FrogPilotDrives") or 0, (frogpilot_stats.get("FrogPilotMeters") or 0) / 1000, frogpilot_stats.get("FrogPilotSeconds") or 0,
      ),
    )

  def update_minutes(self):
    key = "KonikMinutes" if self.use_konik_server else "openpilotMinutes"
    self.params.put(key, int(self.stats.get("all", {}).get("minutes", 0)))

  def render_section(self, rect, title, title_color, routes, distance, distance_unit, hours):
    title_height = round(TITLE_FONT_SIZE * FONT_SCALE)
    value_height = round(VALUE_FONT_SIZE * FONT_SCALE)
    unit_height = round(UNIT_FONT_SIZE * FONT_SCALE)
    title_rect = rl.Rectangle(rect.x, rect.y, rect.width, title_height)

    gui_label(title_rect, title, font_size=TITLE_FONT_SIZE, color=title_color, font_weight=FontWeight.MEDIUM)

    metrics = (
      (routes, tr("Drives")),
      (distance, distance_unit),
      (hours, tr("Hours")),
    )
    values_y = rect.y + title_height + TITLE_GAP
    column_width = (rect.width - (COLUMN_GAP * 2)) / 3

    for index, (value, unit) in enumerate(metrics):
      column_x = rect.x + index * (column_width + COLUMN_GAP)
      value_rect = rl.Rectangle(column_x, values_y, column_width, value_height)
      unit_rect = rl.Rectangle(column_x, values_y + value_height + VALUE_GAP, column_width, unit_height)

      gui_label(value_rect, value, font_size=VALUE_FONT_SIZE, font_weight=FontWeight.NORMAL)
      gui_label(unit_rect, unit, font_size=UNIT_FONT_SIZE, color=UNIT_COLOR, font_weight=FontWeight.NORMAL)

  def _render(self, rect):
    sections = self.get_sections()

    rl.draw_rectangle_rounded(rect, PANEL_ROUNDNESS, 20, PANEL_COLOR)

    inner_x = rect.x + PANEL_PADDING_X
    inner_y = rect.y + PANEL_PADDING_TOP
    inner_width = rect.width - (PANEL_PADDING_X * 2)
    inner_height = rect.height - PANEL_PADDING_TOP - PANEL_PADDING_BOTTOM
    section_height = (inner_height - (SECTION_GAP * (len(sections) - 1))) / len(sections)

    for index, section in enumerate(sections):
      section_y = inner_y + index * (section_height + SECTION_GAP)
      section_rect = rl.Rectangle(inner_x, section_y, inner_width, section_height)
      self.render_section(section_rect, *section)
