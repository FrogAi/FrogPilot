import re
import socket
import struct

from datetime import datetime
from urllib.parse import urlsplit
from pathlib import Path

PARAM_GET_ALLOWLIST = frozenset({
  "IsMetric", "DiscordUsername",
  "DownloadableColors", "DownloadableDistanceIcons", "DownloadableIcons",
  "DownloadableSignals", "DownloadableSounds", "DownloadableWheels",
})
PARAM_MEMORY_GET_ALLOWLIST = frozenset({"ThemeDownloadProgress"})

ONROAD_BLOCKED = frozenset({
  ("POST", "/api/toggles/reset_default"),
  ("POST", "/api/toggles/reset_stock"),
  ("POST", "/api/toggles/restore"),
  ("POST", "/api/themes/apply"),
  ("POST", "/api/tailscale/setup"),
  ("POST", "/api/tailscale/uninstall"),
  ("DELETE", "/api/routes/delete_all"),
  ("DELETE", "/api/screen_recordings/delete_all"),
  ("DELETE", "/api/error_logs/delete_all"),
  ("DELETE", "/api/tmux_log/delete_all"),
})

def is_onroad_blocked(method: str, path: str) -> bool:
  return (method, path) in ONROAD_BLOCKED

_SECOC_RE = re.compile(r"^[0-9a-fA-F]{32}$")

def is_valid_secoc_key(value) -> bool:
  return isinstance(value, str) and bool(_SECOC_RE.fullmatch(value))

_DISPLAY_NAME_MAX = 256
_UNSAFE_NAME_RE = re.compile(r"[<>\x00-\x1f\x7f]")

def is_safe_display_name(name) -> bool:
  if not isinstance(name, str):
    return False
  if len(name) > _DISPLAY_NAME_MAX:
    return False
  return _UNSAFE_NAME_RE.search(name) is None

def route_segment_matches(segment: str, route_name: str) -> bool:
  return segment == route_name or segment.startswith(route_name + "--")

def is_within(base, target) -> bool:
  base_r = Path(base).resolve()
  target_r = Path(target).resolve()
  return base_r == target_r or base_r in target_r.parents

def _bare_host(value: str | None) -> str | None:
  if not value:
    return None
  host = urlsplit(value).hostname if "//" in value else value.split(":", 1)[0]
  return host.lower() if host else None

def origin_allowed(origin: str | None, allowed_hosts: set[str]) -> bool:
  bare = _bare_host(origin)
  return bare is not None and bare in allowed_hosts

def format_ordinal_date(dt: datetime) -> str:
  day = dt.day
  suffix = "th" if 11 <= day <= 13 else {1: "st", 2: "nd", 3: "rd"}.get(day % 10, "th")
  return dt.strftime(f"%B {day}{suffix}, %Y")

MDNS_HOSTNAME = "ThePond.local"

def encode_dns_name(name: str) -> bytes:
  out = bytearray()
  for label in name.split("."):
    if label:
      out.append(len(label))
      out += label.encode("ascii")
  out.append(0)
  return bytes(out)

def is_mdns_query_for(data: bytes, name: str = MDNS_HOSTNAME) -> bool:
  return len(data) >= 12 and not (data[2] & 0x80) and encode_dns_name(name).lower() in data.lower()

def is_mdns_response_for(data: bytes, name: str = MDNS_HOSTNAME) -> bool:
  return len(data) >= 12 and bool(data[2] & 0x80) and encode_dns_name(name).lower() in data.lower()

def build_mdns_a_response(ip: str, name: str = MDNS_HOSTNAME) -> bytes:
  header = struct.pack(">HHHHHH", 0, 0x8400, 0, 1, 0, 0)
  record = struct.pack(">HHIH", 1, 0x8001, 120, 4)
  return header + encode_dns_name(name) + record + socket.inet_aton(ip)
