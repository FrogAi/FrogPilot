"""Verify sign text with PaddleOCR's English recognition model.

See frogpilot/assets/vision_models/README.md and PADDLEOCR-LICENSE for provenance.
Text agreement does not establish lane or conditional-sign applicability.
"""
import cv2
import numpy as np

from openpilot.frogpilot.common.vision_speed_limit import VALID_SPEEDS_MPH

HEADER_CONFIDENCE = 0.85
NUMBER_CONFIDENCE = 0.85
TEXT_INPUT_SIZE = (160, 48)
TEXT_OUTPUT_SHAPE = (1, 20, 438)
# Indices in the pinned en_PP-OCRv5_mobile_rec dictionary, including CTC blank 0.
# Other tokens reject the heading. Numeric verification uses a separate whitelist.
HEADER_TOKENS = {
  14: "D", 15: "E", 19: "I", 22: "L", 23: "M", 26: "P", 29: "S", 30: "T",
  40: "D", 41: "E", 45: "I", 48: "L", 49: "M", 52: "P", 55: "S", 56: "T", 437: " ",
}
# Try the tighter text alignment first; the broader crop remains a fallback.
# Either alignment accepts the same exact heading, so ordering changes cost only.
HEADER_REGIONS = (((0.12, 0.34), (0.32, 0.56)), ((0.04, 0.30), (0.28, 0.52)))
NUMBER_REGIONS = ((0.45, 1.0), (0.50, 0.95))
NUMBER_TOKENS = {index + 1: str(index) for index in range(10)}


def decode_text(scores, tokens):
  if scores.shape != TEXT_OUTPUT_SHAPE or not np.isfinite(scores).all():
    raise ValueError("Invalid speed limit text output")
  probabilities = scores[0]
  if (np.any(probabilities < 0) or np.any(probabilities > 1.00001) or
      not np.allclose(probabilities.sum(axis=1), 1.0, atol=0.001)):
    raise ValueError("Invalid speed limit text probabilities")
  text, confidence = [], []
  previous = -1
  for row in probabilities:
    index = int(np.argmax(row))
    if index and index != previous:
      if index not in tokens:
        return "", []
      text.append(tokens[index])
      confidence.append(float(row[index]))
    previous = index
  return "".join(text), confidence


def is_speed_limit_heading(scores):
  text, confidence = decode_text(scores, HEADER_TOKENS)
  return bool(text.replace(" ", "") == "SPEEDLIMIT" and np.mean(confidence) >= HEADER_CONFIDENCE)


def read_speed_limit_number(scores):
  text, confidence = decode_text(scores, NUMBER_TOKENS)
  if not confidence or min(confidence) < NUMBER_CONFIDENCE or text not in {str(speed) for speed in VALID_SPEEDS_MPH}:
    return None
  return int(text)


class SpeedLimitTextVerifier:
  def __init__(self, network):
    self.network = network
    self.read(np.zeros((48, 160, 3), dtype=np.uint8))

  def read(self, image):
    # Heading rows use a full-width BGR input; digits preserve their aspect ratio.
    self.network.setInput(cv2.dnn.blobFromImage(image, 1 / 127.5, TEXT_INPUT_SIZE, mean=(127.5,) * 3))
    return is_speed_limit_heading(self.network.forward())

  def has_heading(self, crop):
    height, width = crop.shape[:2]
    if height < 40 or width < 28:
      return False
    # The recognizer reads one text line. Place the two heading rows side by side
    # to check both words in one bounded inference, with one alternate alignment.
    for regions in HEADER_REGIONS:
      strips = [cv2.resize(crop[int(height * top):int(height * bottom), int(width * 0.05):int(width * 0.95)], (80, 48))
                for top, bottom in regions]
      if self.read(np.concatenate(strips, axis=1)):
        return True
    return False

  def matches_value(self, crop, speed_mph):
    height, width = crop.shape[:2]
    if height < 40 or width < 28:
      return False
    # Stretching two digits across the full text-line input distorts their shape.
    # Preserve aspect ratio and right-pad after normalization, as PaddleOCR does.
    for top, bottom in NUMBER_REGIONS:
      digits = crop[int(height * top):int(height * bottom), int(width * 0.05):int(width * 0.95)]
      resized_width = min(160, int(np.ceil(48 * digits.shape[1] / digits.shape[0])))
      image = cv2.resize(digits, (resized_width, 48)).astype(np.float32) / 127.5 - 1
      blob = np.zeros((1, 3, 48, 160), dtype=np.float32)
      blob[0, :, :, :resized_width] = image.transpose(2, 0, 1)
      self.network.setInput(blob)
      value = read_speed_limit_number(self.network.forward())
      if value is not None:
        # A confident disagreement cannot be overturned by another crop.
        return value == speed_mph
    return False
