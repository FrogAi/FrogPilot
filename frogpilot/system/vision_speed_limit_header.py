"""Check a rejected sign's heading with PaddleOCR's English recognition model.

See frogpilot/assets/vision_models/README.md and PADDLEOCR-LICENSE for provenance.
This checks the words SPEED LIMIT, not lane or conditional-sign applicability.
"""
import cv2
import numpy as np

HEADER_CONFIDENCE = 0.85
HEADER_INPUT_SIZE = (160, 48)
HEADER_OUTPUT_SHAPE = (1, 20, 438)
# Indices in the pinned en_PP-OCRv5_mobile_rec dictionary, including CTC blank 0.
# All other emitted tokens reject the heading; numbers never become speed reads.
HEADER_TOKENS = {
  14: "D", 15: "E", 19: "I", 22: "L", 23: "M", 26: "P", 29: "S", 30: "T",
  40: "D", 41: "E", 45: "I", 48: "L", 49: "M", 52: "P", 55: "S", 56: "T", 437: " ",
}
# Try the tighter text alignment first; the broader crop remains a fallback.
# Either alignment accepts the same exact heading, so ordering changes cost only.
HEADER_REGIONS = (((0.12, 0.34), (0.32, 0.56)), ((0.04, 0.30), (0.28, 0.52)))


def is_speed_limit_heading(scores):
  if scores.shape != HEADER_OUTPUT_SHAPE or not np.isfinite(scores).all():
    raise ValueError("Invalid speed limit header output")
  probabilities = scores[0]
  if (np.any(probabilities < 0) or np.any(probabilities > 1.00001) or
      not np.allclose(probabilities.sum(axis=1), 1.0, atol=0.001)):
    raise ValueError("Invalid speed limit header probabilities")
  text, confidence = [], []
  previous = -1
  for row in probabilities:
    index = int(np.argmax(row))
    if index and index != previous:
      if index not in HEADER_TOKENS:
        return False
      text.append(HEADER_TOKENS[index])
      confidence.append(float(row[index]))
    previous = index
  return bool("".join(text).replace(" ", "") == "SPEEDLIMIT" and np.mean(confidence) >= HEADER_CONFIDENCE)


class SpeedLimitHeader:
  def __init__(self, network):
    self.network = network
    self.read(np.zeros((48, 160, 3), dtype=np.uint8))

  def read(self, image):
    # PaddleOCR's BGR input normalization. Only the heading model sees this image.
    self.network.setInput(cv2.dnn.blobFromImage(image, 1 / 127.5, HEADER_INPUT_SIZE, mean=(127.5,) * 3))
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
