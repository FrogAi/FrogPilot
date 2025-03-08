# PFEIFER - MTSC - Modified by FrogAi for FrogPilot
#!/usr/bin/env python3
import json
import math

from openpilot.selfdrive.frogpilot.frogpilot_utilities import calculate_distance_to_point
from openpilot.selfdrive.frogpilot.frogpilot_variables import PLANNER_TIME, TO_RADIANS, params_memory

def calculate_curvature(p1, p2, p3):
  lat1, lon1 = p1
  lat2, lon2 = p2
  lat3, lon3 = p3

  lat1_rad, lon1_rad = lat1 * TO_RADIANS, lon1 * TO_RADIANS
  lat2_rad, lon2_rad = lat2 * TO_RADIANS, lon2 * TO_RADIANS
  lat3_rad, lon3_rad = lat3 * TO_RADIANS, lon3 * TO_RADIANS

  side_a = calculate_distance_to_point(lat2_rad, lon2_rad, lat3_rad, lon3_rad)
  side_b = calculate_distance_to_point(lat1_rad, lon1_rad, lat3_rad, lon3_rad)
  side_c = calculate_distance_to_point(lat1_rad, lon1_rad, lat2_rad, lon2_rad)

  s = (side_a + side_b + side_c) / 2

  area_squared = s * (s - side_a) * (s - side_b) * (s - side_c)
  if area_squared <= 0:
    return 0

  area = math.sqrt(area_squared)

  radius = (side_a * side_b * side_c) / (4 * area)
  if radius == 0:
    return 0

  curvature = 1 / radius
  return curvature

class MapTurnSpeedController:
  def get_map_curvature(self, v_ego):
    position = json.loads(params_memory.get("LastGPSPosition") or "{}")
    if not position:
      return 1e-6
    current_latitude = position["latitude"]
    current_longitude = position["longitude"]

    distances = []
    minimum_idx = 0
    minimum_distance = 1000.0

    target_velocities = json.loads(params_memory.get("MapTargetVelocities") or "[]")
    for i, target_velocity in enumerate(target_velocities):
      target_latitude = target_velocity["latitude"]
      target_longitude = target_velocity["longitude"]

      distance = calculate_distance_to_point(current_latitude * TO_RADIANS, current_longitude * TO_RADIANS, target_latitude * TO_RADIANS, target_longitude * TO_RADIANS)
      distances.append(distance)

      if distance < minimum_distance:
        minimum_distance = distance
        minimum_idx = i

    forward_distances = distances[minimum_idx:]

    cumulative_distance = 0.0
    target_idx = None

    for i, distance in enumerate(forward_distances):
      cumulative_distance += distance
      if cumulative_distance >= PLANNER_TIME * v_ego:
        target_idx = i
        break

    forward_points = target_velocities[minimum_idx:]

    if target_idx is None or target_idx == 0 or target_idx >= len(forward_points) - 1:
      return 1e-6

    p1 = (forward_points[target_idx - 1]["latitude"], forward_points[target_idx - 1]["longitude"])
    p2 = (forward_points[target_idx]["latitude"], forward_points[target_idx]["longitude"])
    p3 = (forward_points[target_idx + 1]["latitude"], forward_points[target_idx + 1]["longitude"])

    return max(calculate_curvature(p1, p2, p3), 1e-6)
