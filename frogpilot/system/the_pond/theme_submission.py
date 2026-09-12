import base64
import hashlib
import shutil
import uuid

from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urljoin, urlparse

import requests

from openpilot.frogpilot.common import frogpilot_api

THEME_SUBMISSION_SCHEMA_VERSION = 1

_API_ATTEMPTS = 3
_MAX_ASSET_BYTES = 32 * 1024 * 1024
_TRANSIENT_STATUSES = frozenset((408, 429, 500, 502, 503, 504))
_TUS_CHUNK_BYTES = 6 * 1024 * 1024


class ThemeSubmissionError(RuntimeError):
  pass


@dataclass(frozen=True)
class ThemeAsset:
  content_type: str
  kind: str
  path: Path
  sha256: str
  size_bytes: int

  @classmethod
  def from_path(cls, kind, content_type, path):
    path = Path(path)
    digest = hashlib.sha256()
    size_bytes = 0
    with path.open("rb") as asset_file:
      while chunk := asset_file.read(1024 * 1024):
        size_bytes += len(chunk)
        digest.update(chunk)
    if not 1 <= size_bytes <= _MAX_ASSET_BYTES:
      raise ThemeSubmissionError(f"Invalid {kind} asset size")
    return cls(content_type, kind, path, digest.hexdigest(), size_bytes)

  @classmethod
  def from_steering_wheel_upload(cls, upload, theme_path):
    suffix = Path(upload.filename).suffix.lower()
    content_types = {
      ".gif": "image/gif",
      ".jpeg": "image/jpeg",
      ".jpg": "image/jpeg",
      ".png": "image/png",
    }
    content_type = content_types.get(suffix)
    if content_type:
      wheel_path = Path(theme_path).parent / f"steering-wheel-upload{suffix}"
      upload.seek(0)
      with wheel_path.open("wb") as destination:
        shutil.copyfileobj(upload.stream, destination)
    else:
      wheel_path = Path(theme_path) / "WheelIcon" / "wheel.png"
      content_type = "image/png"

    if not wheel_path.is_file():
      raise ThemeSubmissionError("Invalid steering wheel asset")
    return cls.from_path("steering_wheel", content_type, wheel_path)

  def verify(self):
    current = ThemeAsset.from_path(self.kind, self.content_type, self.path)
    if current.size_bytes != self.size_bytes or current.sha256 != self.sha256:
      raise ThemeSubmissionError(f"{self.kind} changed during upload")


def _safe_upload_url(value, base_url=None):
  if not isinstance(value, str) or not value:
    raise ThemeSubmissionError("Invalid upload URL")
  url = urljoin(base_url, value) if base_url else value
  parsed = urlparse(url)
  if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password:
    raise ThemeSubmissionError("Invalid upload URL")
  return url


def _validated_headers(value):
  if not isinstance(value, dict) or not all(isinstance(item, str) for pair in value.items() for item in pair):
    raise ThemeSubmissionError("Invalid upload headers")
  return value


def _tus_metadata(value):
  if not isinstance(value, dict) or any(not isinstance(key, str) or not isinstance(item, str) for key, item in value.items()):
    raise ThemeSubmissionError("Invalid resumable upload metadata")
  return ",".join(f"{key} {base64.b64encode(item.encode()).decode()}" for key, item in value.items())


def _tus_offset(upload_url, headers, session):
  try:
    response = session.head(upload_url, headers={**headers, "Tus-Resumable": "1.0.0"},
                            timeout=30, allow_redirects=False)
  except requests.exceptions.RequestException as error:
    raise ThemeSubmissionError("Failed to resume asset upload") from error
  if response.status_code not in (200, 204):
    raise ThemeSubmissionError(f"Failed to resume asset upload ({response.status_code})")
  try:
    return int(response.headers["Upload-Offset"])
  except (KeyError, TypeError, ValueError) as error:
    raise ThemeSubmissionError("Invalid resumable upload offset") from error


