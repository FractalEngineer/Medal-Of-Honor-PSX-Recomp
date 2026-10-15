# VR Optimization Plan — Medal of Honor PSX Recomp

**Status:** Proposed / profiling required  
**Scope:** `FractalEngineer/Medal-Of-Honor-PSX-Recomp` and its pinned `psxrecomp` submodule  
**Primary objective:** Eliminate noticeable VR stuttering and improve frame-time consistency without changing PSX simulation speed, breaking coherent stereo, or regressing flat play.  
**Secondary objectives:** Reduce expensive per-eye replay, state checkpoint, GPU capture, and OpenXR presentation overhead once measurements establish which components dominate.

## 0. Evidence and scope

This plan follows a source review of the primary repository and its pinned `psxrecomp` framework. It **is not a measured performance profile**. Treat bottlenecks as hypotheses unless the implementation itself is explicitly identified as a confirmed mechanism below.

Reviewed anchors (verify against current working-tree lines before editing):

- Game stereo hook: `vr/psx_vr_stereo.c`, particularly `vr_wait_entry()` and the call to `psx_mod_render_stereo()`.
- Framework stereo entry point: `psxrecomp/runtime/src/render_pass.c`, `psx_mod_render_stereo()`.
- Framework transaction/checkpoint: `render_transaction()`, `checkpoint_save()`, `checkpoint_restore()` in the same file.
- OpenXR frame lifecycle: `psxrecomp/runtime/src/psx_openxr.c`, `psx_openxr_begin()` / `psx_openxr_end()` and related swapchain copy.
- OpenGL backend: `psxrecomp/runtime/src/gpu_gl_renderer.c`.
- Other relevant files: `psxrecomp/runtime/src/gpu.c`, `gpu_render.c`, `main.cpp`, `render_thread.c`, and the existing runtime performance/test utilities.
- Existing diagnostics: `vr/check_video_pacing.py`, `vr/capture_stereo.py`, plus framework stereo/render-pass/OpenXR statistics.
- Existing backlog: `docs/reverse/VR_ALPHA_TODO.md` (notably independent headset cadence) and `docs/reverse/VR_HANDOFF.md`. Some older documents record historical framework pins; rely on actual gitlinks for implementation.

**Submodule pin observed during review:** `psxrecomp` at `8d1c983b5910a42347d1af6304b9a68da94652f2`. `recomp-ui` is also a submodule, but no UI-specific runtime bottleneck has been demonstrated. Do not optimize UI without a profile implicating it.

## 1. Architecture and confirmed mechanisms

Simplified path:

```text
PSX simulation / native scene draw
    -> Medal of Honor vr_wait_entry()
    -> psx_mod_render_stereo()
       -> left-eye render_transaction()
          -> checkpoint guest state -> replay drawing -> capture -> restore
       -> right-eye render_transaction()
          -> checkpoint / reuse where eligible -> replay -> capture -> restore
       -> atomic stereo-pair publish
    -> OpenXR swapchain acquire / GPU image copy / release / xrEndFrame
    -> headset compositor
```

### 1.1 P0: Attempt-based stereo shedding can cause long stale-pair bursts

`psx_mod_render_stereo()` contains logic equivalent to:

```c
if (s_stereo.avg_pair_ms > budget_ms && ++s_pair_retry < 30u) {
    s_stereo.shed++;
    return 0;
}
```

The budget is based on `period_vblanks`, the PSX VBlank duration, and an 80% factor. Average pair time is an exponential moving average updated by successful render attempts. A brief overload can therefore suppress many subsequent attempts before another measurement is obtained. **Confirmed algorithmic risk, not proof of the reported stutter.**

### 1.2 P0: Stereo replay is sequential and involves guest-machine restoration

Each eye is produced by invoking guest rendering through `render_transaction()`. Checkpoint code copies live PSX RAM, CPU/associated hardware state and relevant GPU state, and restores them after replay. Some GPU-state reuse already exists: do not assume every component is duplicated twice. Measure save/replay/capture/restore independently.

