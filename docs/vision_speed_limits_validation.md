# Vision speed limit validation record

These are dated development results, not a general accuracy or safety claim.
Private route recordings are not distributed with the port.

## Cleanup and handover fixes (2026-09-21)

- 282 host checks pass, including final cruise outcomes for source loss, same/higher/
  lower replacements, automatic increases, acceptance/denial, Mapbox handover,
  driver overrides, curve interaction and disengagement. Changed Python passes Ruff.
- A paired recognition comparison used 961 development frames: 858 samples from
  ten existing drive clips (including complete known sign-approach windows), all
  101 frames around the nighttime failure and two public sign fixtures. Replacing
  weighted confidence and crop bonuses with the lowest agreeing classifier score
  retained the same 58 accepted frame readings and rejected every incident frame.
  These are repeated views of a few signs, not 58 independently validated signs.
- Removing crop expansions lost three accepted frames; removing tint support lost
  12; removing the glare fallback lost 22. Restricting heading OCR to one alignment
  lost one, and restricting digit OCR to one alignment lost two. Those bounded
  fallbacks are retained. This development set does not establish unseen-route
  accuracy or calibrate the confidence thresholds.
- The five-minute observation ceiling remains a retention policy. Fresh map segment
  and direction changes now clear it, including unnamed roads. Source loss cannot
  raise the cruise target by exposing a higher stored setting. Physical sign
  tracking and lane/conditional applicability remain outside this implementation;
  box overlap alone would reject the known moving 40/30 mph sign approaches.

### Recorded results (2026-09-20)

The following results cover runtime `42f682c7bfaeed4bc7d4a2b05ec62e510112503e`, before
the independent digit check. These are recorded results, not new test runs:

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

### Nighttime misread and digit-verification candidate (2026-09-20)

A later drive exposed a repeated 30-to-70 mph misclassification that raised the
cruise target and commanded acceleration. Two-frame confirmation did not reject
the repeated error. Offline NV12 replay reproduces it; the independent digit check
rejects it while preserving the existing positive fixtures. The candidate passes
255 host tests. Paired two-minute native VisionIPC/Params/SLC host replay selected
70 mph with an 80 mph target before the change and retained 30 mph with a target at
most 35 mph after it. Revision `c7c523fb791ad09861b13ec19c4e1dd9684cccba` subsequently passed 255 native ARM
checks and all 24 padded-NV12 fixtures (six positive, two route shields and 16
incident frames). Fixture inference took 0.21-0.74 seconds. Physical-camera, driving
model and calibration checks produced zero invalid messages in 15-second
vision-off/on observation windows. The native build, installation and reboot
verification passed, preserving 21 settings. This does not constitute a new road
test. Full temporal replay on C3 was not completed: software HEVC decoding was
too slow to provide the required camera rate.
