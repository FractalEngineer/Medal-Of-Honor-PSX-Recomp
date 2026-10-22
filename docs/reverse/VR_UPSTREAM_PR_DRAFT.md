# VR upstream PR drafts

Eight stacked pull requests for `RetroPortingToolKit/psxrecomp`, based on
upstream master `9caf4c22`. Each feature branch contains one commit relative
to its parent branch. All eight PRs are open and ready for review together.
After a prerequisite merges, rebase the next branch onto upstream master and
rerun its listed checks.

| PR | Feature branch | Parent | Commit |
| --- | --- | --- | --- |
| 1 | [#739](https://github.com/RetroPortingToolKit/psxrecomp/pull/739) | [`vr-pr/01-frame-tracing`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/01-frame-tracing) | `upstream/master` | `8400b55d` |
| 2 | [#740](https://github.com/RetroPortingToolKit/psxrecomp/pull/740) | [`vr-pr/02-bounded-shedding`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/02-bounded-shedding) | PR 1 | `0cdf9595` |
| 3 | [#741](https://github.com/RetroPortingToolKit/psxrecomp/pull/741) | [`vr-pr/03-pair-checkpoint`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/03-pair-checkpoint) | PR 2 | `4419d234` |
| 4 | [#742](https://github.com/RetroPortingToolKit/psxrecomp/pull/742) | [`vr-pr/04-paired-hud-capture`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/04-paired-hud-capture) | PR 3 | `e4f1f985` |
| 5 | [#743](https://github.com/RetroPortingToolKit/psxrecomp/pull/743) | [`vr-pr/05-synthetic-head-pose`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/05-synthetic-head-pose) | PR 4 | `18d52c8b` |
| 6 | [#744](https://github.com/RetroPortingToolKit/psxrecomp/pull/744) | [`vr-pr/06-origin-recovery`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/06-origin-recovery) | PR 5 | `36f1ff3e` |
| 7 | [#745](https://github.com/RetroPortingToolKit/psxrecomp/pull/745) | [`vr-pr/07-projective-replay`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/07-projective-replay) | PR 6 | `fef7a1b6` |
| 8 | [#746](https://github.com/RetroPortingToolKit/psxrecomp/pull/746) | [`vr-pr/08-production-readiness`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/08-production-readiness) | PR 7 | `534f9ce2` |

## 1. VR: add opt-in stereo and XR frame attribution

**Current behavior:** Stereo and OpenXR performance is visible mainly as a
whole-frame total. A slow frame cannot be attributed reliably to guest scene
traversal, checkpointing, either eye, HUD capture, OpenXR waits, or submission.

**Desired behavior:** An opt-in trace records one compact row per stereo frame
and exposes stage costs and state through TCP as JSON or a Chrome/Perfetto
trace. Normal production play remains untraced and pays only the cost of a
disabled branch.

**What it changes:**

- Adds a fixed-capacity trace ring with pair identity, per-eye checkpoint,
  guest-draw and restore timing, failure/stale/shed state, guest cycles,
  primitive counts, texture-upload deltas, and atomic publication results.
- Separates HUD capture, draw, and readback timing; records OpenXR cadence and
  wait/acquire/copy/submit stages without presenting CPU timings as GPU time.
- Adds guarded nested span markers so a game can attribute world traversal or
  visibility selection inside an eye.
- Keeps allocation and file I/O out of the record path and documents trace
  configuration and export.

**Validation:** The trace, allocation/I/O guard, render sandbox, abort, and
XR-off input tests pass on Windows/GCC 13.2 Release. The TCP command index is
current.

[Source and validation receipt](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/01-frame-tracing/docs/internal/upstream/vr-frame-tracing.md)

## 2. VR: bound stereo shedding and recover at the display deadline

**Current behavior:** A stale pair-time estimate can suppress many consecutive
updates after one overloaded frame. The shed state can also remain latched
after rendering has recovered, producing a held or empty headset image.

**Desired behavior:** Shedding has a bounded duration and streak, periodically
probes with a fresh render, and exits as soon as a complete pair meets the
display deadline. A state or cadence change clears obsolete budget history.
Stereo publication remains all-or-nothing.

**What it changes:**

- Extracts the shedding decision into a testable whole-pair policy with
  elapsed-time and consecutive-skip limits.
- Prefers the OpenXR display period for the budget, with an explicit override
  and a guest VBlank fallback.
- Resets the estimate on scene, mode, geometry, and XR lifecycle changes;
  exposes streak, stale age, reset, and budget-source counters.
- Keeps shedding opt-in at the application layer. Medal of Honor continues to
  publish every complete pair by default.

**Validation:** Policy, render sandbox, and abort tests pass on Windows/GCC
13.2 Release. Historical reproduction reduced a pathological 895-frame shed
streak to 62 frames; this fixes runaway suppression and does not promise full
frame-rate parity under sustained overload.

[Source and validation receipt](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/02-bounded-shedding/docs/internal/upstream/vr-bounded-shedding.md)

## 3. VR: reuse one guest checkpoint per stereo pair

**Current behavior:** Each eye copies the same large guest baseline, so the
right-eye transaction repeats CPU, RAM, DMA, timer, and related checkpoint
work already performed for the left eye.

**Desired behavior:** Capture the pair baseline once, restore it after each eye,
and retain the per-eye GPU and precision state required for deterministic
replay. Any failed, mismatched, or unrelated transaction falls back cleanly to
an independent capture.

**What it changes:**

- Shares the checkpoint only between the two eyes of one protected stereo pair
  and restores the complete guest state after each eye.
- Verifies restored state before reuse; a mismatch retires reuse for the
  session, while `PSX_STEREO_BASELINE=0` provides the old behavior as a control.
- Adds checkpoint-save, reuse, and copied-byte counters and marks reused
  baselines in the frame trace.

**Validation:** Trace, render sandbox, and abort tests pass, including shared
baseline restoration, per-eye precision, explicit opt-out, and failure cleanup.

[Source and validation receipt](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/03-pair-checkpoint/docs/internal/upstream/vr-pair-checkpoint.md)

## 4. VR: capture both HUD mattes with one checkpoint and readback

**Current behavior:** Producing the two native-HUD mattes uses two separate
capture calls, two checkpoint baselines, and two GPU readbacks. That leaves
additional synchronization and rollback work on the critical path.

**Desired behavior:** One paired capture API renders both mattes from the same
guest checkpoint and returns two contiguous image planes from one readback.
The operation restores renderer state, returns no partial result, and does not
publish or impersonate a stereo world pair.

**What it changes:**

- Adds `psx_mod_render_capture_rgba_pair` and a double-height staging/readback
  path for the two RGBA mattes.
- Shares the first matte's checkpoint with the second while keeping that scope
  separate from the later eye transaction.
- Rejects invalid or incomplete captures and updates sandbox/abort stubs.
- Consumers must migrate from the single-matte API and rebuild.

**Validation:** Sandbox and abort tests pass with both output buffers and
byte-identical matte checks. Historical measurements moved paired capture p50
from 3.41 ms to 3.19 ms and p95 from 6.91 ms to 5.10 ms in the test scene.

[Source and validation receipt](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/04-paired-hud-capture/docs/internal/upstream/vr-paired-hud-capture.md)

## 5. VR: add debug synthetic head translation and pitch

**Current behavior:** Headless runs with a null HMD cannot apply a controlled
headset translation or pitch, so small eye-motion regressions require a person
to move their head to test them.

**Desired behavior:** Debug builds can apply an absolute room-space translation
and optional pitch to both eye poses while leaving normal tracking untouched.
Overrides are bounded, reject invalid input, and can be cleared explicitly.

**What it changes:**

- Adds the TCP `openxr_pose_override` control for millimetre X/Y/Z translation,
  coarse `pitch_deg`, and fine `pitch_mdeg`.
- Applies the synthetic pose to both eyes after origin capture and reports the
  exact applied values in JSON.
- Limits translation to 10 metres and pitch to 90 degrees, rejects non-finite
  values, and keeps the feature unavailable outside the debug surface.

**Validation:** XR-off input tests cover bounds, binocular translation, pitch,
disabled behavior, and clearing; the command index is current.

[Source and validation receipt](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/05-synthetic-head-pose/docs/internal/upstream/vr-synthetic-head-pose.md)

## 6. VR: recover an inverted XR reference origin

**Current behavior:** A bad initial XR origin can leave world geometry upside
down for the whole session while the HUD and hands remain correctly oriented.
A user-requested recenter repairs the measured relative pose.

**Desired behavior:** The runtime drops an invalid reference origin after a
persistent inversion and adopts the next valid pose. Short tracking glitches
and ordinary yaw turns do not trigger recovery.

**What it changes:**

- Tests the relative head-up axis against the captured origin and recenters
  only after ten consecutive inverted samples.
- Clears the streak when tracking becomes invalid or upright and resets it when
  the reference itself changes.
- Distinguishes an inverted origin from a general image flip; no eye-image flip
  is included in this change.

**Validation:** The XR input fixture covers the ten-sample threshold, transient
and upright resets, ordinary yaw, and reference invalidation. Physical
reconnect coverage remains a hardware follow-up.

[Source and validation receipt](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/06-origin-recovery/docs/internal/upstream/vr-origin-recovery.md)

## 7. VR: preserve precise eye poses and clip near-camera geometry

**Current behavior:** Eye replay loses fractional head translation and
rotation when it reuses the guest's integer GTE view, causing stepped world
motion. Triangles crossing an eye's near plane can also receive the wrong
facing or become an oversized black floor/geometry wedge.

**Desired behavior:** VR replay uses the runtime's exact eye pose, projects
tagged geometry with signed homogeneous depth and facing, and clips it at the
eye near plane. Small physical head motions remain smooth, the nearby world
stays filled, and normal guest rendering keeps its existing behavior.

**What it changes:**

- Extends the stereo view with precise rotation and translation and transforms
  full camera-space accumulators without applying eye translation twice.
- Carries signed camera-space provenance, corrects replay facing before screen
  saturation, and enables the existing projective clipping path in eye passes.
- Scopes PGXP provenance and CPU tracking to the rollback-protected eye
  transaction and restores the previous modes after success, failure, or abort.
- Keeps unsupported provenance, HUD capture, and ordinary guest GTE execution
  on their existing fallback paths. Consumers must rebuild because the render
  view structure changes.

**Validation:** Six focused GTE, GPU, pose, sandbox, and abort fixtures pass,
along with the production GPU-history guard. Historical Quest 3/VDXR testing
accepted the floor coverage and head-motion stability. The correctness path
adds rendering cost, and this PR does not claim a performance improvement.

[Source and validation receipt](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/07-projective-replay/docs/internal/upstream/vr-projective-replay.md)

## 8. VR: report XR readiness without the debug server

**Current behavior:** Production startup inspection depends on the heavyweight
debug server and TCP. Debug-free builds therefore have no compact machine
readable confirmation that state loading settled and XR submitted a real frame.

**Desired behavior:** A production build can write one opt-in startup receipt
after state loading and actual XR submission, and can optionally log lightweight
VR counters. Normal runs perform no recurring file I/O or diagnostic work.

**What it changes:**

- Adds `PSX_OPENXR_READY_FILE`, which atomically records runtime, tracking,
  submission, empty/failure counters, frame identity, and savestate load result.
- Adds opt-in `PSX_RUNTIME_PERF_DIAG` counters for pair, shed, empty, failure,
  pair-time EMA, and rollback verification.
- Leaves readiness writing disabled unless a fresh output path is requested and
  continues to report the existing guest-frame cadence separately from XR FPS.

**Validation:** A Release/OpenGL/OpenXR/PGXP Medal of Honor build succeeds with
debug tools off. Fresh-boot headless readiness and missing-slot refusal pass.
The older slot-2 fixture does not load on current upstream; that integration
does not change save headers or compatibility checks.

[Source and validation receipt](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/08-production-readiness/docs/internal/upstream/vr-production-readiness.md)

## Submission notes

All eight branches are framework-only and game-agnostic. Medal of Honor consumes
the series through a framework pin bump and rebuild; no game code generation or
BIOS regeneration is required. PR 7 changes GTE replay execution identity, so
old emulator savestates may be rejected after that change and compatibility
checks must remain enabled.

The commits retain Yves `<calibratedbeats@gmail.com>` as author and include the
source/validation receipts linked above. While this stack is under review,
Medal of Honor remains pinned to the combined `vr-head-pitch` development
branch at `9b2100fe`, which includes the accepted fork baseline and current
upstream `9caf4c22`. After all eight PRs merge, repin the submodule to upstream
master and rebuild against the merged API.
