"""Install exception handler for process crash."""
import os
import sentry_sdk
import time
import traceback

from datetime import datetime
from enum import Enum
from sentry_sdk.integrations.threading import ThreadingIntegration

from openpilot.common.params import Params, ParamKeyType
from openpilot.system.athena.registration import is_registered_device
from openpilot.system.hardware import HARDWARE, PC
from openpilot.common.swaglog import cloudlog
from openpilot.system.version import get_build_metadata, get_version

from openpilot.selfdrive.frogpilot.controls.lib.frogpilot_functions import is_url_pingable

CRASHES_DIR = "/data/crashes/"

class SentryProject(Enum):
  # python project
  SELFDRIVE = "https://5ad1714d27324c74a30f9c538bff3b8d@o4505034923769856.ingest.sentry.io/4505034930651136"
  # native project
  SELFDRIVE_NATIVE = "https://5ad1714d27324c74a30f9c538bff3b8d@o4505034923769856.ingest.sentry.io/4505034930651136"


def bind_user() -> None:
  sentry_sdk.set_user({"id": HARDWARE.get_serial()})


def report_tombstone(fn: str, message: str, contents: str) -> None:
  FrogPilot = "frogai" in get_build_metadata().openpilot.git_origin.lower()
  if not FrogPilot or PC:
    return

  no_internet = 0
  while True:
    if is_url_pingable("https://sentry.io"):
      cloudlog.error({'tombstone': message})

      with sentry_sdk.configure_scope() as scope:
        bind_user()
        scope.set_extra("tombstone_fn", fn)
        scope.set_extra("tombstone", contents)
        sentry_sdk.capture_message(message=message)
        sentry_sdk.flush()
      break
    elif no_internet > 10:
      break
    else:
      no_internet += 1
      time.sleep(no_internet * 60)


def capture_fingerprint(candidate, params, blocked=False):
  bind_user()

  params_tracking = Params("/persist/tracking")

  param_types = {
    "FrogPilot Controls": ParamKeyType.FROGPILOT_CONTROLS,
    "FrogPilot Vehicles": ParamKeyType.FROGPILOT_VEHICLES,
    "FrogPilot Visuals": ParamKeyType.FROGPILOT_VISUALS,
    "FrogPilot Other": ParamKeyType.FROGPILOT_OTHER,
    "FrogPilot Tracking": ParamKeyType.FROGPILOT_TRACKING
  }

  matched_params = {label: {} for label in param_types}
  for key in params.all_keys():
    for label, key_type in param_types.items():
      if params.get_key_type(key) & key_type:
        if key_type == ParamKeyType.FROGPILOT_TRACKING:
          try:
            value = params_tracking.get_int(key)
          except Exception:
            value = "0"
        else:
          try:
            value = params.get(key)
            if isinstance(value, bytes):
              value = value.decode('utf-8')
            if isinstance(value, str) and value.replace('.', '', 1).isdigit():
              value = float(value) if '.' in value else int(value)
          except Exception:
            value = "0"
        matched_params[label][key.decode('utf-8')] = value

  for label, key_values in matched_params.items():
    if label == "FrogPilot Tracking":
      matched_params[label] = {k: f"{v:,}" for k, v in key_values.items()}
    else:
      matched_params[label] = {k: int(v) if isinstance(v, float) and v.is_integer() else v for k, v in sorted(key_values.items())}

  no_internet = 0
  while True:
    if is_url_pingable("https://sentry.io"):
      with sentry_sdk.configure_scope() as scope:
        scope.fingerprint = [candidate, HARDWARE.get_serial()]
        for label, key_values in matched_params.items():
          scope.set_extra(label, "\n".join([f"{k}: {v}" for k, v in key_values.items()]))

      if blocked:
        sentry_sdk.capture_message("Blocked user from using the development branch", level='error')
      else:
        sentry_sdk.capture_message(f"Fingerprinted {candidate}", level='info')
        params.put_bool_nonblocking("FingerprintLogged", True)

      sentry_sdk.flush()
      break
    elif no_internet > 10:
      break
    else:
      no_internet += 1
      time.sleep(no_internet * 60)


def capture_exception(*args, **kwargs) -> None:
  exc_text = traceback.format_exc()

  phrases_to_check = [
    "To overwrite it, set 'overwrite' to True.",
  ]

  if any(phrase in exc_text for phrase in phrases_to_check):
    return

  save_exception(exc_text)
  cloudlog.error("crash", exc_info=kwargs.get('exc_info', 1))

  FrogPilot = "frogai" in get_build_metadata().openpilot.git_origin.lower()
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
  build_metadata = get_build_metadata()
  if PC:
    return False

  params = Params()
  installed = params.get("InstallDate", encoding='utf-8')
  updated = params.get("Updated", encoding='utf-8')

  short_branch = build_metadata.channel

  if short_branch == "FrogPilot-Development":
    env = "Development"
  elif build_metadata.tested_channel:
    env = "Staging"
  elif build_metadata.release_channel:
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

  build_metadata = get_build_metadata()

  sentry_sdk.set_user({"id": HARDWARE.get_serial()})
  sentry_sdk.set_tag("origin", build_metadata.openpilot.git_origin)
  sentry_sdk.set_tag("branch", short_branch)
  sentry_sdk.set_tag("commit", build_metadata.openpilot.git_commit)
  sentry_sdk.set_tag("updated", updated)
  sentry_sdk.set_tag("installed", installed)

  if project == SentryProject.SELFDRIVE:
    sentry_sdk.Hub.current.start_session()

  return True