def _tus_upload(asset, upload, session):
  create_url = _safe_upload_url(upload.get("url"))
  headers = _validated_headers(upload.get("headers"))
  chunk_size = upload.get("chunk_size_bytes")
  if chunk_size != _TUS_CHUNK_BYTES:
    raise ThemeSubmissionError("Unsupported resumable upload chunk size")
  create_headers = {
    **headers,
    "Tus-Resumable": "1.0.0",
    "Upload-Length": str(asset.size_bytes),
    "Upload-Metadata": _tus_metadata(upload.get("metadata")),
    "x-upsert": "false",
  }
  try:
    response = session.post(create_url, headers=create_headers, timeout=30, allow_redirects=False)
  except requests.exceptions.RequestException as error:
    raise ThemeSubmissionError(f"Failed to start {asset.kind} upload") from error
  if response.status_code != 201:
    raise ThemeSubmissionError(f"Failed to start {asset.kind} upload ({response.status_code})")
  upload_url = _safe_upload_url(response.headers.get("Location"), create_url)
  if urlparse(upload_url).hostname != urlparse(create_url).hostname:
    raise ThemeSubmissionError("Invalid resumable upload location")

  offset = 0
  transient_failures = 0
  with asset.path.open("rb") as asset_file:
    while offset < asset.size_bytes:
      asset_file.seek(offset)
      chunk = asset_file.read(min(chunk_size, asset.size_bytes - offset))
      patch_headers = {
        **headers,
        "Content-Type": "application/offset+octet-stream",
        "Tus-Resumable": "1.0.0",
        "Upload-Offset": str(offset),
      }
      try:
        patch = session.patch(upload_url, data=chunk, headers=patch_headers,
                              timeout=60, allow_redirects=False)
      except requests.exceptions.RequestException as error:
        transient_failures += 1
        if transient_failures >= _API_ATTEMPTS:
          raise ThemeSubmissionError(f"Failed to upload {asset.kind}") from error
        offset = _tus_offset(upload_url, headers, session)
        continue
      if patch.status_code != 204:
        if patch.status_code in _TRANSIENT_STATUSES:
          transient_failures += 1
          if transient_failures >= _API_ATTEMPTS:
            raise ThemeSubmissionError(f"Failed to upload {asset.kind} ({patch.status_code})")
          offset = _tus_offset(upload_url, headers, session)
          continue
        raise ThemeSubmissionError(f"Failed to upload {asset.kind} ({patch.status_code})")
      try:
        next_offset = int(patch.headers["Upload-Offset"])
      except (KeyError, TypeError, ValueError) as error:
        raise ThemeSubmissionError("Invalid resumable upload offset") from error
      if next_offset != offset + len(chunk):
        raise ThemeSubmissionError("Unexpected resumable upload offset")
      offset = next_offset
      transient_failures = 0


def _upload_asset(asset, upload, session):
  protocol = upload.get("protocol") if isinstance(upload, dict) else None
  if protocol == "signed-put":
    with asset.path.open("rb") as asset_file:
      frogpilot_api.put_upload(upload, asset_file, asset.kind, session)
  elif protocol == "tus":
    _tus_upload(asset, upload, session)
  else:
    raise ThemeSubmissionError("Unsupported asset upload protocol")


def submit_theme_assets(theme_name, discord_username, assets, session=requests):
  assets = list(assets)
  if not assets or len({asset.kind for asset in assets}) != len(assets):
    raise ThemeSubmissionError("No valid theme assets")
  if sum(asset.size_bytes for asset in assets) > _MAX_ASSET_BYTES:
    raise ThemeSubmissionError("Theme submission exceeds the 32MB limit")

  manifest = [{
    "content_type": asset.content_type,
    "kind": asset.kind,
    "sha256": asset.sha256,
    "size_bytes": asset.size_bytes,
  } for asset in assets]
  client_submission_id = str(uuid.uuid5(uuid.NAMESPACE_URL, f"{theme_name}\n{discord_username}\n{manifest}"))
  result = frogpilot_api.post_json("/v1/theme-submissions", {
    "assets": manifest,
    "client_submission_id": client_submission_id,
    "discord_username": discord_username,
    "schema_version": THEME_SUBMISSION_SCHEMA_VERSION,
    "theme_name": theme_name,
  }, session)
  submission_id = result.get("submission_id")
  upload_assets = result.get("assets")
  if not isinstance(submission_id, str) or not isinstance(upload_assets, list):
    raise ThemeSubmissionError("Invalid theme submission response")

  assets_by_kind = {asset.kind: asset for asset in assets}
  asset_ids = {}
  for remote_asset in upload_assets:
    if not isinstance(remote_asset, dict):
      raise ThemeSubmissionError("Invalid theme asset response")
    kind = remote_asset.get("kind")
    asset_id = remote_asset.get("asset_id")
    if kind not in assets_by_kind or not isinstance(asset_id, str) or kind in asset_ids:
      raise ThemeSubmissionError("Invalid theme asset response")
    asset_ids[kind] = asset_id
    upload = remote_asset.get("upload")
    if upload is not None:
      _upload_asset(assets_by_kind[kind], upload, session)

  if set(asset_ids) != set(assets_by_kind):
    raise ThemeSubmissionError("Incomplete theme asset response")
  for asset in assets:
    asset.verify()

  completion = frogpilot_api.post_json(f"/v1/theme-submissions/{submission_id}/complete", {
    "assets": [{
      "asset_id": asset_ids[asset.kind],
      "sha256": asset.sha256,
      "size_bytes": asset.size_bytes,
    } for asset in assets],
    "schema_version": THEME_SUBMISSION_SCHEMA_VERSION,
  }, session, timeout=120)
  if completion.get("accepted") is not True or completion.get("submission_id") != submission_id:
    raise ThemeSubmissionError("Theme submission was not accepted")
  return submission_id
