"""Install exception handler for process crash."""
import os
import sentry_sdk
import socket
import time
import traceback
import urllib.request
import urllib.error

from datetime import datetime
from enum import Enum
from sentry_sdk.integrations.threading import ThreadingIntegration

from openpilot.common.params import Params, ParamKeyType
from openpilot.system.hardware import HARDWARE, PC
from openpilot.common.swaglog import cloudlog
from openpilot.system.version import get_commit, get_short_branch, get_origin, get_version

CRASHES_DIR = "/data/community/crashes/"

class SentryProject(Enum):
  # python project
  SELFDRIVE = "https://5ad1714d27324c74a30f9c538bff3b8d@o4505034923769856.ingest.sentry.io/4505034930651136"
  # native project
  SELFDRIVE_NATIVE = "https://5ad1714d27324c74a30f9c538bff3b8d@o4505034923769856.ingest.sentry.io/4505034930651136"


def sentry_pinged(url="https://sentry.io", timeout=5):
  try:
    urllib.request.urlopen(url, timeout=timeout)
    return True
  except (urllib.error.URLError, socket.timeout):
    return False


def bind_user() -> None:
  sentry_sdk.set_user({"id": HARDWARE.get_serial()})


def report_tombstone(fn: str, message: str, contents: str) -> None:
  FrogPilot = "frogai" in get_origin().lower()
  if not FrogPilot or PC:
    return

  no_internet = 0
  while True:
    if sentry_pinged():
      cloudlog.error({'tombstone': message})

      with sentry_sdk.configure_scope() as scope:
        bind_user()
        scope.set_extra("tombstone_fn", fn)
        scope.set_extra("tombstone", contents)
        sentry_sdk.capture_message(message=message)
        sentry_sdk.flush()
      break
    else:
      if no_internet > 5:
        break
      no_internet += 1
      time.sleep(600)


def chunk_data(data):
  return [data[i:i+1] for i in range(len(data))]


def format_params(params):
  formatted_params = []
  for k, v in sorted(params.items()):
    if isinstance(v, bytes):
      param_value = format(float(v), '.12g') if v.replace(b'.', b'').isdigit() else v.decode()
    elif isinstance(v, float):
      param_value = format(v, '.12g')
    else:
      param_value = v
    formatted_params.append(f"{k}: {param_value}")
  return formatted_params


def get_frogpilot_params_by_type(param_type, params):
  keys = [
    key.decode('utf-8') if isinstance(key, bytes) else key
    for key in params.all_keys()
    if params.get_key_type(key) & param_type
  ]

  return {
    key: (params.get(key).decode('utf-8') if isinstance(params.get(key), bytes) else params.get(key) or '0')
    for key in keys
  }


def set_sentry_scope(scope, chunks, label):
  scope.set_extra(label, '\n'.join('\n'.join(chunk) for chunk in chunks))


def capture_fingerprint(candidate, params, blocked=False):
  bind_user()

  control_params = get_frogpilot_params_by_type(ParamKeyType.FROGPILOT_CONTROLS, params)
  vehicle_params = get_frogpilot_params_by_type(ParamKeyType.FROGPILOT_VEHICLES, params)
  visual_params = get_frogpilot_params_by_type(ParamKeyType.FROGPILOT_VISUALS, params)
  other_params = get_frogpilot_params_by_type(ParamKeyType.FROGPILOT_OTHER, params)
  tracking_params = get_frogpilot_params_by_type(ParamKeyType.FROGPILOT_TRACKING, params)

  control_values = format_params(control_params)
  vehicle_values = format_params(vehicle_params)
  visual_values = format_params(visual_params)
  other_values = format_params(other_params)
  tracking_values = format_params(tracking_params)

  control_chunks = chunk_data(control_values)
  vehicle_chunks = chunk_data(vehicle_values)
  visual_chunks = chunk_data(visual_values)
  other_chunks = chunk_data(other_values)
  tracking_chunks = chunk_data(tracking_values)

  chunks_labels = [
    (control_chunks, "FrogPilot Controls"),
    (vehicle_chunks, "FrogPilot Vehicles"),
    (visual_chunks, "FrogPilot Visuals"),
    (other_chunks, "FrogPilot Other"),
    (tracking_chunks, "FrogPilot Tracking")
  ]

  no_internet = 0
  while True:
    if sentry_pinged():
      for chunks, label in chunks_labels:
        with sentry_sdk.configure_scope() as scope:
          set_sentry_scope(scope, chunks, label)
          scope.fingerprint = [candidate, HARDWARE.get_serial()]

      if blocked:
        sentry_sdk.capture_message("Blocked user from using the development branch", level='error')
      else:
        sentry_sdk.capture_message(f"Fingerprinted {candidate}", level='info')
        params.put_bool("FingerprintLogged", True)

      sentry_sdk.flush()
      break
    else:
      if no_internet > 5:
        break
      no_internet += 1
      time.sleep(600)


def capture_exception(*args, **kwargs) -> None:
  exc_text = traceback.format_exc()

  phrases_to_check = [
    "To overwrite it, set 'overwrite' to True.",
    "device reports readiness to read but returned no data",
  ]

  if any(phrase in exc_text for phrase in phrases_to_check):
    return

  save_exception(exc_text)
  cloudlog.error("crash", exc_info=kwargs.get('exc_info', 1))

  FrogPilot = "frogai" in get_origin().lower()
  if not FrogPilot or PC:
    return

  try:
    bind_user()
    sentry_sdk.capture_exception(*args, **kwargs)
    sentry_sdk.flush()  # https://github.com/getsentry/sentry-python/issues/291
  except Exception:
    cloudlog.exception("sentry exception")


def save_exception(exc_text: str) -> None:
  if not os.path.exists(CRASHES_DIR):
    os.makedirs(CRASHES_DIR)

  files = [
    os.path.join(CRASHES_DIR, datetime.now().strftime('%Y-%m-%d--%H-%M-%S.log')),
    os.path.join(CRASHES_DIR, 'error.txt')
  ]

  for file in files:
    with open(file, 'w') as f:
      if file.endswith("error.txt"):
        lines = exc_text.splitlines()[-10:]
        f.write("\n".join(lines))
      else:
        f.write(exc_text)

  print('Logged current crash to {}'.format(files))


def set_tag(key: str, value: str) -> None:
  sentry_sdk.set_tag(key, value)


def init(project: SentryProject) -> bool:
  params = Params()
  installed = params.get("InstallDate", encoding='utf-8')
  updated = params.get("Updated", encoding='utf-8')

  short_branch = get_short_branch()

  if short_branch == "FrogPilot-Development":
    env = "Development"
  elif short_branch in {"FrogPilot-Staging", "FrogPilot-Testing"}:
    env = "Staging"
  elif short_branch == "FrogPilot":
    env = "Release"
  else:
    env = short_branch

  integrations = []
  if project == SentryProject.SELFDRIVE:
    integrations.append(ThreadingIntegration(propagate_hub=True))

  sentry_sdk.init(project.value,
                  default_integrations=False,
                  release=get_version(),
                  integrations=integrations,
                  traces_sample_rate=1.0,
                  max_value_length=8192,
                  environment=env)

  sentry_sdk.set_user({"id": HARDWARE.get_serial()})
  sentry_sdk.set_tag("branch", short_branch)
  sentry_sdk.set_tag("commit", get_commit())
  sentry_sdk.set_tag("updated", updated)
  sentry_sdk.set_tag("installed", installed)
  sentry_sdk.set_tag("repo", get_origin())

  if project == SentryProject.SELFDRIVE:
    sentry_sdk.Hub.current.start_session()

  return True
