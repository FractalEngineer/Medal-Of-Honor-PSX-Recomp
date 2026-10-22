# VR campaign: changes by proposed PR

Prepared 2026-10-23. This report covers the complete current VR optimization
campaign, including the floor and head-motion fix accepted today. It separates
active source changes, diagnostics, research and reverted experiments.

## Scope and result

| Repository | Baseline (excluded) | Reported tip (included) | Commits | Net files | Net lines |
| --- | --- | --- | ---: | ---: | ---: |
| Medal of Honor | `5822f3b6` (2026-10-15) | `60143e2`, `vr-perf-pr1-pr2` | 95 | 26 | +3663 / -57 |
| psxrecomp | `8d1c983b` (2026-10-15) | `efa10eee`, `vr-head-pitch` | 23 | 31 | +3042 / -107 |

These are each branch's merge bases with locally recorded `origin/master`, not
the start of the entire VR port. Existing stereo, tracked weapons, HUD surfaces
and earlier VR implementation already in those baselines are not new campaign
features. Remote master was not refreshed for this report; submission must rebase
and recheck the actual target. The game pins the reported framework tip.
`recomp-ui` remains `41c88ffea852b970af8d50312620b17404e455ad`, with no changes.

Delivered: attribution tools, bounded shedding recovery, fewer redundant
checkpoints, paired HUD capture, headless diagnostics, inverted-origin recovery,
and the headset-accepted nearby-floor/world-stability fix. The latest visual fix
adds rendering cost in the matched headless scene; it is not an FPS improvement.
Overlay-cache research is not an accepted new timing implementation.

I propose **8 framework PRs and 6 game PRs**. F/G labels below are planning labels,
not existing GitHub PR numbers. This report creates no PRs or split branches.
Several commits span multiple review units, so a clean split requires selecting
hunks and matching tests rather than blindly cherry-picking whole commits.
Historical plan “PR 1–6” labels are separate from this proposed submission series.

## Proposed PR map

| ID | Repository | Proposed title | Dependencies / status |
| --- | --- | --- | --- |
| F1 | Framework | Add opt-in stereo/XR tracing and nested guest attribution | Measurement foundation; implemented, historically tested |
| F2 | Framework | Bound stereo shedding and recover at the display deadline | F1 telemetry; implemented and measured |
| F3 | Framework | Reuse the checkpoint within one stereo pair | F1 counters; implemented and measured |
| F4 | Framework | Pair HUD readbacks and share their checkpoint | F3 helper; image parity and cost measured |
| F5 | Framework | Add synthetic head translation and pitch diagnostics | Existing XR/debug surface; exercised headlessly |
| F6 | Framework | Recover from a persistently inverted XR origin | Independent XR fix; dedicated automated fixture advisable |
| F7 | Framework | Preserve exact eye poses and clip VR geometry projectively | Existing PGXP/projective infrastructure; accepted with G5 |
| F8 | Framework | Document deterministic overlay-cache comparison controls | Research; consolidate superseded conclusions before submission |
| G1 | Game | Correct menu Back and launch the current release build | Independent; Pace variant comments need correction |
| G2 | Game | Export traces and attribute whole-session VR hitches | F1/F2/F3 telemetry |
| G3 | Game | Use the paired native-HUD capture API | F4; coordinate framework pin and consumer migration |
| G4 | Game | Add headless reproduction and projection/visibility controls | F5; older scripts need portability/process-ownership cleanup |
| G5 | Game | Enable the accepted floor and world-stability fix | F7; Release build and headset accepted |
| G6 | Game | Publish evidence, retractions and accepted baseline | Final assembled behavior and limitations |

General runtime behavior belongs in psxrecomp. Medal of Honor addresses, tree
traversal and Quest-profile reproduction remain game-side. Each consumer PR must
pin a framework commit that contains the APIs it uses.

## F1 — Framework tracing and attribution

**Problem/result:** Previously the budget could not be attributed to guest draw,
checkpointing, HUD capture or XR submission. Add a fixed-capacity trace ring,
disabled by default, without allocation or file I/O in its record path. Configure
with `PSX_FRAME_TRACE` / `PSX_FRAME_TRACE_CAP`. Export JSON and Chrome/Perfetto
through TCP; game tools produce CSV and summaries.

Changes:

- Record pair/publish identity, per-eye begin/guest/end/restore, failures,
  shed/stale flags, guest cycles, primitive counts and texture-upload deltas.
  Add a cumulative GL primitive accessor for eye-specific differences.
- Time XR wait/begin/acquire/wait-image/copy/release/end and close trace records
  on failure. These are CPU elapsed/submission times, not GPU execution times.
- Record locate-to-end pose age, predicted-display-time deltas and fresh/repeat
  fields; clear previous display timestamps when destroying a session. This
  measures cadence without adding an independent headset-rate scheduler.
- Add game annotation and stage begin/end APIs. Accept spans only inside traced
  stereo eyes; exclude HUD captures/flat draws. Support nested selection within
  traversal and discard unfinished spans at eye boundaries.
- Attribute HUD capture/draw/readback separately. Register source files, tests,
  the allocation/I/O guard and field/command documentation.

**Source:** `022eb1ed`, `0e0d2de8`, `5c67dced`, `63271ce7`, `571a8f10`,
`87a3e822`. Shared files and all tests are inventoried below.

**Evidence:** Ring/guard tests cover disabled behavior, wrap, monotonic timestamps,
exports and missing telemetry; sandbox tests cover eye/nested spans. Historical
XR cadence measured 1.500 granted periods/frame versus 1.49 from the host clock.
Traversal explained 97.8% of the measured slow-eye increase; host selection was
0.9% of traversal in that capture. These are scene/window observations.

**Review:** Preserve field ordering and nested-stage containment to avoid double
counting. Rerun the complete F1 tests after splitting; today's targeted rendering
validation did not repeat every historical tracing acceptance run.

## F2 — Framework shedding recovery

Replace the retry-counter rule, which could suppress 29 requests after an overload,
with a separately testable whole-pair policy. Bound suppression by elapsed time
and consecutive skips, defaulting to 250 ms / 8 skips. A forced probe replaces the
stale estimate. Reset on scene/state, geometry/cadence changes and XR wait/begin
failures. Keep both-eye publication atomic.

Choose budget in order: `PSX_STEREO_SHED_BUDGET_MS`, runtime display period,
guest-VBlank fallback. Expose current/max streak, stale count, retained-pair age,
reset count, policy state and budget source. Add diagnostic disable, probe,
streak and hysteresis controls.

Include the later latch fix: a recovery probe exits when it meets the deadline,
even inside the hysteresis band. The earlier lower-band test could stay latched.

