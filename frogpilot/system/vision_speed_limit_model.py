"""U.S. speed sign inference adapted from StarPilot.

Portions Copyright (c) 2026, firestar5683 and StarPilot contributors.
See frogpilot/assets/vision_models/STARPILOT-LICENSE and README.md.
"""
import hashlib

from pathlib import Path

import cv2
import numpy as np

from openpilot.frogpilot.common.vision_speed_limit import Detection, VALID_SPEEDS_MPH

MODEL_DIR = Path(__file__).resolve().parents[1] / "assets" / "vision_models"
MODEL_HASHES = {
  "speed_limit_us_detector.onnx": "82408b68c79c269296f0af942130c5383cace4ee06c78e2a4690e8488720116a",
  "speed_limit_us_value_classifier.onnx": "07c6696e530eb940d2757d5849b4bc0f1d785cda704e5296e18c0a94959f30a5",
}
DETECTOR_SIZE = 256
CLASSIFIER_SIZE = 128
CLASSIFIER_SPEEDS = (10, 100, 15, 20, 25, 30, 35, 40, 45, 5, 50, 55, 60, 65, 70, 75, 80, 90)
PROPOSAL_CONFIDENCE = 0.06
CLASSIFIER_CONFIDENCE = 0.60
MAX_PROPOSALS = 4
ROI = (0.45, 0.0, 1.0, 0.82)
CROP_EXPANSIONS = ((0.0, 0.0, 0.0, 0.0), (0.10, 0.06, 0.10, 0.12), (0.0, 0.0, 0.18, 0.18))


def letterbox(image, size):
  height, width = image.shape[:2]
  ratio = min(size / height, size / width)
  resized_width, resized_height = round(width * ratio), round(height * ratio)
  left, top = (size - resized_width) // 2, (size - resized_height) // 2
  output = np.full((size, size, 3), 114, dtype=np.uint8)
  output[top:top + resized_height, left:left + resized_width] = cv2.resize(image, (resized_width, resized_height))
  return output, ratio, left, top


