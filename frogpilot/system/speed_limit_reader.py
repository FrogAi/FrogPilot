#!/usr/bin/env python3
import functools
import json
import MNN
import numpy as np
import os
import time

from cereal import messaging
from msgq.visionipc import VisionIpcClient, VisionStreamType
from openpilot.common.realtime import set_core_affinity

from openpilot.frogpilot.common import frogpilot_variables

def load_model(folder):
  model_file = folder / frogpilot_variables.SPEED_LIMIT_READER_MODEL_FILES[0]

  if model_file.is_file():
    runtime = MNN.nn.create_runtime_manager(({"backend": "CPU", "numThread": 1},))
    return [MNN.nn.load_module_from_file(str(model_file), ["frame", "text", "context"], [mode], runtime_manager=runtime) for mode in ("0", "1")]

  return None

def read(sm, field):
  service, path = field.split(".", 1)

  if path == "age":
    return time.monotonic() - sm.recv_time[service]
  if path == "alive":
    return sm.alive[service]

  return functools.reduce(getattr, path.split("."), sm[service])

def main():
  set_core_affinity([6])
  os.sched_setscheduler(0, os.SCHED_IDLE, os.sched_param(0))

  modes = load_model(frogpilot_variables.SPEED_LIMIT_READER_MODEL_PATH) or load_model(frogpilot_variables.SPEED_LIMIT_READER_BUNDLED_PATH)
  inputs = json.loads(modes[0].get_info()["bizCode"])

  pm = messaging.PubMaster(["frogpilotSignReading"])
  sm = messaging.SubMaster(sorted({field.split(".")[0] for field in inputs["numbers"] + inputs["text"]}))

  client = VisionIpcClient("camerad", VisionStreamType.VISION_STREAM_ROAD, True)
  while not client.connect(False):
    time.sleep(0.1)

  state = np.zeros(7, np.float32)

  while True:
    buffer = client.recv(1000)

    sm.update(0)

    if buffer is not None and client.frame_id >= state[2]:
      text = np.frombuffer("\0".join(str(read(sm, field)) for field in inputs["text"]).encode(), np.uint8)
      context = np.array([client.frame_id, buffer.width, buffer.height, buffer.stride, buffer.uv_offset, *(read(sm, field) for field in inputs["numbers"]), *state], np.float32)

      frame_input = MNN.expr.const(buffer.data, [buffer.data.size], MNN.expr.NCHW, MNN.expr.uint8)
      text_input = MNN.expr.const(text, [text.size], MNN.expr.NCHW, MNN.expr.uint8)
      context_input = MNN.expr.const(context, [context.size], MNN.expr.NCHW, MNN.expr.float)

      state = modes[int(state[3])].forward([frame_input, text_input, context_input])[0].read()

      message = messaging.new_message("frogpilotSignReading")
      message.frogpilotSignReading.frameId = float(state[0])
      message.frogpilotSignReading.mode = float(state[1])
      message.frogpilotSignReading.nextFrameId = float(state[2])
      message.frogpilotSignReading.nextMode = float(state[3])
      message.frogpilotSignReading.signAgreement = float(state[4])
      message.frogpilotSignReading.signSpeedLimit = float(state[5])
      message.frogpilotSignReading.speedLimit = float(state[6])
      pm.send("frogpilotSignReading", message)

if __name__ == "__main__":
  main()
