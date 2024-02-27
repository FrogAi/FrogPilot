import os
import requests
import sentry_sdk
import threading
import time
from typing import Callable, Dict, List, Optional, Tuple

from cereal import car
from openpilot.common.params import Params
from openpilot.common.basedir import BASEDIR
from openpilot.system.version import get_short_branch, is_comma_remote, is_tested_branch
from openpilot.selfdrive.car.interfaces import get_interface_attr
from openpilot.selfdrive.car.fingerprints import eliminate_incompatible_cars, all_legacy_fingerprint_cars
from openpilot.selfdrive.car.vin import get_vin, is_valid_vin, VIN_UNKNOWN
from openpilot.selfdrive.car.fw_versions import get_fw_versions_ordered, get_present_ecus, match_fw_to_car, set_obd_multiplexing
from openpilot.common.swaglog import cloudlog
import cereal.messaging as messaging
import openpilot.selfdrive.sentry as sentry
from openpilot.selfdrive.car import gen_empty_fingerprint

FRAME_FINGERPRINT = 100  # 1s

EventName = car.CarEvent.EventName


def get_startup_event(car_recognized, controller_available, fw_seen):
  if is_comma_remote() and is_tested_branch():
    event = EventName.startup
  else:
    event = EventName.startupMaster

  if not car_recognized:
    if fw_seen:
      event = EventName.startupNoCar
    else:
      event = EventName.startupNoFw
  elif car_recognized and not controller_available:
    event = EventName.startupNoControl
  return event


def get_one_can(logcan):
  while True:
    can = messaging.recv_one_retry(logcan)
    if len(can.can) > 0:
      return can


def load_interfaces(brand_names):
  ret = {}
  for brand_name in brand_names:
    path = f'openpilot.selfdrive.car.{brand_name}'
    CarInterface = __import__(path + '.interface', fromlist=['CarInterface']).CarInterface

    if os.path.exists(BASEDIR + '/' + path.replace('.', '/') + '/carstate.py'):
      CarState = __import__(path + '.carstate', fromlist=['CarState']).CarState
    else:
      CarState = None

    if os.path.exists(BASEDIR + '/' + path.replace('.', '/') + '/carcontroller.py'):
      CarController = __import__(path + '.carcontroller', fromlist=['CarController']).CarController
    else:
      CarController = None

    for model_name in brand_names[brand_name]:
      ret[model_name] = (CarInterface, CarController, CarState)
  return ret


def _get_interface_names() -> Dict[str, List[str]]:
  # returns a dict of brand name and its respective models
  brand_names = {}
  for brand_name, brand_models in get_interface_attr("CAR").items():
    brand_names[brand_name] = [model.value for model in brand_models]

  return brand_names


# imports from directory selfdrive/car/<name>/
interface_names = _get_interface_names()
interfaces = load_interfaces(interface_names)


def can_fingerprint(next_can: Callable) -> Tuple[Optional[str], Dict[int, dict]]:
  finger = gen_empty_fingerprint()
  candidate_cars = {i: all_legacy_fingerprint_cars() for i in [0, 1]}  # attempt fingerprint on both bus 0 and 1
  frame = 0
  car_fingerprint = None
  done = False

  while not done:
    a = next_can()

    for can in a.can:
      # The fingerprint dict is generated for all buses, this way the car interface
      # can use it to detect a (valid) multipanda setup and initialize accordingly
      if can.src < 128:
        if can.src not in finger:
          finger[can.src] = {}
        finger[can.src][can.address] = len(can.dat)

      for b in candidate_cars:
        # Ignore extended messages and VIN query response.
        if can.src == b and can.address < 0x800 and can.address not in (0x7df, 0x7e0, 0x7e8):
          candidate_cars[b] = eliminate_incompatible_cars(can, candidate_cars[b])

    # if we only have one car choice and the time since we got our first
    # message has elapsed, exit
    for b in candidate_cars:
      if len(candidate_cars[b]) == 1 and frame > FRAME_FINGERPRINT:
        # fingerprint done
        car_fingerprint = candidate_cars[b][0]

    # bail if no cars left or we've been waiting for more than 2s
    failed = (all(len(cc) == 0 for cc in candidate_cars.values()) and frame > FRAME_FINGERPRINT) or frame > 200
    succeeded = car_fingerprint is not None
    done = failed or succeeded

    frame += 1

  return car_fingerprint, finger


