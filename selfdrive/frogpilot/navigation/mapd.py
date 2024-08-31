# PFEIFER - MAPD - Modified by FrogAi for FrogPilot to automatically update
import json
import os
import stat
import subprocess
import time
import urllib.request
import http.client
import socket
import openpilot.system.sentry as sentry

from openpilot.common.realtime import Ratekeeper

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import is_url_pingable

VERSION = 'v1'

GITHUB_VERSION_URL = f"https://github.com/FrogAi/FrogPilot-Resources/raw/Versions/mapd_version_{VERSION}.json"
GITLAB_VERSION_URL = f"https://gitlab.com/FrogAi/FrogPilot-Resources/-/raw/Versions/mapd_version_{VERSION}.json"

MAPD_PATH = '/data/media/0/osm/mapd'
VERSION_PATH = '/data/media/0/osm/mapd_version'

def get_latest_version():
  for url in [GITHUB_VERSION_URL, GITLAB_VERSION_URL]:
    try:
      with urllib.request.urlopen(url, timeout=5) as response:
        return json.loads(response.read().decode('utf-8'))['version']
    except (http.client.IncompleteRead, http.client.RemoteDisconnected, socket.gaierror, socket.timeout, urllib.error.HTTPError, urllib.error.URLError) as e:
      sentry.capture_exception(e)
      print(f"Failed to get latest version from {url}. Error: {e}")
  print("Failed to get the latest version from both sources.")
  return None

def download(current_version):
  urls = [
    f"https://github.com/pfeiferj/openpilot-mapd/releases/download/{current_version}/mapd",
    f"https://gitlab.com/FrogAi/FrogPilot-Resources/-/raw/Mapd/{current_version}"
  ]

  os.makedirs(os.path.dirname(MAPD_PATH), exist_ok=True)

  for url in urls:
    try:
      with urllib.request.urlopen(url, timeout=5) as f:
        with open(MAPD_PATH, 'wb') as output:
          output.write(f.read())
          os.fsync(output)
          os.chmod(MAPD_PATH, os.stat(MAPD_PATH).st_mode | stat.S_IEXEC)

        with open(VERSION_PATH, 'w') as version_file:
          version_file.write(current_version)
          os.fsync(version_file)

      print(f"Successfully downloaded mapd from {url}")
      return True
    except (http.client.IncompleteRead, http.client.RemoteDisconnected, socket.gaierror, socket.timeout, urllib.error.HTTPError, urllib.error.URLError) as e:
      sentry.capture_exception(e)
      print(f"Failed to download from {url}. Error: {e}")

  print(f"Failed to download mapd for version {current_version} from both sources.")
  return False

def ensure_mapd_is_running():
  while True:
    try:
      subprocess.run([MAPD_PATH], check=True)
    except Exception as e:
      sentry.capture_exception(e)
      print(f"Error running mapd process: {e}")
    time.sleep(1)

def mapd_thread(sm=None, pm=None):
  rk = Ratekeeper(0.05)

  while True:
    try:
      if is_url_pingable("https://github.com"):
        current_version = get_latest_version()
        if current_version:
          if not os.path.exists(MAPD_PATH):
            if download(current_version):
              continue
          if not os.path.exists(VERSION_PATH):
            if download(current_version):
              continue
          if open(VERSION_PATH).read() != current_version:
            if download(current_version):
              continue
      ensure_mapd_is_running()
    except Exception as e:
      sentry.capture_exception(e)
      print(f"Exception in mapd_thread: {e}")
      time.sleep(1)

    rk.keep_time()

def main(sm=None, pm=None):
  try:
    mapd_thread(sm, pm)
  except Exception as e:
    sentry.capture_exception(e)
    print(f"Unhandled exception in main: {e}")

if __name__ == "__main__":
  main()
