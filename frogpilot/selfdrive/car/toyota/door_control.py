import time

from cereal import messaging
from opendbc.can.parser import CANParser
from openpilot.common.realtime import DT_DMON, DT_HW
from openpilot.selfdrive.car.toyota.carcontroller import LOCK_CMD, UNLOCK_CMD

from openpilot.frogpilot.common import frogpilot_utilities, frogpilot_variables

def create_door_command(command):
  message = messaging.new_message("sendcan", 1)
  message.sendcan[0].address = 0x750
  message.sendcan[0].dat = command
  message.sendcan[0].src = 0
  return message

def send_door_command(lock):
  sm = messaging.SubMaster(["pandaStates"])

  can_parser = CANParser("toyota_nodsu_pt_generated", [("DOOR_LOCKS", 3)], bus=0)
  can_sock = messaging.sub_sock("can", timeout=100)

  pm = messaging.PubMaster(["sendcan"])
  time.sleep(0.2)

  for _ in range(frogpilot_variables.DOOR_COMMAND_ATTEMPTS):
    sm.update()
    if frogpilot_utilities.is_ignition_on(sm):
      return False

    if lock:
      sendcan_send = create_door_command(LOCK_CMD)
    else:
      sendcan_send = create_door_command(UNLOCK_CMD)
    pm.send("sendcan", sendcan_send)

    time.sleep(1)

    update_can_parser(can_parser, can_sock)
    if can_parser.ts_nanos["DOOR_LOCKS"]["LOCK_STATUS"] > 0 and (can_parser.vl["DOOR_LOCKS"]["LOCK_STATUS"] == 0) == lock:
      return True
  return False

def update_can_parser(can_parser, can_sock):
  can_parser.update_strings(messaging.drain_sock_raw(can_sock, wait_for_one=True))

def lock_doors(lock_doors_timer):
  sm = messaging.SubMaster(["deviceState", "driverMonitoringState", "managerState", "pandaStates"])
  while not (sm.seen["deviceState"] and sm.seen["managerState"]):
    sm.update()

  wait_for_no_driver(sm, lock_doors_timer)

  sm.update()
  if frogpilot_utilities.is_ignition_on(sm):
    return

  can_parser = CANParser("toyota_nodsu_pt_generated", [("DOOR_LOCKS", 3)], bus=0)
  can_sock = messaging.sub_sock("can", timeout=100)

  pm = messaging.PubMaster(["sendcan"])
  time.sleep(0.2)

  while True:
    sm.update()

    if frogpilot_utilities.is_ignition_on(sm):
      break

    sendcan_send = create_door_command(LOCK_CMD)
    pm.send("sendcan", sendcan_send)

    time.sleep(1)

    update_can_parser(can_parser, can_sock)
    if can_parser.ts_nanos["DOOR_LOCKS"]["LOCK_STATUS"] > 0 and can_parser.vl["DOOR_LOCKS"]["LOCK_STATUS"] == 0:
      break

def wait_for_no_driver(sm, time_threshold):
  can_parser = CANParser("toyota_nodsu_pt_generated", [("BODY_CONTROL_STATE", 3)], bus=0)
  can_sock = messaging.sub_sock("can", timeout=100)

  while sm["deviceState"].screenBrightnessPercent != 0 or any(proc.name == "dmonitoringd" and proc.running for proc in sm["managerState"].processes):
    sm.update()

    if frogpilot_utilities.is_ignition_on(sm):
      return

    time.sleep(DT_HW)

  frogpilot_variables.params.put_bool("IsDriverViewEnabled", True)

  start_time = time.monotonic()
  while True:
    sm.update()

    elapsed_time = time.monotonic() - start_time
    if elapsed_time >= time_threshold:
      break

    if frogpilot_utilities.is_ignition_on(sm):
      break

    if sm.updated["deviceState"] and not frogpilot_variables.params.get_bool("IsDriverViewEnabled"):
      frogpilot_variables.params.put_bool("IsDriverViewEnabled", True)

    if sm["driverMonitoringState"].faceDetected or not sm.alive["driverMonitoringState"]:
      start_time = time.monotonic()

    update_can_parser(can_parser, can_sock)

    door_open = any([can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_FL"], can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_FR"],
                     can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_RL"], can_parser.vl["BODY_CONTROL_STATE"]["DOOR_OPEN_RR"]])
    if door_open:
      start_time = time.monotonic()

    time.sleep(DT_DMON)

  frogpilot_variables.params.remove("IsDriverViewEnabled")
