import ipaddress
import re
import socket
import struct

from datetime import datetime
from pathlib import Path
from urllib.parse import urlsplit

PARAM_GET_ALLOWLIST = frozenset({
  "DiscordUsername", "DownloadableColors", "DownloadableDistanceIcons", "DownloadableIcons",
  "DownloadableSignals", "DownloadableSounds", "DownloadableWheels", "IsMetric",
})
PARAM_MEMORY_GET_ALLOWLIST = frozenset({"ThemeDownloadProgress"})

ONROAD_ALLOWED = frozenset()

ONROAD_BLOCKED_READS = frozenset({
  "/api/tmux_log/live",
})

def is_onroad_blocked(method: str, path: str) -> bool:
  if method in ("GET", "HEAD", "OPTIONS"):
    return path in ONROAD_BLOCKED_READS
  return path not in ONROAD_ALLOWED

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

def theme_asset_slug(display_name: str) -> str:
  return display_name.lower().replace("(", "").replace(")", "").replace(" ", "_").replace("_animated", "-animated")

_SLUG_RE = re.compile(r"[a-z0-9_'~.-]+")

def is_safe_slug(value) -> bool:
  return isinstance(value, str) and _SLUG_RE.fullmatch(value) is not None and ".." not in value

def route_segment_matches(segment: str, route_name: str) -> bool:
  return segment == route_name or segment.startswith(route_name + "--")

def is_within(base, target) -> bool:
  base_r = Path(base).resolve()
  target_r = Path(target).resolve()
  return base_r == target_r or base_r in target_r.parents

def _http_origin(value: str | None) -> tuple[str, str, int] | None:
  if not value:
    return None
  try:
    parsed = urlsplit(value)
    if parsed.scheme not in ("http", "https") or not parsed.netloc or parsed.username is not None or parsed.password is not None:
      return None
    if parsed.path or parsed.query or parsed.fragment:
      return None
    port = parsed.port or (80 if parsed.scheme == "http" else 443)
  except ValueError:
    return None
  host = parsed.hostname
  return (parsed.scheme, host.lower().rstrip("."), port) if host else None

def origin_allowed(origin: str | None, request_scheme: str, request_host: str, trusted_ports: set[int]) -> bool:
  expected = _http_origin(f"{request_scheme}://{request_host}")
  source = _http_origin(origin)
  if source is None or expected is None or source != expected:
    return False
  return expected[2] in trusted_ports

LOCAL_SUFFIXES = (".local", ".lan", ".home", ".home.arpa", ".internal", ".localdomain", ".fritz.box", ".ts.net")

def host_allowed(request_scheme: str, request_host: str, trusted_hosts: set[str]) -> bool:
  parsed = _http_origin(f"{request_scheme}://{request_host}")
  if parsed is None:
    return False
  host = parsed[1]
  if host in trusted_hosts or host.endswith(LOCAL_SUFFIXES) or "." not in host:
    return True
  try:
    ipaddress.ip_address(host)
    return True
  except ValueError:
    return False

def referer_allowed(referer: str | None, request_scheme: str, request_host: str, trusted_ports: set[int]) -> bool:
  try:
    parsed = urlsplit(referer or "")
  except ValueError:
    return False
  return origin_allowed(f"{parsed.scheme}://{parsed.netloc}", request_scheme, request_host, trusted_ports)

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
