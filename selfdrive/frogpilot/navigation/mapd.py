# PFEIFER - MAPD - Modified by FrogAi for FrogPilot to automatically update
import os
import socket
import stat
import subprocess
import time
import urllib.error
import urllib.request

from openpilot.common.realtime import Ratekeeper

VERSION = 'v1'
VERSION_URL = f"https://github.com/FrogAi/FrogPilot-Resources/raw/master/mapd_version_{VERSION}.txt"

MAPD_PATH = '/data/media/0/osm/mapd'
VERSION_PATH = '/data/media/0/osm/mapd_version'

def github_pinged(url="https://github.com", timeout=5):
  no_internet = 0
  while True:
    if no_internet > 5:
      break
    try:
      urllib.request.urlopen(url, timeout=timeout)
      return True
    except (urllib.error.URLError, socket.timeout):
      no_internet += 1
      time.sleep(10)
  return False

def get_latest_version():
  with urllib.request.urlopen(VERSION_URL) as response:
    return response.read().decode('utf-8').strip()

def download(current_version):
  url = f"https://github.com/pfeiferj/openpilot-mapd/releases/download/{current_version}/mapd"

  mapd_dir = os.path.dirname(MAPD_PATH)
  if not os.path.exists(mapd_dir):
    os.makedirs(mapd_dir)
  with urllib.request.urlopen(url) as f:
    with open(MAPD_PATH, 'wb') as output:
      output.write(f.read())
      os.fsync(output)
      current_permissions = stat.S_IMODE(os.lstat(MAPD_PATH).st_mode) # <-- preserve permissions
      os.chmod(MAPD_PATH, current_permissions | stat.S_IEXEC) # <-- preserve permissions
    with open(VERSION_PATH, 'w') as output:
      output.write(current_version)
      os.fsync(output)

def mapd_thread(sm=None, pm=None):
  rk = Ratekeeper(0.05, print_delay_threshold=None)

  while True:
    try:
      if github_pinged():
        current_version = get_latest_version()
        if not os.path.exists(MAPD_PATH):
          download(current_version)
          continue
        if not os.path.exists(VERSION_PATH):
          download(current_version)
          continue
        with open(VERSION_PATH) as f:
          content = f.read()
          if content != current_version:
            download(current_version)
            continue

      process = subprocess.Popen(MAPD_PATH)
      process.wait()
    except Exception as e:
      print(e)

    rk.keep_time()


def main(sm=None, pm=None):
  mapd_thread(sm, pm)

if __name__ == "__main__":
  main()