**Source:** `cae62891`, `b6a671c1`; game transition calls belong to G2.
**Evidence:** Historical 1024-frame headless latch reproduction: sheds 895 -> 62,
published pairs 3.8 -> 28.4/s at an 11.111 ms budget. Policy tests cover bounds,
probes, age and hysteresis. This fixes pathological suppression, not all pacing.

## F3 — Framework stereo checkpoint reuse

The right eye recopied bulk guest state already captured/restored for the left.
Capture RAM, CPU, scratchpad, instruction cache, DMA, timers and nesting state
once per pair; restore after each eye and retain per-eye GPU/precision arms.

Invalidate reuse across pairs, unrelated transactions and failures. Check buffer
geometry. `PSX_STEREO_BASELINE=0` restores per-eye capture; a verified mismatch
during reuse retires it for the session. Add save/reuse/RAM-byte counters and flag.

**Source:** `063b082a`. **Evidence:** Right-eye begin p50/p95 143/178 -> 36/52 us;
left-eye begin stayed approximately 154 us. Tests cover shared state, precision
arms, bytes and opt-out; historical sandbox/abort/freeze/plan tests passed. The
whole-pair medians in that A/B are not comparable because scene load differed.
No dirty-page restoration or extra simulation ticks were introduced.

## F4 — Framework paired HUD capture

Replace two single-matte capture calls with `psx_mod_render_capture_rgba_pair`:
two callback users, a contiguous two-plane RGBA buffer, two staged draws and one
double-height GL readback. Restore framebuffer/texture/pixel-pack state and reject
invalid/incomplete captures. Captures do not publish a stereo pair.

Share the first matte's bulk checkpoint with the second, in a scope separate
from the later eye pair. Start/end with no reusable baseline; changed guest cycles
or stores between mattes force a fresh capture. Exclude HUD work from eye traces.

**Source:** `a876bae3`, `d2554481`; G3 migrates the known consumer. This replaces
public function/macro names, so review external consumers before upstreaming.

**Evidence:** Black/white mattes and reconstructed panels were byte-identical.
Single readback moved capture p50/p95 3.41/6.91 -> 3.26/5.93 ms; checkpoint sharing
then moved it to 3.19/5.10 ms. HUD draw p50 1220 -> 1087 us. Median readback barely
improved; the original predicted 0.5–0.8 ms median gain did not materialize.
Sandbox and abort fixtures were updated.

## F5 — Framework synthetic head diagnostics

Add debug-gated `openxr_pose_override` for room-space millimeter translation and
pitch applied to both eyes after origin capture. Default off, finite/range
validated and clearable. This exercises true head motion on a fixed null HMD;
game look input changes a different camera. Add `pitch_mdeg` for fine rotation
sweeps while retaining `pitch_deg`, with decimal pitch in JSON replies.

**Source:** `2ed12640`, `a3714fad`, debug-server hunk of `efa10eee`.
The override application is excluded from debug-free XR use. Latest translation
and rotation receipts exercise these controls. No normal tracking override is on
unless the diagnostic is explicitly armed.

## F6 — Framework inverted-origin recovery

A SteamVR session had an inverted world while HUD/weapon were correct. Live
recentering repaired the measured relative eye matrix, identifying the captured
origin rather than a blanket image-orientation defect.

Detect a strongly inverted relative head-up axis for ten consecutive frames and
drop the origin so the current pose becomes the reference. Normal upright yaw
turns do not meet the condition.

**Source:** `96981469`. The image-flip experiment `4dd9ca6d` was reverted by
`928c721c` and is excluded. **Evidence/limits:** Measured matrices and recenter
captures are in the status/Phase 9 notes. A dedicated automated persistence/reset
fixture is not recorded; add one and review intentional inverted-head use and XR
space transitions before presenting this as broadly validated upstream behavior.
Today's floor acceptance alone does not test every origin-recovery edge case.

## F7 — Framework exact poses and projective eye rendering

Integer eye transforms caused motion plateaus. Screen saturation and lost signed
camera depth could corrupt facing at the eye and leave a near-floor wedge.
Preserve exact host poses and signed homogeneous projection during eye replay.

Every functional change in the accepted framework commit:

1. Extend `PSXModRenderView` with a precise flag, double rotation[9] and
   translation[3]. Pose math fills exact and legacy fields; render entry validates
   finite/range/flag values. Consumers must rebuild; old smaller structs fail the
   size check.
2. Transform full RTPS camera accumulators with the exact eye pose and round at
   1/4096 guest unit. Do not add integer eye translation again: this fixes the
   double-offset regression discovered during development.
3. Derive eligible replay screen coordinates from full accumulators instead of
   truncated IR/SZ, retaining per-eye focal/center and authored-H ratio.
4. Attach signed homogeneous X/Y/Z with near Z = 1 through PGXP provenance.
   Correct replay NCLIP facing with the homogeneous determinant where saturated
   screen winding disagrees. Ordinary guest GTE execution keeps its existing path.
5. Extend the existing OpenGL projective path to active eye passes at ordinary
   aspect ratios. Clip all eligible tagged replay triangles, split quads using
   existing order and use perspective texture correction. Invalid/unknown
   provenance and unsupported backends retain fallback behavior.
6. Add projective request/scope services, preserving projection tracking if either
   native-wide or VR needs it. Temporarily enable PGXP/CPU tracking around real
   eyes and restore prior modes after rollback, checkpoint failure or abort.
   Exclude HUD-only capture; disable the request on session reset.
7. Recognize active eye FBO support without requiring native-wide output; add
   packet/draw/provenance-miss counters.
8. Extend GTE/GPU/pose/sandbox/abort tests and stubs. Repair duplicate/missing GP0
   history fixture stubs so its real-GPU debug/production test works. Update the
   session log. Move the millidegree debug hunk to F5 in the proposed clean split.

**Source:** `efa10eee`, currently one accepted combined commit. PGXP and homogeneous
clipping infrastructure predate this campaign; this connects/scopes them for VR.

**Validation:** Release/OpenGL/OpenXR build and targeted GTE, GPU, pose, sandbox,
abort, frustum, GP0 history and Python checks passed. Tests cover signed depth,
winding, fractional pose, double translation, invalid provenance, fallback and
mode restoration. Final scoped capture: 757 rollback checks, zero mismatches.
With G5, every 1 mm interval moves and the floor is filled; owner headset acceptance
covers both reported symptoms. Performance and save limits are below.

## F8 — Framework comparison-method documentation

Six documentation commits (`f5229748`, `1cb15cf4`, `ce7921f6`, `0fe8a425`,
`85c676d2`, `59c3fb3d`) extend `tools/divergence/README.md`: load/sub-frame
confounds, boot-seeded loads/recording, frame/access/value/timing comparison and
pinning shedding for reproducible cross-backend tests.

