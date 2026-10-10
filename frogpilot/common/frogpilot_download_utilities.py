#!/usr/bin/env python3
import requests

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables

GITHUB_URL = f"https://raw.githubusercontent.com/{frogpilot_variables.RESOURCES_REPO}"
GITLAB_URL = f"https://gitlab.com/{frogpilot_variables.RESOURCES_REPO}/-/raw"

class DownloadState:
  def __init__(self):
    self.cancelled = False
    self.progress = ""

def download_file(destination, download_state, session, url):
  try:
    destination.parent.mkdir(parents=True, exist_ok=True)

    if download_state.cancelled:
      handle_error(None, "Download cancelled...", download_state)
      return

    with session.get(url, stream=True, timeout=10) as response:
      if response.status_code == 404 and url.endswith(".gif"):
        return download_file(destination.with_suffix(".png"), download_state, session, url.replace(".gif", ".png"))

      response.raise_for_status()

      total_size = int(response.headers.get("Content-Length", 0))
      if total_size == 0:
        handle_error(None, "Download invalid...", download_state)
        return

      if download_state.cancelled:
        handle_error(None, "Download cancelled...", download_state)
        return

      temp_file_path = destination.with_suffix(destination.suffix + ".tmp")

      try:
        with temp_file_path.open("wb") as temp_file:
          downloaded_size = 0

          for chunk in response.iter_content(chunk_size=16384):
            if download_state.cancelled:
              raise InterruptedError

            if chunk:
              temp_file.write(chunk)
              downloaded_size += len(chunk)

              overall_progress = downloaded_size / total_size * 100

              download_state.progress = f"{overall_progress:.0f}%"

        temp_file_path.replace(destination)
        return destination

      except InterruptedError:
        temp_file_path.unlink(missing_ok=True)
        handle_error(None, "Download cancelled...", download_state)
        return
      except Exception:
        temp_file_path.unlink(missing_ok=True)
        raise

  except Exception as exception:
    handle_request_error(exception, download_state)


def get_remote_file_size(session, url):
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
  if frogpilot_utilities.is_url_pingable("https://github.com", session=session) and not github_rate_limited(session):
    return GITHUB_URL
  if frogpilot_utilities.is_url_pingable("https://gitlab.com", session=session):
    return GITLAB_URL
  return None


def github_rate_limited(session):
  try:
    response = session.get("https://api.github.com/rate_limit", timeout=10)
    response.raise_for_status()
    rate_limit_info = response.json()

    return rate_limit_info.get("resources", {}).get("core", {}).get("remaining", 0) <= 0

  except requests.exceptions.RequestException:
    return True


def handle_error(destination, error_message, download_state):
  if destination:
    frogpilot_utilities.delete_file(destination)

  if download_state and "404" not in error_message:
    download_state.progress = error_message


def handle_request_error(error, download_state):
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

  handle_error(None, f"Failed: {error_message}", download_state)


def verify_download(file_path, session, url):
  if not file_path.is_file():
    return False

  if file_path.suffix == ".png" and url.endswith(".gif"):
    url = url.replace(".gif", ".png")

  remote_file_size = get_remote_file_size(session, url)

  if remote_file_size == 0:
    return False

  return remote_file_size == file_path.stat().st_size
