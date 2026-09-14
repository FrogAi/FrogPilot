import itertools
import json
import math
import sqlite3

from array import array
from contextlib import closing
from pathlib import Path

CITY_LOOKUP_PATH = Path(__file__).with_name("city_lookup.sqlite")

COORDINATE_SCALE = 10_000_000
GRID_CELL_SIZE_E7 = 250_000

UNKNOWN_LOCATION = ("N/A", "N/A", "N/A")


def _point_in_ring(longitude, latitude, points):
  inside = False

  previous_longitude, previous_latitude = points[-2:]

  coordinates = iter(points)

  for current_longitude, current_latitude in zip(coordinates, coordinates, strict=True):
    cross_product = (
      (longitude - previous_longitude) * (current_latitude - previous_latitude)
      - (latitude - previous_latitude) * (current_longitude - previous_longitude)
    )
    longitude_matches = min(previous_longitude, current_longitude) <= longitude <= max(previous_longitude, current_longitude)
    latitude_matches = min(previous_latitude, current_latitude) <= latitude <= max(previous_latitude, current_latitude)

    if cross_product == 0 and longitude_matches and latitude_matches:
      return None

    if (current_latitude > latitude) != (previous_latitude > latitude):
      latitude_span = previous_latitude - current_latitude
      crossing = (longitude - current_longitude) * latitude_span - (latitude - current_latitude) * (previous_longitude - current_longitude)

      if crossing * latitude_span < 0:
        inside = not inside

    previous_longitude, previous_latitude = current_longitude, current_latitude

  return inside


def _matched_regions(database, table, longitude, latitude):
  rows = database.execute(
    f"""
    SELECT region_id, polygon_number, is_hole, points
    FROM {table}
    WHERE minimum_longitude <= ? AND maximum_longitude >= ?
      AND minimum_latitude <= ? AND maximum_latitude >= ?
    ORDER BY id
    """,
    (longitude, longitude, latitude, latitude),
  )

  matches = []
  for (region_id, _), ring_rows in itertools.groupby(rows, key=lambda row: row[:2]):
    if matches and matches[-1] == region_id:
      continue

    _, _, is_hole, points = next(ring_rows)
    if is_hole or _point_in_ring(longitude, latitude, array("i", points)) is False:
      continue

    if any(_point_in_ring(longitude, latitude, array("i", points)) is True for _, _, _, points in ring_rows):
      continue

    matches.append(region_id)

  return matches


def _grid_cell(database, longitude, latitude):
  if longitude == 180 * COORDINATE_SCALE:
    longitude = -180 * COORDINATE_SCALE

  row = min((90 * COORDINATE_SCALE - latitude) // GRID_CELL_SIZE_E7, 180 * COORDINATE_SCALE // GRID_CELL_SIZE_E7 - 1)
  column = (longitude + 180 * COORDINATE_SCALE) // GRID_CELL_SIZE_E7
  span = database.execute(
    """
    SELECT column_end, country_id, state_id
    FROM grid
    WHERE row = ? AND column_start <= ?
    ORDER BY column_start DESC
    LIMIT 1
    """,
    (row, column),
  ).fetchone()

  if span is None or span[0] < column:
    return None, None

  return span[1], span[2]


def get_location(last_gps_position):
  try:
    position = json.loads(last_gps_position or "{}")
    latitude, longitude = position["latitude"], position["longitude"]
    coordinates_are_numbers = all(
      isinstance(value, (int, float)) and not isinstance(value, bool)
      for value in (latitude, longitude)
    )

    if not coordinates_are_numbers:
      return UNKNOWN_LOCATION

    if not -90 <= latitude <= 90 or not -180 <= longitude <= 180:
      return UNKNOWN_LOCATION

    latitude_e7 = round(latitude * COORDINATE_SCALE)
    longitude_e7 = round(longitude * COORDINATE_SCALE)

    latitude_radians = math.radians(latitude)
    longitude_radians = math.radians(longitude)

    x = round(COORDINATE_SCALE * math.cos(latitude_radians) * math.cos(longitude_radians))
    y = round(COORDINATE_SCALE * math.cos(latitude_radians) * math.sin(longitude_radians))
    z = round(COORDINATE_SCALE * math.sin(latitude_radians))

    with closing(sqlite3.connect(f"{CITY_LOOKUP_PATH.as_uri()}?mode=ro", uri=True)) as database:
      matched_country_ids = _matched_regions(database, "country_rings", longitude_e7, latitude_e7)

      if matched_country_ids:
        country_id = matched_country_ids[0]
        state_id = None
        matched_state_ids = _matched_regions(database, "state_rings", longitude_e7, latitude_e7)
        if len(matched_state_ids) == 1:
          state_id = matched_state_ids[0]
      else:
        country_id, state_id = _grid_cell(database, longitude_e7, latitude_e7)

      location = database.execute(
        """
        SELECT COALESCE((
          SELECT city
          FROM places
          WHERE country_id = :country_id
            AND (states.id IS NULL OR state_id = states.id)
          ORDER BY (x - :x) * (x - :x) + (y - :y) * (y - :y) + (z - :z) * (z - :z), place_number
          LIMIT 1
        ), states.capital_city, countries.capital_city, 'N/A'), countries.name, COALESCE(states.name, 'N/A')
        FROM countries
        LEFT JOIN states ON states.id = :state_id AND countries.name = 'United States'
        WHERE countries.id = :country_id
        """,
        {"country_id": country_id, "state_id": state_id, "x": x, "y": y, "z": z},
      ).fetchone()

      return location or UNKNOWN_LOCATION

  except (KeyError, TypeError, ValueError, sqlite3.Error, IndexError):
    return UNKNOWN_LOCATION