Keep the methodological result: seed before boot, pin input/shedding, establish
same-backend reproducibility, then locate the meaningful first divergence. A
backend timing/ordering difference reproduced with those controls. Later game
notes refine the mechanism and limits. Consolidate superseded publication/cycle
hypotheses before an upstream documentation PR. There is no new emitter
implementation in the current framework campaign delta.

## G1 — Game menu and launcher corrections

- Map VR B/menu Back to PSX Triangle (`0x1000`) instead of Circle (`0x2000`),
  matching the game's menus. Update the input test.
- Point Oculus, SteamVR and VDXR launchers from `build-vr-wrist` to `build-release`,
  preserving runtime and weapon/HUD options.
- Add `RunVR-SteamVR-Pace.bat`, omitting wrist replacement to keep overlay content
  signatures compatible with captured shards in that investigation.

**Source:** `389911a`, `7bd36ea`, `928b42c`. The Pace file's comments claim a
budget/reprojection benefit that is not the accepted current fix. Correct those
comments and decide whether to retain it as diagnostic before upstreaming. It
does not install or guarantee a compatible shard cache by omitting HUD flags.

## G2 — Game tracing and hitch reports

Add `trace_frames.py` for raw JSON, CSV, optional Perfetto and summaries of stages,
stale/shed/failure counts, cadence and pose age. Add `record_session.py` to wait
before boot, sample/flush live counters, keep raw windows and expose stalls or
disconnects. Add `hitch_report.py` to deduplicate overlapping records and compare
hitch frames against the rest, including nested traversal/selection attribution.

Bracket game replay environment, world, text, traversal, icon, submission and
laser calls. Nest the host visibility override span inside traversal and close
early-return spans. Reset shedding estimates on scene/state changes and annotate
transitions. These consume F1/F2; normal tracing stays off.

**Principal source:** `046215d`, `288134f`, `08334ad`, `ed4a0d1`, `3c4d769`,
`e416d45`, `d86a678`, `2850982`, plus telemetry pin commits. This provides evidence,
not an FPS gain. Export/readback can disturb sampled frames; retain recorded
instrument-overhead/cross-session limits and use quiet performance windows.

## G3 — Game paired native-HUD consumer

Replace two captures/two arrays with the F4 API and contiguous
`g_hud_mattes[2][...]` storage. Supply separate black/white contexts, pass both
halves to existing reconstruction, and invalidate output on failure. Use feature
macro `PSX_MOD_RENDER_CAPTURE_RGBA_PAIR`. Pin F4's implementation.

**Source:** `efa32b4`, `48fc640`, supporting evidence commits. Acceptance was
byte-identical mattes/panels. This changes capture transport, not HUD layout or
artwork. Coordinate the consumer migration with the removed single-matte API.

## G4 — Game headless reproduction and controls

Added tools:

- `headless_hold.bat`: bounded null-HMD probing/dump session.
- `look_sweep.py`: scripted game look/input cases and screenshots by backend.
- `move_series.py`: per-frame level-producer GTE matrix series during movement.
- `headbob.py`: frontal/sagittal synthetic head-position circles with screenshots,
  guest views and runtime pose receipts.
- `recapture_overlays.py`: capture loaded overlay variants, explicitly flush their
  index, and gate comparisons on actual registration rather than assuming it.

Added game hooks:

- `PSX_VR_EYE_SCALE`: diagnostic eye-translation scale, default 1.
- `PSX_VR_EYE_PROFILE=quest3`: measured asymmetric eye projection replay for a null
  HMD; normal play uses actual runtime projections.
- `PSX_VR_HEAD_FRUSTUM>=2`: bypass PVS/screen-plane rejection in the tree collector;
  ordinary 1 keeps conservative eye-frustum tests. All-tree inclusion ruled out
  selection for the reproduced wedge and is not the shipped fix.
- `PSX_VR_NO_AUTHORED_FOCAL`: bypass the launcher's forced authored focal for
  diagnosis. The temporary authored-H-reference sweep knob was removed.

**Principal source:** `701f26a`, `31634a3`, `2a22573`, `1e08b25`, `2f96631`,
`52582c6`, `e54f35e`, `02d331d`, `96047f5`.

**Before routine/shared use:** Older harnesses have a developer-specific absolute
checkout path, broad process-name kills and local shard-artifact manipulation.
Make paths workspace-relative, manage only owned processes and handle artifacts
explicitly. Do not run them over an existing user game/SteamVR session. This
report does not run them. Correct intermediate causal comments superseded by F7.
No source-tracked game assets or new shard compiler are delivered by these tools.

## G5 — Game accepted rendering integration

Every functional integration change in `60143e2`:

- Add default-on CMake `MOH_PGXP` and pass the `PGXP` runtime option. Existing OFF
  build caches require explicit ON configuration. Compile hooks globally while
  F7 scopes active tracking to eyes.
- Default projective mode on when PGXP is compiled. `PSX_VR_PROJECTIVE=0` is the
  comparison path; unsupported builds decline explicit requests with a counter.
- Arm replay at activation/frame boundary. Clear exact-pose flags for the off
  control and scale exact translation alongside integer eye/weapon scaling.
- Use exact eye rotation/translation consistently in world box tests, tracked
  laser capture, world-ray conversion and hit-dot reprojection.
- Add mutually exclusive `-ProjectiveDiagnostic` / `-NoProjectiveDiagnostic` and
  `-MemcardDirectory` for isolated states/writes.
- Add `check_world_stability.py`: use an existing owned session, optionally load
  an isolated slot, sweep translation/pitch, wait for fresh atomic eye dumps,
  compare a world ROI, record poses/hashes/deltas/PGXP/rollback stats and clear the
  override on exit. This measures continuity, not motion-to-photon latency or
  exhaustive mission fidelity.
- Pin F7 `efa10eee`; record accepted defaults in G6.

**Acceptance:** Owner confirmed nearby floor and world shakiness both fixed in a
30-second Quest 3 / VDXR check on 2026-10-23. The same behavior was promoted to
normal defaults and rebuilt. See [VR_PROJECTIVE_RENDERING.md](VR_PROJECTIVE_RENDERING.md)
for precise recipes and local screenshot/receipt paths.

## G6 — Game evidence and accepted baseline

All six changed documents are inventoried below: optimization plan/status,
projective receipt, handoff, alpha backlog and Phase 9 notes. They record source
pins, measurements, reproduction recipes, retractions, acceptance and limits.
The original floor/stability backlog items are checked; broader pop-in,
ruins/streaming and sector coverage remain open.

