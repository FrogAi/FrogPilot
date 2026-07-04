#!/usr/bin/env python3
import requests
import tempfile

from datetime import datetime, timezone
from pathlib import Path

from openpilot.frogpilot.common.frogpilot_utilities import delete_file, is_url_pingable
from openpilot.frogpilot.common.frogpilot_variables import RESOURCES_REPO, params_memory

GITHUB_URL = f"https://raw.githubusercontent.com/{RESOURCES_REPO}"
GITLAB_URL = f"https://gitlab.com/{RESOURCES_REPO}/-/raw"

def check_github_rate_limit(session):
  try:
    response = session.get("https://api.github.com/rate_limit", timeout=10)
    response.raise_for_status()
    rate_limit_info = response.json()

    rate_info = rate_limit_info.get("resources", {}).get("core", {})
    remaining = rate_info.get("remaining", 0)
    print(f"GitHub API Requests Remaining: {remaining}")

    if remaining <= 0:
      reset_timestamp = rate_info.get("reset", 0)
      reset_time = datetime.fromtimestamp(reset_timestamp, tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
      print("GitHub rate limit reached")
      print(f"GitHub Rate Limit Resets At (UTC): {reset_time}")
      return False
    return True

  except requests.exceptions.RequestException as exception:
    print(f"Error checking GitHub rate limit: {exception}")
    return False

def download_file(cancel_param, destination, progress_param, url, download_param, session, offset_bytes=0, total_bytes=0):
  try:
    destination.parent.mkdir(parents=True, exist_ok=True)

    total_size = get_remote_file_size(url, session)
    if total_size == 0:
      if not url.endswith(".gif"):
        handle_error(None, "Download invalid...", "Download invalid...", download_param, progress_param)
      return

    with session.get(url, stream=True, timeout=10) as response:
      response.raise_for_status()

      with tempfile.NamedTemporaryFile(delete=False, dir=destination.parent) as temp_file:
        temp_file_path = Path(temp_file.name)

        downloaded_size = 0
        for chunk in response.iter_content(chunk_size=16384):
          if params_memory.get_bool(cancel_param):
            temp_file_path.unlink(missing_ok=True)
            handle_error(None, "Download cancelled...", "Download cancelled...", download_param, progress_param)
            return

          if chunk:
            temp_file.write(chunk)
            downloaded_size += len(chunk)

            if total_bytes:
              overall_progress = (offset_bytes + downloaded_size) / total_bytes * 100
            else:
              overall_progress = downloaded_size / total_size * 100

            if overall_progress != 100:
              params_memory.put(progress_param, f"{overall_progress:.0f}%")
            else:
              params_memory.put(progress_param, "Verifying authenticity...")

        temp_file_path.rename(destination)

  except Exception as exception:
    handle_request_error(exception, destination, download_param, progress_param)

def get_remote_file_size(url, session):
  try:
    response = session.head(url, headers={"Accept-Encoding": "identity"}, timeout=10)
    response.raise_for_status()
    return int(response.headers.get("Content-Length", 0))
  except Exception as exception:
    handle_request_error(exception, None, None, None)
    return 0

def get_repository_url(session):
  if is_url_pingable("https://github.com"):
    if check_github_rate_limit(session):
      return GITHUB_URL
  if is_url_pingable("https://gitlab.com"):
    return GITLAB_URL
  return None

def handle_error(destination, error_message, error, download_param, progress_param):
  if destination:
    delete_file(destination)

  if progress_param and "404" not in error_message:
    print(f"Error occurred: {error}")
    params_memory.put(progress_param, error_message)
    params_memory.remove(download_param)

def handle_request_error(error, destination, download_param, progress_param):
  error_map = {
    requests.exceptions.ConnectionError: "Connection dropped",
    requests.exceptions.HTTPError: lambda error: f"Server error ({error.response.status_code})" if error and getattr(error, "response", None) else "Server error",
    requests.exceptions.RequestException: "Network request error. Check connection",
    requests.exceptions.Timeout: "Download timed out",
  }

  error_message = error_map.get(type(error), "Unexpected error")
  handle_error(destination, f"Failed: {error_message}", error, download_param, progress_param)

def verify_download(file_path, url, session):
  remote_file_size = get_remote_file_size(url, session)

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
