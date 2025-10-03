#!/usr/bin/env python3
import json
import requests
import subprocess
import tarfile
import threading
import zipfile

from pathlib import Path

import openpilot.system.sentry as sentry

from openpilot.system.hardware import HARDWARE

running_threads = {}

locks = {
  "backup_toggles": threading.Lock(),
  "update_checks": threading.Lock(),
  "update_openpilot": threading.Lock(),
}


def run_thread_with_lock(name, target, args=(), report=True):
  if not running_threads.get(name, threading.Thread()).is_alive():
    with locks[name]:
      def wrapped_target(*t_args):
        try:
          target(*t_args)
        except Exception as exception:
          print(f"Error in thread '{name}': {exception}")
          if report:
            sentry.capture_exception(exception)
      thread = threading.Thread(target=wrapped_target, args=args, daemon=True)
      thread.start()
      running_threads[name] = thread


# Credit goes to Pfeiferj!
def calculate_road_curvature(modelData, v_ego):
  orientation_rate = np.array(modelData.orientationRate.z)
  velocity = np.array(modelData.velocity.x)
  timebase = np.array(modelData.orientationRate.t)

  lateral_acceleration = orientation_rate * velocity
  index = np.argmax(np.abs(lateral_acceleration))
  predicted_lateral_acc = float(lateral_acceleration[index])
  time_to_curve = float(timebase[index])

  return predicted_lateral_acc / max(v_ego, 1)**2, max(time_to_curve, 1)


def clean_model_name(name):
  return (
    name.replace("🗺️", "")
    .replace("📡", "")
    .replace("👀", "")
    .replace("(Default)", "")
    .strip()
  )


def delete_file(path, print_error=True, report=True):
  path = Path(path)
  if path.is_file() or path.is_symlink():
    run_cmd(["sudo", "rm", "-f", str(path)], f"Deleted file: {path}", f"Failed to delete file: {path}", report=report)
  elif path.is_dir():
    run_cmd(["sudo", "rm", "-rf", str(path)], f"Deleted directory: {path}", f"Failed to delete directory: {path}", report=report)
  elif print_error:
    print(f"File not found: {path}")


def extract_tar(tar_file, extract_path):
  tar_file = Path(tar_file)
  extract_path = Path(extract_path)
  print(f"Extracting {tar_file} to {extract_path}")

  with tarfile.open(tar_file, "r:gz") as tar:
    tar.extractall(path=extract_path)

  tar_file.unlink()
  print(f"Extraction completed: {tar_file} has been removed")


def extract_zip(zip_file, extract_path):
  zip_file = Path(zip_file)
  extract_path = Path(extract_path)
  print(f"Extracting {zip_file} to {extract_path}")

  with zipfile.ZipFile(zip_file, "r") as zip_ref:
    zip_ref.extractall(extract_path)

  zip_file.unlink()
  print(f"Extraction completed: {zip_file} has been removed")


def is_url_pingable(url):
  headers = {"User-Agent": "frogpilot-ping-test/1.0 (https://github.com/FrogAi/FrogPilot)"}
  try:
    response = requests.head(url, headers=headers, timeout=10, allow_redirects=True)
    if response.status_code in (405, 501):
      response = requests.get(url, headers=headers, timeout=10, allow_redirects=True, stream=True)
    return response.ok
  except requests.exceptions.RequestException as error:
    print(f"{error.__class__.__name__} while pinging {url}: {error}")
    return False
  except Exception as exception:
    print(f"Unexpected error while pinging {url}: {exception}")
    return False


def load_json_file(path):
  if path.is_file():
    with open(path) as file:
      return json.load(file)
  return {}


def run_cmd(cmd, success_message, fail_message, report=True, env=None):
  try:
    result = subprocess.run(cmd, capture_output=True, check=True, env=env, text=True)
    print(success_message)
    return result.stdout.strip()
  except Exception as exception:
    print(f"Unexpected error occurred: {exception}")
    print(fail_message)
    if report:
      sentry.capture_exception(exception)
    return None


def update_json_file(path, data):
  with open(path, "w") as file:
    json.dump(data, file, indent=2, sort_keys=True)


def update_openpilot(params, params_memory):
  def update_available():
    run_cmd(["pkill", "-SIGUSR1", "-f", "system.updated.updated"], "Updater check signal sent", "Failed to send updater check signal", report=False)

    while params.get("UpdaterState") != "checking...":
      time.sleep(1)

    while params.get("UpdaterState") == "checking...":
      time.sleep(1)

    if not params.get_bool("UpdaterFetchAvailable"):
      return False

    while params.get("UpdaterState") != "idle":
      time.sleep(60)

    run_cmd(["pkill", "-SIGHUP", "-f", "system.updated.updated"], "Updater refresh signal sent", "Failed to send updater refresh signal", report=False)

    while not params.get_bool("UpdateAvailable"):
      time.sleep(60)

    return True

  if params.get("UpdaterState") != "idle":
    return

  while params.get_bool("IsOnroad") or params_memory.get_bool("UpdateSpeedLimits") or running_threads.get("lock_doors", threading.Thread()).is_alive():
    time.sleep(60)

  if not update_available():
    return

  while True:
    if not update_available():
      break

  HARDWARE.reboot()
