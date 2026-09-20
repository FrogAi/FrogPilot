#!/usr/bin/env python3
"""Best-effort road-camera inference; never runs in the control/model process."""
import time

import cv2
import numpy as np

from openpilot.frogpilot.common.vision_speed_limit import HEARTBEAT_TIMEOUT, VISION_SPEED_LIMIT_PARAM, SpeedLimitConfirmation
from openpilot.frogpilot.system.vision_speed_limit_model import SpeedLimitModel

INFERENCE_INTERVAL = 1 / 6
FOLLOWUP_INTERVAL = 0.1
FOLLOWUP_SECONDS = 2.0
RUNTIME_LOOP_HZ = 30
BUSY_INTERVAL = 1.5
MODEL_RETRY_INTERVAL = 30.0
MAX_FRAME_AGE = 0.5
INPUT_MAX_AGE = {"deviceState": 5.0, "carState": 0.1}


def inputs_valid(sm, now):
  # Inference intentionally blocks this worker. Its receive rate therefore does
  # not measure the publishers' rates; validate freshness at the source instead.
  return (sm.all_alive(list(INPUT_MAX_AGE)) and sm.all_valid(list(INPUT_MAX_AGE)) and
          all(0 <= now - sm.logMonoTime[service] / 1e9 <= max_age for service, max_age in INPUT_MAX_AGE.items()))


def inference_interval(device_state, processing_time, followup=False):
  usage = list(device_state.cpuUsagePercent)
  busy = bool(usage) and (sum(usage) / len(usage) >= 78 or sum(value >= 92 for value in usage) >= 4)
  return max(FOLLOWUP_INTERVAL if followup else INFERENCE_INTERVAL, processing_time * 2.5,
             BUSY_INTERVAL if busy or device_state.memoryUsagePercent >= 88 else 0)


