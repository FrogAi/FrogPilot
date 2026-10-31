#!/usr/bin/env python3
import requests

from datetime import datetime, timezone

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables

GITHUB_URL = f"https://raw.githubusercontent.com/{frogpilot_variables.RESOURCES_REPO}"
GITLAB_URL = f"https://gitlab.com/{frogpilot_variables.RESOURCES_REPO}/-/raw"

class DownloadState:
  def __init__(self):
    self.cancelled = False
    self.progress = ""

  def download_file(self, destination, session, url, offset_bytes=0, total_bytes=0):
    try:
      destination.parent.mkdir(parents=True, exist_ok=True)

      if self.cancelled:
        self.handle_error(None, "Download cancelled...")
        return

      with session.get(url, stream=True, timeout=10) as response:
        if response.status_code == 404 and url.endswith(".gif"):
          return self.download_file(destination.with_suffix(".png"), session, url.replace(".gif", ".png"), offset_bytes, total_bytes)

        response.raise_for_status()

        total_size = int(response.headers.get("Content-Length", 0))
        if total_size == 0:
          self.handle_error(None, "Download invalid...")
          return

        temp_file_path = destination.with_suffix(destination.suffix + ".tmp")

        try:
          with temp_file_path.open("wb") as temp_file:
            downloaded_size = 0

            for chunk in response.iter_content(chunk_size=16384):
              if self.cancelled:
                temp_file_path.unlink(missing_ok=True)
                self.handle_error(None, "Download cancelled...")
                return

              if chunk:
                temp_file.write(chunk)
                downloaded_size += len(chunk)

                overall_progress = (offset_bytes + downloaded_size) / (total_bytes or total_size) * 100

                self.progress = f"{overall_progress:.0f}%"

          temp_file_path.replace(destination)
          return destination

        except Exception:
          temp_file_path.unlink(missing_ok=True)
          raise

    except Exception as exception:
      self.handle_request_error(exception)

  def handle_error(self, destination, error_message):
    if destination:
      frogpilot_utilities.delete_path(destination)

    if "404" not in error_message:
      self.progress = error_message

  def handle_request_error(self, error):
    if isinstance(error, requests.exceptions.HTTPError) and error.response is not None:
      error_message = f"Server error ({error.response.status_code})"
    elif isinstance(error, (requests.exceptions.ChunkedEncodingError, requests.exceptions.ConnectionError)):
      error_message = "Connection dropped"
    elif isinstance(error, requests.exceptions.ReadTimeout):
      error_message = "Read timed out"
    elif isinstance(error, requests.exceptions.RequestException):
      error_message = "Network request error. Check connection"
    else:
      error_message = "Unexpected error"

    self.handle_error(None, f"Failed: {error_message}")

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

def get_remote_file_size(url, session):
  try:
    response = session.head(url, headers={"Accept-Encoding": "identity"}, timeout=10)
    response.raise_for_status()
    size = int(response.headers.get("Content-Length", 0))

    if size == 0:
      with session.get(url, headers={"Accept-Encoding": "identity"}, stream=True, timeout=10) as response:
        response.raise_for_status()
        size = int(response.headers.get("Content-Length", 0))

    return size
  except Exception:
    return 0

def get_repository_url(session):
  if frogpilot_utilities.is_url_pingable("https://github.com", session=session):
    if check_github_rate_limit(session):
      return GITHUB_URL
  if frogpilot_utilities.is_url_pingable("https://gitlab.com", session=session):
    return GITLAB_URL
  return None

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
