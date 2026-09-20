# Vision speed limits

This port adds StarPilot's two-stage sign model as an optional `Vision` source in
FrogPilot's existing Speed Limit Controller. It targets `MAKE-PRS-HERE` at
`f8fb0668ed7fd13caf5a21e1ac27f6378df1b37f`; the default prebuilt distribution is not
the source tree used to build this change.

## Use

In the Qt settings, select the Advanced tuning level and enable **Vision Speed
Limits (U.S.)** under Speed Limit Controller. Select **Vision** in **Speed Limit
Source Priority** to choose its position relative to Map Data and Dashboard.
Three priority slots are supported. Existing first and second choices are
preserved; the new third slot defaults to Vision, which contributes nothing while
detection is disabled. Highest/Lowest modes consider enabled, available sources.

For display without longitudinal control, enable the same toggle under Visuals /
Navigation Widgets and enable Show Speed Limits. Detection alone does not enable
longitudinal control. Existing offsets, change confirmations, gas overrides,
cruise-speed cap, and disengagement behavior remain in FrogPilot's controller.
The optional source panel includes Vision. This branch's Python/Mici UI does not
have the equivalent FrogPilot settings/source panel; that UI is not validated by
this Qt port.

Detection is off by default. Supported readings are U.S. 5–80 mph signs in
increments of 5. Metric displays convert those mph values normally. This is not
a European/Canadian sign model and it does not infer sign units from IsMetric.
The models cannot establish lane applicability or whether a conditional school
or construction limit is active. Recognition errors and missed signs remain
possible; controller confirmation is still configurable.

## Implementation

- `speed_limit_vision` is managed onroad only when the effective FrogPilot toggle
  is enabled. OpenCV runs one CPU thread, on the existing helper cores 0–2 on
  device. Nominal detection is 6 Hz with 10 Hz follow-up for two seconds after a
  candidate, backed off for CPU/memory pressure and measured inference cost.
  Critical memory pressure or high thermal state clears the source and pauses it.
  The worker validates input payloads and source timestamps: car state must be
  at most 0.1 seconds old and device state at most 5 seconds old. Inference blocks
  this worker, so its receive frequency is not used to infer publisher health.
- Vehicle validity and the drive/low gear flag arrive through `frogpilotCarState`.
  The standard `carState` channel already has 15 readers in a fully configured
  FrogPilot, which fills this msgq version's reader capacity. Adding the vision
  worker there evicts existing readers and can invalidate calibration and the
  downstream control stack. Using the auxiliary channel preserves those readers
  without changing the shared-memory layout or relaxing any control checks.
- The worker prefers the road stream and falls back to wide road. NV12 decoding
  respects stride **and UV-plane offset**. Frames older than 0.5 seconds, repeated
  timestamps, offroad/invalid car state, and non-driving gears cannot confirm a sign.
  This branch's camera producer does not populate VisionIPC's `valid` flag;
  frame acceptance checks received buffers, timestamps, and layout instead.
- Model bytes and tensor shapes are checked before use. Inference handles the
  actual single-class proposal detector and 19-class probability output. Advisory
  color rejection, a maximum of four non-overlapping proposals, bounded crop
  reads, and rejection of conflicting numbers limit ambiguous results. These
  heuristics are not a classifier for conditional-sign applicability.
- Confirmation requires two matching independent camera frames within two
  seconds. Multiple crops of a single frame are not independent confirmation.
  Changes below 30 mph from 30 mph or above require at least 0.90 confidence.
- One JSON value in `Params(memory=True)` carries the m/s result, confidence,
  detection time, and last processed camera-frame time together. The SLC validates
  it independently:
  the result expires after 300 seconds without matching observations, or after
  three seconds without a new processed camera frame. Republishing does not extend
  either deadline. Live frames without a recognized sign preserve the held limit
  until its detection expires. Observed road-name changes and camera switches
  clear confirmation.
- Expired/disabled vision is excluded from Previous Limit fallback. Vision limits
  are not saved to the persistent PreviousSpeedLimit parameter. Mapbox, map data,
  and dashboard behavior remain available when inference fails.
  Pending vision confirmations, denied readings, and queued approval taps clear
  when vision becomes unavailable or is no longer selected. Pending confirmations
  from other sources retain the existing controller behavior.
- StarPilot's training collector, automatic bookmarks, raw-frame logging, legacy
  OCR models, optical-flow experiment, and unrelated telemetry are not dependencies.

The source was reviewed rather than copied wholesale. In particular, this port
does not use display units as sign units, assume three detector classes, count
multiple crops as temporal confirmation, keep a result alive because the Python
loop is running, or hold a previous vision limit after the producer dies.

## Validation

With the repository's Linux development dependencies and native bindings built:

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
  frogpilot/system/tests/test_speed_limit_vision.py \
  frogpilot/controls/tests/test_vision_speed_limit_controller.py
