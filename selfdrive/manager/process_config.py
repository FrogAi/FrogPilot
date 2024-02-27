import os

from cereal import car
from openpilot.common.params import Params
from openpilot.system.hardware import PC, TICI
from openpilot.selfdrive.manager.process import PythonProcess, NativeProcess, DaemonProcess

WEBCAM = os.getenv("USE_WEBCAM") is not None

def driverview(started: bool, params: Params, params_memory: Params, CP: car.CarParams) -> bool:
  return started or params.get_bool("IsDriverViewEnabled")

def notcar(started: bool, params: Params, params_memory: Params, CP: car.CarParams) -> bool:
  return started and CP.notCar

def iscar(started: bool, params: Params, params_memory: Params, CP: car.CarParams) -> bool:
  return started and not CP.notCar

def logging(started, params, params_memory, CP: car.CarParams) -> bool:
  run = (not CP.notCar) or not params.get_bool("DisableLogging")
  return started and run

def ublox_available() -> bool:
  return os.path.exists('/dev/ttyHS0') and not os.path.exists('/persist/comma/use-quectel-gps')

def ublox(started, params, params_memory, CP: car.CarParams) -> bool:
  use_ublox = ublox_available()
  if use_ublox != params.get_bool("UbloxAvailable"):
    params.put_bool("UbloxAvailable", use_ublox)
  return started and use_ublox

def qcomgps(started, params, params_memory, CP: car.CarParams) -> bool:
  return started and not ublox_available()

def always_run(started, params, params_memory, CP: car.CarParams) -> bool:
  return True

def only_onroad(started: bool, params, params_memory, CP: car.CarParams) -> bool:
  return started

def only_offroad(started, params, params_memory, CP: car.CarParams) -> bool:
  return not started

# FrogPilot functions
def allow_uploads(started, params, params_memory, CP: car.CarParams) -> bool:
  allow_uploads = not (params_memory.get_bool("FireTheBabysitter") and params_memory.get_bool("NoUploads"))
  at_home = not started or not params_memory.get_bool("DisableOnroadUploads")
  return allow_uploads and at_home

def allow_logging(started, params, params_memory, CP: car.CarParams) -> bool:
  allow_logging = not (params_memory.get_bool("FireTheBabysitter") and params_memory.get_bool("NoLogging"))
  return allow_logging and logging(started, params, params_memory, CP)

def osm(started, params, params_memory, CP: car.CarParams) -> bool:
  return params_memory.get_bool("RoadNameUI") or params_memory.get_bool("SpeedLimitController")

procs = [
  DaemonProcess("manage_athenad", "selfdrive.athena.manage_athenad", "AthenadPid"),

  NativeProcess("camerad", "system/camerad", ["./camerad"], driverview),
  NativeProcess("logcatd", "system/logcatd", ["./logcatd"], allow_logging),
  NativeProcess("proclogd", "system/proclogd", ["./proclogd"], allow_logging),
  PythonProcess("logmessaged", "system.logmessaged", allow_logging),
  PythonProcess("micd", "system.micd", iscar),
  PythonProcess("timed", "system.timed", always_run, enabled=not PC),

  PythonProcess("dmonitoringmodeld", "selfdrive.modeld.dmonitoringmodeld", driverview, enabled=(not PC or WEBCAM)),
  NativeProcess("encoderd", "system/loggerd", ["./encoderd"], allow_logging),
  NativeProcess("stream_encoderd", "system/loggerd", ["./encoderd", "--stream"], notcar),
  NativeProcess("loggerd", "system/loggerd", ["./loggerd"], allow_logging),
  NativeProcess("modeld", "selfdrive/modeld", ["./modeld"], only_onroad),
  NativeProcess("mapsd", "selfdrive/navd", ["./mapsd"], only_onroad),
  PythonProcess("navmodeld", "selfdrive.modeld.navmodeld", only_onroad),
  NativeProcess("sensord", "system/sensord", ["./sensord"], only_onroad, enabled=not PC),
  NativeProcess("ui", "selfdrive/ui", ["./ui"], always_run, watchdog_max_dt=(5 if not PC else None), always_watchdog=only_offroad),
  PythonProcess("soundd", "selfdrive.ui.soundd", only_onroad),
  NativeProcess("locationd", "selfdrive/locationd", ["./locationd"], only_onroad),
  NativeProcess("boardd", "selfdrive/boardd", ["./boardd"], always_run, enabled=False),
  PythonProcess("calibrationd", "selfdrive.locationd.calibrationd", only_onroad),
  PythonProcess("torqued", "selfdrive.locationd.torqued", only_onroad),
  PythonProcess("controlsd", "selfdrive.controls.controlsd", only_onroad),
  PythonProcess("deleter", "system.loggerd.deleter", always_run),
  PythonProcess("dmonitoringd", "selfdrive.monitoring.dmonitoringd", driverview, enabled=(not PC or WEBCAM)),
  PythonProcess("qcomgpsd", "system.qcomgpsd.qcomgpsd", qcomgps, enabled=TICI),
  PythonProcess("navd", "selfdrive.navd.navd", only_onroad),
  PythonProcess("pandad", "selfdrive.boardd.pandad", always_run),
  PythonProcess("paramsd", "selfdrive.locationd.paramsd", only_onroad),
  NativeProcess("ubloxd", "system/ubloxd", ["./ubloxd"], ublox, enabled=TICI),
  PythonProcess("pigeond", "system.sensord.pigeond", ublox, enabled=TICI),
  PythonProcess("plannerd", "selfdrive.controls.plannerd", only_onroad),
  PythonProcess("radard", "selfdrive.controls.radard", only_onroad),
  PythonProcess("thermald", "selfdrive.thermald.thermald", always_run),
  PythonProcess("tombstoned", "selfdrive.tombstoned", allow_logging, enabled=not PC),
  PythonProcess("updated", "selfdrive.updated", always_run, enabled=not PC),
  PythonProcess("uploader", "system.loggerd.uploader", allow_uploads),
  PythonProcess("statsd", "selfdrive.statsd", allow_logging),

  # debug procs
  NativeProcess("bridge", "cereal/messaging", ["./bridge"], notcar),
  PythonProcess("webrtcd", "system.webrtc.webrtcd", notcar),
  PythonProcess("webjoystick", "tools.bodyteleop.web", notcar),

  # FrogPilot processes
  PythonProcess("fleet_manager", "selfdrive.frogpilot.fleetmanager.fleet_manager", always_run),
  PythonProcess("frogpilot_process", "selfdrive.frogpilot.functions.frogpilot_process", always_run),
  PythonProcess("mapd", "selfdrive.frogpilot.functions.mapd", osm),
]

managed_processes = {p.name: p for p in procs}