def decode_nv12(buffer, width, height, stride, uv_offset):
  """Camera planes may be separated by padding; height * stride is not the UV offset."""
  data = np.frombuffer(buffer, dtype=np.uint8)
  if (width <= 0 or height <= 0 or width % 2 or height % 2 or stride < width or
      uv_offset < height * stride or len(data) < uv_offset + height // 2 * stride):
    raise ValueError("Invalid NV12 camera buffer layout")
  y = data[:height * stride].reshape(height, stride)[:, :width]
  uv = data[uv_offset:uv_offset + height // 2 * stride].reshape(height // 2, stride)[:, :width]
  return cv2.cvtColorTwoPlane(y, uv.reshape(height // 2, width // 2, 2), cv2.COLOR_YUV2BGR_NV12)


class SpeedLimitVisionDaemon:
  def __init__(self, params_memory, sm, camera_type, stream_type, logger, model_factory=SpeedLimitModel):
    self.params_memory = params_memory
    self.sm = sm
    self.camera_type = camera_type
    self.stream_type = stream_type
    self.logger = logger
    self.model_factory = model_factory
    self.model = None
    self.client = None
    self.stream = None
    self.confirmation = SpeedLimitConfirmation()
    self.last_road = ""
    self.last_frame_at = 0.0
    self.last_inference_at = -float("inf")
    self.last_publish_at = -float("inf")
    self.last_model_attempt = -float("inf")
    self.processing_time = 0.0
    self.followup_until = 0.0
    self.status = ""
    self.clear("Starting")

  def clear(self, status, disconnect=False):
    self.confirmation.reset()
    self.followup_until = 0.0
    self.params_memory.remove(VISION_SPEED_LIMIT_PARAM)
    if disconnect:
      self.client = None
      self.stream = None
      self.last_frame_at = 0.0
      self.last_road = ""
    self.set_status(status)

  def set_status(self, status):
    if status != self.status:
      self.params_memory.put("VisionSpeedLimitStatus", status)
      self.status = status

  def publish(self, now, force=False):
    if force or now - self.last_publish_at >= 1.0:
      self.params_memory.put(VISION_SPEED_LIMIT_PARAM, self.confirmation.snapshot(now))
      self.last_publish_at = now

  def connect_camera(self):
    streams = self.camera_type.available_streams("camerad", block=False)
    desired = next((stream for stream in (self.stream_type.VISION_STREAM_ROAD, self.stream_type.VISION_STREAM_WIDE_ROAD)
                    if stream in streams), None)
    if desired is None:
      return False
    if self.client is None or self.stream != desired:
      self.clear("Waiting for camera", disconnect=True)
      self.client = self.camera_type("camerad", desired, True)
      self.stream = desired
    return self.client.is_connected() or self.client.connect(False)

  def step(self, now):
    self.sm.update(0)
    now = time.monotonic()
    self.confirmation.expire(now)
    if not inputs_valid(self.sm, now) or not self.sm["deviceState"].started:
      self.clear("Idle", disconnect=True)
      return
    device_state = self.sm["deviceState"]
    if device_state.thermalStatus >= 2 or device_state.memoryUsagePercent >= 94:
      self.clear("Paused: device load", disconnect=True)
      return
    if self.sm["carState"].gearShifter not in ("drive", "low"):
      self.clear("Idle", disconnect=True)
      return

    if self.sm.valid["mapdOut"] and self.sm.alive["mapdOut"]:
      road = self.sm["mapdOut"].roadName
      if road and self.last_road and road != self.last_road:
        self.clear("Scanning")
      self.last_road = road or self.last_road

    if self.model is None:
      if now - self.last_model_attempt < MODEL_RETRY_INTERVAL:
        return
      self.last_model_attempt = now
      self.clear("Loading models")
      try:
        self.model = self.model_factory()
      except (OSError, ValueError, cv2.error):
        self.logger.exception("Unable to load vision speed limit models")
        self.clear("Models unavailable", disconnect=True)
        return

    if now - self.last_inference_at < inference_interval(device_state, self.processing_time, now < self.followup_until):
      # A heartbeat is evidence of a live camera, not merely a live Python loop.
      if now - self.last_frame_at <= HEARTBEAT_TIMEOUT:
        self.publish(now)
      else:
        self.clear("Waiting for camera", disconnect=True)
      return

    self.last_inference_at = now
    if not self.connect_camera():
      self.clear("Waiting for camera", disconnect=True)
      return
    buffer = self.client.recv(timeout_ms=100)
    now = time.monotonic()
    frame_time = self.client.timestamp_eof / 1e9
    # This branch's camerad does not populate VisionIpcBufExtra.valid. Validate
    # received frames using their timestamps and buffer layout instead.
    if buffer is None or not 0 <= now - frame_time <= MAX_FRAME_AGE:
      if now - self.last_frame_at > HEARTBEAT_TIMEOUT:
        self.clear("Waiting for camera", disconnect=True)
      return
    if frame_time <= self.confirmation.last_frame_time:
      if now - self.last_frame_at > HEARTBEAT_TIMEOUT:
        self.clear("Waiting for camera", disconnect=True)
      return

    self.last_frame_at = now
    started_at = now
    frame = decode_nv12(buffer.data, self.client.width, self.client.height, self.client.stride, self.client.uv_offset)
    detection = self.model.detect(frame)
    finished_at = time.monotonic()
    self.processing_time = finished_at - started_at
    # Slow/incomplete inference is not allowed to refresh the source indefinitely.
    if finished_at - frame_time > HEARTBEAT_TIMEOUT:
      self.clear("Paused: slow inference")
      return
    self.confirmation.update(detection, frame_time, finished_at)
    if detection is not None:
      self.followup_until = finished_at + FOLLOWUP_SECONDS
    self.publish(finished_at, force=True)
    self.set_status("Tracking" if self.confirmation.speed_mph else "Scanning")

  def run(self):
    from openpilot.common.realtime import Ratekeeper

    ratekeeper = Ratekeeper(RUNTIME_LOOP_HZ, None)
    try:
      while True:
        try:
          self.step(time.monotonic())
        except (OSError, ValueError, cv2.error):
          self.logger.exception("Vision speed limit inference failed")
          self.model = None
          self.last_model_attempt = time.monotonic()
          self.clear("Inference unavailable", disconnect=True)
        ratekeeper.keep_time()
    finally:
      self.clear("Stopped", disconnect=True)


def main():
  from cereal import messaging
  from msgq.visionipc import VisionIpcClient, VisionStreamType
  from openpilot.common.params import Params
  from openpilot.common.realtime import set_core_affinity
  from openpilot.common.swaglog import cloudlog
  from openpilot.system.hardware import PC

  if not PC:
    set_core_affinity([0, 1, 2])
  cv2.setNumThreads(1)
  cv2.ocl.setUseOpenCL(False)
  sm = messaging.SubMaster(["deviceState", "carState", "mapdOut"], frequency=RUNTIME_LOOP_HZ)
  SpeedLimitVisionDaemon(Params(memory=True), sm, VisionIpcClient, VisionStreamType, cloudlog).run()


if __name__ == "__main__":
  main()
