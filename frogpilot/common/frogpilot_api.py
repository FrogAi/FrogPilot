import dataclasses
import fcntl
import hashlib
import json
import jwt
import os
import requests
import secrets
import threading
import time

from contextlib import contextmanager

from openpilot.common.api import get_key_pair
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import HARDWARE

API_VERSION = 1

FROGPILOT_API = "https://api.frogpilot.com"

class FrogPilotAPIError(RuntimeError):
  pass


class FrogPilotAPI:
  def __init__(self, params):
    self.params = params

    self._credential_thread_lock = threading.Lock()

  @contextmanager
  def credential_lock(self):
    with self._credential_thread_lock:
      lock_fd = os.open(f"{self.params.get_param_path()}.frogpilot_api.lock", os.O_CREAT | os.O_RDWR, 0o600)
      try:
        fcntl.lockf(lock_fd, fcntl.LOCK_EX)
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

      if current_token and current_token != failed_token:
        return current_token

      if not system_time_valid():
        return None

      for attempt in range(2):
        api_token = self.generate_token()
        response = self.signed_post("/v1/token", {"api_token_hash": hashlib.sha256(api_token.encode()).hexdigest()}, session=session)

        if response is not None and 200 <= response.status_code < 300:
          self.params.put("FrogPilotApiToken", api_token)
          return api_token

        if response is None or response.status_code != 409 or attempt:
          break

        retry_time = int(time.time()) + 1
        time.sleep(1)
        if int(time.time()) < retry_time:
          break

      self.params.remove("FrogPilotRegistration")
      return None

  def register_device(self, build_metadata):
    def register_thread():
      while not system_time_valid():
        time.sleep(1)

      profile = {
        "build_metadata": dataclasses.asdict(build_metadata),
        "device_type": HARDWARE.get_device_type(),
        "os_version": HARDWARE.get_os_version(),
        "profile_schema_version": API_VERSION,
      }

      while True:
        with self.credential_lock():
          api_token = self.get_token()
          if api_token:
            payload = {**profile, "api_token_hash": hashlib.sha256(api_token.encode()).hexdigest()}
            digest = self.body_digest(payload)
            if self.params.get("FrogPilotDongleId") and self.params.get("FrogPilotRegistration") == digest:
              return

          api_token = self.generate_token()
          payload = {**profile, "api_token_hash": hashlib.sha256(api_token.encode()).hexdigest()}
          digest = self.body_digest(payload)

          response = self.signed_post("/v1/register", payload)
          if response is not None:
            if 200 <= response.status_code < 300:
              try:
                frogpilot_dongle_id = response.json()["frogpilot_dongle_id"]
                if not isinstance(frogpilot_dongle_id, str) or not frogpilot_dongle_id.strip():
                  raise ValueError("Invalid device ID")
              except (KeyError, TypeError, ValueError):
                break

              self.params.put("FrogPilotApiToken", api_token)
              self.params.put("FrogPilotDongleId", frogpilot_dongle_id)
              self.params.put("FrogPilotRegistration", digest)
              return
            elif response.status_code not in (409, 429) and response.status_code < 500:
              break

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

    if token:
      response = send(token)

      if response is None or response.status_code != 401:
        return response

    token = self.regenerate_token(token, session=session)
    return send(token) if token else None

  def post_json(self, path, payload, session=requests, timeout=30):
    response = self.post(path, json=payload, timeout=timeout, session=session)

    if response is None:
      raise FrogPilotAPIError(f"POST {path} failed (no response)")

    if not 200 <= response.status_code < 300:
      raise FrogPilotAPIError(f"POST {path} failed ({response.status_code})")

    return response.json()

  def body_digest(self, payload):
    return hashlib.sha256(json.dumps({**payload, "public_key": get_key_pair()[2]}, separators=(",", ":"), sort_keys=True).encode()).hexdigest()

  def signed_post(self, path, payload, session=requests):
    algorithm, private_key, public_key = get_key_pair()
    if not private_key:
      return None

    body = json.dumps({**payload, "public_key": public_key}, separators=(",", ":"), sort_keys=True)
    now = int(time.time())
    token = jwt.encode({
      "aud": "api.frogpilot.com",
      "auth_version": API_VERSION,
      "body_sha256": hashlib.sha256(body.encode()).hexdigest(),
      "exp": now + 2 * 60,
      "iat": now,
      "method": "POST",
      "path": path,
    }, private_key, algorithm=algorithm)

    return self._post(path, session=session, timeout=20, data=body, headers={"Authorization": f"JWT {token}", "Content-Type": "application/json"})

  def put_upload(self, upload, data, description, session=requests):
    if not upload["url"].lower().startswith("https://"):
      raise FrogPilotAPIError(f"{description} upload URL is not https")

    response = session.put(upload["url"], data=data, headers=upload.get("headers"), timeout=60, allow_redirects=False)

    if not 200 <= response.status_code < 300:
      raise FrogPilotAPIError(f"{description} upload failed ({response.status_code})")
