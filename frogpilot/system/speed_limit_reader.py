#!/usr/bin/env python3
import functools
import json
import math
import MNN
import numpy as np
import os
import time

from cereal import custom, messaging
from msgq.visionipc import VisionIpcClient, VisionStreamType
from openpilot.common.gps import get_gps_location_service
from openpilot.common.params import Params
from openpilot.common.realtime import set_core_affinity
from openpilot.common.utils import atomic_write
from PIL import Image

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_utilities, frogpilot_variables

SIGN_BOX_OUTPUTS = ("heldSignLeft", "heldSignTop", "heldSignRight", "heldSignBottom")
SIGN_OUTPUTS = ("heldSignConfidence", *SIGN_BOX_OUTPUTS)

def capture_sign(buffer, state, outputs, sm, gps_service, best_confidences):
  confidence = state[outputs["heldSignConfidence"]]
  speed_limit = float(state[outputs["speedLimit"]])

  if confidence <= 0 or speed_limit < 1:
    return

  if not frogpilot_utilities.is_gps_location_valid(gps_service, sm):
    return

  map_match = sm["mapdOut"]
  if not (sm.alive["mapdOut"] and map_match.tileLoaded and map_match.wayId > 0):
    return

  key = (map_match.wayId, map_match.isForward, round(speed_limit * 100))
  if confidence <= best_confidences.get(key, 0):
    return

  best_confidences[key] = confidence

  image_path = frogpilot_paths.SPEED_LIMIT_FILLER_PENDING_IMAGES_PATH / f"{key[0]}_{int(key[1])}_{key[2]}.jpg"

  box = [float(state[outputs[name]]) for name in SIGN_BOX_OUTPUTS]
  save_sign_image(buffer, box, image_path)

  gps_location = sm[gps_service]
  frogpilot_utilities.update_json_file(image_path.with_suffix(".json"), {
    "bearing": gps_location.bearingDeg,
    "captured_at": gps_location.unixTimestampMillis,
    "latitude": gps_location.latitude,
    "longitude": gps_location.longitude,
  })

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

def save_sign_image(buffer, box, path):
  left, top, right, bottom = box

  margin_x = (right - left) * frogpilot_variables.SIGN_IMAGE_MARGIN
  margin_y = (bottom - top) * frogpilot_variables.SIGN_IMAGE_MARGIN

  left = max(math.floor(left - margin_x), 0) // 2 * 2
  top = max(math.floor(top - margin_y), 0) // 2 * 2
  right = min(math.ceil((right + margin_x) / 2) * 2, buffer.width)
  bottom = min(math.ceil((bottom + margin_y) / 2) * 2, buffer.height)

  y_plane = buffer.data[:buffer.stride * buffer.height].reshape(buffer.height, buffer.stride)
  uv_plane = buffer.data[buffer.uv_offset:buffer.uv_offset + buffer.stride * buffer.height // 2].reshape(buffer.height // 2, buffer.stride)

  y = np.ascontiguousarray(y_plane[top:bottom, left:right])
  uv = uv_plane[top // 2:bottom // 2, left:right]
  u = uv[:, 0::2].repeat(2, axis=0).repeat(2, axis=1)
  v = uv[:, 1::2].repeat(2, axis=0).repeat(2, axis=1)

  image = Image.merge("YCbCr", [Image.fromarray(y), Image.fromarray(u), Image.fromarray(v)])
  with atomic_write(str(path), mode="wb", overwrite=True) as file:
    image.save(file, "JPEG")
    file.flush()
    os.fsync(file.fileno())

def main():
  set_core_affinity([6])
  os.sched_setscheduler(0, os.SCHED_IDLE, os.sched_param(0))

  modes = load_model(frogpilot_paths.SPEED_LIMIT_READER_MODEL_PATH) or load_model(frogpilot_paths.SPEED_LIMIT_READER_BUNDLED_PATH)
  inputs = json.loads(modes[0].get_info()["bizCode"])
  outputs = {name: index for index, name in enumerate(inputs["outputs"])}
  fields = [(name, index) for name, index in outputs.items() if name in custom.FrogPilotSignReading.schema.fieldnames]

  capture = all(name in outputs for name in SIGN_OUTPUTS) and frogpilot_variables.get_frogpilot_toggles().speed_limit_filler

  gps_service = get_gps_location_service(Params())

  services = {field.split(".")[0] for field in inputs["numbers"] + inputs["text"]}
  if capture:
    services |= {gps_service, "mapdOut"}

    frogpilot_paths.SPEED_LIMIT_FILLER_PENDING_IMAGES_PATH.mkdir(parents=True, exist_ok=True)
    for path in frogpilot_paths.SPEED_LIMIT_FILLER_PENDING_IMAGES_PATH.iterdir():
      path.unlink(missing_ok=True)

  pm = messaging.PubMaster(["frogpilotSignReading"])
  sm = messaging.SubMaster(sorted(services))

  client = VisionIpcClient("camerad", VisionStreamType.VISION_STREAM_ROAD, True)
  while not client.connect(False):
    time.sleep(0.1)

  state = np.zeros(len(outputs), np.float32)

  best_confidences = {}

  while True:
    buffer = client.recv(1000)

    sm.update(0)

    if buffer is not None and client.frame_id >= state[outputs["nextFrameId"]]:
      text = np.frombuffer("\0".join(str(read(sm, field)) for field in inputs["text"]).encode(), np.uint8)
      context = np.array([client.frame_id, buffer.width, buffer.height, buffer.stride, buffer.uv_offset, *(read(sm, field) for field in inputs["numbers"]), *state], np.float32)

      frame_input = MNN.expr.const(buffer.data, [buffer.data.size], MNN.expr.NCHW, MNN.expr.uint8)
      text_input = MNN.expr.const(text, [text.size], MNN.expr.NCHW, MNN.expr.uint8)
      context_input = MNN.expr.const(context, [context.size], MNN.expr.NCHW, MNN.expr.float)

      state = modes[int(state[outputs["nextMode"]])].forward([frame_input, text_input, context_input])[0].read()

      message = messaging.new_message("frogpilotSignReading")
      for name, index in fields:
        setattr(message.frogpilotSignReading, name, float(state[index]))
      pm.send("frogpilotSignReading", message)

      if capture:
        capture_sign(buffer, state, outputs, sm, gps_service, best_confidences)

if __name__ == "__main__":
  main()
