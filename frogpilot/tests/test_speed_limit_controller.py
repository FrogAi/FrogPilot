import types

from openpilot.frogpilot.controls.lib.speed_limit_controller import SpeedLimitController


def get_map_speed_limit(current_limit, next_limit, target=0, v_ego=20):
  frogpilot_vcruise = types.SimpleNamespace(frogpilot_planner=types.SimpleNamespace(speed_limit_filler=None))
  controller = SpeedLimitController(frogpilot_vcruise)
  controller.frogpilot_toggles = types.SimpleNamespace(map_speed_lookahead_higher=0, map_speed_lookahead_lower=5, speed_limit_filler=False)
  controller.target = target

  map_match = {
    "is_forward": True,
    "next_speed_limits": [{"distance": 50, "is_forward": True, "speed_limit": next_limit, "way_id": 2}],
    "speed_limit": current_limit,
    "way_id": 1,
  }
  controller.update_map_speed_limit(map_match, v_ego)
  return controller.map_speed_limit


def test_lower_limit_with_map_limit():
  assert get_map_speed_limit(30, 20) == 20


def test_higher_limit_with_map_limit():
  assert get_map_speed_limit(20, 30) == 20


def test_lower_limit_without_map_limit():
  assert get_map_speed_limit(0, 15, target=25) == 15
  assert get_map_speed_limit(0, 15, v_ego=25) == 15


def test_higher_limit_without_map_limit():
  assert get_map_speed_limit(0, 25, target=15) == 0
  assert get_map_speed_limit(0, 25, v_ego=10) == 0