### 1.3 P1: OpenXR lifecycle and GL share the emulation/main thread

Framework documentation/source places OpenXR entry points, GL presentation and certain debug hooks on the GL-owning emulation/main thread. `xrWaitFrame`, eye capture, swapchain acquisition/wait/copy/release, and submission can therefore interact with guest-frame scheduling. Time these calls separately; a blocking wait is not automatically a bug, but excessive wait or badly placed waits can result in poor pacing.

### 1.4 P1: GPU synchronization/capture costs are plausible but unmeasured

Stereo-eye capture and GPU/VRAM transaction maintenance may force synchronization. Find exact GL operations and call sites before changing readbacks, FBO allocation or resource lifetimes. The existence of `psx_mod_render_capture_rgba()` does **not** prove that the ordinary stereo path performs CPU RGBA readbacks.

### 1.5 P1/P2: Guest redraw cadence and headset cadence are coupled

The game invokes stereo at a native render-wait boundary. A PSX-era redraw cadence can be slower or irregular relative to 72/90/120 Hz VR presentation. Separate **guest simulation**, **guest redraw**, **successful stereo pair**, **OpenXR submission**, and **compositor/headset display** metrics. Do not conflate reported FPS with actual motion smoothness.

## 2. Reproducibility and baseline protocol

Before optimization:

1. Record tested commit and **all recursive submodule SHAs**, build type, CMake switches, OpenXR runtime, headset refresh rate, GPU/CPU, headset streaming method, display mode and resolution.
2. Establish fixed repeatable runs in (a) light/static scene, (b) dense geometry/enemies, (c) quick head rotation, (d) new room/sector transition, and (e) known stuttering mission area. Include saved-game/slot and route instructions, not game assets.
3. Run flat and VR modes where meaningful; use fixed synthetic pose and live tracked pose as controls.
4. Perform a warm-up, then collect at least 30 seconds of data per run; repeat runs to detect outliers, shader/asset warm-up and thermal effects.
5. Keep guest simulation, input, weapon rendering, stereo correctness, UI and asset state unchanged between compared builds.
6. Measure **frame-time distributions** (P50/P95/P99/max), >1/2/3-headset-period intervals, latest-pair age, XR empty submissions, shedding bursts and stage-specific costs. Report absolute values and deltas, not only average FPS.
7. When using 72/90/120 Hz, nominal frame budgets are 13.89/11.11/8.33 ms; these are **headset presentation deadlines**, not unconditional requirements to redo full PSX guest drawing at those rates.

## 3. Proposed PR sequence

### PR 1 — Low-overhead frame-time tracing (P0, low complexity)

**Goal:** Convert stutter reports into attributed CPU/GPU/XR timing events.

**Files:** Framework `render_pass.c`, `psx_openxr.c`, potentially `gpu_gl_renderer.c`, `main.cpp`; game-side `vr/` capture scripts.

**Work:**

- Add fixed-capacity in-memory ring-buffer frame records with monotonic timestamps and no per-frame heap allocation or synchronous filesystem I/O.
- Record guest simulation/render duration; left/right `checkpoint_save`, guest replay, capture/end and restore; stereo pair total; draw/primitive counts if available; GPU texture upload/VRAM dirty costs; XR `xrWaitFrame`, `xrBeginFrame`, acquire, wait swapchain image, image copy, release and `xrEndFrame`.
- Capture unique guest-frame ID, source redraw ID, stereo attempt/last pair ID, pair age, failure/refusal reason, shed count and duration, XR submission/empty counts, runtime, selected mode and target refresh.
- Include load/IO events if observable; annotate scene transitions.
- Add an explicit opt-in trace command or debug flag to export CSV and optionally Chrome/Perfetto trace JSON **after capture**, outside hot paths.
- Avoid timing perturbations: run disabled-by-default controls; compare instrumentation enabled vs disabled.
- Avoid treating GPU command-submission CPU time as GPU execution time. If GPU timers are added, use delayed/nonblocking retrieval and mark unavailable samples.

