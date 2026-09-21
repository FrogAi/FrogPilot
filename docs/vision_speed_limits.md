# Vision speed limits

Adds StarPilot's sign detector and number classifier, with independent PaddleOCR text
verification, as an optional **Vision** source in FrogPilot's Speed Limit Controller.
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
  the result. Agreeing crops use the lowest classifier score without a crop-count
  bonus or a weighted detector score; this is not calibrated recognition accuracy.
  A color filter rejects advisory signage and adjusts brightness with
  bounded gain for shadows. Tinted panels require stronger detector/classifier
  scores. A distinct yellow header above a white panel rejects the observed school/
  conditional layout before crop expansion; uniformly tinted signs remain eligible.
  This does not detect every condition or its activation status.
  Glare-rejected panels additionally require an exact `SPEED LIMIT` heading,
  with at most two OCR alignments per proposal. Every accepted number, including on
  neutral white panels, must also match a separate OCR digit read. This uses at most
  two aspect-preserving crops and requires confidence for each digit. A confident
  disagreement rejects the proposal. OCR verifies the classifier's number rather
  than supplying a replacement. Automatic increases and driver settings are preserved.
  See [model provenance and preparation](../frogpilot/assets/vision_models/README.md).
- **Confirmation:** two matching, separately captured frames within two seconds are required,
  using camera capture times. Crop variants do not count as separate observations.
  Changes below 30 mph from 30 mph or above require at least 0.90 confidence. A
  confirmed match ends follow-up; weak reads cannot start it. Strong numeric
  candidates with unreadable headings can request a two-second retry followed by a
  two-second cooldown, without publishing a limit. Confirmed speed changes are logged.
- **Freshness:** one JSON value in shared `Params(memory=True)` carries m/s,
  confidence, detection time and last processed frame time. The SLC independently
  expires results after 300 seconds without matching observations or three seconds
  without a new processed frame. Republishing extends neither deadline. Live frames
  without a sign preserve the held limit until expiry. This five-minute ceiling is
  a bounded retention policy, not proof that the sign still applies. Fresh matched
  map segment/direction changes and camera switches clear confirmation; road names
  do not identify a segment. A match must have a loaded tile, positive way ID and a
  location timestamp no more than two seconds old. Without a usable map match, the
  worker cannot detect road transitions; time and camera expiry still apply.
- **Controller:** expired/disabled Vision cannot survive through Previous Limit
  fallback, and Vision readings are never persisted as `PreviousSpeedLimit`.
  Source handovers compare against the last accepted target, so losing Vision
  cannot bypass higher/lower confirmation. Pending decisions and queued taps are
  tied to their source. Valid replacements still increase automatically when
  confirmation is disabled. With no accepted replacement, an engaged controller
  keeps the last Vision cruise ceiling while the displayed reading becomes
  unavailable. A new accepted limit, disengagement, disabling SLC or an explicit
  cruise increase releases that ceiling; gas overrides retain their selected mode.
  Curve speed reductions do not become permanent Vision ceilings.

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
  frogpilot/system/vision_speed_limit_text.py \
  frogpilot/system/tests/test_speed_limit_vision.py \
  frogpilot/controls/tests/test_vision_speed_limit_controller.py
uv lock --check
```

Recorded results are kept in [the validation record](vision_speed_limits_validation.md).

## Limitations and review items

- Model [licensing and provenance](../frogpilot/assets/vision_models/README.md) remain
  an upstream inclusion question. The StarPilot weights declare AGPL-3.0; retaining
  notices does not resolve corresponding-source obligations or license acceptance.
- The models cannot establish lane applicability or whether a conditional school
  or construction limit is active. Matching numbers in two frames do not establish
  physical sign identity or make their errors statistically independent. Recognition errors and missed signs are possible.
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
