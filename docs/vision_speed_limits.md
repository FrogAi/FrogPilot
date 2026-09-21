# Vision speed limits

Adds StarPilot's sign detector and number classifier, with a PaddleOCR heading
fallback, as an optional **Vision** source in FrogPilot's Speed Limit Controller.
This port targets `MAKE-PRS-HERE` at `f8fb0668ed7fd13caf5a21e1ac27f6378df1b37f`;
the default prebuilt distribution is not the source tree used for this change.

## Use

In Qt settings, select the Advanced tuning level and enable **Vision Speed Limits
(U.S.)** under Speed Limit Controller. Set **Speed Limit Source Priority** to
choose Vision's position relative to Map Data and Dashboard. Three slots are
supported; existing first/second choices are preserved and the third defaults to
Vision. Detection is off by default. Highest/Lowest modes use available sources.

For display without longitudinal control, enable the same toggle under Visuals /
Navigation Widgets and enable **Show Speed Limits**. Detection alone does not
enable longitudinal control. Existing offsets, driver confirmations, gas overrides,
cruise-speed caps and disengagement behavior remain in the controller. The optional
source panel includes Vision.

Supported readings are U.S. 5-80 mph signs in increments of 5. Metric displays
convert those mph readings; display units never determine sign units.

## Architecture

- **Worker:** `speed_limit_vision` runs onroad when enabled, using OpenCV with two
  CPU threads on device helper cores 0-2. Scheduling targets 6 Hz while scanning
  and 10 Hz during two-second follow-ups, limited by measured inference cost and
  CPU/memory pressure. These are scheduling targets, not measured device rates.
  Processing backoff is 1.5 times inference duration normally and 1.0 during
  follow-up. High thermal state or critical memory pressure clears the source.
- **Inputs:** vehicle validity and drive/low gear come from `frogpilotCarState`,
  preserving the full `carState` channel's 15 existing readers. The worker checks
  payload validity and source age (0.1 seconds for vehicle state, 5 for device
  state); its inference-limited receive frequency does not measure publisher health.
  It prefers road camera and falls back to wide road. NV12 decoding respects stride
  and UV-plane offset. Buffer layout and timestamps establish frame acceptance
  because this camera producer does not populate VisionIPC's `valid` flag. Frames
  older than 0.5 seconds, duplicate timestamps, invalid/offroad state and non-driving
  gears cannot confirm a sign.
- **Recognition:** model hashes and output contracts are checked. Up to four
  non-overlapping proposals receive bounded crop reads; conflicting numbers reject
  the result. A color filter rejects advisory signage and adjusts brightness with
  bounded gain for shadows. Tinted panels require stronger detector/classifier
  scores. Glare-rejected panels additionally require an exact `SPEED LIMIT` heading,
  with at most two OCR alignments per proposal. OCR never supplies the numeric speed.
  See [model provenance and preparation](../frogpilot/assets/vision_models/README.md).
- **Confirmation:** two matching independent frames within two seconds are required,
  using camera capture times. Crop variants do not count as separate observations.
  Changes below 30 mph from 30 mph or above require at least 0.90 confidence. A
  confirmed match ends follow-up; weak reads cannot start it. Strong numeric
  candidates with unreadable headings can request a two-second retry followed by a
  two-second cooldown, without publishing a limit. Confirmed speed changes are logged.
- **Freshness:** one JSON value in shared `Params(memory=True)` carries m/s,
  confidence, detection time and last processed frame time. The SLC independently
  expires results after 300 seconds without matching observations or three seconds
  without a new processed frame. Republishing extends neither deadline. Live frames
  without a sign preserve the held limit until expiry. Road-name changes and camera
  switches clear confirmation.
- **Controller:** expired/disabled Vision cannot survive through Previous Limit
  fallback, and Vision readings are never persisted as `PreviousSpeedLimit`.
  Pending Vision confirmations, denied readings and queued acceptance taps clear
  when the source disappears or is deselected. Other source confirmations and
  Mapbox/map/dashboard fallback retain their existing behavior.

