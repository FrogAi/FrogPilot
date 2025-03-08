#!/usr/bin/env python3
import requests
import tempfile

from datetime import datetime
from pathlib import Path

from openpilot.selfdrive.frogpilot.frogpilot_utilities import delete_file, is_url_pingable

GITHUB_URL = "https://raw.githubusercontent.com/FrogAi/FrogPilot-Resources"
GITLAB_URL = "https://gitlab.com/FrogAi/FrogPilot-Resources/-/raw"

def check_github_rate_limit():
  try:
    response = requests.get("https://api.github.com/rate_limit")
    response.raise_for_status()
    rate_limit_info = response.json()

    remaining = rate_limit_info["rate"]["remaining"]
    print(f"GitHub API Requests Remaining: {remaining}")
    if remaining > 0:
      return True

    reset_time = datetime.utcfromtimestamp(rate_limit_info["rate"]["reset"]).strftime("%Y-%m-%d %H:%M:%S")
    print("GitHub rate limit reached")
    print(f"GitHub Rate Limit Resets At (UTC): {reset_time}")
    return False
  except requests.exceptions.RequestException as error:
    print(f"Error checking GitHub rate limit: {error}")
    return False

def download_file(cancel_param, destination, progress_param, url, download_param, params_memory):
  try:
    destination.parent.mkdir(parents=True, exist_ok=True)

    total_size = get_remote_file_size(url)
    if total_size == 0:
      return

    with requests.get(url, stream=True, timeout=10) as response:
      response.raise_for_status()

      with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as temp_file:
        temp_file_path = Path(temp_file.name)

        downloaded_size = 0
        for chunk in response.iter_content(chunk_size=16384):
          if params_memory.get_bool(cancel_param):
            temp_file_path.unlink(missing_ok=True)
            handle_error(None, "Download cancelled...", "Download cancelled...", download_param, progress_param, params_memory)
            return

          if chunk:
            temp_file.write(chunk)
            downloaded_size += len(chunk)

            progress = (downloaded_size / total_size) * 100
            if progress != 100:
              params_memory.put(progress_param, f"{progress:.0f}%")
            else:
              params_memory.put(progress_param, "Verifying authenticity...")

        temp_file_path.rename(destination)

  except Exception as error:
    handle_request_error(error, destination, download_param, progress_param, params_memory)

def handle_error(destination, error_message, error, download_param, progress_param, params_memory):
  if destination:
    delete_file(destination)

  if progress_param and "404" not in error_message:
    print(f"Error occurred: {error}")
    params_memory.put(progress_param, error_message)
    params_memory.remove(download_param)

def handle_request_error(error, destination, download_param, progress_param, params_memory):
  error_map = {
    requests.ConnectionError: "Connection dropped",
    requests.HTTPError: lambda error: f"Server error ({error.response.status_code})" if error.response else "Server error",
    requests.RequestException: "Network request error. Check connection",
    requests.Timeout: "Download timed out"
  }

  error_message = error_map.get(type(error), "Unexpected error")
  handle_error(destination, f"Failed: {error_message}", error, download_param, progress_param, params_memory)

def get_remote_file_size(url):
  try:
    response = requests.head(url, headers={"Accept-Encoding": "identity"}, timeout=10)
    response.raise_for_status()
    return int(response.headers.get("Content-Length", 0))
  except Exception as error:
    handle_request_error(error, None, None, None, None)
    return 0

def get_repository_url():
  if is_url_pingable("https://github.com"):
    if check_github_rate_limit():
      return GITHUB_URL
  if is_url_pingable("https://gitlab.com"):
    return GITLAB_URL
  return None

def verify_download(file_path, url):
  remote_file_size = get_remote_file_size(url)

  if remote_file_size == 0:
    print(f"Error fetching remote size for {file_path}")
    return False

  if not file_path.is_file():
    print(f"File not found: {file_path}")
    return False

  if remote_file_size != file_path.stat().st_size:
    print(f"File size mismatch for {file_path}")
    return False

  return True
