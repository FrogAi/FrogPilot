import os
import requests

from openpilot.selfdrive.frogpilot.frogpilot_utilities import delete_file, is_url_pingable

GITHUB_URL = "https://raw.githubusercontent.com/FrogAi/FrogPilot-Resources/"
GITLAB_URL = "https://gitlab.com/FrogAi/FrogPilot-Resources/-/raw/"

def download_file(cancel_param, destination, temp_destination, progress_param, url, download_param, params_memory):
  try:
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    total_size = get_remote_file_size(url)
    if total_size == 0:
      return

    downloaded_size = 0
    with requests.get(url, stream=True, timeout=5) as response, open(temp_destination, 'wb') as file:
      response.raise_for_status()
      for chunk in response.iter_content(chunk_size=8192):
        if params_memory.get_bool(cancel_param):
          handle_error(temp_destination, "Download cancelled", "Download cancelled", download_param, progress_param, params_memory)
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
    handle_request_error(e, temp_destination, download_param, progress_param, params_memory)

def handle_error(destination, error_message, error, download_param, progress_param, params_memory):
  if destination:
    delete_file(destination)

  if download_param:
    params_memory.remove(download_param)

  if progress_param and "404" not in error_message:
    print(f"Error occurred: {error}")
    params_memory.put(progress_param, error_message)

def handle_request_error(error, destination, download_param, progress_param, params_memory):
  error_map = {
    requests.ConnectionError: "Connection dropped",
    requests.HTTPError: lambda e: f"Server error ({e.response.status_code})" if e.response else "Server error",
    requests.RequestException: "Network request error. Check connection",
    requests.Timeout: "Download timed out"
  }

  error_message = error_map.get(type(error), "Unexpected error")
  handle_error(destination, f"Failed: {error_message}", error, download_param, progress_param, params_memory)

def get_remote_file_size(url):
  try:
    response = requests.head(url, headers={'Accept-Encoding': 'identity'}, timeout=5)
    if response.status_code == 404:
      print(f"URL not found: {url}")
      return 0
    response.raise_for_status()
    return int(response.headers.get('Content-Length', 0))
  except requests.HTTPError as e:
    if e.response and e.response.status_code == 404:
      print(f"URL not found: {url}")
      return 0
    handle_request_error(e, None, None, None, None)
    return 0
  except Exception as e:
    handle_request_error(e, None, None, None, None)
    return 0

def get_repository_url():
  if is_url_pingable("https://github.com"):
    return GITHUB_URL
  if is_url_pingable("https://gitlab.com"):
    return GITLAB_URL
  return None

def verify_download(file_path, temp_file_path, url, initial_download=True):
  remote_file_size = get_remote_file_size(url)
  if remote_file_size is None:
    print(f"Error fetching remote size for {file_path}")
    return False if initial_download else True

  if not os.path.isfile(temp_file_path):
    print(f"File not found: {temp_file_path}")
    return False

  try:
    if remote_file_size != os.path.getsize(temp_file_path):
      print(f"File size mismatch for {temp_file_path}")
      return False
  except Exception as e:
    print(f"An unexpected error occurred while trying to verify the {temp_file_path} download: {e}")
    return False

  os.rename(temp_file_path, file_path)
  return True