## Validation

With this repository's Linux development dependencies, build the native targets
and run the focused checks:

```sh
scons --minimal -j4 common/params_pyx.so \
  msgq_repo/msgq/ipc_pyx.so msgq_repo/msgq/visionipc/visionipc_pyx.so \
  selfdrive/pandad/pandad_api_impl.so \
  selfdrive/controls/lib/longitudinal_mpc_lib/c_generated_code/acados_ocp_solver_pyx.so \
  selfdrive/ui/ui
pytest -n0 frogpilot/system/tests/test_speed_limit_vision.py \
  frogpilot/controls/tests/test_vision_speed_limit_controller.py common/tests/test_params.py
ruff check frogpilot/common/vision_speed_limit.py \
  frogpilot/system/speed_limit_vision.py frogpilot/system/vision_speed_limit_model.py \
  frogpilot/system/vision_speed_limit_header.py \
  frogpilot/system/tests/test_speed_limit_vision.py \
  frogpilot/controls/tests/test_vision_speed_limit_controller.py
uv lock --check
```

### Recorded results (2026-09-20)

The tested runtime is `42f682c7bfaeed4bc7d4a2b05ec62e510112503e`. Later documentation
edits do not change that runtime. These are recorded results, not new test runs:

- **Host:** 219 tests passed (206 feature and 13 Params), covering actual ONNX
  inference, native padded-NV12 VisionIPC/messaging/shared-Params/SLC integration,
  units, confirmation, stale limits, camera loss, load shedding, reader preservation
  and the real confirmation logger. Native Qt/bindings builds, changed-Python Ruff,
  lock validation and diff checks passed; model conversion reproduced the pinned hash.
- **Replay:** 12,000 recorded frames plus native daemon/SLC and full desktop
  model/controls/planning/Qt replay recovered the inspected 30/40/30 mph signs and
  road-change clearing. Inspected Route 6/50 shields and a recreation sign supplied
  no accepted speed. These private development recordings are not an independent
  accuracy benchmark or a closed-loop vehicle simulation.
- **Native C3:** staged and canonical ARM builds passed before reboot into the tested
  revision with Qt running. Native NV12/model/Params/SLC fixtures confirmed 30/40/30
  mph, rejected inspected negatives and expired camera-stale results. Fixture
  inference took 0.20-0.58 seconds. Physical-camera/model/calibration checks produced
  zero invalid messages during 15-second vision-off/on windows; enabled inference
  samples took 0.58-0.61 seconds. A separate 20-second post-reboot check in Park found
  all 14 monitored channels healthy, stable readers, no alerts and no unexpected
  stopped processes. Vision was correctly Idle in Park. The 12 candidate manifest
  hashes matched and eight backed-up steering/tuning settings were preserved.
- **Road feedback:** the tester reported a successful subsequent drive. No labeled
  sign count, miss rate, distance or lighting coverage accompanied that report.

## Limitations and review items

- Model [licensing and provenance](../frogpilot/assets/vision_models/README.md) remain
  an upstream inclusion question. The StarPilot weights declare AGPL-3.0; retaining
  notices does not resolve corresponding-source obligations or license acceptance.
- The models cannot establish lane applicability or whether a conditional school
  or construction limit is active. Recognition errors and missed signs are possible.
  This is not a European/Canadian sign model. Python/Mici settings and source display
  are not implemented or validated by this Qt port.
- Broader labeled-route accuracy, sustained control/model deadlines, thermal/memory
  headroom, camera restarts and drive transitions remain unestablished. A timing
  simulation missed the brief 40 mph sign in 10/40 starting phases at 1.5 times the
  estimated inference cost. Short hardware checks and road feedback do not establish
  general reliability or sustained inference rates.

Detailed investigation history is retained in the
[PR discussion](https://github.com/FrogAi/FrogPilot/pull/321#issuecomment-5754480633). Private route recordings
and device diagnostics are not distributed with the port.
