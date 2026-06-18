import dataclasses
import hashlib
import json
import random
import secrets

from datetime import UTC, datetime

import jwt
import requests

from openpilot.common.api import get_key_pair
from openpilot.common.params import Params
from openpilot.common.time_helpers import system_time_valid
from openpilot.system.hardware import HARDWARE
from openpilot.system.version import get_build_metadata

from openpilot.frogpilot.common.frogpilot_variables import FROGPILOT_API

params = Params()


def get_info():
  return {
    "build_metadata": dataclasses.asdict(get_build_metadata()),
    "device_type": HARDWARE.get_device_type(),
    "dongle_id": params.get("FrogPilotDongleId"),
    "os_version": HARDWARE.get_os_version(),
  }


def get_token():
  return params.get("FrogPilotApiToken")


def get_retry_delay(response, default_minimum=60, default_maximum=90, maximum=120):
  retry_after = response.headers.get("Retry-After", "") if response is not None else ""
  if retry_after.isdigit():
    return min(max(int(retry_after), 1), maximum)
  return random.uniform(default_minimum, default_maximum)


def _post(path, session=requests, timeout=10, **kwargs):
  try:
    return session.post(f"{FROGPILOT_API}{path}", timeout=timeout, allow_redirects=False, **kwargs)
  except requests.exceptions.RequestException:
    return None


def signed_post(path, payload, session=requests):
  algorithm, private_key, public_key = get_key_pair()
  if not private_key or not public_key:
    return None

  body = json.dumps({**payload, "public_key": public_key}, separators=(",", ":"), sort_keys=True)
  now = int(datetime.now(UTC).timestamp())
  token = jwt.encode({
    "body_sha256": hashlib.sha256(body.encode()).hexdigest(),
    "exp": now + 15 * 60,
    "iat": now,
  }, private_key, algorithm=algorithm)

  return _post(path, session=session, timeout=20, data=body, headers={"Authorization": f"JWT {token}", "Content-Type": "application/json"})


def regenerate_token(session=requests):
  if not system_time_valid():
    return False

  api_token = secrets.token_urlsafe(32)
  response = signed_post("/v1/token", {
    "api_token_hash": hashlib.sha256(api_token.encode()).hexdigest(),
  }, session=session)
  if response is None or response.status_code != 204:
    return False

  params.put("FrogPilotApiToken", api_token)
  return True


def post(path, headers=None, session=requests, **kwargs):
  api_token = get_token()
  if not api_token:
    if not regenerate_token(session=session):
      return None
    api_token = get_token()

  request_headers = {**(headers or {}), "Authorization": f"Bearer {api_token}"}
  response = _post(path, session=session, headers=request_headers, **kwargs)
  if response is None or response.status_code != 401:
    return response

  if not regenerate_token(session=session):
    return response

  refreshed_headers = {**(headers or {}), "Authorization": f"Bearer {get_token()}"}
  return _post(path, session=session, headers=refreshed_headers, **kwargs)
