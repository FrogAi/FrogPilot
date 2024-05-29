import filecmp
import os
import shutil
import subprocess

from openpilot.common.basedir import BASEDIR
from openpilot.system.hardware import HARDWARE

class FrogPilotFunctions:
  @classmethod
  def run_cmd(cls, cmd, success_msg, fail_msg):
    try:
      subprocess.check_call(cmd)
      print(success_msg)
    except subprocess.CalledProcessError as e:
      print(f"{fail_msg}: {e}")
    except Exception as e:
      print(f"Unexpected error occurred: {e}")

  @classmethod
  def setup_frogpilot(cls):
    frogpilot_boot_logo = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/frogpilot_boot_logo.png'
    boot_logo_location = '/usr/comma/bg.jpg'
    boot_logo_save_location = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/original_bg.jpg'

    remount_root = ['sudo', 'mount', '-o', 'remount,rw', '/']
    cls.run_cmd(remount_root, "File system remounted as read-write.", "Failed to remount file system.")

    if not os.path.exists(boot_logo_save_location):
      shutil.copy(boot_logo_location, boot_logo_save_location)
      print("Successfully backed up the original boot logo.")

    if not filecmp.cmp(frogpilot_boot_logo, boot_logo_location, shallow=False):
      copy_cmd = ['sudo', 'cp', frogpilot_boot_logo, boot_logo_location]
      cls.run_cmd(copy_cmd, "Successfully replaced bg.jpg with frogpilot_boot_logo.png.", "Failed to replace boot logo.")

  @classmethod
  def uninstall_frogpilot(cls):
    original_boot_logo = f'{BASEDIR}/selfdrive/frogpilot/assets/other_images/original_bg.jpg'
    boot_logo_location = '/usr/comma/bg.jpg'

    copy_cmd = ['sudo', 'cp', original_boot_logo, boot_logo_location]
    cls.run_cmd(copy_cmd, "Successfully restored the original boot logo.", "Failed to restore the original boot logo.")

    HARDWARE.uninstall()
