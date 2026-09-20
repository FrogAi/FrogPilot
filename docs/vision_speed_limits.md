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
  is enabled. OpenCV runs two CPU threads, on the existing helper cores 0–2 on
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
  heuristics are not a classifier for conditional-sign applicability. The color
  filter normalizes brightness with a bounded gain so shadowed white panels are
  not rejected solely for being dim; hue, saturation, and model inputs are unchanged.
  Moderately tinted panels require stronger detector and classifier scores. A
  separate, pinned PaddleOCR heading model can rescue glare-rejected panels only
  after those stronger scores and an exact `SPEED LIMIT` text check. A strong
  numeric read by itself also accepts route shields and is not sufficient.
  The heading check has at most two alignments per proposal and never supplies
  a speed value; see the [model provenance](../frogpilot/assets/vision_models/README.md).
- Confirmation requires two matching independent camera frames within two
  seconds. Multiple crops of a single frame are not independent confirmation.
  That window uses capture times, so inference latency does not shorten it.
  Processing backoff is 1.5 times inference duration while scanning and 1.0
  times during a bounded follow-up window. A confirmed match ends that window;
  weak reads cannot start it. A strong numeric candidate with an unreadable
  heading can request a two-second retry burst, followed by a two-second
  cooldown. That hint cannot publish a limit. Confirmed speed changes are logged
  for diagnosis.
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
  frogpilot/system/vision_speed_limit_header.py \
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

- 142 tests passed: 129 feature tests and 13 existing Params tests, using the
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

### Drive investigation and shadow-filter regression (2026-09-19)

The subsequent C3 road trial reported `Vision N/A`. Recorded messages during the
driving segments were valid, the vision worker remained running with no logged
inference exceptions, and the auxiliary gear flag correctly indicated Drive.
The device stayed below the worker's thermal and memory pause thresholds, but
`slcVisionSpeedLimit` remained zero. Existing logs do not record each vision
frame's decisions, so they cannot establish the exact live inference cadence or
explain every missed sign.

Offline replay reproduced a specific rejection on a clearly visible 30 mph sign:
the proposal model located it, but the color filter rejected its shadowed white
panel before classification. Direct classification of that crop correctly read
30 mph. The filter now adjusts its brightness reference using the crop's 90th
percentile, with gain limited to 3x. It preserves hue/saturation checks, the
original model inputs, confidence thresholds, and temporal confirmation.

At 6.67 sampled frames per second over a 60-second full-resolution recording,
the original pipeline produced no readings; the correction produced six 30 mph
readings on successive sampled frames and confirmed the limit. No other speed
was produced in that sample. This replay rate is not a measurement of C3 runtime
performance, and this single sign does not establish general recognition accuracy.

All 142 host tests pass, including shadowed neutral/cool white signs, dim colored
sign rejection, featureless crops, and bounded handling of near-black inputs.
The four new dim-white regression cases fail before the fix. The changed Python
files pass Ruff. These were the results before the subsequent C3 bench checks
and glare investigation below. No native UI/schema rebuild is introduced by
the recognition and scheduling changes.
Private route recordings are not included in the repository.

### Glare recognition and scheduling follow-up (2026-09-20)

Further review found two independent limitations: sunset-tinted and glare-covered
white signs still failed the color filter, and the original processing backoff
could leave too few inference opportunities before a sign passed. Simply copying
StarPilot's color thresholds also admitted Route 50 shields from the same footage.
The updated recognition gates and bounded follow-up scheduling address these
reproduced cases without counting crop variants as independent observations.

Current host checks pass 218 tests (205 feature tests and 13 Params tests),
including the shipped heading model's preprocessing/dictionary, rejected route,
speed-bump and weight-limit headings, strong-score requirements, finite output
checks, capture-time confirmation and follow-up bounds. The synthetic heading
tests verify the model interface, not road accuracy. Changed Python files pass
Ruff. The converted heading graph passes ONNX validation and comparison against
the pinned original graph; its preparation script reproduces the bundled hash.

Dense inference over ten 60-second full-resolution clips (12,000 actual frames)
recognizes the inspected shadowed 30, tinted 40 and glare-covered 30 signs.
Neither the inspected recreation sign nor the Route 6/50 shields yields an
accepted speed. This is a small, geographically limited regression set used
during development, not an independent accuracy benchmark. Individual-frame
results do not establish that an on-device worker confirms signs in time.

The same ten minutes were decoded to NV12 and sent at their recorded camera
timestamps through native VisionIPC, the actual vision daemon, shared Params,
and FrogPilot's `FrogPilotVCruise`/SLC in a separate WSL checkout. Messaging and
parameters used unique test prefixes. Recorded qlog vehicle/device/map payloads
were held between samples and republished at service rates; Vision-only selection
was enabled. This exercised the feature and cruise-target integration, not the
driving model, the complete manager stack, vehicle actuation or the onroad UI.

- The first drive delivered 8,400 frames. With a processing delay floor of 1.25
  times the prior C3 per-network cost estimate, the selected limit changed to
  30 mph at 58.19 seconds, 40 at 110.08, and 30 at 317.17. Recorded road-name
  changes cleared it at 157.82 and 395.81 seconds. Configured offsets and cruise
  caps remained in the existing controller path.
- The second drive delivered 3,600 frames at native host inference speed. It
  selected 30 mph at 66.03 seconds and cleared it when the recorded gear changed
  to Park. Neither replay selected another speed or logged an inference failure.
- Maximum camera delivery lateness was 141 ms in the first run and 111 ms in
  the second, below the worker's 500 ms input-age limit. These are host transport
  measurements, not C3 performance measurements.
- A focused native-camera glare replay confirmed 30 mph with a 1.5-times cost
  floor and removed it three seconds after camera input ended. Trying the tighter
  heading alignment first reduced inference work without changing the accepted
  heading set. Rejected headings can request the bounded retry described above.
- A simulation using the actual daemon/Ratekeeper and the dense NV12 inference
  cache recovered the full sequence at all 40 starting phases at both the cost
  estimate and 1.25 times that estimate. At 1.5 times the estimate, 30/40 phases
  passed; ten missed the brief 40 mph observation window. **Slowdown margin is
  limited.** The delay model uses earlier C3 measurements (212 ms base, 29 ms per
  numeric classification and 150 ms per heading inference); it does not model
  final-device contention and is not a hardware deadline guarantee.

Earlier RGB-only replay and timing simulations missed failures exposed by this
NV12 path. These regressions therefore cannot be replaced by a few still-image
successes. Raw recordings and private diagnostic outputs are not distributed.

Before adding the heading fallback, a native C3 build completed and an isolated
test passed actual padded NV12 VisionIPC frames through the ONNX models, shared
parameters and Vision-only SLC: 30 then 40 mph, with no change on route shields
and removal after camera input expired. Two-thread inference on those examples
took about 0.25–0.34 seconds. Physical camera/model/calibration coexistence tests
measured 15 seconds per phase after settling, with 60 valid calibration messages
and 301 valid messages each from modelV2 and cameraOdometry, and zero invalid
messages, both with vision disabled and with it enabled.

**Those C3 results precede the heading fallback.** Its standalone recognizer ran
on the C3, but the complete final three-model worker still needs native latency,
coexistence, startup and road validation. The updated candidate is not yet
installed. Host replay cannot establish ARM scheduling, sustained thermal
headroom or driving safety. No claim of general sign accuracy or driving
readiness is made.
