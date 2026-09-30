# Vision speed limit validation record

Results below cover the runtime at `ba5d6469dfa4da0598f3365063e3ac8fcf4a6a94`,
tested on 2026-09-21. These are development checks, not a general accuracy or
safety claim. Private recordings are not distributed with the port.

## Automated and recorded checks

- 284 checks passed on the Linux host and native ARM comma 3. They cover final
  cruise behavior across source expiry, higher/lower/equal replacements,
  confirmation, denial, Mapbox handover, driver overrides, curve interaction and
  disengagement, as well as model contracts, worker freshness and native Params.
  Changed Python passes Ruff; the dependency lock and diff checks pass.
- A paired comparison covered 963 development frames: 858 samples from ten drive
  clips, including complete known sign approaches; 101 incident frames; two public
  school-sign frames; and two later nighttime school-sign frames. The final model
  retained all 56 ordinary accepted readings and rejected the four conditional
  readings. No other acceptance changed. These are repeated views of a few signs,
  not 56 independently validated signs.
- Component-removal comparisons showed why the remaining bounded fallbacks stay:
  removing crop expansions lost three accepted frames, tint support lost 12, and
  the glare fallback lost 22. One heading alignment lost one, and one digit
  alignment lost two. Removing the weighted confidence/crop bonus in favor of the
  lowest agreeing classifier score preserved the sampled readings. These
  development comparisons do not calibrate the scores or establish unseen-route
  accuracy.
- Full two-minute host replay supplied all 2,400 camera frames through native
  padded-NV12 VisionIPC, the worker, Params and FrogPilot's cruise controller, with
  recorded message timestamps rebased consistently. No frame was over 100 ms late.
  Accepted limits were only 30 mph or unavailable. The maximum engaged cruise
  target during the incident window was 34.461 mph. After camera expiry the
  reading was zero and the engaged target remained 34.797 mph, without jumping to
  the higher stored cruise setting. Disengaged targets are not acceleration commands.

## Device preparation

The staged native `scons --minimal -j2` build passed in 346 seconds, followed by
model startup and the ARM checks above. The installed checkout remained untouched
during preparation. No new schema, C++, UI or dependency changes were needed for
this cleanup; staging still rebuilt native model artifacts for its new path.

The C3 processed 26 hash-checked recorded NV12 fixtures through its actual worker
and controller. Ordinary 30→40→30 changes passed; two route shields, all 16
previously false-70 frames and two conditional-header frames retained the held
30 mph limit. Camera-stale expiry passed. Inference took 0.19–0.73 seconds.

Physical-camera/driving-model/calibration coexistence also passed: 60 seconds
with Vision off and 180 seconds on, excluding the first ten seconds of each phase
from message counts. The 50/170-second measurement windows contained 1,001/3,401
valid model and odometry messages and 200/680 valid calibration messages, with no
invalid messages. Enabled inference samples took 0.64–0.75 seconds. The phase-end
device reports were thermally green, with 58%/60% memory use. This bounded desk
exercise does not establish sustained road-control deadlines or thermal limits.

Full-rate HEVC replay on the C3 is not a passing result: the earlier software
decoder could not provide 20 FPS. Temporal replay runs on the host; device checks
use decoded recorded frames and physical cameras.

## Remaining limits

The five-minute observation ceiling is a retention policy. Fresh matched map
segment/direction changes now clear it, including unnamed roads. Without a usable
map match, road transitions remain unobserved. Physical sign tracking is not
implemented: overlap alone would reject the known moving 40/30 mph approaches.

Both the old and simplified score could read 20 mph on a school sign without
knowing whether its condition was active. Visible yellow-header layouts are now
rejected, including the public fixtures that previously tested only the printed
number. This does not solve other conditional layouts or lane applicability.

Broader independently labeled routes, missed-sign rates, sustained control
deadlines, thermal limits and road validation remain open. Model licensing and
maintainer acceptance are separate upstream prerequisites. Earlier results and
the 30→70 incident investigation remain in the Git/PR history; an earlier positive
drive report is not evidence that these limits have been resolved.