# **** for use live only ****
def fingerprint(logcan, sendcan, num_pandas):
  fixed_fingerprint = os.environ.get('FINGERPRINT', "")
  skip_fw_query = os.environ.get('SKIP_FW_QUERY', False)
  disable_fw_cache = os.environ.get('DISABLE_FW_CACHE', False)
  ecu_rx_addrs = set()
  params = Params()

  start_time = time.monotonic()
  if not skip_fw_query:
    cached_params = params.get("CarParamsCache")
    if cached_params is not None:
      with car.CarParams.from_bytes(cached_params) as cached_params:
        if cached_params.carName == "mock":
          cached_params = None

    if cached_params is not None and len(cached_params.carFw) > 0 and \
       cached_params.carVin is not VIN_UNKNOWN and not disable_fw_cache:
      cloudlog.warning("Using cached CarParams")
      vin_rx_addr, vin_rx_bus, vin = -1, -1, cached_params.carVin
      car_fw = list(cached_params.carFw)
      cached = True
    else:
      cloudlog.warning("Getting VIN & FW versions")
      # enable OBD multiplexing for VIN query
      # NOTE: this takes ~0.1s and is relied on to allow sendcan subscriber to connect in time
      set_obd_multiplexing(params, True)
      # VIN query only reliably works through OBDII
      vin_rx_addr, vin_rx_bus, vin = get_vin(logcan, sendcan, (0, 1))
      ecu_rx_addrs = get_present_ecus(logcan, sendcan, num_pandas=num_pandas)
      car_fw = get_fw_versions_ordered(logcan, sendcan, ecu_rx_addrs, num_pandas=num_pandas)
      cached = False

    exact_fw_match, fw_candidates = match_fw_to_car(car_fw)
  else:
    vin_rx_addr, vin_rx_bus, vin = -1, -1, VIN_UNKNOWN
    exact_fw_match, fw_candidates, car_fw = True, set(), []
    cached = False

  if not is_valid_vin(vin):
    cloudlog.event("Malformed VIN", vin=vin, error=True)
    vin = VIN_UNKNOWN
  cloudlog.warning("VIN %s", vin)
  params.put("CarVin", vin)

  # disable OBD multiplexing for CAN fingerprinting and potential ECU knockouts
  set_obd_multiplexing(params, False)
  params.put_bool("FirmwareQueryDone", True)

  fw_query_time = time.monotonic() - start_time

  # CAN fingerprint
  # drain CAN socket so we get the latest messages
  messaging.drain_sock_raw(logcan)
  car_fingerprint, finger = can_fingerprint(lambda: get_one_can(logcan))

  exact_match = True
  source = car.CarParams.FingerprintSource.can

  # If FW query returns exactly 1 candidate, use it
  if len(fw_candidates) == 1:
    car_fingerprint = list(fw_candidates)[0]
    source = car.CarParams.FingerprintSource.fw
    exact_match = exact_fw_match

  if fixed_fingerprint:
    car_fingerprint = fixed_fingerprint
    source = car.CarParams.FingerprintSource.fixed

  cloudlog.event("fingerprinted", car_fingerprint=car_fingerprint, source=source, fuzzy=not exact_match, cached=cached,
                 fw_count=len(car_fw), ecu_responses=list(ecu_rx_addrs), vin_rx_addr=vin_rx_addr, vin_rx_bus=vin_rx_bus,
                 fingerprints=repr(finger), fw_query_time=fw_query_time, error=True)
  return car_fingerprint, finger, vin, car_fw, source, exact_match

def chunk_data(data, size):
  return [data[i:i+size] for i in range(0, len(data), size)]

def format_params(params):
  return [f"{key}: {value.decode('utf-8') if isinstance(value, bytes) else value}" for key, value in params.items()]

def get_frogpilot_params(params, keys):
  return {key: params.get(key) or '0' for key in keys}

def set_sentry_scope(scope, chunks, label):
  scope.set_extra(label, '\n'.join(['\n'.join(chunk) for chunk in chunks]))

def is_connected_to_internet(timeout=5):
  try:
    requests.get("https://sentry.io", timeout=timeout)
    return True
  except Exception:
    return False

