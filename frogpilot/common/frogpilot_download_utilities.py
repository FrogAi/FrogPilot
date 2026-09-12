#!/usr/bin/env python3
import requests

from datetime import datetime, timezone

from openpilot.frogpilot.common.frogpilot_utilities import delete_file, is_url_pingable
from openpilot.frogpilot.common.frogpilot_variables import RESOURCES_REPO

GITHUB_URL = f"https://raw.githubusercontent.com/{RESOURCES_REPO}"
GITLAB_URL = f"https://gitlab.com/{RESOURCES_REPO}/-/raw"

def download_file(cancel_param, destination, download_param, params_memory, progress_param, session, url, offset_bytes=0, total_bytes=0):
  try:
    destination.parent.mkdir(parents=True, exist_ok=True)

    if cancel_param and params_memory.get_bool(cancel_param):
      handle_error(None, download_param, "Download cancelled...", "Download cancelled...", params_memory, progress_param)
      return

    with session.get(url, stream=True, timeout=10) as response:
      if response.status_code == 404 and url.endswith(".gif"):
        print(f"GIF download failed (404). Attempting fallback to PNG for {destination.name}")
        return download_file(cancel_param, destination.with_suffix(".png"), download_param, params_memory, progress_param, session, url.replace(".gif", ".png"), offset_bytes, total_bytes)

      response.raise_for_status()

      total_size = int(response.headers.get("Content-Length", 0))
      if total_size == 0:
        handle_error(None, download_param, "Download invalid...", "Download invalid...", params_memory, progress_param)
        return

      if cancel_param and params_memory.get_bool(cancel_param):
        handle_error(None, download_param, "Download cancelled...", "Download cancelled...", params_memory, progress_param)
        return

      temp_file_path = destination.with_suffix(destination.suffix + ".tmp")

      try:
        with temp_file_path.open("wb") as temp_file:
          downloaded_size = 0

          for chunk in response.iter_content(chunk_size=16384):
            if params_memory.get_bool(cancel_param):
              raise InterruptedError

            if chunk:
              temp_file.write(chunk)
              downloaded_size += len(chunk)

              if total_bytes:
                overall_progress = (offset_bytes + downloaded_size) / total_bytes * 100
              elif total_size > 0:
                overall_progress = downloaded_size / total_size * 100
              else:
                overall_progress = 0

              if overall_progress < 100:
                params_memory.put(progress_param, f"{overall_progress:.0f}%")
              else:
                params_memory.put(progress_param, "Verifying authenticity...")

        temp_file_path.replace(destination)

      except InterruptedError:
        temp_file_path.unlink(missing_ok=True)
        handle_error(None, download_param, "Download cancelled...", "Download cancelled...", params_memory, progress_param)
        return
      except Exception:
        temp_file_path.unlink(missing_ok=True)
        raise

  except Exception as exception:
    handle_request_error(destination, download_param, exception, params_memory, progress_param)


def get_remote_file_size(params_memory, session, url):
  try:
    response = session.head(url, headers={"Accept-Encoding": "identity"}, timeout=10)
    response.raise_for_status()
    size = int(response.headers.get("Content-Length", 0))

    if size == 0:
      with session.get(url, headers={"Accept-Encoding": "identity"}, stream=True, timeout=10) as response:
        response.raise_for_status()
        size = int(response.headers.get("Content-Length", 0))

    return size
  except Exception as exception:
    handle_request_error(None, None, exception, params_memory, None)
    return 0


def get_repository_url(session):
  if is_url_pingable("https://github.com", session=session) and not github_rate_limited(session):
    return GITHUB_URL
  if is_url_pingable("https://gitlab.com", session=session):
    return GITLAB_URL
  return None


def github_rate_limited(session):
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
      return True
    return False

  except requests.exceptions.RequestException as exception:
    print(f"Error checking GitHub rate limit: {exception}")
    return True


def handle_error(destination, download_param, error, error_message, params_memory, progress_param):
  if destination:
    delete_file(destination)

  if progress_param and "404" not in error_message:
    print(f"Error occurred: {error}")
    params_memory.put(progress_param, error_message)
    params_memory.remove(download_param)


def handle_request_error(destination, download_param, error, params_memory, progress_param):
  if isinstance(error, requests.exceptions.HTTPError) and error.response is not None:
    error_message = f"Server error ({error.response.status_code})"
  elif isinstance(error, (requests.exceptions.ChunkedEncodingError, requests.exceptions.ConnectionError)):
    error_message = "Connection dropped"
  elif isinstance(error, requests.exceptions.ReadTimeout):
    error_message = "Read timed out"
  elif isinstance(error, requests.exceptions.RequestException):
    error_message = "Network request error. Check connection"
  elif isinstance(error, requests.exceptions.Timeout):
    error_message = "Download timed out"
  else:
    error_message = "Unexpected error"

  handle_error(destination, download_param, error, f"Failed: {error_message}", params_memory, progress_param)


def verify_download(file_path, params_memory, session, url):
  if not file_path.is_file():
    print(f"File not found: {file_path}")
    return False

  if file_path.suffix == ".png" and url.endswith(".gif"):
    url = url.replace(".gif", ".png")

  remote_file_size = get_remote_file_size(params_memory, session, url)

  if remote_file_size == 0:
    print(f"Error fetching remote size for {file_path}")
    return False

  local_size = file_path.stat().st_size
  if remote_file_size != local_size:
    print(f"File size mismatch for {file_path}: Remote {remote_file_size} vs Local {local_size}")
    return False

  return True