uv lock --check
```

Tests cover positive and blank-image inference, model corruption, output shape and
probability validation, sign conflicts, units, confirmation, stale snapshots,
camera layout/loss, load shedding, source priorities, driver acceptance/rejection,
gas overrides, and stale-limit fallback. An integration test sends padded NV12
frames through real VisionIPC and device/car-state messages through PubMaster /
SubMaster, runs the actual ONNX models, and passes results through native shared
parameters to the controller. It also checks that the controller stops using
those results when their camera-frame timestamp expires. The two positive frames are a small
regression fixture; they do not establish onroad accuracy.

Before merge/deployment, model [licensing and provenance](../frogpilot/assets/vision_models/README.md)
must be resolved. Hardware validation remains necessary: check on-device inference
latency and model/control deadlines, memory and thermal headroom, camera restart,
drive transitions, and a labeled raw-camera replay set with both ordinary signs
and hard negatives (advisory signs, school conditions, side roads, night/glare).
Host checks do not establish those properties.

### Host validation record (2026-09-19)

Linux x86-64 under WSL, Python 3.12.3, OpenCV 4.11.0:

- 100 tests passed: 87 feature tests and 13 existing Params tests, using the
  repository pytest configuration and native parameter bindings.
- The parameter/messaging/VisionIPC bindings compiled, and the complete Qt UI
  compiled and linked with the repository SCons configuration.
- An isolated 2160x1080 Qt preview confirmed the Vision toggle and source selection
  dialogs render, all three priorities save correctly, and cancelling the second
  dialog preserves the existing order. This is a desktop settings check, not a
  complete device image or on-device UI validation.
- New modules/tests and the edited controller/process configuration pass Ruff.
  The variables module has 56 pre-existing diagnostics; comparison against the
  base revision found no new diagnostics in changed existing Python files.
- `uv lock --check --offline` and `git diff --check` passed. Model checksums match
  the pinned StarPilot files; positive frame inference and blank-image rejection
  ran using the actual ONNX weights.

These host results do not establish comma hardware performance, labeled route
accuracy, native Mici UI support, or license clearance.

### C3 investigation and follow-up validation (2026-09-19)

Initial vehicle testing of `4ff97619` on a comma 3 running AGNOS 12.8 exposed
communication errors. The full process stack exceeded `carState`'s 15-reader
capacity when the vision worker subscribed, repeatedly evicting readers and
invalidating calibration and downstream outputs. Temporarily pausing only the
vision worker restored their validity. Isolated input replay did not reproduce
the failure because it did not have the full stack's reader count.

The worker now consumes `frogpilotCarState`, which carries the same CAN-valid flag
and a drive/low gear flag. No checks in calibration or the vehicle control
processes have been relaxed. This branch is not validated for driving.

A separate worker bug was reproduced using the C3's physical cameras and driving
model, with isolated messaging and recorded/synthetic vehicle-state inputs.
Inference took approximately 0.33–0.37 seconds, reducing the worker's receive rate
and incorrectly triggering its own frequency check. The worker now checks message
validity and source age instead. The same bench test then stayed in `Scanning`;
calibration produced 60 valid and zero invalid messages in each 15-second measured
window with vision disabled and enabled. This short, isolated test does not
validate the complete vehicle process stack, recognition accuracy, or sustained
thermal performance.

The reader-capacity fix passes 100 host tests (87 feature tests and 13 Params
tests), including preservation of all 15 existing car-state readers and the
auxiliary publisher's gear/validity behavior, the real SubMaster frequency tracker,
and rejection of stale, future-dated, and invalid inputs. The native Qt UI and the
CAN/Panda binding compile with the updated schema. The feature modules and tests
pass Ruff; the existing long line in `card.py` is unchanged.

Revision `77fc9ba3` was then built and started on the C3 in the parked vehicle,
with ignition on and FrogPilot disengaged. After a 12-second settling period,
a 20-second observation of the complete running stack found:

- All 12 monitored services were alive, valid, and within their receive-frequency
  checks. None of their sampled messages was invalid, including 80 calibration
  messages and 401 messages each from live pose, longitudinal planning, radar,
  live parameters, and driver assistance.
- No selfdrive alert was present in 1,999 observed state messages, and no managed
  process that should have been running was stopped.
- `carState` held a stable set of 15 reader IDs throughout the window, compared
  with repeated eviction before the fix. These counters were read directly from
  shared memory; adding a diagnostic subscriber to this full channel would itself
  exceed the limit.
- The vision worker was running. Its `Idle` status was expected in Park; this
  parked check did not exercise inference while driving.

The temporary calibration instrumentation was removed from the device's source
after collection. These results verify recovery from the reproduced parked
communication failure, not road sign accuracy or driving behavior.
