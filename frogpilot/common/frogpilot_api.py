import hashlib
import json
import jwt
import requests
import secrets
import time

from urllib.parse import urlparse

from openpilot.common.api import get_key_pair
from openpilot.common.time import system_time_valid

from openpilot.frogpilot.common.frogpilot_variables import params

DEVICE_PROFILE_SCHEMA_VERSION = 1

FROGPILOT_API = "https://api.frogpilot.com"


class FrogPilotAPIError(RuntimeError):
  pass


def get_token():
  return params.get("FrogPilotApiToken", encoding="utf-8")


def regenerate_token():
  if not system_time_valid():
    return False

  api_token = secrets.token_urlsafe(32)
  response = signed_post("/v1/token", {"api_token_hash": hashlib.sha256(api_token.encode()).hexdigest()})

  if response is None or not 200 <= response.status_code < 300:
    params.remove("FrogPilotRegistration")
    return False

  params.put("FrogPilotApiToken", api_token)
  return True


def _post(path, session=requests, timeout=10, **kwargs):
  try:
    return session.post(f"{FROGPILOT_API}{path}", timeout=timeout, allow_redirects=False, **kwargs)
  except requests.exceptions.RequestException:
    return None


def post(path, headers=None, **kwargs):
  def send(token):
    return _post(path, headers={**(headers or {}), "Authorization": f"Bearer {token}"}, **kwargs)

  token = get_token()
  if token:
    response = send(token)

    if response is None or response.status_code != 401:
      return response

  return send(get_token()) if regenerate_token() else None


def post_json(path, payload, session=requests, timeout=30):
  response = post(path, json=payload, timeout=timeout, session=session)
  status = getattr(response, "status_code", 0)

  if not 200 <= status < 300:
    raise FrogPilotAPIError(f"POST {path} failed ({status or 'no response'})")

  return response.json()


def _signed_body(payload, public_key):
  return json.dumps({**payload, "public_key": public_key}, separators=(",", ":"), sort_keys=True)


def body_digest(payload):
  return hashlib.sha256(_signed_body(payload, get_key_pair()[2]).encode()).hexdigest()


def signed_post(path, payload):
  algorithm, private_key, public_key = get_key_pair()
  if not private_key:
    return None

  body = _signed_body(payload, public_key)
  now = int(time.time())
  token = jwt.encode({
    "aud": "api.frogpilot.com",
    "auth_version": 2,
    "body_sha256": hashlib.sha256(body.encode()).hexdigest(),
    "exp": now + 15 * 60,
    "iat": now,
    "method": "POST",
    "path": path,
  }, private_key, algorithm=algorithm)

  return _post(path, timeout=20, data=body, headers={"Authorization": f"JWT {token}", "Content-Type": "application/json"})


def put_upload(upload, data, description, session=requests):
  url = upload.get("url", "")
  parsed = urlparse(url) if isinstance(url, str) else None

  if parsed is None or parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password:
    raise FrogPilotAPIError(f"{description} upload URL is not https")

  response = session.put(url, data=data, headers=upload.get("headers"), timeout=60, allow_redirects=False)

  if not 200 <= response.status_code < 300:
    raise FrogPilotAPIError(f"{description} upload failed ({response.status_code})")
