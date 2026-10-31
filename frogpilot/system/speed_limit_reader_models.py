#!/usr/bin/env python3
import requests

from openpilot.frogpilot.common import frogpilot_api, frogpilot_download_utilities, frogpilot_paths, frogpilot_utilities, frogpilot_variables

ENDPOINT = "/v1/speed-limit-reader"

class SpeedLimitReaderModels:
  def __init__(self, params):
    self.api = frogpilot_api.FrogPilotAPI(params)

    self.download_state = frogpilot_download_utilities.DownloadState()

    self.session = requests.Session()

  def download(self, manifest):
    for file in manifest["files"]:
      url = f"{frogpilot_api.FROGPILOT_API}{ENDPOINT}/{manifest['version']}/{file['name']}"
      if self.download_state.download_file(frogpilot_paths.SPEED_LIMIT_READER_MODEL_PATH / file["name"], self.session, url) is None:
        return

    frogpilot_utilities.update_json_file(frogpilot_paths.SPEED_LIMIT_READER_MANIFEST_PATH, manifest)

  def update(self, sm):
    if not frogpilot_utilities.on_unmetered_network(sm["deviceState"]):
      return

    response = self.api.get(f"{ENDPOINT}/{frogpilot_paths.SPEED_LIMIT_READER_MANIFEST_PATH.name}", session=self.session)
    if response is None or not 200 <= response.status_code < 300:
      return

    manifest = response.json()

    if manifest["reader_compatibility"] != frogpilot_variables.SPEED_LIMIT_READER_COMPATIBILITY:
      return

    if sorted(file["name"] for file in manifest["files"]) != sorted(frogpilot_variables.SPEED_LIMIT_READER_MODEL_FILES):
      return

    if manifest == frogpilot_utilities.load_json_file(frogpilot_paths.SPEED_LIMIT_READER_MANIFEST_PATH):
      return

    self.session.headers["Authorization"] = f"Bearer {self.api.get_token()}"
    self.download(manifest)