def is_regulatory_sign(crop):
  """Reject colored advisory/signage crops before interpreting a number as mph.

  This is StarPilot's white-panel/color filter. It is a heuristic, not proof that
  a sign applies to this lane or that a conditional limit is currently in effect.
  """
  if crop.size == 0:
    return False
  height, width = crop.shape[:2]
  hsv = cv2.cvtColor(crop, cv2.COLOR_BGR2HSV)
  hue, saturation, value = cv2.split(hsv)
  white = (value >= 135) & (saturation <= 70)
  dark = (value <= 115) & (saturation <= 110)
  white_ratio = float(white.mean())
  if white_ratio < 0.08 or float(dark.mean()) < 0.01:
    return False
  color_masks = (
    ((hue >= 12) & (hue <= 45) & (saturation >= 70) & (value >= 85), 0.12, 0.45),
    (((hue <= 12) | (hue >= 168)) & (saturation >= 80) & (value >= 60), 0.10, 0.35),
    ((hue >= 45) & (hue <= 90) & (saturation >= 70) & (value >= 70), 0.35, 0.60),
    ((hue >= 90) & (hue <= 135) & (saturation >= 70) & (value >= 70), 0.35, 0.60),
  )
  if any(float(mask.mean()) > max(minimum, white_ratio * factor) for mask, minimum, factor in color_masks):
    return False
  contours, _ = cv2.findContours(white.astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
  for contour in contours:
    area = cv2.contourArea(contour)
    _, _, box_width, box_height = cv2.boundingRect(contour)
    if (area >= height * width * 0.012 and box_height >= height * 0.2 and box_width >= width * 0.12 and
        0.28 <= box_width / box_height <= 1.25 and area / (box_width * box_height) >= 0.36):
      return True
  return False


class SpeedLimitModel:
  def __init__(self, model_dir=MODEL_DIR):
    networks = []
    for name, expected_hash in MODEL_HASHES.items():
      path = Path(model_dir) / name
      with path.open("rb") as model_file:
        if hashlib.file_digest(model_file, "sha256").hexdigest() != expected_hash:
          raise ValueError(f"Speed limit model checksum mismatch: {name}")
      network = cv2.dnn.readNetFromONNX(str(path))
      network.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
      network.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)
      networks.append(network)
    self.detector, self.classifier = networks

    # Check the pinned model contract before any results can reach the controller.
    self.proposals(np.zeros((480, 960, 3), dtype=np.uint8))
    self.classify(np.zeros((CLASSIFIER_SIZE, CLASSIFIER_SIZE, 3), dtype=np.uint8))

  def proposals(self, frame):
    height, width = frame.shape[:2]
    left, top, right, bottom = (int(size * fraction) for size, fraction in zip((width, height, width, height), ROI, strict=True))
    crop, ratio, pad_x, pad_y = letterbox(frame[top:bottom, left:right], DETECTOR_SIZE)
    blob = cv2.dnn.blobFromImage(crop, 1 / 255.0, (DETECTOR_SIZE, DETECTOR_SIZE), swapRB=True)
    self.detector.setInput(blob)
    predictions = self.detector.forward()
    # The bundled YOLO26 proposal model has ONE class, not three sign classes.
    if predictions.shape != (1, 5, 1344) or not np.isfinite(predictions).all():
      raise ValueError("Invalid speed limit detector output")
    boxes, confidences = [], []
    for center_x, center_y, box_width, box_height, confidence in predictions[0].T:
      if not PROPOSAL_CONFIDENCE <= confidence <= 1.0:
        continue
      x1 = max(int((center_x - box_width / 2 - pad_x) / ratio) + left, left)
      y1 = max(int((center_y - box_height / 2 - pad_y) / ratio) + top, top)
      x2 = min(int((center_x + box_width / 2 - pad_x) / ratio) + left, right)
      y2 = min(int((center_y + box_height / 2 - pad_y) / ratio) + top, bottom)
      box_width, box_height = x2 - x1, y2 - y1
      if box_width < 28 or box_height < 40 or box_width * box_height > width * height * 0.18:
        continue
      boxes.append([x1, y1, box_width, box_height])
      confidences.append(float(confidence))

    # Duplicate anchors must not consume the bounded classifier budget.
    indices = cv2.dnn.NMSBoxes(boxes, confidences, PROPOSAL_CONFIDENCE, 0.45)
    return [(boxes[int(index)], confidences[int(index)]) for index in np.asarray(indices).reshape(-1)[:MAX_PROPOSALS]]

  def classify(self, crop):
    image, _, _, _ = letterbox(crop, CLASSIFIER_SIZE)
    self.classifier.setInput(cv2.dnn.blobFromImage(image, 1 / 255.0, (CLASSIFIER_SIZE, CLASSIFIER_SIZE), swapRB=True))
    scores = self.classifier.forward()
    if scores.shape != (1, 19) or not np.isfinite(scores).all():
      raise ValueError("Invalid speed limit classifier output")
    probabilities = scores[0]
    # This pinned export already includes softmax, including the reject class.
    if np.any(probabilities < 0) or np.any(probabilities > 1) or not 0.99 <= probabilities.sum() <= 1.01:
      raise ValueError("Invalid speed limit classifier probabilities")
    index = int(np.argmax(probabilities))
    if index == len(CLASSIFIER_SPEEDS):
      return None
    speed_mph, confidence = CLASSIFIER_SPEEDS[index], float(probabilities[index])
    minimum = 0.90 if speed_mph in (5, 10, 80) else CLASSIFIER_CONFIDENCE
    if speed_mph not in VALID_SPEEDS_MPH or confidence < minimum:
      return None
    return Detection(speed_mph, confidence)

  def detect(self, frame) -> Detection | None:
    height, width = frame.shape[:2]
    detections = []
    for (x, y, box_width, box_height), proposal_confidence in self.proposals(frame):
      reads = []
      for expand_left, expand_top, expand_right, expand_bottom in CROP_EXPANSIONS:
        x1, y1 = max(int(x - box_width * expand_left), 0), max(int(y - box_height * expand_top), 0)
        x2 = min(int(x + box_width * (1 + expand_right)), width)
        y2 = min(int(y + box_height * (1 + expand_bottom)), height)
        crop = frame[y1:y2, x1:x2]
        if is_regulatory_sign(crop):
          read = self.classify(crop)
          if read is not None:
            reads.append(read)
      if not reads:
        continue
      # Disagreeing crop reads are ambiguous; do not pick whichever scores highest.
      if len({read.speed_mph for read in reads}) != 1:
        continue
      confidence = min(0.95, max(read.confidence for read in reads) * 0.72 + proposal_confidence * 0.24 + (len(reads) - 1) * 0.06)
      detections.append(Detection(reads[0].speed_mph, confidence))
    if not detections or len({detection.speed_mph for detection in detections}) != 1:
      return None
    return max(detections, key=lambda detection: detection.confidence)
