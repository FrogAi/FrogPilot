import dataclasses
import fcntl
import gzip
import hashlib
import json
import jwt
import os
import requests
import secrets
import threading
import time
import xml.etree.ElementTree as ET

from contextlib import contextmanager
from cryptography.hazmat.primitives import serialization
from email.utils import parsedate_to_datetime

from openpilot.common.api import get_key_pair
from openpilot.common.time_helpers import system_time_valid
from openpilot.system import sentry
from openpilot.system.hardware import HARDWARE
from openpilot.system.version import get_build_metadata

API_VERSION = 1

FROGPILOT_API = "https://api.frogpilot.com"

class FrogPilotAPIError(RuntimeError):
  pass


class FrogPilotAPI:
  def __init__(self, params):
    self.params = params

  @contextmanager
  def credential_lock(self):
    lock_fd = os.open(f"{self.params.get_param_path()}.frogpilot_api.lock", os.O_CREAT | os.O_RDWR, 0o600)
    try:
      fcntl.flock(lock_fd, fcntl.LOCK_EX)
      yield
    finally:
      os.close(lock_fd)

  def generate_token(self):
    return secrets.token_urlsafe(32)

  def get_token(self):
    return self.params.get("FrogPilotApiToken")

  def regenerate_token(self, failed_token, session=requests):
    with self.credential_lock():
      current_token = self.get_token()

      if current_token != failed_token or not current_token:
        return current_token

      api_token = self.generate_token()
      response = self.signed_post("/v1/token", {"api_token_hash": hashlib.sha256(api_token.encode()).hexdigest()}, session=session)

      if response is not None and 200 <= response.status_code < 300:
        if self.params.put("FrogPilotApiToken", api_token) != 0:
          raise FrogPilotAPIError("Unable to save the FrogPilot API token")
        return api_token

      if response is not None and response.status_code == 403:
        self.params.remove("FrogPilotApiToken")
        self.register_device(get_build_metadata())

      return None

  def register_device(self, build_metadata):
    def register_thread():
      profile = {
        "build_metadata": dataclasses.asdict(build_metadata),
        "device_type": HARDWARE.get_device_type(),
        "os_version": HARDWARE.get_os_version(),
      }
      digest = hashlib.sha256(json.dumps({**profile, "public_key": get_key_pair()[2]}, separators=(",", ":"), sort_keys=True).encode()).hexdigest()

      while True:
        while not system_time_valid():
          time.sleep(1)

        with self.credential_lock():
          if self.get_token() and self.params.get("FrogPilotRegistration") == digest:
            return

          api_token = self.generate_token()
          payload = {**profile, "api_token_hash": hashlib.sha256(api_token.encode()).hexdigest()}

          response = self.signed_post("/v1/register", payload)
          if response is not None:
            if 200 <= response.status_code < 300:
              for key, value in (("FrogPilotApiToken", api_token), ("FrogPilotRegistration", digest)):
                if self.params.put(key, value) != 0:
                  raise FrogPilotAPIError(f"Unable to save {key}")
              return
            elif response.status_code not in (408, 409, 429) and response.status_code < 500:
              if response.status_code == 400:
                self.params.remove("FrogPilotApiToken")
              raise FrogPilotAPIError(f"FrogPilot registration rejected ({response.status_code})")

        time.sleep(60)

    threading.Thread(target=register_thread, daemon=True).start()

  def _post(self, path, session=requests, timeout=10, **kwargs):
    try:
      return session.post(f"{FROGPILOT_API}{path}", timeout=timeout, allow_redirects=False, **kwargs)
    except requests.exceptions.RequestException:
      return None

  def post(self, path, headers=None, session=requests, **kwargs):
    def send(token):
      return self._post(path, session=session, headers={**(headers or {}), "Authorization": f"Bearer {token}"}, **kwargs)

    token = self.get_token()
    response = None

    if token:
      response = send(token)

      if response is None or response.status_code != 401:
        return response

    new_token = self.regenerate_token(token, session=session)
    return send(new_token) if new_token else response

  def post_gzip(self, path, body, timeout):
    return self.post(path, data=gzip.compress(body, compresslevel=9, mtime=0), headers={
      "Content-Encoding": "gzip",
      "Content-Type": "application/json",
    }, timeout=timeout)

  def post_json(self, path, payload, session=requests, timeout=30):
    response = self.post(path, json=payload, timeout=timeout, session=session)

    if response is None:
      raise FrogPilotAPIError(f"POST {path} failed (no response)")

    if not 200 <= response.status_code < 300:
      raise FrogPilotAPIError(f"POST {path} failed ({response.status_code})")

    return response.json()

  def signed_post(self, path, payload, session=requests):
    algorithm, private_key, public_key = get_key_pair()
    if not private_key:
      return None

    signing_key = serialization.load_pem_private_key(private_key.encode(), password=None, unsafe_skip_rsa_key_validation=True)

    body = json.dumps({**payload, "public_key": public_key}, separators=(",", ":"), sort_keys=True)
    body_sha256 = hashlib.sha256(body.encode()).hexdigest()
    now = int(time.time())
    signed_at = time.monotonic()
    for attempt in range(2):
      token = jwt.encode({
        "aud": "api.frogpilot.com",
        "auth_version": API_VERSION,
        "body_sha256": body_sha256,
        "exp": now + 2 * 60,
        "iat": now,
        "method": "POST",
        "path": path,
      }, signing_key, algorithm=algorithm)

      response = self._post(path, session=session, timeout=20, data=body, headers={"Authorization": f"JWT {token}", "Content-Type": "application/json"})
      received_at = time.monotonic()
      if response is None or response.status_code != 403 or attempt:
        return response

      server_time = parsedate_to_datetime(response.headers["Date"]).timestamp()
      if abs(server_time - (now + received_at - signed_at)) <= 5:
        return response

      now = int(server_time + time.monotonic() - received_at)

  def put_upload(self, upload, data, description, session=requests):
    response = session.put(upload["url"], data=data, headers=upload.get("headers"), timeout=60, allow_redirects=False)

    if not 200 <= response.status_code < 300:
      try:
        error = ET.fromstring(response.content)
      except ET.ParseError:
        error = ET.Element("Error")

      code = error.findtext("Code")
      upload_kind = "capture" if description.endswith(".hevc") else "telemetry"

      sentry.capture_message(f"B2 upload PUT failed ({response.status_code} {code})", level="error",
                             fingerprint=[str(response.status_code), str(code)],
                             extras={"message": error.findtext("Message")},
                             tags={"upload_kind": upload_kind})

      raise FrogPilotAPIError(f"{description} upload failed ({response.status_code})")
