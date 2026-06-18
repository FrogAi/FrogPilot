import importlib
import os
import signal
import struct
import time
import subprocess
from collections.abc import Callable, ValuesView
from abc import ABC, abstractmethod
from multiprocessing import Process
from types import SimpleNamespace

from setproctitle import setproctitle

from cereal import car, log
import cereal.messaging as messaging
import openpilot.system.sentry as sentry
from openpilot.common.basedir import BASEDIR
from openpilot.common.params import Params
from openpilot.common.swaglog import cloudlog
from openpilot.common.watchdog import WATCHDOG_FN

ENABLE_WATCHDOG = os.getenv("NO_WATCHDOG") is None

WATCHDOG_COREDUMP_GRACE_S = 120.0
WATCHDOG_HANG_DUMP_TIMEOUT_S = 15.0


def coredump_in_progress(pid: int) -> bool:
  try:
    with open("/proc/sys/kernel/core_pattern") as f:
      core_pattern = f.read().strip()
  except OSError:
    return False

  if not core_pattern.startswith("|"):
    return False

  tokens = core_pattern[1:].split()
  pid_index = next((i for i, token in enumerate(tokens) if "%p" in token), None)
  if pid_index is None:
    return False
  expected_pid_arg = tokens[pid_index].replace("%p", str(pid))

  for entry in os.scandir("/proc"):
    if not entry.name.isdigit():
      continue

    try:
      with open(f"/proc/{entry.name}/cmdline", "rb") as f:
        argv = f.read().decode(errors="replace").split("\0")
    except OSError:
      continue

    if len(argv) > pid_index and argv[0] == tokens[0] and argv[pid_index] == expected_pid_arg:
      return True

  return False


def launcher(proc: str, name: str) -> None:
  try:
    # import the process
    mod = importlib.import_module(proc)

    # rename the process
    setproctitle(proc)

    # create new context since we forked
    messaging.reset_context()

    # add daemon name tag to logs
    cloudlog.bind(daemon=name)
    sentry.set_tag("daemon", name)

    # exec the process
    mod.main()
  except KeyboardInterrupt:
    cloudlog.warning(f"child {proc} got SIGINT")
  except Exception:
    # can't install the crash handler because sys.excepthook doesn't play nice
    # with threads, so catch it here.
    sentry.capture_exception()
    raise


def nativelauncher(pargs: list[str], cwd: str, name: str) -> None:
  os.environ['MANAGER_DAEMON'] = name

  # exec the process
  os.chdir(cwd)
  os.execvp(pargs[0], pargs)


def join_process(process: Process, timeout: float) -> None:
  # Process().join(timeout) will hang due to a python 3 bug: https://bugs.python.org/issue28382
  # We have to poll the exitcode instead
  t = time.monotonic()
  while time.monotonic() - t < timeout and process.exitcode is None:
    time.sleep(0.001)


class ManagerProcess(ABC):
  daemon = False
  sigkill = False
  should_run: Callable[[bool, Params, car.CarParams, SimpleNamespace], bool]
  proc: Process | None = None
  enabled = True
  name = ""

  last_watchdog_time = 0
  watchdog_max_dt: int | None = None
  watchdog_seen = False
  watchdog_coredump_deadline: float | None = None
  watchdog_hang_dump_deadline: float | None = None
  shutting_down = False

  @abstractmethod
  def prepare(self) -> None:
    pass

  @abstractmethod
  def start(self) -> None:
    pass

  def restart(self) -> None:
    self.stop(sig=signal.SIGKILL)
    self.start()

  def check_watchdog(self, started: bool) -> None:
    if self.watchdog_max_dt is None or self.proc is None:
      return

    try:
      fn = WATCHDOG_FN + str(self.proc.pid)
      with open(fn, "rb") as f:
        self.last_watchdog_time = struct.unpack('Q', f.read())[0]
    except Exception:
      pass

    dt = time.monotonic() - self.last_watchdog_time / 1e9

    if dt > self.watchdog_max_dt:
      if self.watchdog_seen and ENABLE_WATCHDOG:
        if self.wait_for_coredump():
          return

        if self.dump_hung_process():
          return

        cloudlog.error(f"Watchdog timeout for {self.name} (exitcode {self.proc.exitcode}) restarting ({started=})")
        self.restart()
    else:
      self.watchdog_seen = True

  def wait_for_coredump(self) -> bool:
    if self.proc is None or not coredump_in_progress(self.proc.pid):
      self.watchdog_coredump_deadline = None
      return False

    if self.watchdog_coredump_deadline is None:
      self.watchdog_coredump_deadline = time.monotonic() + WATCHDOG_COREDUMP_GRACE_S
      cloudlog.error(f"Watchdog timeout for {self.name}, core dump in progress; deferring restart up to {WATCHDOG_COREDUMP_GRACE_S}s")

    return time.monotonic() < self.watchdog_coredump_deadline

  def dump_hung_process(self) -> bool:
    if self.proc is None or self.proc.exitcode is not None or coredump_in_progress(self.proc.pid):
      self.watchdog_hang_dump_deadline = None
      return False

    if self.watchdog_hang_dump_deadline is None:
      self.watchdog_hang_dump_deadline = time.monotonic() + WATCHDOG_HANG_DUMP_TIMEOUT_S
      cloudlog.error(f"Watchdog timeout for {self.name} with nothing to collect; aborting it to capture a core")
      self.signal(signal.SIGABRT)
      return True

    if time.monotonic() < self.watchdog_hang_dump_deadline:
      return True

    self.watchdog_hang_dump_deadline = None
    return False

  def stop(self, retry: bool = True, block: bool = True, sig: signal.Signals = None) -> int | None:
    if self.proc is None:
      return None

    if self.proc.exitcode is None:
      if not self.shutting_down:
        cloudlog.info(f"killing {self.name}")
        if sig is None:
          sig = signal.SIGKILL if self.sigkill else signal.SIGINT
        self.signal(sig)
        self.shutting_down = True

        if not block:
          return None

      join_process(self.proc, 5)

      # If process failed to die send SIGKILL
      if self.proc.exitcode is None and retry:
        cloudlog.info(f"killing {self.name} with SIGKILL")
        self.signal(signal.SIGKILL)
        self.proc.join()

    ret = self.proc.exitcode
    cloudlog.info(f"{self.name} is dead with {ret}")

    if self.proc.exitcode is not None:
      self.shutting_down = False
      self.proc = None

    return ret

  def signal(self, sig: int) -> None:
    if self.proc is None:
      return

    # Don't signal if already exited
    if self.proc.exitcode is not None and self.proc.pid is not None:
      return

    # Can't signal if we don't have a pid
    if self.proc.pid is None:
      return

    cloudlog.info(f"sending signal {sig} to {self.name}")
    os.kill(self.proc.pid, sig)

  def get_process_state_msg(self):
    state = log.ManagerState.ProcessState.new_message()
    state.name = self.name
    if self.proc:
      state.running = self.proc.is_alive()
      state.shouldBeRunning = self.proc is not None and not self.shutting_down
      state.pid = self.proc.pid or 0
      state.exitCode = self.proc.exitcode or 0
    return state


