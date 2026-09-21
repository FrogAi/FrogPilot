"""Confirmation and freshness rules shared by vision detection and the SLC."""
import math

from collections import deque
from dataclasses import dataclass

from openpilot.common.constants import CV

VISION_SPEED_LIMIT_PARAM = "VisionSpeedLimit"
HISTORY_SECONDS = 2.0
HOLD_SECONDS = 300.0
HEARTBEAT_TIMEOUT = 3.0
MIN_CONFIDENCE = 0.70
LOW_LIMIT_CONFIDENCE = 0.90
VALID_SPEEDS_MPH = frozenset(range(5, 85, 5))


@dataclass(frozen=True)
class Detection:
  speed_mph: int
  confidence: float


class SpeedLimitConfirmation:
  def __init__(self):
    self.reset()

  def reset(self):
    self.history = deque(maxlen=20)
    self.speed_mph = 0
    self.confidence = 0.0
    self.detected_at = 0.0
    self.last_frame_time = 0.0

  def update(self, detection: Detection | None, frame_time: float, now: float):
    # Multiple crops of one sign in one frame are not independent observations.
    if not math.isfinite(frame_time) or not self.last_frame_time < frame_time <= now:
      return
    self.last_frame_time = frame_time
    self.expire(now)
    # Compare camera capture times. Inference latency must not shorten the
    # interval in which two independent observations can agree.
    while self.history and frame_time - self.history[0][0] > HISTORY_SECONDS:
      self.history.popleft()
    if detection is None:
      return
    if detection.speed_mph not in VALID_SPEEDS_MPH or not MIN_CONFIDENCE <= detection.confidence <= 1.0:
      return
    if self.speed_mph >= 30 and detection.speed_mph < 30 and detection.confidence < LOW_LIMIT_CONFIDENCE:
      return

    # Require two consecutive matching reads, even for a highly confident crop.
    if self.history and self.history[-1][1].speed_mph != detection.speed_mph:
      self.history.clear()
    self.history.append((frame_time, detection))
    if len(self.history) >= 2:
      self.speed_mph = detection.speed_mph
      self.confidence = min(entry.confidence for _, entry in self.history)
      self.detected_at = frame_time

  def expire(self, now: float):
    if self.speed_mph and not 0 <= now - self.detected_at <= HOLD_SECONDS:
      self.speed_mph = 0
      self.confidence = 0.0
      self.detected_at = 0.0
      self.history.clear()

  def snapshot(self, now: float) -> dict:
    self.expire(now)
    return {
      "speedLimit": self.speed_mph * CV.MPH_TO_MS,
      "confidence": self.confidence,
      "detectedAt": self.detected_at,
      # Republishing a held result cannot extend the camera's freshness window.
      "timestamp": self.last_frame_time,
    }


def read_vision_speed_limit(snapshot, now: float) -> float:
  """Return m/s only while both the detection and its producer are fresh."""
  if not isinstance(snapshot, dict):
    return 0.0
  values = [snapshot.get(key) for key in ("speedLimit", "confidence", "detectedAt", "timestamp")]
  if any(type(value) not in (int, float) or not math.isfinite(value) for value in values):
    return 0.0
  speed, confidence, detected_at, timestamp = values
  if not MIN_CONFIDENCE <= confidence <= 1.0 or not 0 < detected_at <= timestamp <= now:
    return 0.0
  if now - timestamp > HEARTBEAT_TIMEOUT or now - detected_at > HOLD_SECONDS:
    return 0.0
  speed_mph = speed * CV.MS_TO_MPH
  if round(speed_mph) not in VALID_SPEEDS_MPH or abs(speed_mph - round(speed_mph)) > 0.01:
    return 0.0
  return speed
