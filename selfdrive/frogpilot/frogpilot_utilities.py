import math
import numpy as np
import shutil
import subprocess
import urllib.request
import zipfile

from pathlib import Path

from openpilot.common.numpy_fast import interp

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

def delete_file(path):
  path = Path(path)
  try:
    if path.is_file():
      path.unlink()
      print(f"Deleted file: {path}")
    elif path.is_dir():
      shutil.rmtree(path)
      print(f"Deleted directory: {path}")
    else:
      print(f"File not found: {path}")
  except Exception as error:
    print(f"An error occurred when deleting {path}: {error}")

def extract_zip(zip_file, extract_path):
  zip_file = Path(zip_file)
  extract_path = Path(extract_path)
  print(f"Extracting {zip_file} to {extract_path}")

  try:
    with zipfile.ZipFile(zip_file, 'r') as zip_ref:
      zip_ref.extractall(extract_path)
    zip_file.unlink()
    print(f"Extraction completed: {zip_file} has been removed")
  except Exception as error:
    print(f"An error occurred while extracting {zip_file}: {error}")

def is_url_pingable(url, timeout=10):
  try:
    urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'}), timeout=timeout)
    return True
  except Exception as error:
    print(f"Failed to ping {url}: {error}")
    return False

def run_cmd(cmd, success_message, fail_message):
  try:
    subprocess.check_call(cmd)
    print(success_message)
  except Exception as error:
    print(f"Unexpected error occurred: {error}")
    print(fail_message)

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
