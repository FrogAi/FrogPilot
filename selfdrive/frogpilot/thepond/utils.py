import subprocess
import os
import json
import importlib
import pathlib
import sys
import time
import shutil
import traceback

from openpilot.common.conversions import Conversions as CV
from openpilot.system.hardware import HARDWARE
from openpilot.system.loggerd.config import get_available_bytes, get_used_bytes

from openpilot.selfdrive.frogpilot.frogpilot_utilities import update_frogpilot_toggles

def is_running_on_comma():
  return os.path.exists("/data/persist")

RUNNING_ON_COMMA = is_running_on_comma()

DIR_OF_THIS_FILE = pathlib.Path(__file__).parent.absolute()
DATA_PATH = "" # On device /data is from the root
CAR_VALUES_PATH = f"{DIR_OF_THIS_FILE}/../../car"
""" Used to find models of all car brands """

if RUNNING_ON_COMMA:
  # Do this to import the car modules. Let me know if there is a better way.
  sys.path.append(f"{DIR_OF_THIS_FILE}/../..") # Adds higher directory to python modules path.
  from common.params import Params
  params = Params()
  params_memory = Params("/dev/shm/params")
  params_tracking = Params("/persist/tracking")
else:
  # If running this on a computer, there is a bunch of fake stuff inside fixtures that
  # will substitute openpilot stuff
  DATA_PATH = f"{DIR_OF_THIS_FILE}/fixtures"
  CAR_VALUES_PATH = f"{DIR_OF_THIS_FILE}/fixtures/selfdrive_car"
  from .fixtures.fake_modules.params import Params
  params = Params()
  params_memory = params
  params_tracking = params

SCREENRECORD_PATH = f"{DATA_PATH}/data/media/screen_recordings/"
ERROR_LOGS_PATH = f"{DATA_PATH}/data/crashes/"
FOOTAGE_PATH = f"{DATA_PATH}/data/media/0/realdata/"

def video_to_gif(input_path, output_path) -> None:
  """
  Creates a looping gif from the input_path sped
  up by a factor of 35.
  """
  if os.path.exists(output_path):
    return

  print(f"Create gif from video: {input_path}")
  # First create a sped up mp4
  sped_up_path = output_path.replace(".gif", ".mp4")
  print(f"Create temporary sped up mp4: {sped_up_path}")
  run_ffmpeg(['-i', input_path, '-an', '-vf', 'setpts=PTS/35', sped_up_path])

  # Next create a gif from the sped up mp4
  print(f"Create gif from sped up mp4: {output_path}")
  run_ffmpeg(["-i", sped_up_path, "-loop", "0", output_path])
  # Finally remove the sped up mp4
  os.remove(sped_up_path)

def video_to_png(input_path, output_path) -> None:
  """
  Creates a single frame png from the input_path
  """
  if os.path.exists(output_path):
    return
  run_ffmpeg(['-i', input_path, '-ss', '2', '-vframes', '1', output_path])
  print(f"PNG file created at: {output_path}")

