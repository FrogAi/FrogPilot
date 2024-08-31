import os
import requests

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import delete_file, is_url_pingable

GITHUB_URL = "https://raw.githubusercontent.com/FrogAi/FrogPilot-Resources/"
GITLAB_URL = "https://gitlab.com/FrogAi/FrogPilot-Resources/-/raw/"

def download_file(cancel_param, destination, progress_param, url, download_param, params_memory):
  try:
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    total_size = get_remote_file_size(url)
    if total_size == 0:
      return

    downloaded_size = 0
    with requests.get(url, stream=True, timeout=5) as response, open(destination, 'wb') as file:
      response.raise_for_status()
      for chunk in response.iter_content(chunk_size=8192):
        if params_memory.get_bool(cancel_param):
          handle_error(destination, "Download cancelled.", "Download cancelled.", download_param, progress_param, params_memory)
          return

        if chunk:
          file.write(chunk)
          downloaded_size += len(chunk)
          progress = (downloaded_size / total_size) * 100

          if progress != 100:
            params_memory.put(progress_param, f"{progress:.0f}%")
          else:
            params_memory.put(progress_param, "Verifying authenticity...")
  except Exception as e:
    handle_request_error(e, destination, download_param, progress_param, params_memory)

def handle_error(destination, error_message, error, download_param, progress_param, params_memory):
  print(f"Error occurred: {error}")
  if destination:
    delete_file(destination)
  if download_param:
    params_memory.remove(download_param)
  if progress_param:
    params_memory.put(progress_param, error_message)

def handle_request_error(error, destination, download_param, progress_param, params_memory):
  if isinstance(error, requests.HTTPError):
    if error.response.status_code == 404:
      return
    error_message = f"Server error ({error.response.status_code})" if error.response else "Server error."
  elif isinstance(error, requests.ConnectionError):
    error_message = "Connection dropped."
  elif isinstance(error, requests.Timeout):
    error_message = "Download timed out."
  elif isinstance(error, requests.RequestException):
    error_message = "Network request error. Check connection."
  else:
    error_message = "Unexpected error."

  handle_error(destination, f"Failed: {error_message}", error, download_param, progress_param, params_memory)

def get_remote_file_size(url):
  try:
    response = requests.head(url, headers={'Accept-Encoding': 'identity'}, timeout=5)
    response.raise_for_status()
    return int(response.headers.get('Content-Length', 0))
  except requests.HTTPError as e:
    if e.response.status_code == 404:
      return 0
    else:
      handle_request_error(e, None, None, None, None)
  except Exception as e:
    handle_request_error(e, None, None, None, None)
    return 0

def get_repository_url():
  if is_url_pingable("https://github.com"):
    return GITHUB_URL
  if is_url_pingable("https://gitlab.com"):
    return GITLAB_URL
  return None

def link_valid(url):
  try:
    response = requests.head(url, allow_redirects=True, timeout=5)
    response.raise_for_status()
    return True
  except Exception as e:
    handle_request_error(e, None, None, None, None)
    return False

def verify_download(file_path, url):
  if not os.path.exists(file_path):
    print(f"File not found: {file_path}")
    return False

  remote_file_size = get_remote_file_size(url)
  if remote_file_size is None:
    return False

  return remote_file_size == os.path.getsize(file_path)