class NativeProcess(ManagerProcess):
  def __init__(self, name, cwd, cmdline, should_run, enabled=True, sigkill=False, watchdog_max_dt=None):
    self.name = name
    self.cwd = cwd
    self.cmdline = cmdline
    self.should_run = should_run
    self.enabled = enabled
    self.sigkill = sigkill
    self.watchdog_max_dt = watchdog_max_dt
    self.launcher = nativelauncher

  def prepare(self) -> None:
    pass

  def start(self) -> None:
    # In case we only tried a non blocking stop we need to stop it before restarting
    if self.shutting_down:
      self.stop()

    if self.proc is not None:
      return

    cwd = os.path.join(BASEDIR, self.cwd)
    cloudlog.info(f"starting process {self.name}")
    self.proc = Process(name=self.name, target=self.launcher, args=(self.cmdline, cwd, self.name))
    self.proc.start()
    self.watchdog_seen = False
    self.shutting_down = False


class PythonProcess(ManagerProcess):
  def __init__(self, name, module, should_run, enabled=True, sigkill=False, watchdog_max_dt=None):
    self.name = name
    self.module = module
    self.should_run = should_run
    self.enabled = enabled
    self.sigkill = sigkill
    self.watchdog_max_dt = watchdog_max_dt
    self.launcher = launcher

  def prepare(self) -> None:
    if self.enabled:
      cloudlog.info(f"preimporting {self.module}")
      importlib.import_module(self.module)

  def start(self) -> None:
    # In case we only tried a non blocking stop we need to stop it before restarting
    if self.shutting_down:
      self.stop()

    if self.proc is not None:
      return

    # TODO: this is just a workaround for this tinygrad check:
    # https://github.com/tinygrad/tinygrad/blob/ac9c96dae1656dc220ee4acc39cef4dd449aa850/tinygrad/device.py#L26
    name = self.name if "modeld" not in self.name else "MainProcess"

    cloudlog.info(f"starting python {self.module}")
    self.proc = Process(name=name, target=self.launcher, args=(self.module, self.name))
    self.proc.start()
    self.watchdog_seen = False
    self.shutting_down = False


class DaemonProcess(ManagerProcess):
  """Python process that has to stay running across manager restart.
  This is used for athena so you don't lose SSH access when restarting manager."""
  def __init__(self, name, module, param_name, enabled=True):
    self.name = name
    self.module = module
    self.param_name = param_name
    self.enabled = enabled
    self.params = None

  @staticmethod
  def should_run(started, params, CP, frogpilot_toggles):
    return True

  def prepare(self) -> None:
    pass

  def start(self) -> None:
    if self.params is None:
      self.params = Params()

    pid = self.params.get(self.param_name)
    if pid is not None:
      try:
        os.kill(int(pid), 0)
        with open(f'/proc/{pid}/cmdline') as f:
          if self.module in f.read():
            # daemon is running
            return
      except (OSError, FileNotFoundError):
        # process is dead
        pass

    cloudlog.info(f"starting daemon {self.name}")
    proc = subprocess.Popen(['python', '-m', self.module],
                               stdin=open('/dev/null'),
                               stdout=open('/dev/null', 'w'),
                               stderr=open('/dev/null', 'w'),
                               preexec_fn=os.setpgrp)

    self.params.put(self.param_name, proc.pid)

  def stop(self, retry=True, block=True, sig=None) -> None:
    pass


def ensure_running(procs: ValuesView[ManagerProcess], started: bool, params=None, CP: car.CarParams=None,
                   not_run: list[str] | None=None, frogpilot_toggles: SimpleNamespace=None) -> list[ManagerProcess]:
  if not_run is None:
    not_run = []

  running = []
  for p in procs:
    if p.enabled and p.name not in not_run and p.should_run(started, params, CP, frogpilot_toggles):
      running.append(p)
    else:
      p.stop(block=False)

    p.check_watchdog(started)

  for p in running:
    p.start()

  return running