**Tests:** Ring wrap; timestamp monotonicity; missing/disabled telemetry; export validity; no allocations or file writes in draw path; no change in gameplay output with tracing on/off.

**Exit criteria:** Every recorded visible stutter can be correlated with one or more measured stage delays or stale submissions. Establish baseline metrics for multiple repro cases.

**Rollback:** Feature flag disables all hooks. Never alter guest scheduling just for telemetry.

### PR 2 — Stereo shedding, recovery and stale-pair policy (P0, low–medium)

**Goal:** Prevent a transient over-budget frame from causing prolonged skip streaks.

**Files:** `psxrecomp/runtime/src/render_pass.c`, appropriate test files and user-visible diagnostics.

**Work:**

- Preserve a baseline diagnostic mode with shedding fully disabled, to compare cause and effect; do not ship it as the sole permanent solution.
- Replace attempt-count-only suppression with bounded time-based recovery/probe attempts. Log both elapsed time and number of skipped opportunities.
- Track last successful pair age separately from rolling compute-time estimate; ensure stale averages cannot indefinitely prevent fresh samples.
- Evaluate a short retry/backoff or adaptive threshold with hysteresis while respecting actual XR deadlines; tune based on traces, not hardcoded guessed percentages.
- If the pair cannot finish safely, retain the last valid **whole** stereo pair; never submit unmatched eyes.
- Measure guest lateness, GPU lateness and XR wait blocking independently. Do not use a PSX VBlank budget as a proxy for every OpenXR display deadline.
- Define explicit behavior for capture failure, watchdog abort, OpenXR failure or lost tracking, and reset of budget estimates on scene/mode transitions.

**Tests:** Synthetic short spike, persistent overload, alternating load, transient XR failure, stale timing estimate, both-eye integrity, no unbounded failure loop, no change to guest-cycle accounting.

**Exit criteria:** No long induced stale-pair streaks after a one-off spike; fewer repeated/headset-period misses than baseline in affected scenes; no worse worst-case headset latency in sustained overload.

**Rollback:** Runtime opt-out and easy revert of adaptive policy.

### PR 3 — Reuse stereo checkpoints safely (P1, medium)

**Goal:** Reduce per-eye CPU state handling without sacrificing exact rollback.

**Files:** `psxrecomp/runtime/src/render_pass.c`, checkpoint/transaction helpers, tests.

**Work:**

- First quantify number of checkpoint saves, RAM bytes copied, DMA/device snapshots, GPU backup/reuse and restore time **per eye**.
- Preserve existing GPU backup reuse paths; avoid rewriting proven mechanisms.
- Test a pair-scoped common baseline: save immutable guest baseline once; restore it after each replay; stage two images atomically. Verify that any state required by the renderer is correctly reset between eyes.
- Only if profiling justifies it, prototype dirty-region/dirty-page restoration. Track guest writes accurately, including memory aliases, DMA and special paths. Benchmark write barrier overhead against full memcpy.
- Do not optimize away side-effect interception, guest-clock freeze, watchdog safety, or longjmp cleanup.

**Tests:** Existing render-pass sandbox, freeze, abort, restore verification and frame fingerprint tests; both-eye state hash equivalence; heavy DMA/texture-change scenes; partial/failed right-eye replay.

**Exit criteria:** Demonstrable decrease in P95 checkpoint/replay frame time; exact guest-state equality and visual equivalence.

**Rollback:** Build/runtime guard for revised snapshot strategy; fall back to original snapshots on validation failure.

### PR 4 — GPU capture and submission optimization (P1, medium–high)

**Goal:** Remove measured synchronization, excess copying, and unnecessary state churn in the OpenGL → OpenXR path.

**Files:** `gpu_gl_renderer.c`, `gpu.c`, `gpu_render.c`, `psx_openxr.c`, possibly `main.cpp`.

**Work:**

