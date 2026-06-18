"""Install exception handler for process crash."""
import json
import sentry_sdk
import traceback
from datetime import datetime
from enum import Enum
from sentry_sdk.integrations.threading import ThreadingIntegration
from sentry_sdk.transport import HttpTransport

from openpilot.common.params import Params
from openpilot.system.athena.registration import is_registered_device
from openpilot.system.hardware import HARDWARE, PC
from openpilot.common.swaglog import cloudlog
from openpilot.system.version import get_build_metadata, get_version

from openpilot.frogpilot.common.frogpilot_variables import ERROR_LOGS_PATH, SENTRY_QUEUE_PATH

MAX_QUEUED_TOMBSTONES = 50
SENTRY_FLUSH_TIMEOUT = 10.0


class SentryProject(Enum):
  # python project
  SELFDRIVE = "https://7ba43fba4cfcf1a6c0eff83d40374e43@o4505034923769856.ingest.us.sentry.io/4505034930651136"
  # native project
  SELFDRIVE_NATIVE = "https://7ba43fba4cfcf1a6c0eff83d40374e43@o4505034923769856.ingest.us.sentry.io/4505034930651136"


class DeliveryTrackingTransport(HttpTransport):
  def __init__(self, options):
    super().__init__(options)
    self.delivered = True

  def on_dropped_event(self, reason) -> None:
    self.delivered = False


def deliver_tombstone(fn: str, message: str, contents: str) -> bool:
  transport = getattr(sentry_sdk.get_client(), "transport", None)
  if not isinstance(transport, DeliveryTrackingTransport):
    return False

  transport.delivered = True

  with sentry_sdk.configure_scope() as scope:
    scope.set_extra("tombstone_fn", fn)
    scope.set_extra("tombstone", contents)
    sentry_sdk.capture_message(message=message)
    sentry_sdk.flush(timeout=SENTRY_FLUSH_TIMEOUT)

  return transport.delivered


def queue_tombstone(fn: str, message: str, contents: str) -> None:
  try:
    SENTRY_QUEUE_PATH.mkdir(parents=True, exist_ok=True)
    queued = sorted(SENTRY_QUEUE_PATH.glob("*.json"))
    for expired in queued[:max(0, len(queued) + 1 - MAX_QUEUED_TOMBSTONES)]:
      expired.unlink(missing_ok=True)

    name = datetime.now().strftime("%Y-%m-%d--%H-%M-%S-%f") + ".json"
    (SENTRY_QUEUE_PATH / name).write_text(json.dumps({"fn": fn, "message": message, "contents": contents}))
    cloudlog.warning(f"queued tombstone for a later connection: {name}")
  except OSError:
    cloudlog.exception("sentry.queue_tombstone")


def flush_queued_tombstones() -> None:
  for path in sorted(SENTRY_QUEUE_PATH.glob("*.json")) if SENTRY_QUEUE_PATH.is_dir() else []:
    try:
      report = json.loads(path.read_text())
    except (OSError, ValueError):
      path.unlink(missing_ok=True)
      continue

    if not deliver_tombstone(report["fn"], report["message"], report["contents"]):
      return

    path.unlink(missing_ok=True)
    cloudlog.warning(f"delivered queued tombstone: {path.name}")


def report_tombstone(fn: str, message: str, contents: str) -> None:
  cloudlog.error({'tombstone': message})

  if not deliver_tombstone(fn, message, contents):
    queue_tombstone(fn, message, contents)


def capture_exception(*args, crash_log=True, **kwargs) -> None:
  exc_text = traceback.format_exc()

  errors_to_ignore = [
  ]

  if any(error in exc_text for error in errors_to_ignore):
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
    if file_path.name == "error.txt":
      if not crash_log:
        continue
      lines = exc_text.splitlines()[-10:]
      file_path.write_text("\n".join(lines))
    else:
      file_path.write_text(exc_text)


def init(project: SentryProject) -> bool:
  build_metadata = get_build_metadata()
  # forks like to mess with this, so double check
  FrogPilot = "frogai" in build_metadata.openpilot.git_origin.lower()
  if not FrogPilot or build_metadata.openpilot.is_dirty or PC:
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
                  transport=DeliveryTrackingTransport,
                  environment=env)

  params = Params()

  sentry_sdk.set_user({"id": params.get("DongleId")})
  sentry_sdk.set_tag("origin", build_metadata.openpilot.git_origin)
  sentry_sdk.set_tag("branch", short_branch)
  sentry_sdk.set_tag("commit", build_metadata.openpilot.git_commit)
  sentry_sdk.set_tag("updated", params.get("Updated"))
  sentry_sdk.set_tag("installed", params.get("InstallDate"))

  return True
