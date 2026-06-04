#!/usr/bin/env python3
import requests
import zipfile

from contextlib import suppress
from pathlib import Path
from PIL import Image

from openpilot.frogpilot.common import frogpilot_utilities

REPOSITORY_SOURCES = (
  ("github", "GitHub", f"https://github.com/{frogpilot_variables.RESOURCES_REPO}", f"https://raw.githubusercontent.com/{frogpilot_variables.RESOURCES_REPO}"),
  ("gitlab", "GitLab", f"https://gitlab.com/{frogpilot_variables.RESOURCES_REPO}", f"https://gitlab.com/{frogpilot_variables.RESOURCES_REPO}/-/raw"),
)

IMAGE_FORMATS = {
  ".gif": "GIF",
  ".png": "PNG",
}


def cleanup_download_target(destination):
  if destination is None:
    return

  try:
    destination_path = Path(destination)
    if destination_path.is_file() or destination_path.is_symlink():
      destination_path.unlink(missing_ok=True)
    elif destination_path.is_dir():
      frogpilot_utilities.delete_file(destination_path)
  except Exception as cleanup_error:
    print(f"Failed to clean up download target: {cleanup_error}")


def download_attempts(destination, url):
  attempts = [(destination, url)]
  if url.endswith(".gif"):
    attempts.append((destination.with_suffix(".png"), png_fallback_url(url)))
  return attempts


def download_file(cancel_param, destination, download_param, params, progress_param, session, url, offset_bytes=0, total_bytes=0):
  for download_destination, download_url in download_attempts(destination, url):
    temp_file_path = download_destination.with_suffix(download_destination.suffix + ".tmp")

    try:
      download_destination.parent.mkdir(parents=True, exist_ok=True)

      with session.get(download_url, stream=True, timeout=10) as response:
        if response.status_code == 404 and download_url.endswith(".gif"):
          print(f"GIF download failed (404). Attempting fallback to PNG for {download_destination.name}")
          download_destination.unlink(missing_ok=True)
          continue

        response.raise_for_status()

        write_download(response, temp_file_path, cancel_param, params, progress_param, offset_bytes, total_bytes)

      temp_file_path.replace(download_destination)
      return download_destination, None, False

    except InterruptedError:
      temp_file_path.unlink(missing_ok=True)
      return None, "Download cancelled...", True
    except Exception as error:
      temp_file_path.unlink(missing_ok=True)
      error_message, _, _ = handle_request_error(error)
      return None, f"Failed: {error_message}", False


def get_content_length(response):
  try:
    content_length = int(response.headers.get("Content-Length", -1))
  except (TypeError, ValueError):
    return None

  return content_length if content_length >= 0 else None


def get_remote_file_size(session, url):
  headers = {"Accept-Encoding": "identity"}

  try:
    with suppress(requests.exceptions.RequestException):
      with session.head(url, headers=headers, timeout=10, allow_redirects=True) as response:
        remote_size = get_content_length(response)
        if response.status_code < 400 and remote_size is not None:
          return remote_size

    with session.get(url, headers=headers, stream=True, timeout=10, allow_redirects=True) as response:
      response.raise_for_status()

      remote_size = get_content_length(response)
      if remote_size is not None:
        return remote_size

      return sum(len(chunk) for chunk in response.iter_content(chunk_size=16384) if chunk)

  except Exception as error:
    error_message, _, _ = handle_request_error(error)
    print(f"Failed to determine remote file size for {url}: {error_message}")
    return None


def get_repository_sources():
  return [
    (source, name, url)
    for source, name, ping_url, url in REPOSITORY_SOURCES
    if frogpilot_utilities.is_url_pingable(ping_url)
  ]


def handle_error(destination, download_param, error, error_message, params, progress_param):
  if error is not None:
    print(f"Error occurred: {error}")

  cleanup_download_target(destination)

  if progress_param:
    params.put(progress_param, error_message)
  if download_param:
    params.remove(download_param)


def handle_request_error(error):
  if isinstance(error, requests.exceptions.HTTPError) and error.response is not None:
    status_code = error.response.status_code
    return f"Server error ({status_code})", status_code in {408, 409, 425, 429, 500, 502, 503, 504}, status_code

  request_errors = (
    (requests.exceptions.ReadTimeout, "Read timed out", True),
    (requests.exceptions.Timeout, "Download timed out", True),
    (requests.exceptions.TooManyRedirects, "Too many redirects", False),
    ((requests.exceptions.ChunkedEncodingError, requests.exceptions.ConnectionError), "Connection dropped", True),
    (requests.exceptions.RequestException, "Network request error. Check connection", True),
  )

  for error_type, error_message, retryable in request_errors:
    if isinstance(error, error_type):
      return error_message, retryable, None

  return "Unexpected error", False, None


def png_fallback_url(url):
  return f"{url.removesuffix('.gif')}.png"


def verify_download(file_path, session, url):
  if not file_path.is_file():
    return False, f"Downloaded file is missing: {file_path.name}"

  verified_url = png_fallback_url(url) if file_path.suffix == ".png" and url.endswith(".gif") else url

  remote_file_size = get_remote_file_size(session, verified_url)
  if remote_file_size is None:
    return False, f"Unable to verify file size for {file_path.name}"

  local_size = file_path.stat().st_size
  if remote_file_size != local_size:
    return False, f"File size mismatch for {file_path.name}: Remote {remote_file_size} vs Local {local_size}"

  file_type_error = verify_file_type(file_path)
  if file_type_error is not None:
    return False, file_type_error

  return True, None


def verify_file_type(file_path):
  file_suffix = file_path.suffix.lower()

  if file_suffix == ".zip":
    if not zipfile.is_zipfile(file_path):
      return f"Downloaded ZIP is invalid: {file_path.name}"

    with zipfile.ZipFile(file_path) as archive:
      bad_member = archive.testzip()
      if bad_member is not None:
        return f"Downloaded ZIP contains a corrupt file: {bad_member}"
      if not archive.namelist():
        return f"Downloaded ZIP is empty: {file_path.name}"
    return None

  expected_format = IMAGE_FORMATS.get(file_suffix)
  if expected_format is None:
    return None

  try:
    with Image.open(file_path) as image:
      if image.format != expected_format:
        return f"Downloaded {expected_format} has an invalid signature: {file_path.name}"
      image.verify()
  except OSError:
    return f"Downloaded {expected_format} is invalid: {file_path.name}"

  return None


def write_download(response, temp_file_path, cancel_param, params, progress_param, offset_bytes=0, total_bytes=0):
  total_size = get_content_length(response)
  downloaded_size = 0

  with temp_file_path.open("wb") as temp_file:
    for chunk in response.iter_content(chunk_size=16384):
      if params.get_bool(cancel_param):
        raise InterruptedError

      if not chunk:
        continue

      temp_file.write(chunk)
      downloaded_size += len(chunk)

      if total_bytes:
        overall_progress = (offset_bytes + downloaded_size) / total_bytes * 100
      elif total_size and total_size > 0:
        overall_progress = downloaded_size / total_size * 100
      else:
        overall_progress = 0

      if total_size is None and not total_bytes:
        params.put(progress_param, "Downloading...")
      elif overall_progress < 100:
        params.put(progress_param, f"{overall_progress:.0f}%")
      else:
        params.put(progress_param, "Verifying download...")