Most of the 95 game commits are receipts, retractions and pin bumps, not separate
runtime features. Preserve their evidence while presenting current findings first.
Historical “next steps”/causal claims below the status file's current banner are
not all current. An upstream-facing summary should consolidate them. Raw game
screenshots/captures remain in ignored local evidence directories, outside PRs.

## Measurements and validation

| Measurement | Before | After | Meaning |
| --- | ---: | ---: | --- |
| Shedding latch, published pairs/s | 3.8 | 28.4 | Historical matched pathological case, F2 |
| Right-eye begin p50/p95 | 143/178 us | 36/52 us | F3 stage gain; pair-total scene was not matched |
| HUD capture p50/p95 | 3.41/6.91 ms | 3.19/5.10 ms | F4 series with byte-identical panels |
| Floor bottom 6% black band L/R | 100% / 91.72% | 0% / 0.74% | Includes residual weapon pixels; not a universal coverage metric |
| 1 mm sweep meaningful intervals L/R | 5/20, 4/20 | 20/20, 20/20 | Quantized versus exact pose |
| Translation-delta variation L/R | 173% / 200% | 3.4% / 2.6% | Coefficient of variation in world ROI |
| 0.01-degree pitch sweep | — | 20/20 per eye; CV 3.7%/4.9% | Rotational continuity |
| Final scoped rollback checks | — | 757 checks, 0 mismatches | Captured test window |
| Headless pair p50/p95, projective off/on | 11.552/12.736 ms | 16.822/18.393 ms | Visual fix adds 5.27 ms at median |
| Same headless guest interval p50 | 33 ms | 40 ms | Performance work remains open |

Performance A/B used the same PGXP binary, slot/settings, verification/shedding
off, 35 seconds warmup then a quiet 30-second trace with no screenshot readbacks.
Do not add gains from unrelated historical windows together as one FPS result.

Stationary control had no meaningful ROI motion but tiny pixel flicker; full-image
hashes are not universally identical. Forest/bunker, up/down and 60/120 mm offsets
were visually checked, without claiming exhaustive mission parity. The moving
headset session is not a matched performance A/B: zero XR/tracking/surface
failures, with 9 sheds/10 empty submissions at startup and no later growth.

Latest checks passed: Release/OpenGL/OpenXR build; GTE architectural/projective
regressions; GPU provenance/projection/fallback/scope tests; pose/frustum checks;
sandbox/abort; GP0 history debug/production guard; harness Python compilation.
Historical package tests are identified above. Writing this report did not rerun
the full framework/upstream regression matrix.

## Compatibility, reverted work and open work

- F4 replaces a public capture API; F7 grows a public view struct. Rebuild mods and
  coordinate game/framework pins; old binary consumers are not assumed compatible.
- GTE source identity and PGXP ABI flavor alter savestate compatibility. Original
  personal saves remained untouched. Tests used explicitly copied/rebased render-only
  fixtures with unchanged payloads; that is not general state migration. Keep
  compatibility checks and create fresh current-build states for ordinary use.
  Memory cards are unaffected.
- Fixtures, screenshot assets and local overlay DLLs are outside source PRs.
- Guest-projection replacement `0381cd8` was reverted by `5be5f13`; a filled image
  obtained by changing/cropping projection was rejected geometrically.
- Emitter budget pin/regeneration `05cbaf6` / `0e33ab9` were reverted by
  `c8e1a3e` / `38ee058`. No net generated-game-C change or accepted interrupt-budget
  implementation remains in this reported range. Separate timing branches and
  local shard caches are outside the delivered source delta.
- Framework blanket image flip `4dd9ca6d` was reverted by `928c721c`; game pins
  `9f23631` / `afb1eb8` reflect that experiment. F6 is the retained origin fix.
- Authored-H-reference knob `401a9e9` / `7d97173` is absent in the final tree.
  Earlier margin/clamp/selection diagnoses are historical hypotheses, not the
  accepted implementation.
- Independent headset-rate scheduling, further HUD transfer optimization, broad
  mission/ruins/streaming checks and portable overlay acceleration remain open.
  Future optimization must preserve the accepted floor and motion stability.

## Submission order

1. F1 measurement, then F2/F3 and G2 with their matching APIs and scene-reset calls.
2. F4/G3 with matching consumer pins.
3. F5 and cleaned G4 reproduction tools; F6 as a separate XR recovery review.
4. F7/G5 as the paired accepted rendering delivery. This priority fix can also be
   proposed against a suitable existing dependency base without waiting for every
   research-document cleanup.
5. G1 independently; consolidated F8/G6 evidence around the final assembled series.

The accepted fork branch commits are already pushed. This report does not rewrite
them, open PRs or claim an upstream merge status.

## Appendix A — Every net changed game file

Relative to the game repository. Shared rows name every proposed owning unit.
The submodule pin counts as one changed game entry.

| File | Proposed PR | Change |
| --- | --- | --- |
| `CMakeLists.txt` | G5 | Default-on MOH_PGXP build option. |
| `RunVR-Oculus.bat` | G1 | Use build-release. |
| `RunVR-SteamVR-Pace.bat` | G1 | No-wrist diagnostic variant; comments need correction. |
| `RunVR-SteamVR.bat` | G1 | Use build-release. |
| `RunVR-VDXR.bat` | G1 | Use build-release. |
| `docs/reverse/VR_ALPHA_TODO.md` | G6 | Evidence, acceptance, scope or historical diagnosis. |
| `docs/reverse/VR_HANDOFF.md` | G6 | Evidence, acceptance, scope or historical diagnosis. |
| `docs/reverse/VR_OPTIMIZATION_PLAN.md` | G6 | Evidence, acceptance, scope or historical diagnosis. |
| `docs/reverse/VR_OPTIMIZATION_STATUS.md` | G6 | Evidence, acceptance, scope or historical diagnosis. |
| `docs/reverse/VR_PHASE9_STATUS.md` | G6 | Evidence, acceptance, scope or historical diagnosis. |
| `docs/reverse/VR_PROJECTIVE_RENDERING.md` | G6 | Evidence, acceptance, scope or historical diagnosis. |
| `psxrecomp` | G2/G3/G4/G5 | Pin framework delivery; split pins follow API availability. |
| `vr/check_world_stability.py` | G5 | Fine head-motion sweeps and atomic eye receipts. |
| `vr/headbob.py` | G4 | Synthetic head-position circles. |
| `vr/headless_hold.bat` | G4 | Bounded headless launch. |
| `vr/hitch_report.py` | G2 | Deduplicated hitch/guest attribution. |
| `vr/look_sweep.py` | G4 | Scripted game-look captures. |
| `vr/moh_vr_frustum.h` | G4/G5 | All-tree diagnostic; exact box-test pose. |
| `vr/moh_vr_input.c` | G1 | B maps to Triangle in VR menus. |
| `vr/move_series.py` | G4 | Per-frame GTE matrix movement series. |
| `vr/psx_vr_stereo.c` | G2/G3/G4/G5 | Trace/reset hooks, paired HUD, controls, projective/laser integration. |
| `vr/recapture_overlays.py` | G4 | Capture-index/registration reproduction. |
| `vr/record_session.py` | G2 | Continuous counters and raw trace windows. |
| `vr/run_vr.ps1` | G5 | Projective controls and isolated memcard directory. |
| `vr/test_moh_vr_input.c` | G1 | Expected Back mapping. |
| `vr/trace_frames.py` | G2 | Trace export, summaries, cadence and flags. |

