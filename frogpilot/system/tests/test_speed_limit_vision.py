import tempfile

from pathlib import Path
from types import SimpleNamespace

import cv2
import numpy as np
import pytest

from cereal import messaging
from openpilot.frogpilot.common.tests.vision_helpers import MemoryParams
from openpilot.common.constants import CV
from openpilot.frogpilot.common.vision_speed_limit import (
  HEARTBEAT_TIMEOUT,
  HOLD_SECONDS,
  VISION_SPEED_LIMIT_PARAM,
  Detection,
  SpeedLimitConfirmation,
  read_vision_speed_limit,
)
from openpilot.frogpilot.system.speed_limit_vision import INPUT_MAX_AGE, SpeedLimitVisionDaemon, decode_nv12, inference_interval, inputs_valid
from openpilot.frogpilot.system.vision_speed_limit_model import CLASSIFIER_SIZE, SpeedLimitModel, is_regulatory_sign
from openpilot.frogpilot.system.vision_speed_limit_text import TEXT_OUTPUT_SHAPE, SpeedLimitTextVerifier, is_speed_limit_heading, read_speed_limit_number


class TestConfirmation:
  @pytest.fixture(autouse=True)
  def setup(self, mocker):
    self.mocker = mocker
    self.state = SpeedLimitConfirmation()

  def confirm(self, speed=55, now=10, confidence=0.95):
    self.state.update(Detection(speed, confidence), now, now)
    self.state.update(Detection(speed, confidence), now + 0.2, now + 0.2)

  def test_two_independent_frames_required(self):
    self.state.update(Detection(55, 0.99), 10, 10)
    self.state.update(Detection(55, 0.99), 10, 10.1)
    assert self.state.speed_mph == 0
    self.state.update(Detection(55, 0.9), 10.2, 10.2)
    assert self.state.speed_mph == 55
    assert self.state.confidence == pytest.approx(0.9)

  def test_disagreeing_reads_restart_confirmation(self):
    for now, speed in ((10, 55), (10.1, 35), (10.2, 55)):
      self.state.update(Detection(speed, 0.99), now, now)
    assert self.state.speed_mph == 0

  def test_old_history_cannot_confirm(self):
    self.state.update(Detection(55, 0.99), 10, 10)
    self.state.update(Detection(55, 0.99), 13, 13)
    assert self.state.speed_mph == 0

  def test_confirmation_window_uses_capture_times_with_slow_inference(self):
    self.state.update(Detection(40, 0.95), 10.0, 10.7)
    self.state.update(Detection(40, 0.95), 11.75, 12.45)
    assert self.state.speed_mph == 40
    assert self.state.detected_at == 11.75
    assert read_vision_speed_limit(self.state.snapshot(12.45), 12.45) == pytest.approx(40 * CV.MPH_TO_MS)

  def test_slow_inference_does_not_extend_capture_history(self):
    self.state.update(Detection(40, 0.95), 10.0, 10.7)
    self.state.update(Detection(40, 0.95), 12.01, 12.71)
    assert self.state.speed_mph == 0

  def test_invalid_detections_and_timestamps(self):
    for detection in (Detection(90, 0.99), Detection(52, 0.99), Detection(55, float('nan')), Detection(55, 1.1), Detection(55, 0.69)):
      self.state.update(detection, 10, 10)
      self.state.update(detection, 10.1, 10.1)
      assert self.state.speed_mph == 0
      self.state.reset()
    for timestamp in (float('nan'), float('inf'), -1, 11):
      self.state.update(Detection(55, 0.99), timestamp, 10)
      assert self.state.speed_mph == 0

  def test_low_limit_change_requires_stronger_reads(self):
    self.confirm()
    self.confirm(25, 11, 0.89)
    assert self.state.speed_mph == 55
    self.confirm(25, 12, 0.95)
    assert self.state.speed_mph == 25

  def test_hold_does_not_refresh_detection_age(self):
    self.confirm()
    detection_time = self.state.detected_at
    self.state.update(None, 20, 20)  # Live camera, with no new sign observation.
    assert read_vision_speed_limit(self.state.snapshot(20), 20) > 0
    assert self.state.detected_at == detection_time
    assert self.state.snapshot(11 + HOLD_SECONDS)['speedLimit'] == 0

  def test_mph_is_independent_of_display_units(self):
    self.confirm(50)
    snapshot = self.state.snapshot(11)
    assert read_vision_speed_limit(snapshot, 11) == pytest.approx(50 * CV.MPH_TO_MS)
    assert snapshot['speedLimit'] * CV.MS_TO_KPH == pytest.approx(80.4672)

  def test_dead_producer_expires_held_limit(self):
    self.confirm()
    snapshot = self.state.snapshot(11)
    assert read_vision_speed_limit(snapshot, 11 + HEARTBEAT_TIMEOUT + 0.01) == 0

  def test_republishing_does_not_extend_camera_freshness(self):
    self.confirm()
    last_frame = self.state.last_frame_time
    snapshot = self.state.snapshot(last_frame + HEARTBEAT_TIMEOUT - 0.1)
    assert read_vision_speed_limit(snapshot, last_frame + HEARTBEAT_TIMEOUT + 0.1) == 0

  def test_malformed_snapshots_fail_closed(self):
    self.confirm()
    original = self.state.snapshot(11)
    for value in (None, [], '55', {}, {**original, 'timestamp': 12}, {**original, 'detectedAt': 12}):
      assert read_vision_speed_limit(value, 11) == 0
    for field in original:
      for value in (None, True, '1', float('nan'), float('inf'), -1):
        assert read_vision_speed_limit({**original, field: value}, 11) == 0


