# PFEIFER - MAPD - Modified by FrogAi for FrogPilot
#!/usr/bin/env python3
import json
import os
import shutil
import stat
import subprocess
import time
import urllib.request

from pathlib import Path

from openpilot.selfdrive.frogpilot.frogpilot_utilities import is_url_pingable
from openpilot.selfdrive.frogpilot.frogpilot_variables import MAPD_PATH

VERSION = "v1"

GITHUB_VERSION_URL = f"https://github.com/FrogAi/FrogPilot-Resources/raw/Versions/mapd_version_{VERSION}.json"
GITLAB_VERSION_URL = f"https://gitlab.com/FrogAi/FrogPilot-Resources/-/raw/Versions/mapd_version_{VERSION}.json"

VERSION_PATH = Path("/data/media/0/osm/mapd_version")

def download():
  Path(MAPD_PATH).parent.mkdir(parents=True, exist_ok=True)

  while not (is_url_pingable("https://github.com") or is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  latest_version = get_latest_version()

  urls = [
    f"https://github.com/pfeiferj/openpilot-mapd/releases/download/{latest_version}/mapd",
    f"https://gitlab.com/FrogAi/FrogPilot-Resources/-/raw/Mapd/{latest_version}"
  ]

  for url in urls:
    try:
      with urllib.request.urlopen(url) as response:
        with open(MAPD_PATH, "wb") as mapd:
          shutil.copyfileobj(response, mapd)
          os.fsync(mapd.fileno())
          os.chmod(MAPD_PATH, os.stat(MAPD_PATH).st_mode | stat.S_IEXEC)
      with open(VERSION_PATH, "w") as version_file:
        version_file.write(latest_version)
        os.fsync(version_file.fileno())
      return
    except Exception as error:
      print(f"Failed to download mapd from {url}: {error}")

def get_latest_version():
  while not (is_url_pingable("https://github.com") or is_url_pingable("https://gitlab.com")):
    time.sleep(60)

  for url in [GITHUB_VERSION_URL, GITLAB_VERSION_URL]:
    try:
      with urllib.request.urlopen(url, timeout=10) as response:
        return json.loads(response.read().decode("utf-8"))["version"]
    except Exception as error:
      print(f"Error fetching mapd version from {url}: {error}")
  return "v0"

def mapd_thread():
  if os.path.exists(MAPD_PATH) and os.path.isdir(MAPD_PATH):
    shutil.rmtree(MAPD_PATH)

  while True:
    if not os.path.exists(MAPD_PATH):
      print(f"{MAPD_PATH} not found. Downloading...")
      download()
      continue

    if not os.path.exists(VERSION_PATH):
      download()
      continue

    with open(VERSION_PATH) as version_file:
      if is_url_pingable("https://github.com") or is_url_pingable("https://gitlab.com"):
        if version_file.read().strip() != get_latest_version():
          print("New mapd version available. Downloading...")
          download()
          continue

    process = subprocess.Popen(MAPD_PATH)
    process.wait()

def main():
  mapd_thread()

if __name__ == "__main__":
  main()