## Appendix B — Every net changed framework file

Paths are relative to psxrecomp.

| File | Proposed PR | Change |
| --- | --- | --- |
| `docs/FRAME_TRACE.md` | F1 | Trace configuration, fields and interpretation. |
| `docs/STEREO_RENDERING.md` | F2 | Shedding policy/controls. |
| `docs/TCP_COMMANDS.md` | F1/F2 | Trace command and stereo telemetry. |
| `docs/internal/FAITHFUL_TIMING_PLAN.md` | F7 | Accepted rendering session log. |
| `runtime/CMakeLists.txt` | F1/F2 | Trace/policy tests and sandbox/abort dependencies. |
| `runtime/include/frame_trace.h` | F1/F3 | Records/stages/API and reuse flag. |
| `runtime/include/gpu_gl_renderer.h` | F1/F4 | Primitive accessor and paired readback declaration. |
| `runtime/include/mod_plugins.h` | F1/F2/F4/F7 | Trace/budget/capture/view/projective APIs. |
| `runtime/include/render_pass.h` | F2/F3 | Policy, age and checkpoint stats. |
| `runtime/include/stereo_shed_policy.h` | F2 | Policy contract/state. |
| `runtime/runtime.cmake` | F1/F2 | Compile trace/policy sources. |
| `runtime/src/debug_server.c` | F1/F2/F3/F5 | Trace export, policy/checkpoint stats, synthetic pose/pitch. |
| `runtime/src/frame_trace.c` | F1/F3 | Ring, exports, cadence and reuse telemetry. |
| `runtime/src/gpu.c` | F7 | Scoped PGXP and projective packet dispatch. |
| `runtime/src/gpu_gl_renderer.c` | F1/F4/F7 | Primitive counters, paired readback, eye FBO support. |
| `runtime/src/gte.cpp` | F7 | Exact RTPS, signed projection, homogeneous facing. |
| `runtime/src/psx_openxr.c` | F1/F2/F5/F6 | Trace/cadence/budget, synthetic pose, origin guard. |
| `runtime/src/render_pass.c` | F1/F2/F3/F4/F7 | Trace/policy, shared baselines, paired capture, precise validation/scope. |
| `runtime/src/stereo_shed_policy.c` | F2 | Bounds, authoritative probes and deadline exit. |
| `runtime/src/vr_pose_math.c` | F7 | Populate exact eye fields. |
| `runtime/tests/gpu_exec_stubs.inc` | F7 | Projective GPU test dependencies. |
| `runtime/tests/test_frame_trace.c` | F1 | Ring/export/cadence coverage. |
| `runtime/tests/test_frame_trace_guard.py` | F1 | No-allocation/I/O guard. |
| `runtime/tests/test_gpu_gp0_history_guard.py` | F7 | Repair duplicate/missing gate fixture stubs. |
| `runtime/tests/test_gte_register_access.cpp` | F7 | Signed/fractional/facing/double-translation tests. |
| `runtime/tests/test_native_wide_projection.c` | F7 | VR provenance/fallback/scope restoration. |
| `runtime/tests/test_render_pass_abort.c` | F1/F2/F4/F7 | API/dependency updates to abort fixture. |
| `runtime/tests/test_render_pass_sandbox.c` | F1/F3/F4/F7 | Nested spans, baselines/capture, precise validation/scope. |
| `runtime/tests/test_stereo_shed_policy.c` | F2 | Bounds/probe/age/hysteresis tests. |
| `runtime/tests/test_vr_pose_math.c` | F7 | Subunit pose precision. |
| `tools/divergence/README.md` | F8 | Comparison protocol, investigation and retractions. |

## Appendix C — Complete commit inventories

Chronological, baseline excluded and reported tip included. Includes reverts,
pin bumps and documentation-only steps. Net delivered behavior and proposed PR
ownership are above; a historical subject is not a claim its diagnosis remains
correct. Every campaign commit is listed even if it has no net runtime effect.

### Game: 95 commits