class TestModel:
  @pytest.fixture(autouse=True)
  def setup(self, mocker):
    self.mocker = mocker

  @classmethod
  def setup_class(cls):
    cv2.setNumThreads(1)

  def test_real_models_load_and_reject_blank_frame(self):
    model = SpeedLimitModel()
    assert model.detect(np.zeros((1208, 1928, 3), dtype=np.uint8)) is None

  @pytest.mark.parametrize('number', [140, 146])
  def test_readable_school_limit_is_not_an_unconditional_limit(self, number):
    model = SpeedLimitModel()
    folder = Path(__file__).parent / 'fixtures' / 'vision_speed_limit'
    frame = cv2.imread(str(folder / f'sign_20_frame_{number}.png'))
    assert frame is not None
    assert any(model.text.matches_value(frame[y:y + height, x:x + width], 20)
               for (x, y, width, height), _ in model.proposals(frame))
    assert model.detect(frame) is None

  def test_duplicate_detector_boxes_do_not_multiply_work(self):
    model = SpeedLimitModel.__new__(SpeedLimitModel)
    model.detector = self.mocker.Mock()
    predictions = np.zeros((1, 5, 1344), dtype=np.float32)
    predictions[0, :, :10] = np.array([128, 128, 50, 80, 0.9])[:, None]
    model.detector.forward.return_value = predictions
    assert len(model.proposals(np.zeros((480, 960, 3), dtype=np.uint8))) == 1
    model.detector.forward.return_value = np.zeros((1, 7, 1344), dtype=np.float32)
    with pytest.raises(ValueError):
      model.proposals(np.zeros((480, 960, 3), dtype=np.uint8))

  def test_models_are_required_and_verified(self):
    with tempfile.TemporaryDirectory() as folder:
      with pytest.raises(FileNotFoundError):
        SpeedLimitModel(folder)
      (Path(folder) / 'speed_limit_us_detector.onnx').write_bytes(b'invalid model')
      with pytest.raises(ValueError, match='checksum'):
        SpeedLimitModel(folder)

  def test_classifier_contract_and_reject_class(self):
    model = SpeedLimitModel.__new__(SpeedLimitModel)
    model.classifier = self.mocker.Mock()
    crop = np.zeros((CLASSIFIER_SIZE, CLASSIFIER_SIZE, 3), np.uint8)
    for index in range(19):
      scores = np.zeros((1, 19), np.float32)
      scores[0, index] = 1
      model.classifier.forward.return_value = scores
      result = model.classify(crop)
      if index in (1, 17, 18):
        assert result is None
      else:
        assert result is not None
    for scores in (np.zeros((1, 18)), np.full((1, 19), np.nan), np.zeros((1, 19))):
      model.classifier.forward.return_value = scores
      with pytest.raises(ValueError):
        model.classify(crop)

  @pytest.mark.parametrize('brightness', [1.0, 0.65, 0.45, 0.25])
  @pytest.mark.parametrize('background', [(230, 230, 230), (230, 210, 190)])
  def test_regulatory_sign_in_sun_and_shadow(self, brightness, background):
    regulatory = np.full((100, 80, 3), background, np.uint8)
    cv2.putText(regulatory, '55', (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 0), 3)
    assert is_regulatory_sign((regulatory * brightness).astype(np.uint8))

  @pytest.mark.parametrize('max_white_saturation', [70, 110])
  @pytest.mark.parametrize('brightness', [1.0, 0.45, 0.25])
  @pytest.mark.parametrize('background', [(0, 220, 255), (0, 130, 255), (0, 0, 230), (0, 200, 0), (220, 70, 20)])
  @pytest.mark.parametrize('foreground', [0, 230])
  def test_colored_sign_is_rejected_in_sun_and_shadow(self, brightness, background, foreground, max_white_saturation):
    colored = np.full((100, 80, 3), background, np.uint8)
    cv2.putText(colored, '35', (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (foreground,) * 3, 3)
    assert not is_regulatory_sign((colored * brightness).astype(np.uint8), max_white_saturation)

  @pytest.mark.parametrize('brightness', [0, 40, 100, 230])
  def test_featureless_crop_is_rejected(self, brightness):
    assert not is_regulatory_sign(np.full((100, 80, 3), brightness, np.uint8))

  def test_near_black_crop_is_not_amplified_into_a_sign(self):
    crop = np.full((100, 80, 3), 20, np.uint8)
    cv2.putText(crop, '35', (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 0), 3)
    assert not is_regulatory_sign(crop)

  def test_moderate_warm_tint_requires_the_stronger_filter_mode(self):
    crop = np.full((100, 80, 3), (140, 190, 220), np.uint8)
    cv2.putText(crop, '40', (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 0), 3)
    assert not is_regulatory_sign(crop)
    assert is_regulatory_sign(crop, max_white_saturation=110)

  @pytest.mark.parametrize('proposal_confidence', [0.59, 0.60, 0.90])
  @pytest.mark.parametrize('classifier_confidence', [0.94, 0.95, 0.99])
  def test_tinted_panel_requires_both_stronger_model_thresholds(self, proposal_confidence, classifier_confidence):
    model = SpeedLimitModel.__new__(SpeedLimitModel)
    model.text = self.mocker.Mock()
    model.text.matches_value.return_value = True
    model.proposals = self.mocker.Mock(return_value=[([300, 50, 80, 100], proposal_confidence)])
    model.classify = self.mocker.Mock(return_value=Detection(40, classifier_confidence))
    self.mocker.patch('openpilot.frogpilot.system.vision_speed_limit_model.is_regulatory_sign',
                      side_effect=lambda crop, max_white_saturation=70: max_white_saturation == 110)
    result = model.detect(np.full((480, 960, 3), 230, np.uint8))
    assert (result is not None) == (proposal_confidence >= 0.60 and classifier_confidence >= 0.95)

  def test_conflicting_signs_are_not_selected(self):
    model = SpeedLimitModel.__new__(SpeedLimitModel)
    model.text = self.mocker.Mock()
    model.text.matches_value.return_value = True
    model.proposals = self.mocker.Mock(return_value=[([300, 50, 80, 100], 0.9), ([450, 50, 80, 100], 0.9)])
    model.classify = self.mocker.Mock(side_effect=[Detection(55, 0.99)] * 3 + [Detection(35, 0.99)] * 3)
    self.mocker.patch('openpilot.frogpilot.system.vision_speed_limit_model.is_regulatory_sign', return_value=True)
    assert model.detect(np.full((480, 960, 3), 230, np.uint8)) is None

  @pytest.mark.parametrize('heading', [False, True])
  @pytest.mark.parametrize('proposal_confidence', [0.59, 0.60])
  @pytest.mark.parametrize('classifier_confidence', [0.94, 0.95])
  def test_glare_requires_a_heading_and_two_strong_model_scores(self, heading, proposal_confidence, classifier_confidence):
    model = SpeedLimitModel.__new__(SpeedLimitModel)
    model.proposals = self.mocker.Mock(return_value=[([300, 50, 80, 100], proposal_confidence)])
    model.classify = self.mocker.Mock(return_value=Detection(30, classifier_confidence))
    model.text = self.mocker.Mock()
    model.text.has_heading.return_value = heading
    model.text.matches_value.return_value = True
    self.mocker.patch('openpilot.frogpilot.system.vision_speed_limit_model.is_regulatory_sign', return_value=False)
    result = model.detect(np.full((480, 960, 3), 230, np.uint8))
    assert (result is not None) == (heading and proposal_confidence >= 0.60 and classifier_confidence >= 0.95)
    assert model.text.has_heading.call_count == int(proposal_confidence >= 0.60 and classifier_confidence >= 0.95)
    assert model.needs_followup == (proposal_confidence >= 0.60 and classifier_confidence >= 0.95)

  @pytest.mark.parametrize('number_matches', [False, True])
  def test_white_panel_requires_independent_number_agreement(self, number_matches):
    model = SpeedLimitModel.__new__(SpeedLimitModel)
    model.proposals = self.mocker.Mock(return_value=[([300, 50, 80, 100], 0.9)])
    model.classify = self.mocker.Mock(return_value=Detection(70, 0.99))
    model.text = self.mocker.Mock()
    model.text.matches_value.return_value = number_matches
    self.mocker.patch('openpilot.frogpilot.system.vision_speed_limit_model.is_regulatory_sign', return_value=True)
    state = SpeedLimitConfirmation()
    state.update(Detection(30, 0.99), 99.0, 99.0)
    state.update(Detection(30, 0.99), 99.2, 99.2)
    for now in (100.0, 100.7):
      result = model.detect(np.full((480, 960, 3), 230, np.uint8))
      state.update(result, now, now)
    assert state.speed_mph == (70 if number_matches else 30)
    assert model.text.matches_value.call_count == 2
    assert all(call.args[1] == 70 for call in model.text.matches_value.call_args_list)
    model.text.has_heading.assert_not_called()
    assert model.needs_followup is not number_matches

  def test_repeated_crops_cannot_promote_a_weak_classification(self):
    model = SpeedLimitModel.__new__(SpeedLimitModel)
    model.proposals = self.mocker.Mock(return_value=[([300, 50, 80, 100], 0.99)])
    model.classify = self.mocker.Mock(return_value=Detection(70, 0.65))
    model.text = self.mocker.Mock()
    model.text.matches_value.return_value = True
    self.mocker.patch('openpilot.frogpilot.system.vision_speed_limit_model.is_regulatory_sign', return_value=True)
    state = SpeedLimitConfirmation()
    for now in (100.0, 100.2):
      state.update(model.detect(np.full((480, 960, 3), 230, np.uint8)), now, now)
    assert state.speed_mph == 0

  @pytest.mark.parametrize('background', [(230, 230, 230), (140, 190, 220)])
  def test_yellow_conditional_header_cannot_be_diluted_by_crop_expansion(self, background):
    frame = np.full((480, 960, 3), 230, np.uint8)
    frame[50:150, 300:380] = background
    frame[50:70, 300:380] = (0, 220, 230)
    model = SpeedLimitModel.__new__(SpeedLimitModel)
    model.proposals = self.mocker.Mock(return_value=[([300, 50, 80, 100], 0.99)])
    model.classify = self.mocker.Mock(return_value=Detection(20, 0.99))
    model.text = self.mocker.Mock()
    model.text.matches_value.return_value = True
    assert model.detect(frame) is None
    model.classify.assert_not_called()


class TestTextVerifier:
  @pytest.mark.parametrize('words, expected', [
    (('SPEED', 'LIMIT'), True), (('ROUTE', '50'), False),
    (('SPEED', 'BUMP'), False), (('WEIGHT', 'LIMIT'), False),
  ])
  def test_real_model_heading_preprocessing_and_dictionary(self, words, expected):
    # A synthetic panel tests the shipped graph, BGR normalization, row crops,
    # and pinned dictionary together. It is not evidence of road-sign accuracy.
    model = SpeedLimitModel()
    panel = np.full((200, 120, 3), 255, dtype=np.uint8)
    for word, baseline in zip(words, (45, 88), strict=True):
      width = cv2.getTextSize(word, cv2.FONT_HERSHEY_SIMPLEX, 0.65, 2)[0][0]
      cv2.putText(panel, word, ((120 - width) // 2, baseline), cv2.FONT_HERSHEY_SIMPLEX,
                  0.65, (0, 0, 0), 2, cv2.LINE_AA)
    assert model.text.has_heading(panel) is expected

  @staticmethod
  def scores(text='', confidence=0.99):
    tokens = {' ': 437, **{chr(ord('A') + index): 11 + index for index in range(26)},
              **{str(index): index + 1 for index in range(10)}}
    sequence = []
    previous = None
    for character in text:
      token = tokens[character]
      if token == previous:
        sequence.append(0)
      sequence.append(token)
      previous = token
    output = np.zeros(TEXT_OUTPUT_SHAPE, dtype=np.float32)
    output[:, :, 0] = 1.0
    for step, token in enumerate(sequence):
      if token:
        output[0, step, 0] = 1 - confidence
        output[0, step, token] = confidence
    return output

  @pytest.mark.parametrize('text', ['SPEEDLIMIT', 'SPEED LIMIT', ' SPEED LIMIT '])
  def test_exact_heading_with_ctc_repeats(self, text):
    assert is_speed_limit_heading(self.scores(text))

  @pytest.mark.parametrize('text', ['', 'EAST', 'JCT', 'SPEED', 'LIMIT', 'RAMP SPEED', 'SPED LIMIT', 'SPEED LIM1T'])
  def test_other_words_and_partial_headings_are_rejected(self, text):
    assert not is_speed_limit_heading(self.scores(text))

  def test_uncertain_heading_is_rejected(self):
    assert not is_speed_limit_heading(self.scores('SPEED LIMIT', confidence=0.8))

  @pytest.mark.parametrize('scores', [np.zeros((1, 10, 438)), np.full(TEXT_OUTPUT_SHAPE, np.nan), np.zeros(TEXT_OUTPUT_SHAPE)])
  def test_invalid_model_outputs(self, scores):
    with pytest.raises(ValueError):
      is_speed_limit_heading(scores)

  def test_heading_work_is_bounded(self, mocker):
    network = mocker.Mock()
    network.forward.return_value = self.scores()
    header = SpeedLimitTextVerifier(network)
    network.reset_mock()
    assert not header.has_heading(np.zeros((100, 80, 3), dtype=np.uint8))
    assert network.forward.call_count == 2
    network.reset_mock()
    assert not header.has_heading(np.zeros((0, 0, 3), dtype=np.uint8))
    network.forward.assert_not_called()

  @pytest.mark.parametrize('speed', range(5, 85, 5))
  def test_number_dictionary_and_ctc_repeats(self, speed):
    assert read_speed_limit_number(self.scores(str(speed))) == speed

  @pytest.mark.parametrize('text', ['', '0', '03', '90', '100', '30 MPH', '3O', '30 70'])
  def test_ambiguous_numbers_are_rejected(self, text):
    assert read_speed_limit_number(self.scores(text)) is None

  def test_each_digit_requires_confidence(self):
    scores = self.scores('70')
    scores[0, 0] = 0
    scores[0, 0, 8] = 0.8
    scores[0, 0, 0] = 0.2
    assert read_speed_limit_number(scores) is None

  @pytest.mark.parametrize('scores', [np.zeros((1, 10, 438)), np.full(TEXT_OUTPUT_SHAPE, np.nan), np.zeros(TEXT_OUTPUT_SHAPE)])
  def test_invalid_number_outputs(self, scores):
    with pytest.raises(ValueError):
      read_speed_limit_number(scores)

  def test_number_mismatch_cannot_be_overturned(self, mocker):
    network = mocker.Mock()
    network.forward.return_value = self.scores()
    header = SpeedLimitTextVerifier(network)
    network.reset_mock()
    network.forward.side_effect = [self.scores('30'), self.scores('70')]
    assert not header.matches_value(np.zeros((100, 80, 3), dtype=np.uint8), 70)
    assert network.forward.call_count == 1

  def test_number_work_is_bounded_and_preserves_aspect(self, mocker):
    network = mocker.Mock()
    network.forward.return_value = self.scores()
    header = SpeedLimitTextVerifier(network)
    network.reset_mock()
    assert not header.matches_value(np.zeros((100, 80, 3), dtype=np.uint8), 70)
    assert network.forward.call_count == 2
    blob = network.setInput.call_args_list[0].args[0]
    assert blob.shape == (1, 3, 48, 160)
    assert np.all(blob[0, :, :, :63] == -1)
    assert np.all(blob[0, :, :, 63:] == 0)
    network.reset_mock()
    assert not header.matches_value(np.zeros((0, 0, 3), dtype=np.uint8), 70)
    network.forward.assert_not_called()

  @pytest.mark.parametrize('speed', [20, 30, 40, 70])
  def test_real_model_number_verification(self, speed):
    # Exercises the existing graph and digit preprocessing, not road accuracy.
    model = SpeedLimitModel()
    panel = np.full((200, 120, 3), 255, dtype=np.uint8)
    text = str(speed)
    width = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, 2.0, 3)[0][0]
    cv2.putText(panel, text, ((120 - width) // 2, 170), cv2.FONT_HERSHEY_SIMPLEX,
                2.0, (0, 0, 0), 3, cv2.LINE_AA)
    assert model.text.matches_value(panel, speed)
    assert not model.text.matches_value(panel, 30 if speed != 30 else 70)


class FakeSubMaster(dict):
  def __init__(self):
    super().__init__(
      deviceState=SimpleNamespace(started=True, thermalStatus=0, memoryUsagePercent=30, cpuUsagePercent=[10] * 8),
      frogpilotCarState=SimpleNamespace(drivingGear=True),
      mapdOut=SimpleNamespace(roadName='Main Street', tileLoaded=True, wayId=1, isForward=True, locationMonoTime=10_000_000_000),
    )
    self.valid = dict.fromkeys(self, True)
    self.alive = dict.fromkeys(self, True)
    self.logMonoTime = dict.fromkeys(self, 0)

  def update(self, timeout):
    pass

  def all_alive(self, services):
    return all(self.alive[key] for key in services)

  def all_valid(self, services):
    return all(self.valid[key] for key in services)


class TestRuntime:
  @pytest.fixture(autouse=True)
  def setup(self, mocker):
    self.mocker = mocker
    self.params = MemoryParams()
    self.sm = FakeSubMaster()
    self.camera = self.mocker.Mock()
    self.camera_type = self.mocker.Mock(return_value=self.camera)
    self.camera_type.available_streams.return_value = [0]
    self.stream_type = SimpleNamespace(VISION_STREAM_ROAD=0, VISION_STREAM_WIDE_ROAD=1)
    self.camera.width, self.camera.height, self.camera.stride, self.camera.uv_offset = (4, 4, 8, 40)
    self.camera.recv.return_value = SimpleNamespace(data=bytes([128] * 56))
    self.camera.is_connected.return_value = True
    self.camera.valid = True
    self.model = self.mocker.Mock()
    self.model.detect.return_value = Detection(55, 0.95)
    self.model.needs_followup = False
    self.daemon = SpeedLimitVisionDaemon(self.params, self.sm, self.camera_type, self.stream_type, self.mocker.Mock(), lambda: self.model)

  def step(self, now):
    self.camera.timestamp_eof = int(now * 1000000000.0)
    self.sm.logMonoTime = dict.fromkeys(self.sm, int(now * 1e9))
    self.mocker.patch('openpilot.frogpilot.system.speed_limit_vision.time.monotonic', return_value=now)
    self.daemon.step()

  def confirm(self):
    self.step(10)
    self.step(10.2)
    assert read_vision_speed_limit(self.params.get(VISION_SPEED_LIMIT_PARAM), 10.2) > 0

  def test_padded_camera_layout(self):
    padded = np.full(56, 128, dtype=np.uint8)
    padded[32:40] = 0
    image = decode_nv12(padded, 4, 4, 8, 40)
    expected = cv2.cvtColor(np.full((6, 4), 128, np.uint8), cv2.COLOR_YUV2BGR_NV12)
    np.testing.assert_array_equal(image, expected)
    with pytest.raises(ValueError):
      decode_nv12(padded[:40], 4, 4, 8, 40)

  def test_end_to_end_frames_publish_meters_per_second(self):
    self.confirm()
    assert self.params.get(VISION_SPEED_LIMIT_PARAM)['speedLimit'] == pytest.approx(55 * CV.MPH_TO_MS)

  def test_confirmation_with_real_frogpilot_logger(self, caplog):
    import logging
    from openpilot.common.logging_extra import SwagLogger

    logger = SwagLogger()
    logger.setLevel(logging.INFO)
    logger.addHandler(caplog.handler)
    self.daemon.logger = logger
    self.confirm()
    self.step(10.4)
    assert self.daemon.status == 'Tracking'
    assert read_vision_speed_limit(self.params.get(VISION_SPEED_LIMIT_PARAM), 10.4) == pytest.approx(55 * CV.MPH_TO_MS)
    assert len(caplog.records) == 1
    assert caplog.records[0].msg['event'] == 'Vision speed limit confirmed'
    assert caplog.records[0].msg['speed_mph'] == 55

  def test_unpopulated_vipc_valid_flag_does_not_reject_camera_frames(self):
    # camera_common.cc does not populate VisionIpcBufExtra.valid on this branch.
    self.camera.valid = False
    self.confirm()

  def test_offroad_parking_and_pressure_clear_results(self):
    for field, value in (('started', False), ('thermalStatus', 2), ('memoryUsagePercent', 94)):
      self.confirm()
      previous = getattr(self.sm['deviceState'], field)
      setattr(self.sm['deviceState'], field, value)
      self.step(11)
      assert self.params.get(VISION_SPEED_LIMIT_PARAM) is None
      assert self.daemon.client is None
      setattr(self.sm['deviceState'], field, previous)
      self.daemon.last_inference_at = 0
    self.confirm()
    self.sm['frogpilotCarState'].drivingGear = False
    self.step(11)
    assert self.params.get(VISION_SPEED_LIMIT_PARAM) is None

  def test_camera_disappearance_clears_result(self):
    self.confirm()
    self.camera_type.available_streams.return_value = []
    self.step(11)
    assert self.params.get(VISION_SPEED_LIMIT_PARAM) is None

  def test_duplicate_frame_cannot_keep_source_alive(self):
    self.confirm()
    self.daemon.last_inference_at = 0
    self.sm.logMonoTime = dict.fromkeys(self.sm, int(14 * 1e9))
    self.mocker.patch('openpilot.frogpilot.system.speed_limit_vision.time.monotonic', return_value=14)
    self.daemon.step()
    assert self.params.get(VISION_SPEED_LIMIT_PARAM) is None

  def test_worker_stopping_after_camera_stall_does_not_extend_deadline(self):
    self.confirm()
    self.camera.recv.return_value = None
    self.step(11.2)
    self.step(11.23)  # Republish between inference attempts, without a new frame.
    snapshot = self.params.get(VISION_SPEED_LIMIT_PARAM)
    assert read_vision_speed_limit(snapshot, 10.2 + HEARTBEAT_TIMEOUT + 0.1) == 0

  @pytest.mark.parametrize('field,value', [('wayId', 2), ('isForward', False)])
  def test_matched_road_change_requires_new_confirmation(self, field, value):
    self.confirm()
    # Names may be identical or absent on distinct roads.
    setattr(self.sm['mapdOut'], field, value)
    self.step(11)
    assert self.params.get(VISION_SPEED_LIMIT_PARAM)['speedLimit'] == 0

  @pytest.mark.parametrize('field,value', [('tileLoaded', False), ('wayId', 0), ('locationMonoTime', 1),
                                         ('locationMonoTime', 12_000_000_000)])
  def test_unusable_map_match_does_not_invent_a_road_change(self, field, value):
    self.confirm()
    self.sm['mapdOut'].wayId = 2
    setattr(self.sm['mapdOut'], field, value)
    self.model.detect.return_value = None
    self.step(11)
    assert self.params.get(VISION_SPEED_LIMIT_PARAM)['speedLimit'] > 0

  def test_name_change_on_same_matched_road_preserves_confirmation(self):
    self.confirm()
    self.sm['mapdOut'].roadName = ''
    self.model.detect.return_value = None
    self.step(11)
    assert self.params.get(VISION_SPEED_LIMIT_PARAM)['speedLimit'] > 0

  def test_missing_model_retries_slowly(self):
    self.daemon.model_factory = self.mocker.Mock(side_effect=FileNotFoundError())
    self.step(10)
    self.step(11)
    assert self.daemon.model_factory.call_count == 1
    assert self.params.get(VISION_SPEED_LIMIT_PARAM) is None
    self.step(41)
    assert self.daemon.model_factory.call_count == 2

  def test_cpu_and_processing_backoff(self):
    state = self.sm['deviceState']
    assert inference_interval(state, 0) == pytest.approx(1 / 6)
    assert inference_interval(state, 1) == pytest.approx(1.5)
    assert inference_interval(state, 0.4, followup=True) == pytest.approx(0.4)
    state.cpuUsagePercent = [95] * 8
    assert inference_interval(state, 0) >= 1.5
    assert inference_interval(state, 0, followup=True) >= 1.5

  def test_slow_model_gets_a_prompt_independent_followup_frame(self):
    for started_at in (10.0, 10.35):
      self.camera.timestamp_eof = int(started_at * 1e9)
      self.sm.logMonoTime = dict.fromkeys(self.sm, int(started_at * 1e9))
      self.mocker.patch('openpilot.frogpilot.system.speed_limit_vision.time.monotonic',
                        side_effect=[started_at, started_at, started_at + 0.3])
      self.daemon.step()
    assert self.model.detect.call_count == 2
    assert read_vision_speed_limit(self.params.get(VISION_SPEED_LIMIT_PARAM), 10.65) == pytest.approx(55 * CV.MPH_TO_MS)

  def test_followup_ends_when_a_sign_is_confirmed(self):
    self.step(10)
    assert self.daemon.followup_until == 12
    self.step(10.2)
    assert self.daemon.followup_until == 0
    self.daemon.logger.event.assert_called_once()
    self.step(10.4)
    self.daemon.logger.event.assert_called_once()

  def test_followup_window_is_not_extended_by_unconfirmed_reads(self):
    self.step(10)
    self.model.detect.return_value = Detection(40, 0.95)
    self.step(10.2)
    assert self.daemon.followup_until == 12
    self.model.detect.return_value = Detection(30, 0.95)
    self.step(10.4)
    assert self.daemon.followup_until == 12

  def test_low_confidence_candidate_does_not_trigger_followup(self):
    self.model.detect.return_value = Detection(40, 0.69)
    self.step(10)
    assert self.daemon.followup_until == 0

  def test_rejected_heading_gets_bounded_retry_without_publishing_a_speed(self):
    self.model.detect.return_value = None
    self.model.needs_followup = True
    self.step(10)
    assert self.daemon.followup_until == 12
    for now in (10.2, 11.9, 12.1, 13.9):
      self.step(now)
      assert self.daemon.followup_until == 12
      assert read_vision_speed_limit(self.params.get(VISION_SPEED_LIMIT_PARAM), now) == 0
    self.step(14.1)
    assert self.daemon.followup_until == 16.1

  def test_verified_number_can_start_followup_during_heading_retry_cooldown(self):
    self.model.detect.return_value = None
    self.model.needs_followup = True
    self.step(10)
    self.model.detect.return_value = Detection(40, 0.95)
    self.model.needs_followup = False
    self.step(12.1)
    assert self.daemon.followup_until == 14.1
    self.step(12.3)
    assert read_vision_speed_limit(self.params.get(VISION_SPEED_LIMIT_PARAM), 12.3) == pytest.approx(40 * CV.MPH_TO_MS)

  def test_inference_failure_clears_source(self):
    self.confirm()
    self.mocker.patch.object(self.daemon, 'step', side_effect=ValueError('Invalid classifier output'))
    ratekeeper = self.mocker.patch('openpilot.common.realtime.Ratekeeper')
    ratekeeper.return_value.keep_time.side_effect = KeyboardInterrupt
    with pytest.raises(KeyboardInterrupt):
      self.daemon.run()
    assert self.params.get(VISION_SPEED_LIMIT_PARAM) is None
    assert self.daemon.model is None
    self.daemon.logger.exception.assert_called_once()

  def test_invalid_car_messages_clear_source(self):
    self.confirm()
    self.sm.alive['frogpilotCarState'] = False
    self.step(11)
    assert self.params.get(VISION_SPEED_LIMIT_PARAM) is None

  def test_slow_inference_does_not_reject_fresh_inputs(self):
    sm = messaging.SubMaster(list(INPUT_MAX_AGE), frequency=30)
    for now in (10, 10.35, 10.7):
      messages = [messaging.new_message(service, valid=True) for service in INPUT_MAX_AGE]
      for message in messages:
        message.logMonoTime = int(now * 1e9)
      sm.update_msgs(now, messages)
    assert not sm.all_freq_ok()
    assert inputs_valid(sm, now)

  @pytest.mark.parametrize('service', INPUT_MAX_AGE)
  @pytest.mark.parametrize('age', [-0.01, 0.0, 0.05, 0.11, 6.0])
  def test_input_source_timestamps(self, service, age):
    self.confirm()
    self.sm.logMonoTime[service] = int((10.2 - age) * 1e9)
    if 0 <= age <= INPUT_MAX_AGE[service]:
      assert inputs_valid(self.sm, 10.2)
    else:
      self.daemon.step()
      assert self.params.get(VISION_SPEED_LIMIT_PARAM) is None

  @pytest.mark.parametrize('service', INPUT_MAX_AGE)
  def test_invalid_input_payload_clears_result(self, service):
    self.confirm()
    self.sm.valid[service] = False
    self.step(11)
    assert self.params.get(VISION_SPEED_LIMIT_PARAM) is None

  def test_wide_camera_fallback_and_switch_require_confirmation(self):
    self.confirm()
    self.camera_type.available_streams.return_value = [1]
    self.step(11)
    assert self.daemon.stream == 1
    assert self.params.get(VISION_SPEED_LIMIT_PARAM)['speedLimit'] == 0