- Locate all `glReadPixels`, `glFinish`, `glFlush`, fences, framebuffer transfers/blits, texture uploads, dirty VRAM maintenance, FBO/resource lifecycle and dynamic allocations on the stereo hot path.
- Instrument command submission separately from actual GPU completion. Prefer timestamp queries with delayed results where supported.
- Verify whether ordinary stereo eye images remain GPU resident end-to-end. Avoid introducing CPU-side image readback into headset path.
- Reuse persistent-sized targets where safe; combine redundant state changes; bound texture uploads and eliminate unnecessary duplicate eye work.
- Explore direct GPU-to-GPU copy/blit into OpenGL-backed OpenXR images when formats, dimensions, gamma treatment and lifetime constraints permit.
- Check that optimizations preserve sRGB/linear presentation, alpha behavior, depth/viewport rules, menu quad layers and atomic pair publish.

**Tests:** Compare both eyes pixel-by-pixel in fixed camera cases, color fixtures, menu/video/gameplay transitions, fail/recovery, flat regression, GPU resource leak checks.

**Exit criteria:** Lower GPU-related P95/P99 spikes in measured cases; no image/color/visibility regressions.

**Rollback:** Keep original image-copy path selectable until parity is proven.

### PR 5 — Guest/XR pacing improvements (P2, high)

**Goal:** Make headset presentation less dependent on uneven native guest redraws, while preserving original simulation semantics.

**Files:** `vr/psx_vr_stereo.c`, `psx_openxr.c`, `main.cpp`, relevant render-pass/render-thread facilities.

**Work:**

- Diagram and trace the relationship between game VBlank, native draw, stereo replay, `xrWaitFrame`, pose sampling, image acquire/copy and submission.
- Establish the age of the pose relative to render and predicted display time; identify submission misses/repeated pairs.
- Prototype predictable presentation of the most recent coherent stereo pair and independent XR frame submission **only where supported by runtime/GL ownership**.
- Consider later headset-rate pose update, projection correction or timewarp-assisted reprojection. Do not pretend duplicated legacy frames are true high-FPS world motion.
- Assess existing `render_thread.c` architecture before changing thread ownership; OpenGL context and OpenXR frame lifecycle constraints are first-class correctness requirements.
- Preserve consistent eye camera transforms and the guest's intended clock rate; no fake extra simulation ticks.

**Tests:** Static/fixed pose, live 6DoF pose, rapid head turns, moving weapon, pause/menu/film transitions, focus loss, headset re-connect, cadence at multiple supported refresh rates.

**Exit criteria:** Lower display-period miss/stale-pair rates and better movement smoothness, without simulation drift or eye mismatch.

**Rollback:** Experimental opt-in path; retain the prior known-good main-thread scheduler.

### PR 6 — Mission/sector-specific hotspots (P1/P3, medium–high)

**Goal:** Fix load-dependent stutter in the actual reported areas after tracing points to the responsible subsystem.

**Files:** Determined by profiles; likely game hooks and framework GPU/asset/visibility paths.

**Work:**

- Reproduce in specific mission positions and record deterministic camera/movement routes.
- Correlate per-eye replay time with geometry/primitive counts, visible sectors, entity count, GTE operations, texture uploads, CD/image reads and CPU stalls.
- Compare flat vs stereo at same guest checkpoint; compare fixed vs tracked head and warm vs cold scene entry.
- If visibility expansion causes cost, profile per-eye AABB/sector traversal and prune conservatively **without** reintroducing missing tiles, head-turn pop-in, or near-player floor gaps.
- If asset streaming causes stalls, cache/preload only validated assets, bound uploads, and avoid blocking disk/decoding in critical frame path when safe.
- If generated guest code dominates, profile specific guest functions before considering game-specific fast paths; preserve guest timing and replay reproducibility.

**Tests:** Same routes before/after; texture correctness; visibility/geometry, shadow or fog accuracy; gameplay interactions; flat regression.