def get_video_duration(input_path) -> float:
  """
  Returns the duration of the video in seconds
  """
  result = subprocess.run(['ffprobe', '-v', 'error', '-show_entries', 'format=duration', '-of', 'default=noprint_wrappers=1:nokey=1', input_path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  return float(result.stdout)

def run_ffmpeg(args) -> None:
  """
  Just a wrapper around the ffmpeg command, that hides
  all the verbose output and passss the -y flag to
  overwrite the output file if it already exists.
  """
  subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y'] + args)

def get_available_cameras(segment_path) -> list:
  """
  Checks the segment_path for camera files and returns a list with
  "forward", "wide", "driver" depending on if qcamera.ts, ecamera.hevc and dcamera.ts
  are present.
  """
  available_cameras = []
  if os.path.exists(os.path.join(segment_path, "qcamera.ts")):
    available_cameras.append("forward")
  if os.path.exists(os.path.join(segment_path, "ecamera.hevc")):
    available_cameras.append("wide")
  if os.path.exists(os.path.join(segment_path, "dcamera.hevc")):
    available_cameras.append("driver")
  return available_cameras

def get_all_car_models() -> dict:
  models = {}
  # List all directories in the car folder into a variable
  makes = os.listdir(CAR_VALUES_PATH)
  for make in makes:
    # Check if the values.py file exists, and if it does import it using importlib
    if os.path.exists(f"{CAR_VALUES_PATH}/{make}/values.py"):
      package = f"car.{make}"
      if not RUNNING_ON_COMMA:
        package = f"thepond.fixtures.selfdrive_car.{make}"
      values = importlib.import_module(f".values", package)
      # Get all the car models from the values.py file
      for model in values.CAR:
        model = model.value
        brand = model.split(" ")[0]
        model = " ".join(model.split(" ")[1:])
        if brand not in models:
          models[brand] = []
        models[brand].append(model)
  return models


def convert_param_type(value: bytes | None):
  """
  Small util to convert the value of a param
  to int if that is the type, otherwise the original
  input is returned
  """
  if value == None:
    return None
  output = value.decode("utf-8")
  try:
    output = int(value)
  except Exception:
    pass
  return output


def convert_settings_dict_to_array(settings: dict) -> list:
  """
  Takes a dict where the keys are the setting keys and the values are the setting values
  and converts it to an array of Setting objects where the key is inserted into the object
  """
  for key, setting in settings.items():
    setting["key"] = key
    if len(setting.get("subsettings", [])) > 0:
      setting["subsettings"] = convert_settings_dict_to_array(setting["subsettings"])
    if len(setting.get("toggles", [])) > 0:
      setting["toggles"] = convert_settings_dict_to_array(setting["toggles"])
  return list(settings.values())


def get_settings_value(setting: dict) -> None:
  """
  Reads the value for the setting, and loads all
  values for subsettings and toggles as well recursivly
  """
  value = params.get(setting["key"])
  setting["value"] = convert_param_type(value)
  if len(setting.get("subsettings", [])) > 0:
    for subsetting in setting["subsettings"]:
      get_settings_value(subsetting)
  if len(setting.get("toggles", [])) > 0:
    for toggle in setting["toggles"]:
      get_settings_value(toggle)


def find_setting(key, params):
  """
  Find a setting in the params array by key. Searches subsettings and toggles as
  well, recursivly
  """
  for param in params:
    if param["key"] == key:
      return param
    if len(param.get("subsettings", [])) > 0:
      found = find_setting(key, param["subsettings"])
      if found:
        return found
    if len(param.get("toggles", [])) > 0:
      found = find_setting(key, param["toggles"])
      if found:
        return found
  return None


def load_settings():
  """
  Load the params.json file, then load the actual values
  from the filesystem
  """
  with open(f"{DIR_OF_THIS_FILE}/params.json") as f:
    usedParams = json.load(f)

  usedParams = convert_settings_dict_to_array(usedParams)

  # Read the values from the module
  for param in usedParams:
    get_settings_value(param)
  return usedParams


def save_setting(key, value) -> None:
  """
  Save the value to the filesystem. If running
  on a comma it saves it into the file at
  /data/params/d/{key}
  If running on computer it saves it to params.txt
  """
  value = str(value)
  params.put(key, value)
  print(f"Saved param: {key} with value: {value}")

  update_frogpilot_toggles()


def get_disk_usage():
  """
  Get the disk usage of the comma device
  """
  try:
    free = get_available_bytes()
    used = get_used_bytes()
    total = used + free

    results = [{
      "mount": HARDWARE.get_device_type(),
      "size": f"{total // (2**30)} GB",
      "used": f"{used // (2**30)} GB",
      "free": f"{free // (2**30)} GB",
      "usedPercentage": f"{(used / total) * 100:.2f}%"
    }]

    return results, None

  except Exception as e:
    error_message = f"Failed getting disk usage: {e}"
    print(traceback.format_exc())
    print(error_message)
    return [], [error_message]

def get_drive_stats():
  """
  Get the drive stats of the comma device
  """
  errors = []

  try:
    stats_value = params.get("ApiCache_DriveStats", encoding='utf-8') or "{}"
    print(f"ApiCache_DriveStats: {stats_value}")
    stats = json.loads(stats_value)

  except Exception:
    error_message = f"Failed to retrieve or parse ApiCache_DriveStats: {traceback.format_exc()}"
    print(error_message)
    errors.append(error_message)
    return None, errors

  try:
    is_metric = params.get_bool("IsMetric")
    conversion_factor = CV.KPH_TO_MPH if not is_metric else 1
    unit = "km" if is_metric else "miles"

    def process_stats(timeframe):
      return {
        "distance": stats.get(timeframe, {}).get("distance", 0) * conversion_factor,
        "drives": stats.get(timeframe, {}).get("routes", 0),
        "hours": stats.get(timeframe, {}).get("minutes", 0) / 60,
        "unit": unit
      }

    stats["all"] = process_stats("all")
    stats["week"] = process_stats("week")

    stats["frogpilot"] = {
      "distance": params_tracking.get_int("FrogPilotKilometers") * conversion_factor,
      "hours": params_tracking.get_int("FrogPilotMinutes") / 60,
      "drives": params_tracking.get_int("FrogPilotDrives"),
      "unit": unit
    }

    print(stats)
    return stats, None

  except Exception:
    error_message = f"Failed processing drive stats: {traceback.format_exc()}"
    print(error_message)
    errors.append(error_message)
    return None, errors
