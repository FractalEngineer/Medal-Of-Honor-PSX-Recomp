# VR upstream PR drafts

Eight stacked pull requests for `RetroPortingToolKit/psxrecomp`. The stack is
based on upstream `e3705227`. Each feature branch contains one commit relative
to its parent branch.

| PR | Feature branch | Parent | Current head |
| --- | --- | --- | --- |
| [#739](https://github.com/RetroPortingToolKit/psxrecomp/pull/739) | `vr-pr/01-frame-tracing` | upstream master | `8400b55d` |
| [#740](https://github.com/RetroPortingToolKit/psxrecomp/pull/740) | `vr-pr/02-bounded-shedding` | PR 1 | `0cdf9595` |
| [#741](https://github.com/RetroPortingToolKit/psxrecomp/pull/741) | `vr-pr/03-pair-checkpoint` | PR 2 | `4419d234` |
| [#742](https://github.com/RetroPortingToolKit/psxrecomp/pull/742) | `vr-pr/04-paired-hud-capture` | PR 3 | `e4f1f985` |
| [#743](https://github.com/RetroPortingToolKit/psxrecomp/pull/743) | `vr-pr/05-synthetic-head-pose` | PR 4 | `18d52c8b` |
| [#744](https://github.com/RetroPortingToolKit/psxrecomp/pull/744) | `vr-pr/06-origin-recovery` | PR 5 | `689f3b34` |
| [#745](https://github.com/RetroPortingToolKit/psxrecomp/pull/745) | `vr-pr/07-projective-replay` | PR 6 | `510560e5` |
| [#746](https://github.com/RetroPortingToolKit/psxrecomp/pull/746) | `vr-pr/08-production-readiness` | PR 7 | `5c68581a` |

## 1. VR: add opt-in stereo and XR frame attribution

**Current behavior:** Stereo and OpenXR performance is visible mainly as a
whole-frame total. A slow frame cannot be attributed reliably to guest scene
traversal, checkpointing, either eye, HUD capture, OpenXR waits, or submission.

**Desired behavior:** An opt-in trace records one compact row per stereo frame
and exposes stage costs and state through TCP as JSON or a Chrome/Perfetto
trace. Normal production play remains untraced and pays only the cost of a
disabled branch.

**Changes:**

- Adds a fixed-capacity trace ring with pair identity, per-eye checkpoint,
  guest-draw and restore timing, failure/stale/shed state, guest cycles,
  primitive counts, texture-upload deltas, and atomic publication results.
- Separates HUD capture, draw, and readback timing; records OpenXR cadence and
  wait/acquire/copy/submit stages without presenting CPU timings as GPU time.
- Adds guarded nested span markers so a game can attribute world traversal or
  visibility selection inside an eye.
- Keeps allocation and file I/O out of the record path.

**Validation:** Trace, allocation/I/O guard, render sandbox, abort, and XR-off
input tests pass on Windows/GCC 13.2 Release.

## 2. VR: bound stereo shedding and recover at the display deadline

**Current behavior:** A stale pair-time estimate can suppress many consecutive
updates after one overloaded frame. The shed state can remain latched after
rendering recovers, producing a held or empty headset image.

**Desired behavior:** Shedding has a bounded duration and streak, periodically
probes with a fresh render, and exits as soon as a complete pair meets the
display deadline. A state or cadence change clears obsolete budget history.
Stereo publication remains all-or-nothing.

**Changes:**

- Extracts the shedding decision into a testable whole-pair policy with
  elapsed-time and consecutive-skip limits.
- Prefers the OpenXR display period for the budget, with an explicit override
  and a guest VBlank fallback.
- Resets the estimate on scene, mode, geometry, and XR lifecycle changes;
  exposes streak, stale age, reset, and budget-source counters.
- Keeps shedding opt-in at the application layer.

**Validation:** Policy, render sandbox, and abort tests pass. The historical
reproduction reduces a pathological 895-frame shed streak to 62 frames.

## 3. VR: reuse one guest checkpoint per stereo pair

**Current behavior:** Each eye copies the same large guest baseline, so the
right-eye transaction repeats CPU, RAM, DMA, timer, and related checkpoint
work already performed for the left eye.

**Desired behavior:** Capture the pair baseline once, restore it after each eye,
and retain the per-eye GPU and precision state required for deterministic
replay. Any failed, mismatched, or unrelated transaction falls back cleanly to
an independent capture.

**Changes:**

- Shares the checkpoint only between the two eyes of one protected stereo pair.
- Verifies restored state before reuse; a mismatch retires reuse for the
  session, while `PSX_STEREO_BASELINE=0` provides the old behavior as a control.
- Adds checkpoint-save, reuse, and copied-byte counters and marks reused
  baselines in the frame trace.

**Validation:** Trace, render sandbox, and abort tests pass, including shared
baseline restoration, per-eye precision, explicit opt-out, and failure cleanup.

## 4. VR: capture both HUD mattes with one checkpoint and readback

**Current behavior:** Producing the two native-HUD mattes uses two separate
capture calls, two checkpoint baselines, and two GPU readbacks, adding
synchronization and rollback work to the critical path.

**Desired behavior:** One paired capture API renders both mattes from the same
guest checkpoint and returns two contiguous image planes from one readback.
The operation restores renderer state, returns no partial result, and does not
publish or impersonate a stereo world pair.

**Changes:**

- Adds `psx_mod_render_capture_rgba_pair` and a double-height staging/readback
  path for the two RGBA mattes.
- Shares the first matte's checkpoint with the second while keeping that scope
  separate from the later eye transaction.
- Rejects invalid or incomplete captures and updates sandbox/abort stubs.
- Requires consumers to migrate from the single-matte API and rebuild.

**Validation:** Sandbox and abort tests pass with both output buffers and
byte-identical matte checks. Historical test-scene timings moved paired capture
p50 from 3.41 ms to 3.19 ms and p95 from 6.91 ms to 5.10 ms.

## 5. VR: add debug synthetic head translation and pitch

**Current behavior:** Headless runs with a null HMD cannot apply a controlled
headset translation or pitch, so small eye-motion regressions require a person
to move their head to test them.

**Desired behavior:** Debug builds can apply an absolute room-space translation
and optional pitch to both eye poses while leaving normal tracking untouched.
Overrides are bounded, reject invalid input, and can be cleared explicitly.

**Changes:**

- Adds the TCP `openxr_pose_override` control for millimetre X/Y/Z translation,
  coarse `pitch_deg`, and fine `pitch_mdeg`.
- Applies the synthetic pose to both eyes after origin capture and reports the
  exact applied values in JSON.
- Limits translation to 10 metres and pitch to 90 degrees, rejects non-finite
  values, and keeps the feature unavailable outside the debug surface.

**Validation:** XR-off input tests cover bounds, binocular translation, pitch,
disabled behavior, and clearing.

## 6. VR: diagnose ambiguous XR origin poses without recentering

A relative pose being inverted cannot distinguish a bad origin from a
legitimate sustained head roll.

**Current behavior:** The previous candidate automatically changed the XR
reference after ten inverted samples. That also triggered when a user
deliberately held their head rolled or upside down.

**Desired behavior:** Origin changes remain owned by explicit recenter and
OpenXR reference-space change events. The backend reports an inverted-pose
streak as diagnostic data without silently moving the world.

**Changes:**

- Removes automatic recentering from the pose heuristic.
- Exposes `origin_up_dot`, `origin_inversion_streak`, and `origin_ambiguous`
  through `openxr_stats`.
- Clears the diagnostic streak on invalid tracking, explicit recenter,
  reference changes, and session teardown.
- Adds controls for ordinary yaw, sustained 180-degree roll, invalid tracking,
  and reference-change timing.

**Validation:** The focused OpenXR input fixture passes on Windows/GCC 13.2
Release. The sustained-roll control confirms that ambiguous motion does not
request an origin reset.

## 7. VR: preserve precise eye poses and clip near-camera geometry

**Current behavior:** Eye replay loses fractional head translation and rotation,
causing stepped world motion. Near-camera triangles can receive the wrong
facing or become an oversized black geometry wedge.

**Desired behavior:** VR replay uses the exact runtime eye pose, signed
homogeneous projection, and near-plane clipping while preserving the guest and
PGXP state outside each protected eye transaction.

**Changes:**

- Carries precise eye rotation and translation and full camera-space
  accumulators without applying the eye offset twice.
- Corrects replay facing and clips tagged geometry at the signed near plane.
- Opens projective PGXP mode after the precision checkpoint and closes it
  before rollback, preserving the real generation and prior enabled/CPU-mode
  state through success, refusal, and abort.
- Refuses a projective pass if PGXP cannot be enabled, then rolls back the
  complete render transaction.

**Validation:** Native-wide projection, real PGXP scope rollback, projective
refusal restoration, GPU provenance, GTE facing, render sandbox, render abort,
VR pose math, and GP0 history pass on Windows/GCC 13.2 Release. Historical
Quest 3/VDXR testing accepted floor coverage and head-motion stability.

## 8. VR: report XR readiness without the debug server

**Current behavior:** Debug-free builds have no compact machine-readable
confirmation that the requested save load settled and that the current XR
session submitted a real frame.

**Desired behavior:** A production build atomically publishes one receipt per
XR session after the requested load succeeds and that session submits a frame.
Failed, invalid, or refused loads cannot appear successful, and a later XR
session resets readiness.

**Changes:**

- Publishes through a same-directory temporary followed by atomic replacement;
  an existing destination is refused at process start.
- Adds XR session-generation fields so readiness is bound to a submission from
  the current session.
- Waits for the requested `PSX_LOAD_SLOT` outcome and a later current-session
  submission.
- Reports write and close failures without exposing partial JSON.
- Keeps opt-in production performance counters separate from the receipt.

**Validation:** The production runtime builds with debug tools off. Atomic
replacement, OpenXR input, PGXP, render sandbox, render abort, native-wide
projection, GTE register access, and GPU history pass. A headless game run with
an invalid load slot publishes `ready:false` and
`state_load_request_invalid`.

## Submission notes

The stack is under review. PRs 744-746 were revised after maintainer review;
GitHub still shows the previous `CHANGES_REQUESTED` state until the maintainer
reviews the new heads. The game remains pinned to the combined framework fix at
`9fd75a84`. After the eight PRs merge, repin the game to upstream master and
rebuild against the merged API.
