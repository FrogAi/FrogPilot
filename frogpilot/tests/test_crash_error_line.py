import json
import pytest

from openpilot.system import sentry, tombstoned

from openpilot.frogpilot.common import frogpilot_paths

CRASH_REPORT = """ProblemType: Crash
ExecutablePath: /data/openpilot/system/loggerd/encoderd
ProcStatus:
 Name:\tencoderd
 Pid:\t57334
 PPid:\t17848
Signal: 6
"""

STACKTRACE = """--- stack trace ---
#0  __pthread_kill_implementation () at ./nptl/pthread_kill.c:44
#1  util::safe_ioctl(int, unsigned long, void*, char const*) ()
#2  V4LEncoder::V4LEncoder(EncoderInfo const&, int, int) ()"""


@pytest.fixture
def crash(mocker, monkeypatch, tmp_path):
  monkeypatch.setenv("LOG_ROOT", str(tmp_path / "log_root"))
  mocker.patch.object(frogpilot_paths, "SWAGLOG_ERROR_PATH", tmp_path / "swaglog_error")
  mocker.patch.object(tombstoned, "get_apport_stacktrace", return_value=STACKTRACE)

  crash = tmp_path / "_data_openpilot_system_loggerd_encoderd.1000.crash"
  crash.write_text(CRASH_REPORT)

  return crash


def write_error(pid, text):
  frogpilot_paths.SWAGLOG_ERROR_PATH.with_suffix(f".{pid}").write_text(text)


def write_record(pid, msg):
  write_error(pid, json.dumps({"filename": "common/util.cc", "lineno": 162, "msg": msg}))


class TestCrashErrorLine:
  @pytest.mark.parametrize("text", ["", '{"filename": "common/util.cc", "lineno": 162, "msg": "safe_ioctl error: VIDIOC_S_F'])
  def test_damaged_error_file(self, crash, mocker, text):
    write_error(57334, text)
    report_tombstone = mocker.patch.object(sentry, "report_tombstone")

    tombstoned.report_tombstone_apport(str(crash))

    assert report_tombstone.call_args.args[2] == f"{STACKTRACE}\n\n{CRASH_REPORT}\n"

  def test_last_error(self, crash, mocker):
    write_record(57334, "safe_ioctl error: VIDIOC_S_FMT failed Device or resource busy(16) (fd: 7 request: c0d05605 argp: 0x7fc1a2b3c0)")
    report_tombstone = mocker.patch.object(sentry, "report_tombstone")

    tombstoned.report_tombstone_apport(str(crash))

    last_error = "common/util.cc:162 safe_ioctl error: VIDIOC_S_FMT failed Device or resource busy(16) (fd: 7 request: c0d05605 argp: 0x7fc1a2b3c0)"
    assert report_tombstone.call_args.args[2] == f"--- last error ---\n{last_error}\n\n{STACKTRACE}\n\n{CRASH_REPORT}\n"

  def test_other_pid(self, crash, mocker):
    write_record(17848, "safe_ioctl error: VIDIOC_QBUF failed Invalid argument(22) (fd: 9 request: c058560f argp: 0x7fd4e5f6a0)")
    report_tombstone = mocker.patch.object(sentry, "report_tombstone")

    tombstoned.report_tombstone_apport(str(crash))

    assert report_tombstone.call_args.args[2] == f"{STACKTRACE}\n\n{CRASH_REPORT}\n"