| Commit | Recorded change |
| --- | --- |
| [046215dc](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/046215dcd8af96d174183b4e3c01a8b728f8cd0a) | Pin framework frame trace and add its game-side exporter |
| [288134f5](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/288134f5fe74911395e46a15470027d8c718f00f) | Pin bounded shedding recovery and reset the estimate on scene changes |
| [389911aa](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/389911aa27893f14c422c374b2c23154843418b4) | Map the B button to Triangle in VR menus |
| [2d360442](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/2d360442e550705a3c4aecc141f79f23f933c8a9) | Pin pair-scoped stereo checkpoint reuse |
| [7bd36eac](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/7bd36eac4c14838db05dd4593bf3348b18f0c339) | Point the VR launchers at build-release |
| [08334ad7](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/08334ad7e120e58f495148bf47475d32e4bfeefd) | Pin frame-trace XR cadence and report the granted-period miss rate |
| [f489f2d6](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/f489f2d6b8b038f2cdcd9b8c334b1151585b9872) | Record the VR optimization campaign status and measured evidence |
| [902c0184](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/902c0184e0eb6663797e3231fcaf3d84fa84f9a9) | Pin the stereo shedding deadline exit |
| [8b77fae3](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/8b77fae31d29893ed884dafd4bd2b0212508ac72) | Pin the HUD capture trace stage and report it |
| [8f89b68a](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/8f89b68abca03732d89a8247511372ae9862a34e) | Record the shedding latch fix, the headless harness and the HUD cost |
| [9a146f5e](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/9a146f5ed16ad41686d6dd0c3fd0b9d25c200f4b) | Pin the HUD capture stage split |
| [c3f82bcf](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/c3f82bcf34e09760afb2ea334c696cfbb5131e02) | Record where the HUD capture cost actually is |
| [bfb7e003](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/bfb7e0034f8d974762347e5da5209ad82e267259) | Record the chosen HUD capture change and its design |
| [efa32b4d](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/efa32b4dad30378668e90fbd6f800f5aaa6f60cf) | Pin the single-readback HUD capture |
| [6031f0ae](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/6031f0ae785d376dafaa3694365e2ac23d176de6) | Record the single-readback outcome and the save-screen trap it exposed |
| [59dfca07](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/59dfca071a904e94666a4470191e318b6422871a) | Bring the campaign status up to date |
| [48fc6400](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/48fc6400d1530784059889d50587459057f4e77b) | Pin the shared capture checkpoint |
| [2c7942e8](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/2c7942e835551f58d5d9ad9ec9e88890b40a582f) | Record the shared capture checkpoint and its measured result |
| [ed4a0d1a](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/ed4a0d1a69afb67ed752ac426c1dccf1472d552e) | Record a headset session and attribute its stutters |
| [a055b34a](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/a055b34af47a1f13354f2d9e4414a100c36f50b8) | Record the headset sessions, the panels and where the stutters are |
| [3c4d7693](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/3c4d7693bf29983c80c58e5953023e9a596721db) | Bracket the per-eye replay so the frame trace can attribute it |
| [e416d459](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/e416d45926aa7b79eb7f13a55a19ae8adbfa7d62) | Read the per-eye replay split in the attribution report |
| [f7dae7f5](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/f7dae7f59ee251685728f2c76db90c4f0d756f8e) | Record the per-eye replay split and its first reading |
| [ad796408](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/ad796408fd44e223adaec69629c25ef4838ace4f) | Attribute the stretch to one guest call, and to the machine rather than the host |
| [00ebe30d](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/00ebe30d712fac1c7f571deaa08cfdbe884edad1) | Record what the shakier session measured as, and rule smoking guns in or out |
| [d86a6782](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/d86a6782f9ef23e83373917408b9f09785d103be) | Bracket the hook's visibility override so the trace can split the traversal |
| [2850982c](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/2850982c8d3d0d120c226f77ebdc58a762e470c4) | Read the traversal's own split, and bound the instrument by measurement |
| [60391fa7](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/60391fa7c79ed8d8fbc352c06d11c22e40c7e8e0) | Measure where the traversal's guest work goes: it is the framework's interpreter |
| [bafdba9d](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/bafdba9d39583b6fbb958e1cc63055461dd9f431) | Route PR 6 to the overlay cache, with the capture already in hand |
| [8e4954b3](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/8e4954b35312eb4ed98172c25f2078c6104b6d87) | Compile the overlay cache: a large win, and a fidelity fork it cannot cross yet |
| [557c43f3](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/557c43f3166c2c38b90bf92359b78c56a30410e1) | Pin the framework at f5229748 |
| [ea80e78c](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/ea80e78cf5cbd1a78cc18686bbdfd3da5e0763e6) | Record the load-frame trap, and stop reading the overlay A/B as a settled failure |
| [84c7e071](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/84c7e0712014979c76d31ae2ceeea7325ce5d1a7) | Record the new gameplay saves, and that the weapon flags are launch-side |
| [63bf323b](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/63bf323bbd60f1536d805b9a13a376b17efa49bf) | Close the confound list: no readable verdict until the load seed exists |
| [e4cd34b5](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/e4cd34b52a27893033489fc393237337782dbe82) | Pin the framework at 1cb15cf4 |
| [fd40aec4](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/fd40aec4d699713328577e2c002d06757cc36254) | Pin the framework at ce7921f6 |
| [c92f2c58](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/c92f2c5853b1042a7880e2afe7d2553e26314f2a) | Record the readable verdict: the shards fork the guest at frame 240 |
| [16a7e3b0](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/16a7e3b01d53d7648800400fe0de0b1fe8bf4573) | Pin the framework at 0fe8a425 |
| [fdeabb17](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/fdeabb17cc0fabae737ba6b27f60f255c9a81445) | Record the Layer 3 mechanism: shard charges publish on a different schedule |
| [d5727144](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/d572714472381eaea3704f8ffa570d60780a2b0d) | Refresh the pin line and the step history, and point PR 6 at the mechanism |
| [9c7082c7](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/9c7082c7d74b134343ad1f22b9cd05e10b35a06c) | Pin the framework at 85c676d2 |
| [2fd6bce4](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/2fd6bce47a297dfbd6061036759e4d1cc658ddda) | Retract the frame-240 fork: the interp reference moves run to run |
| [7e2c92a9](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/7e2c92a9ad0aa72a8d6b02558c96efdd2f9d41c3) | Pin the framework at 59c3fb3d |
| [80b34aa3](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/80b34aa378f4cfb2dc95533c666a758aafd591a2) | Correct the record: the fork is real, shedding was the wobble |
| [e936c698](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/e936c698000bc7f39d41c961925e34cb5e43945c) | Fix the reproduce recipe: boot seeds, PSX_STEREO_SHED=0, save the captures file |
| [23654ec3](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/23654ec3385ccc87bb6810f715b2f177877bbe44) | Bracket the cycle offset's origin to a two-pc window, and fix the recorder method |
| [a01723f8](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/a01723f82f9352fa01faecf11deae152662786e0) | The charged span is identical in both backends, so the offset is delivery, not accounting |
| [a14ca09c](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/a14ca09c5e4fa441d7dc33fce01e1137fa1f58f0) | The fork is a pure guest-clock phase, proven by the timer reads |
| [893684d7](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/893684d74613e2738a17ff6e80502d72e19c86af) | The fork is a sub-frame interrupt delivery lag; both code-side explanations are dead |
| [1d99f6a1](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/1d99f6a17564c8ef47e8e667dd2cf808ee36411f) | The event ring names the site: a long shard block with no interrupt check delays a CD IRQ |
| [6f90a0b5](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/6f90a0b57ee18a116cda480c6e07d6d07cdaa486) | The route's fidelity delta is 4 late interrupts out of 294, and the fix has a price |
| [f9d8070e](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/f9d8070e8492b1a693b295ae317bf5614ca0dc32) | The check budget's real cost is a cache tag bump, so the tail is the better trade |
| [aaee751e](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/aaee751eb5c514e7733e1a3029291268f92543ea) | The acceptance reading: ordering-level, not state-level |
| [361bc8f8](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/361bc8f834bc3c4e7233106411a0a5012c7fcabd) | The visual half passed headlessly: byte-identical eyes at matched guest cycles |
| [701f26a8](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/701f26a8731419bd2def06734c7d508044898e42) | Scripted look sweep: still frames identical, camera motion differs |
| [31634a3b](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/31634a3bd24cb7d51ab61a32dc86be8b6db2a171) | View matrices identical except under movement: the difference is movement code |
| [e1b3462b](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/e1b3462bec2d1a29f29f7f3360fbc26764012c59) | Pin the framework with the head-pose test override |
| [2a225731](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/2a225731bb5a01f305e831147ff6ca2eab454cc2) | Head bob harness: pose and reprojection traces are unaffected by the route |
| [0803df59](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/0803df59489f19d39c8d7a27c4ea6fe86257e76d) | Record that the route does not activate from a savestate, retracting today's A/Bs |
| [1e08b254](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/1e08b254463428b6c23b895c362a95780a95738e) | Restore the overlay index and gate the harnesses on registration |
| [cda238d1](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/cda238d1349a4c1c77b9de2ed67d85d556a65d45) | Record why the loader found no shard: the mod flags change the guest code |
| [76f0338d](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/76f0338dd71bb04276ef95da13c4ac9e84417f72) | Record that the flag variant capture is the same, so the mismatch is elsewhere |
| [3a73e313](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/3a73e3135032b5c7598cda12dc86a817a1f4bdd7) | Record the VR capture method, the stereo offset measurement, and the live numbers |
| [1a2af345](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/1a2af345f7a240405813209391fba2fbf98981a0) | Correct the VR eye mechanism: the eyes replay the guest render with a plus or minus 24 view offset |
| [ee320b13](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/ee320b137f239c1c684df8764bbb2010d9dd7d3f) | Record that the eye wedge reproduces headlessly with identical measurements |
| [6711cd88](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/6711cd8873e4b233ebdb9beac05ed57bbcafbce0) | Locate the wedge: the per-eye tree cull, and record that the harness forces its own environment |
| [37542fe9](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/37542fe9f6d6ea5096d19b6fb2eb6d69b129f8c4) | Measure the scene interior: the floor is drawn fine in both eyes, so the black is the undrawn eye margin |
| [0381cd84](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/0381cd84375a01560c255a39bd9d81c69262d450) | Keep the guest projection for the eye view so the drawn scene covers the eye surface |
| [be5791aa](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/be5791aaa7ca37910c391e90d86f473764b0c521) | Confirm the eye margin fix: bottom band 92.65 to 3.28 percent black, eyes agree within 0.4 points |
| [066f1fe5](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/066f1fe5e5eeaa3d9b04ea1f30a05bec390e1428) | Record that the wrist flags and the shard route are mutually exclusive, measured |
| [928b42cc](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/928b42ccd7fe0b629e8ee903148202b09223d509) | Add a SteamVR launcher variant without the wrist HUD replacement so the shard route stays live |
| [5be5f139](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/5be5f13991fe3ee9fb464b9d89b1e46e38ed7d13) | Revert the eye projection replacement: it made both eyes geometrically wrong |
| [05cbaf63](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/05cbaf63ba24a2551fb2605fef457136c9cf332d) | Pin the framework with the interrupt check budget |
| [0e33ab92](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/0e33ab92ec2cdbe5c68f64b828b05e926910a094) | Regenerate game C from the emitter with the interrupt check budget |
| [c8e1a3e4](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/c8e1a3e4d85b460d748b2cc311faad91f92f66b7) | Revert "Regenerate game C from the emitter with the interrupt check budget" |
| [38ee0585](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/38ee058531cb84184e3c1ea6aea3083ded3955e6) | Revert "Pin the framework with the interrupt check budget" |
| [f8ec8486](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/f8ec8486edb091d25a93047ced4bcae31fdd7aad) | Record why the interrupt check budget cannot land yet: any emitter change invalidates savestates |
| [3f04d6fc](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/3f04d6fc9ce835834c89755bce56b846ee3c1c77) | Pin the framework with the synthetic head pitch override |
| [2f966311](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/2f966311f985934e7878866992e38a0256569fb8) | Add a headless hold launcher for probing and stereo dumps |
| [0e2b3f31](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/0e2b3f314c25aad3c4d5b0eb0f50c1947061b521) | Record where the campaign stands, with the retractions in one place |
| [401a9e96](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/401a9e96bfceef0f6a373c3149a75e7c2656aeaa) | Make the eye focal reference sweepable and record what the sweep measured |
| [7d971737](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/7d9717376646f862911869b984b8f474af577d46) | Remove the authored H ref knob: it magnifies the guest render, it does not widen it |
| [aab4cb66](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/aab4cb6639706bb8439cfe7acd9499c5927c5434) | Record the shard as clipped triangles rather than a projection margin |
| [52582c67](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/52582c676ea5ffddefa0105042f515981342733e) | Add a test-only scale on the eye view translation |
| [9f23631c](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/9f23631cba20bad8267c4da5da1700fb77056d9c) | Pin the framework with the eye submission flip corrected |
| [afb1eb8f](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/afb1eb8f76df33bf5f879815c1ea2a8ec2bb5410) | Revert the eye submission flip change: it inverted the HUD layer as well |
| [126b2f5f](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/126b2f5ffeb32e947ab181aa0bc0ed3220f4d93a) | Pin the framework that drops an inverted world origin |
| [7085229d](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/7085229da910cb39d4f0ca8c6b0db4e384807479) | Record the inverted world as an origin half turn, with the measured matrices and proofs |
| [e54f35ed](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/e54f35ed2e61e76d091892a5f142740db693c351) | Replay a measured headset projection headlessly, and pin the compatibility work |
| [d94432c1](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/d94432c1e5d7c916effee648a229efd32f733cec) | Measure the black wedge against the eye offset: the offset is not the cause |
| [02d331db](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/02d331db08dd266c017be1c0c11b60666286f846) | Exclude selection as the cause of the black wedge, and keep the instrument that did it |
| [96047f5d](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/96047f5d835c5f050f802e5ba0c427625fe028a8) | Isolate the projection override from the black wedge, and measure its share |
| [0fd82ac5](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/0fd82ac5c1720bba62bd683696592bdf86761606) | Locate the black wedge in the guest own frame and measure its shape |
| [a664ba42](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/a664ba427d085a4b859c3b7d2cc1a487d0823b69) | Record the savestate cookie as gte.cpp's execution identity, and exonerate a GTE clamp |
| [60143e28](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/commit/60143e28ff96b96350ae5c0674c7aa19ab39698d) | Enable headset-accepted VR floor and world stability fixes |