def crash_log(params, candidate):
  serial_id = params.get("HardwareSerial", encoding='utf-8')

  control_keys, vehicle_keys, visual_keys = [
    "AdjustablePersonalities", "PersonalitiesViaWheel", "PersonalitiesViaScreen", "AlwaysOnLateral", "AlwaysOnLateralMain",
    "ConditionalExperimental", "CESpeed", "CESpeedLead", "CECurves", "CECurvesLead", "CENavigation", "CENavigationIntersections",
    "CENavigationLead", "CENavigationTurns", "CESlowerLead", "CEStopLights", "CEStopLightsLead", "CESignal", "CustomPersonalities",
    "AggressiveFollow", "AggressiveJerk", "StandardFollow", "StandardJerk", "RelaxedFollow", "RelaxedJerk", "DeviceShutdown",
    "ExperimentalModeActivation", "ExperimentalModeViaLKAS", "ExperimentalModeViaScreen", "FireTheBabysitter", "NoLogging", "MuteOverheated",
    "OfflineMode", "LateralTune", "ForceAutoTune", "NNFF", "SteerRatio", "UseLateralJerk", "LongitudinalTune", "AccelerationProfile",
    "DecelerationProfile", "AggressiveAcceleration", "StoppingDistance", "LeadDetectionThreshold", "SmoothBraking", "Model", "MTSCEnabled",
    "DisableMTSCSmoothing", "MTSCAggressiveness", "MTSCCurvatureCheck", "MTSCLimit", "NudgelessLaneChange", "LaneChangeTime", "LaneDetection",
    "LaneDetectionWidth", "OneLaneChange", "QOLControls", "DisableOnroadUploads", "HigherBitrate", "NavChill", "PauseLateralOnSignal", "ReverseCruise",
    "ReverseCruiseUI", "SetSpeedLimit", "SetSpeedOffset",  "SpeedLimitController", "Offset1", "Offset2", "Offset3", "Offset4", "SLCConfirmation",
    "SLCFallback", "SLCPriority1", "SLCPriority2", "SLCPriority3", "SLCOverride", "TurnDesires", "VisionTurnControl", "DisableVTSCSmoothing",
    "CurveSensitivity", "TurnAggressiveness"
  ], [
    "ForceFingerprint", "DisableOpenpilotLongitudinal", "EVTable", "GasRegenCmd", "LongPitch", "LowerVolt", "CrosstrekTorque", "CydiaTune",
    "DragonPilotTune", "FrogsGoMooTune", "LockDoors", "SNGHack"
  ], [
    "CustomTheme", "HolidayThemes", "CustomColors", "CustomIcons", "CustomSignals", "CustomSounds", "GoatScream", "AlertVolumeControl", "DisengageVolume",
    "EngageVolume", "PromptVolume", "PromptDistractedVolume", "RefuseVolume", "WarningSoftVolume", "WarningImmediateVolume", "CameraView",
    "Compass", "CustomAlerts", "GreenLightAlert", "LeadDepartingAlert", "LoudBlindspotAlert", "SpeedLimitChangedAlert", "CustomUI", "AccelerationPath",
    "AdjacentPath", "AdjacentPathMetrics", "BlindSpotPath", "FPSCounter", "LeadInfo", "UseSI", "PedalsOnUI", "RoadNameUI", "UseVienna", "DriverCamera",
    "ModelUI", "DynamicPathWidth", "LaneLinesWidth", "PathEdgeWidth", "PathWidth", "RoadEdgesWidth", "UnlimitedLength", "QOLVisuals", "DriveStats",
    "FullMap", "HideSpeed", "HideSpeedUI", "ShowSLCOffset", "SpeedLimitChangedAlert", "WheelSpeed", "RandomEvents", "ScreenBrightness", "WheelIcon",
    "RotatingWheel", "NumericalTemp", "Fahrenheit", "ShowCPU", "ShowGPU", "ShowIP", "ShowMemoryUsage", "ShowStorageLeft", "ShowStorageUsed", "Sidebar"
  ]

  control_params, vehicle_params, visual_params = map(lambda keys: get_frogpilot_params(params, keys), [control_keys, vehicle_keys, visual_keys])
  control_values, vehicle_values, visual_values = map(format_params, [control_params, vehicle_params, visual_params])
  control_chunks, vehicle_chunks, visual_chunks = map(lambda data: chunk_data(data, 50), [control_values, vehicle_values, visual_values])

  while not is_connected_to_internet():
    time.sleep(60)

  with sentry_sdk.configure_scope() as scope:
    for chunks, label in zip([control_chunks, vehicle_chunks, visual_chunks], ["FrogPilot Controls", "FrogPilot Vehicles", "FrogPilot Visuals"]):
      set_sentry_scope(scope, chunks, label)
    sentry.capture_warning(f"Fingerprinted: {candidate}", serial_id)

def get_car(logcan, sendcan, experimental_long_allowed, num_pandas=1):
  params = Params()
  car_brand = params.get("CarMake", encoding='utf-8')
  car_model = params.get("CarModel", encoding='utf-8')
  dongle_id = params.get("DongleId", encoding='utf-8')

  force_fingerprint = params.get_bool("ForceFingerprint")

  candidate, fingerprints, vin, car_fw, source, exact_match = fingerprint(logcan, sendcan, num_pandas)

  if candidate is None or force_fingerprint:
    if car_model is not None:
      candidate = car_model
    else:
      cloudlog.event("car doesn't match any fingerprints", fingerprints=repr(fingerprints), error=True)
      candidate = "mock"
  elif car_model is None:
    params.put("CarMake", candidate.split(' ')[0].title())
    params.put("CarModel", candidate)

  if get_short_branch() == "FrogPilot-Development" and not Params("/persist/comma/params").get_bool("FrogsGoMoo"):
    candidate = "mock"

  setFingerprintLog = threading.Thread(target=crash_log, args=(params, candidate,))
  setFingerprintLog.start()

  CarInterface, CarController, CarState = interfaces[candidate]
  CP = CarInterface.get_params(params, candidate, fingerprints, car_fw, experimental_long_allowed, docs=False)
  CP.carVin = vin
  CP.carFw = car_fw
  CP.fingerprintSource = source
  CP.fuzzyFingerprint = not exact_match

  return CarInterface(CP, CarController, CarState), CP

def write_car_param(fingerprint="mock"):
  params = Params()
  CarInterface, _, _ = interfaces[fingerprint]
  CP = CarInterface.get_non_essential_params(fingerprint)
  params.put("CarParams", CP.to_bytes())

def get_demo_car_params():
  fingerprint="mock"
  CarInterface, _, _ = interfaces[fingerprint]
  CP = CarInterface.get_non_essential_params(fingerprint)
  return CP
