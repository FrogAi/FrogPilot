import math
import numpy as np
import os
import shutil
import subprocess
import threading
import time
import urllib.request

from openpilot.common.numpy_fast import interp, mean

EARTH_RADIUS = 6378137  # Radius of the Earth in meters

def calculate_distance_to_point(ax, ay, bx, by):
  a = math.sin((bx - ax) / 2) * math.sin((bx - ax) / 2) + math.cos(ax) * math.cos(bx) * math.sin((by - ay) / 2) * math.sin((by - ay) / 2)
  c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
  return EARTH_RADIUS * c

def calculate_lane_width(lane, current_lane, road_edge):
  current_x = np.array(current_lane.x)
  current_y = np.array(current_lane.y)

  lane_y_interp = interp(current_x, np.array(lane.x), np.array(lane.y))
  road_edge_y_interp = interp(current_x, np.array(road_edge.x), np.array(road_edge.y))

  distance_to_lane = np.mean(np.abs(current_y - lane_y_interp))
  distance_to_road_edge = np.mean(np.abs(current_y - road_edge_y_interp))

  return float(min(distance_to_lane, distance_to_road_edge))

# Credit goes to Pfeiferj!
def calculate_road_curvature(modelData, v_ego):
  orientation_rate = np.abs(modelData.orientationRate.z)
  velocity = modelData.velocity.x
  max_pred_lat_acc = np.amax(orientation_rate * velocity)
  return max_pred_lat_acc / max(v_ego, 1)**2

def copy_if_exists(source, destination, single_file_name=None):
  if not os.path.exists(source):
    print(f"Source directory {source} does not exist. Skipping copy.")
    return

  if single_file_name:
    os.makedirs(destination, exist_ok=True)
    for item in os.listdir(source):
      shutil.copy2(os.path.join(source, item), os.path.join(destination, single_file_name))
      print(f"Successfully copied {item} to {single_file_name}.")
  else:
    shutil.copytree(source, destination, dirs_exist_ok=True)
    print(f"Successfully copied {source} to {destination}.")

def delete_file(file):
  try:
    if os.path.isfile(file):
      os.remove(file)
      print(f"Deleted file: {file}")
    else:
      print(f"File not found: {file}")
  except Exception as e:
    print(f"An error occurred when deleting {file}: {e}")

def is_url_pingable(url, timeout=5):
  try:
    urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'}), timeout=timeout)
    return True
  except Exception as e:
    print(f"Failed to ping {url}: {e}")
    return False

def run_cmd(cmd, success_message, fail_message):
  try:
    subprocess.check_call(cmd)
    print(success_message)
    return True
  except Exception as e:
    print(f"Unexpected error occurred: {e}")
    print(fail_message)
    return False

class MovingAverageCalculator:
  def __init__(self):
    self.reset_data()

  def add_data(self, value):
    if len(self.data) == 5:
      self.total -= self.data.pop(0)
    self.data.append(value)
    self.total += value

  def get_moving_average(self):
    if len(self.data) == 0:
      return None
    return self.total / len(self.data)

  def reset_data(self):
    self.data = []
    self.total = 0