### Framework: 23 commits

| Commit | Recorded change |
| --- | --- |
| [022eb1ed](https://github.com/FractalEngineer/psxrecomp/commit/022eb1edb2d6b2d9d505641e198bd9aeb5eb8ce1) | Add opt-in frame-trace ring for stereo/OpenXR frame attribution |
| [cae62891](https://github.com/FractalEngineer/psxrecomp/commit/cae62891e9236e9252e43b80aa9dd47f7554aea4) | Bound stereo shedding recovery and track stale-pair age |
| [063b082a](https://github.com/FractalEngineer/psxrecomp/commit/063b082ae959488436596d90e6933d825e22e780) | Reuse one stereo checkpoint baseline across the pair |
| [0e0d2de8](https://github.com/FractalEngineer/psxrecomp/commit/0e0d2de82777857248ae6c1bf7e9b7cba05e3d0a) | Record the submitted pose age and the runtime's display cadence |
| [b6a671c1](https://github.com/FractalEngineer/psxrecomp/commit/b6a671c1db4ba15d02369ce5561754d485881a55) | Exit stereo shedding on the display deadline, not on the hysteresis band |
| [5c67dced](https://github.com/FractalEngineer/psxrecomp/commit/5c67dced708a67e905b77be455263009f566d4bb) | Measure the HUD capture transactions in the frame trace |
| [63271ce7](https://github.com/FractalEngineer/psxrecomp/commit/63271ce7b4e2fb5f616050e7d59a2e4642a33b55) | Split the HUD capture into its draw and readback halves |
| [a876bae3](https://github.com/FractalEngineer/psxrecomp/commit/a876bae3385a8b972d8a5cf083ad5e30cf05c4bd) | Read both HUD mattes in one readback |
| [d2554481](https://github.com/FractalEngineer/psxrecomp/commit/d2554481fac39409cebef7602e59bd155e000fe6) | Reuse one checkpoint across a capture's two mattes |
| [571a8f10](https://github.com/FractalEngineer/psxrecomp/commit/571a8f1082dbbc95af19e90ce0226a0cb40f302d) | Attribute the per-eye guest replay, and record each eye separately |
| [87a3e822](https://github.com/FractalEngineer/psxrecomp/commit/87a3e8223cf7c05761326bfce06e80ffc527e132) | Nest a stage for the hook's visibility override inside the traversal |
| [f5229748](https://github.com/FractalEngineer/psxrecomp/commit/f5229748b93557e82370da44e5bc8be9abbb730f) | Record that an unmatched savestate load frame reads as a codegen fork |
| [1cb15cf4](https://github.com/FractalEngineer/psxrecomp/commit/1cb15cf4956e5c69cdfcca0de85c2a2bb9324618) | Record that the load frame cannot be matched from the probe side |
| [ce7921f6](https://github.com/FractalEngineer/psxrecomp/commit/ce7921f6469589be216416c829ee9111c5767577) | The load frame is pinnable with the boot seeds, and the first fork is at frame 240 |
| [0fe8a425](https://github.com/FractalEngineer/psxrecomp/commit/0fe8a4254128c32fee68c5a89d723d8218c8ada3) | Name the mechanism behind the shard fork: a cycle publication schedule split |
| [85c676d2](https://github.com/FractalEngineer/psxrecomp/commit/85c676d268ac1705310b6e73cf8bcead95f3acbb) | Retract the frame-240 shard fork: the interp reference was never controlled |
| [59c3fb3d](https://github.com/FractalEngineer/psxrecomp/commit/59c3fb3dc17119d27aa9b64e7f1f60c0caf4194e) | Correct the retraction: the frame-240 fork is real, the interp wobble was shedding |
| [2ed12640](https://github.com/FractalEngineer/psxrecomp/commit/2ed12640adab3602acbf3307d42e9c2739b7d0eb) | Add a debug-gated synthetic head translation for headless head-motion checks |
| [a3714fad](https://github.com/FractalEngineer/psxrecomp/commit/a3714fada4d0582500001b0c22e4dfa00840403e) | Add a gated synthetic head pitch to the pose override |
| [4dd9ca6d](https://github.com/FractalEngineer/psxrecomp/commit/4dd9ca6df533a8205ea0f9f52fca03bb331c520d) | Flip the eye image on the non-sRGB submission branch too |
| [928c721c](https://github.com/FractalEngineer/psxrecomp/commit/928c721ca2c55d01dc986f36a1a637389bad841e) | Revert "Flip the eye image on the non-sRGB submission branch too" |
| [96981469](https://github.com/FractalEngineer/psxrecomp/commit/96981469c4a3de0b31c3d2a282efdc7892f81361) | Drop an origin whose up axis points down so the world cannot stay inverted |
| [efa10eee](https://github.com/FractalEngineer/psxrecomp/commit/efa10eee331bf9d6c5ab4c7ea987bc80e434fa9a) | Fix VR near-plane geometry and preserve precise eye poses |

## Follow-up after the reported campaign tip: slot 2

The original inventories above remain bounded by `60143e2` / `efa10eee`.
[VR_SLOT2_PERFORMANCE.md](VR_SLOT2_PERFORMANCE.md) records the next delivery:

- **F9 — Production XR startup receipt and lightweight VR performance reporting.**
  `runtime/src/main.cpp` writes one opt-in readiness/state-load receipt without
  debug TCP, adds opt-in VR counters/timing logs, and ignores an empty boot-slot
  setting. Framework stereo docs and session log describe the contract.
- **G7 — Production VR launch defaults and slot-2 validation.** `CMakeLists.txt`
  defaults detailed diagnostics OFF; `vr/run_vr.ps1` defaults shedding OFF and
  projective ON, boot-loads normal slots, verifies the production receipt and
  supports independent log output. `vr/psx_vr_stereo.c` ignores an empty projective
  setting. Pin F9 and update the handoff/evidence documents.

These are two additional review units; rejected batching/inlining experiments
are not source changes in this follow-up. No additional headset check was asked.

Exact follow-up file inventory:

| Repository | File | Proposed PR |
| --- | --- | --- |
| Framework | `runtime/src/main.cpp` | F9 |
| Framework | `docs/STEREO_RENDERING.md` | F9 |
| Framework | `docs/internal/FAITHFUL_TIMING_PLAN.md` | F9 |
| Game | `CMakeLists.txt` | G7 |
| Game | `vr/run_vr.ps1` | G7 |
| Game | `vr/psx_vr_stereo.c` | G7 |
| Game | `psxrecomp` gitlink, now `5932092c` | G7 |
| Game | `docs/reverse/VR_SLOT2_PERFORMANCE.md` | G7 |
| Game | `docs/reverse/VR_HANDOFF.md` | G7 |
| Game | `docs/reverse/VR_OPTIMIZATION_STATUS.md` | G7 |
| Game | `docs/reverse/VR_PROJECTIVE_RENDERING.md` | G7 |
| Game | `docs/reverse/VR_CAMPAIGN_PR_REPORT.md` | G7 |