**Exit criteria:** Lower P95/P99 and worst spikes at previously problematic coordinates, with no additional world visibility/correctness defects.

**Rollback:** Isolate level-specific performance changes behind testable guards; revert if visibility or reproducibility fails.

## 4. Symptom-to-measurement triage

| Symptom | Initial suspects | Confirm using |
|---|---|---|
| Heavy enemy/geometry scene stutters | guest draw/GTE/render replay | per-eye guest ms, primitive/entity counts |
| Fast head turn stutters | expanded visibility, pose-age/cadence | view selection counts, pair age, XR deltas |
| Room entry causes hitch | IO, texture decode/upload, scene transition | IO spans, uploads, warm/cold comparison |
| Flat smooth, VR stutters | replay/capture/XR sync | controlled flat vs stereo stage trace |
| FPS seems stable but head motion judders | irregular/new-frame cadence, stale images | headset intervals, submitted IDs, pair age |
| Several stutters follow one heavy frame | shedding feedback | consecutive sheds, last-pair age, avg_pair_ms |
| Stationary camera stutters | rendering/scheduling/transfer | fixed-pose trace and GPU/XR timing |

## 5. Acceptance, non-goals and safety constraints

**Mandatory acceptance gates for every PR:**

- Measured improvement in affected profile(s), especially P95/P99/worst frame times and stale stereo-pair age; no unsupported performance claims.
- No guest-simulation speed changes or input/control regressions.
- Two eyes represent one coherent guest checkpoint; no one-eye-only publishes.
- No checkpoint/restore verification failures, CPU/GTE/RAM/timer/DMA/GPU state leaks, or watchdog cleanup regressions.
- No regression in accepted color/contrast, stereo parallax, weapons/tracking, HUD, menus/videos, or flat rendering.
- No increase in VR-induced geometry loss, missing floor tiles, pop-in or later-area transparent textures; unresolved existing issues must be documented rather than obscured.
- Faults and unavailable OpenXR runtime/headset do not crash the game or trap the system in repeated expensive reset/retry loops.
- New instrumentation is disabled-by-default or very cheap when enabled.

**Do not** immediately perform a wholesale multithreaded renderer rewrite, disable all shedding permanently, eliminate guest-timing guards, or make unverified graphics fidelity compromises for apparent speed. Large changes require baseline traces and incremental proof.

## 6. Suggested Codex execution contract

> Implement the VR performance program in small, reviewable PRs in the order above. Begin by inspecting the checked-out main-repo SHA and recursive submodule pins; source files and line numbers may have changed. First add low-overhead instrumentation and obtain measured traces before claiming bottlenecks. Keep generalized runtime optimizations in `psxrecomp`; keep Medal of Honor-specific hooks and reproducible gameplay tests in the parent project. Do not silently float submodule branches. Before each PR, list exact functions/files to modify, measurement hypothesis, unit/integration tests, correctness risks and rollback method. Do not alter gameplay simulation speed, stereo-state integrity, accepted VR appearance or flat-mode correctness. Report before/after frame-time distributions and failure counters from reproducible scenes, distinguishing hypothesis from measurement.

## 7. Key reference links

- [Main repository](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp)
- [Pinned framework render-pass code](https://github.com/FractalEngineer/psxrecomp/blob/8d1c983b5910a42347d1af6304b9a68da94652f2/runtime/src/render_pass.c)
- [Pinned OpenXR code](https://github.com/FractalEngineer/psxrecomp/blob/8d1c983b5910a42347d1af6304b9a68da94652f2/runtime/src/psx_openxr.c)
- [Game VR stereo code](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/blob/master/vr/psx_vr_stereo.c)
- [VR active backlog](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/blob/master/docs/reverse/VR_ALPHA_TODO.md)

**Recommended first implementation:** PR 1 (instrumentation), followed by PR 2 (bounded recovery from shedding). Let the resulting traces decide whether checkpoint copying, GPU capture, game geometry, streaming, or XR scheduling deserves the next optimization.
