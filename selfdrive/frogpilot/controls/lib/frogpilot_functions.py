import filecmp
import os
import shutil
import subprocess

from openpilot.common.basedir import BASEDIR
from openpilot.common.params_pyx import Params, UnknownKeyName
from openpilot.system.hardware import HARDWARE

def run_cmd(cmd, success_msg, fail_msg):
  try:
    subprocess.check_call(cmd)
    print(success_msg)
  except subprocess.CalledProcessError as e:
    print(f"{fail_msg}: {e}")
  except Exception as e:
    print(f"Unexpected error occurred: {e}")

def convert_params(params, params_storage):
  def convert_param(key, action_func):
    try:
      if params_storage.check_key(key) and params_storage.get_bool(key):
        action_func()
    except UnknownKeyName:
      pass

  version = 8

  try:
    if params_storage.check_key("ParamConversionVersion") and params_storage.get_int("ParamConversionVersion") == version:
      print("Params already converted, moving on.")
      return
  except UnknownKeyName:
    pass

  print("Converting params...")
  convert_param("ModelSelector", lambda: params.put_nonblocking("ModelManagement", "True"))
  convert_param("DragonPilotTune", lambda: params.put_nonblocking("FrogsGoMooTune", "True"))

  print("Params successfully converted!")
  params_storage.put_int_nonblocking("ParamConversionVersion", version)

def frogpilot_boot_functions(params, params_storage):
  convert_params(params, params_storage)

def setup_frogpilot():
  remount_root = ['sudo', 'mount', '-o', 'remount,rw', '/']
  run_cmd(remount_root, "File system remounted as read-write.", "Failed to remount file system.")

  frogpilot_boot_logo = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/frogpilot_boot_logo.png'
  frogpilot_boot_logo_jpg = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/frogpilot_boot_logo.jpg'

  boot_logo_location = '/usr/comma/bg.jpg'
  boot_logo_save_location = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/original_bg.jpg'

  if not os.path.exists(boot_logo_save_location):
    shutil.copy(boot_logo_location, boot_logo_save_location)
    print("Successfully saved original_bg.jpg.")

  if filecmp.cmp(boot_logo_save_location, frogpilot_boot_logo_jpg, shallow=False):
    os.remove(boot_logo_save_location)

  if not filecmp.cmp(frogpilot_boot_logo, boot_logo_location, shallow=False):
    run_cmd(['sudo', 'cp', frogpilot_boot_logo, boot_logo_location], "Successfully replaced bg.jpg with frogpilot_boot_logo.png.", "Failed to replace boot logo.")

def uninstall_frogpilot():
  boot_logo_location = '/usr/comma/bg.jpg'
  boot_logo_restore_location = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/original_bg.jpg'

  copy_cmd = ['sudo', 'cp', boot_logo_restore_location, boot_logo_location]
  run_cmd(copy_cmd, "Successfully restored the original boot logo.", "Failed to restore the original boot logo.")

  HARDWARE.uninstall()
