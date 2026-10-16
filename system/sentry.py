"""Install exception handler for process crash."""
import json
import sentry_sdk
import traceback
from datetime import UTC, datetime
from enum import Enum
from sentry_sdk.integrations.threading import ThreadingIntegration
from sentry_sdk.transport import HttpTransport

from openpilot.common.params import Params
from openpilot.system.athena.registration import is_registered_device
from openpilot.system.hardware import HARDWARE, PC
from openpilot.common.swaglog import cloudlog
from openpilot.system.version import get_build_metadata, get_version

from openpilot.frogpilot.common import frogpilot_variables

# FrogPilot variables
MAX_QUEUED_TOMBSTONES = 50
SENTRY_FLUSH_TIMEOUT = 10.0


class SentryProject(Enum):
  # python project
  SELFDRIVE = "https://7ba43fba4cfcf1a6c0eff83d40374e43@o4505034923769856.ingest.us.sentry.io/4505034930651136"
  # native project
  SELFDRIVE_NATIVE = "https://7ba43fba4cfcf1a6c0eff83d40374e43@o4505034923769856.ingest.us.sentry.io/4505034930651136"


# FrogPilot variables
class DeliveryTrackingTransport(HttpTransport):
  def on_dropped_event(self, reason) -> None:
    self.delivered = False


def deliver_tombstone(fn: str, message: str, contents: str) -> bool:
  transport = sentry_sdk.get_client().transport
  transport.delivered = True

  with sentry_sdk.configure_scope() as scope:
    scope.set_extra("tombstone_fn", fn)
    scope.set_extra("tombstone", contents)
    sentry_sdk.capture_message(message=message)
    sentry_sdk.flush(timeout=SENTRY_FLUSH_TIMEOUT)

  return transport._worker._timed_queue_join(0) and transport.delivered


def queue_tombstone(fn: str, message: str, contents: str) -> None:
  try:
    frogpilot_variables.SENTRY_QUEUE_PATH.mkdir(parents=True, exist_ok=True)
    queued = sorted(frogpilot_variables.SENTRY_QUEUE_PATH.glob("*.json"))
    expired_count = max(0, len(queued) + 1 - MAX_QUEUED_TOMBSTONES)

    for expired in queued[:expired_count]:
      expired.unlink(missing_ok=True)

    name = datetime.now().strftime("%Y-%m-%d--%H-%M-%S-%f") + ".json"
    report = {"contents": contents, "fn": fn, "message": message}

    (frogpilot_variables.SENTRY_QUEUE_PATH / name).write_text(json.dumps(report))
    cloudlog.warning(f"queued tombstone for a later connection: {name}")
  except OSError:
    cloudlog.exception("sentry.queue_tombstone")


def flush_queued_tombstones() -> None:
  if not frogpilot_variables.SENTRY_QUEUE_PATH.is_dir():
    return

  for path in sorted(frogpilot_variables.SENTRY_QUEUE_PATH.glob("*.json")):
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

  # FrogPilot variables
  if not deliver_tombstone(fn, message, contents):
    queue_tombstone(fn, message, contents)


def capture_exception(*args, crash_log=True, **kwargs) -> None:
  # FrogPilot variables
  exc_text = traceback.format_exc()

  save_exception(exc_text, crash_log)

  cloudlog.error("crash", exc_info=kwargs.get('exc_info', 1))

  try:
    sentry_sdk.capture_exception(*args, **kwargs)
    sentry_sdk.flush()  # https://github.com/getsentry/sentry-python/issues/291
  except Exception:
    cloudlog.exception("sentry exception")


# FrogPilot variables
def capture_message(message: str, **kwargs) -> None:
  sentry_sdk.capture_message(message, **kwargs)


def set_tag(key: str, value: str) -> None:
  sentry_sdk.set_tag(key, value)


# FrogPilot variables
def save_exception(exc_text: str, crash_log) -> None:
  log_name = datetime.now().astimezone().strftime("%Y-%m-%d--%H-%M-%S.log")
  (frogpilot_variables.ERROR_LOGS_PATH / log_name).write_text(exc_text)

  if crash_log:
    frogpilot_variables.ERROR_LOG_PATH.write_text("\n".join(exc_text.splitlines()[-10:]))


def init(project: SentryProject) -> bool:
  build_metadata = get_build_metadata()
  # forks like to mess with this, so double check
  frogpilot_remote = "frogai" in build_metadata.openpilot.git_origin.lower()
  if not frogpilot_remote or build_metadata.openpilot.is_dirty or PC:
    return False

  # FrogPilot variables
  short_branch = build_metadata.channel

  if short_branch == "HEAD":
    return False

  env = "Release" if build_metadata.release_channel else short_branch.removeprefix("FrogPilot-")

  integrations = []
  if project == SentryProject.SELFDRIVE:
    integrations.append(ThreadingIntegration(propagate_hub=True))

  sentry_sdk.init(project.value,
                  default_integrations=False,
                  include_local_variables=False,
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
  sentry_sdk.set_tag("device", HARDWARE.get_device_type())

  # FrogPilot variables
  sentry_sdk.set_tag("updated", params.get("Updated"))

  install_date = params.get("InstallDate")

  if install_date is not None:
    install_date = install_date.replace(tzinfo=UTC)

  sentry_sdk.set_tag("installed", install_date)

  return True
