import tempfile

from pathlib import Path
from types import SimpleNamespace

import cv2
import numpy as np
import pytest

from cereal import messaging
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


class MemoryParams:
  def __init__(self):
    self.values = {}

  def put(self, key, value):
    self.values[key] = value

  def get(self, key):
    return self.values.get(key)

  def remove(self, key):
    self.values.pop(key, None)


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

  def test_real_sign_frames_require_temporal_confirmation(self):
    model = SpeedLimitModel()
    state = SpeedLimitConfirmation()
    folder = Path(__file__).parent / 'fixtures' / 'vision_speed_limit'
    for index, number in enumerate((140, 146)):
      frame = cv2.imread(str(folder / f'sign_20_frame_{number}.png'))
      assert frame is not None
      detection = model.detect(frame)
      assert detection is not None and detection.speed_mph == 20
      now = 100 + index * 0.2
      state.update(detection, now, now)
      assert state.speed_mph == (0 if index == 0 else 20)
    assert read_vision_speed_limit(state.snapshot(101), 101) == pytest.approx(20 * CV.MPH_TO_MS)

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

  @pytest.mark.parametrize('brightness', [1.0, 0.45, 0.25])
  @pytest.mark.parametrize('background', [(0, 220, 255), (0, 130, 255), (0, 0, 230), (0, 200, 0), (220, 70, 20)])
  @pytest.mark.parametrize('foreground', [0, 230])
  def test_colored_sign_is_rejected_in_sun_and_shadow(self, brightness, background, foreground):
    colored = np.full((100, 80, 3), background, np.uint8)
    cv2.putText(colored, '35', (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (foreground,) * 3, 3)
    assert not is_regulatory_sign((colored * brightness).astype(np.uint8))

  @pytest.mark.parametrize('brightness', [0, 40, 100, 230])
  def test_featureless_crop_is_rejected(self, brightness):
    assert not is_regulatory_sign(np.full((100, 80, 3), brightness, np.uint8))

  def test_near_black_crop_is_not_amplified_into_a_sign(self):
    crop = np.full((100, 80, 3), 20, np.uint8)
    cv2.putText(crop, '35', (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 0), 3)
    assert not is_regulatory_sign(crop)

  def test_conflicting_signs_are_not_selected(self):
    model = SpeedLimitModel.__new__(SpeedLimitModel)
    model.proposals = self.mocker.Mock(return_value=[([300, 50, 80, 100], 0.9), ([450, 50, 80, 100], 0.9)])
    model.classify = self.mocker.Mock(side_effect=[Detection(55, 0.99)] * 3 + [Detection(35, 0.99)] * 3)
    self.mocker.patch('openpilot.frogpilot.system.vision_speed_limit_model.is_regulatory_sign', return_value=True)
    assert model.detect(np.full((480, 960, 3), 230, np.uint8)) is None


class FakeSubMaster(dict):
  def __init__(self):
    super().__init__(
      deviceState=SimpleNamespace(started=True, thermalStatus=0, memoryUsagePercent=30, cpuUsagePercent=[10] * 8),
      frogpilotCarState=SimpleNamespace(drivingGear=True),
      mapdOut=SimpleNamespace(roadName='Main Street'),
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
    self.daemon = SpeedLimitVisionDaemon(self.params, self.sm, self.camera_type, self.stream_type, self.mocker.Mock(), lambda: self.model)

  def step(self, now):
    self.camera.timestamp_eof = int(now * 1000000000.0)
    self.sm.logMonoTime = dict.fromkeys(self.sm, int(now * 1e9))
    self.mocker.patch('openpilot.frogpilot.system.speed_limit_vision.time.monotonic', return_value=now)
    self.daemon.step(now)

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
    self.daemon.step(14)
    assert self.params.get(VISION_SPEED_LIMIT_PARAM) is None

  def test_worker_stopping_after_camera_stall_does_not_extend_deadline(self):
    self.confirm()
    self.camera.recv.return_value = None
    self.step(11.2)
    self.step(11.23)  # Republish between inference attempts, without a new frame.
    snapshot = self.params.get(VISION_SPEED_LIMIT_PARAM)
    assert read_vision_speed_limit(snapshot, 10.2 + HEARTBEAT_TIMEOUT + 0.1) == 0

  def test_road_change_requires_new_confirmation(self):
    self.confirm()
    self.sm['mapdOut'].roadName = 'Side Street'
    self.step(11)
    assert self.params.get(VISION_SPEED_LIMIT_PARAM)['speedLimit'] == 0

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
    assert inference_interval(state, 1) >= 2.5
    state.cpuUsagePercent = [95] * 8
    assert inference_interval(state, 0) >= 1.5
    assert inference_interval(state, 0, followup=True) >= 1.5

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
      self.daemon.step(10.2)
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
