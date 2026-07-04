"""Install exception handler for process crash."""
import sentry_sdk
import traceback
from datetime import datetime
from enum import Enum
from sentry_sdk.integrations.threading import ThreadingIntegration

from openpilot.common.params import Params
from openpilot.system.hardware import HARDWARE, PC
from openpilot.common.swaglog import cloudlog
from openpilot.system.version import get_build_metadata, get_version

from openpilot.frogpilot.common.frogpilot_variables import ERROR_LOGS_PATH, params

class SentryProject(Enum):
  # python project
  SELFDRIVE = "https://7ba43fba4cfcf1a6c0eff83d40374e43@o4505034923769856.ingest.us.sentry.io/4505034930651136"
  # native project
  SELFDRIVE_NATIVE = "https://7ba43fba4cfcf1a6c0eff83d40374e43@o4505034923769856.ingest.us.sentry.io/4505034930651136"


def report_tombstone(fn: str, message: str, contents: str) -> None:
  cloudlog.error({'tombstone': message})

  with sentry_sdk.configure_scope() as scope:
    scope.set_extra("tombstone_fn", fn)
    scope.set_extra("tombstone", contents)
    sentry_sdk.capture_message(message=message)
    sentry_sdk.flush()


def capture_block():
  with sentry_sdk.push_scope() as scope:
    sentry_sdk.capture_message("Blocked user from using the development branch", level='info')
    sentry_sdk.flush()


def capture_exception(*args, crash_log=True, **kwargs) -> None:
  exc_text = traceback.format_exc()

  phrases_to_check = [
    "already exists. To overwrite it, set 'overwrite' to True",
    "failed after retry",
  ]

  if any(phrase in exc_text for phrase in phrases_to_check):
    return

  save_exception(exc_text, crash_log)
  cloudlog.error("crash", exc_info=kwargs.get('exc_info', 1))

  try:
    sentry_sdk.capture_exception(*args, **kwargs)
    sentry_sdk.flush()  # https://github.com/getsentry/sentry-python/issues/291
  except Exception:
    cloudlog.exception("sentry exception")


def set_tag(key: str, value: str) -> None:
  sentry_sdk.set_tag(key, value)


def save_exception(exc_text: str, crash_log) -> None:
  files = [
    ERROR_LOGS_PATH / datetime.now().astimezone().strftime("%Y-%m-%d--%H-%M-%S.log"),
    ERROR_LOGS_PATH / "error.txt"
  ]

  for file_path in files:
    if file_path.name == "error.txt" and crash_log:
      lines = exc_text.splitlines()[-10:]
      file_path.write_text("\n".join(lines))
    else:
      file_path.write_text(exc_text)


def init(project: SentryProject) -> bool:
  build_metadata = get_build_metadata()
  # forks like to mess with this, so double check
  frogpilot_remote = "frogai" in build_metadata.openpilot.git_origin.lower()
  if not frogpilot_remote or build_metadata.openpilot.is_dirty or PC:
    return False

  short_branch = build_metadata.channel

  if short_branch in ["COMMA", "HEAD"]:
    return False
  elif short_branch == "FrogPilot-Development":
    env = "Development"
  elif build_metadata.release_channel:
    env = "Release"
  elif short_branch == "FrogPilot-Testing":
    env = "Testing"
  elif short_branch == "FrogPilot-Staging":
    env = "Staging"
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

  params = Params()

  sentry_sdk.set_user({"id": params.get("DongleId", encoding="utf-8")})
  sentry_sdk.set_tag("origin", build_metadata.openpilot.git_origin)
  sentry_sdk.set_tag("branch", short_branch)
  sentry_sdk.set_tag("commit", build_metadata.openpilot.git_commit)
  sentry_sdk.set_tag("updated", params.get("Updated", encoding="utf-8"))
  sentry_sdk.set_tag("installed", params.get("InstallDate", encoding="utf-8"))

  if project == SentryProject.SELFDRIVE:
    sentry_sdk.Hub.current.start_session()

  return True
