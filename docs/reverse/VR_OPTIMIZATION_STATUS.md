# VR optimization campaign: status and evidence

Companion to [VR_OPTIMIZATION_PLAN.md](VR_OPTIMIZATION_PLAN.md). Everything below is
measured, with the capture it came from named so it can be re-read. Framework changes live
in the `psxrecomp` branch `vr-perf-pr1-pr2`.

## Branches and pins

- Parent branch `vr-perf-pr1-pr2`, tip `08334ad`, which pins the framework at `0e0d2de8`.
- Framework branch `vr-perf-pr1-pr2`, tip `0e0d2de8`.
- One commit per PR, one pin bump per PR, no PRs opened. The parent's
  `psxrecomp` gitlink always matches the framework branch tip used for that build.

## Work package status

| PR | Scope | Status |
| --- | --- | --- |
| PR 1 | Low-overhead frame-time tracing | Done. Trace ring, TCP export, `vr/trace_frames.py`. |
| PR 2 | Stereo shedding, recovery, stale-pair policy | Done. Exit criteria measured, see below. |
| PR 3 | Reuse stereo checkpoints safely | Done. Exit criteria measured, see below. |
| PR 5 | Guest/XR pacing | Slice 1 (cadence and pose-age tracing) done and verified. Slice 2 (opt-in resubmission) not started. |
| PR 4 | GPU capture and submission optimization | Not started. |
| PR 6 | Mission/sector hotspots | Not started. |

## Environment

Headset is 90 Hz. VirtualDesktopXR grants the app one submission per **22.222 ms**, that is
two display frames per app frame. Guest vblank is 60 Hz. Runs use `RunVR-VDXR.bat`
(`build-release`, `-WeaponPoseDiagnostic -WristHud -WristHudReplace`), tracing on
(`PSX_FRAME_TRACE=1`, `PSX_FRAME_TRACE_CAP=1024`), 90 to 100 second windows.

## PR 2 exit criteria: met

Captures: `analysis/vr-proof/pr1-2-live`, `analysis/vr-proof/pr3-live`.

- One bounded shed burst: 57 shed frames, `shed_streak_max` 8, plus 57 XR empty
  submissions, all during boot/menu before pairs ramped up. After that, zero sheds and zero
  stale pairs for the rest of the run.
- The budget came from the runtime (`budget_source` 1), not the VBlank: it reported
  11.111 ms early and settled at 22.222 ms.
- No long induced stale-pair streak after a one-off spike, which is the criterion.

## PR 3 exit criteria: met

Same protocol, 1024 pairs per build, framework at `cae62891` (PR 2) versus `063b082a`
(PR 3):

| stage | PR 2 p50 / p95 | PR 3 p50 / p95 |
| --- | --- | --- |
| `eye_begin_r` (the checkpoint PR 3 removes) | 143 / 178 us | **36 / 52 us** |
| `eye_begin_l` (still creates the shared baseline) | 154 / 192 us | 154 / 196 us |
| `eye_restore_r` (each replay must still restore) | 151 / 191 us | 144 / 191 us |

- `checkpoint_reuses` = 1.0 per pair, and all 1024 traced pairs carry `baseline_reused`.
- `checkpoint_saves` = 2.99 per pair, not 1. The other two are the wrist HUD's black/white
  matte captures (`psx_mod_render_capture_rgba` at `vr/psx_vr_stereo.c:710-712`), which set
  `s_capture_only` and are therefore permanently ineligible for baseline reuse.
- Framework tests at the pin: `render_pass_sandbox_test`, `render_pass_abort_test`,
  `render_pass_freeze_test`, `render_pass_plan_test` all pass.
- Caveat recorded at the time: the PR 3 run drew a slightly heavier scene (guest draw
  +590 us per eye), so `pair_total` is not comparable across the two runs (9093 to 10192 p50).
  The checkpoint stages are directly comparable and are the exit criterion.

## PR 5 slice 1: implemented, committed, verified

Framework `0e0d2de8`, parent `08334ad`. Adds `PSX_TRACE_POSE_AGE`, the record fields
`xr_display_delta_us` and `xr_submit_fresh`, and the flag `PSX_FRAME_TRACE_XR_REPEAT`
(`xr_repeat`), plus the reporting in `vr/trace_frames.py`.

Verification against the acceptance line "the miss rate from the runtime's own display times
agrees with the offline `open_ms` figure". Capture: `analysis/vr-proof/pr5-slice1-live`,
1024 pairs.

| measure | from the runtime clock (new field) | from `open_ms` (offline) |
| --- | --- | --- |
| granted periods per presented frame, p50 | 1.500 | 1.49 |
| p95 | 1.934 | 1.94 |
| mean | 1.539 | 1.54 |
| display periods with no new pair | 552 of 1576 (35.0%) | 482 of 1505 (32.0%) |

The two clocks agree, which is the criterion. The repeat counts differ slightly because the
first ring record carries a delta from a submission that has been evicted, and one figure
sums exact ratios while the other rounds per interval.

New measurement from the same capture:

- `pose_age` (xrLocateViews return to xrEndFrame return): p50 **14.14 ms**, p95 **19.52 ms**,
  p99 **22.11 ms**, max **29.09 ms**. This is the age of the pose the compositor reprojects
  from, and it sits against the 22.222 ms grant.
- `pair_total` in the same window is p50 10.68 ms, so roughly 3.5 ms of that pose age is
  frame work between `xrLocateViews` and `xrEndFrame` that is not the pair render. The prime
  suspect is the two per-frame wrist HUD capture transactions, already implicated by the
  PR 3 checkpoint counts.
- `xr_repeat_frames` is 0, correctly: no sheds means nothing was submitted without a new
  pair. The flag is exercised by the unit test only, so far.
- `xr_wait_frame` p50 2 us. The app never blocks on the runtime, so the app is the slow side.

## What the trace says is left

1. **Cadence.** One rendered pair serves about 1.5 granted periods, so roughly a third of
   display periods carry nothing new and rely on reprojection of a pose up to 33 ms old.
   This is the residual behind "less and shorter slowdowns, but still some".
2. **Budget arithmetic.** Inside a 33 ms frame: guest logic and native present about 22.7 ms,
   stereo pair 10.0 ms p50, XR copy and end about 0.5 ms. Guest-only work is already at the
   22.222 ms grant, so the grant cannot be met while advancing the guest without reducing
   guest work (PR 6). PR 5 can only present at the granted cadence without advancing the
   guest, and must pay for that honestly.
3. **HUD captures.** Two of every three per-pair checkpoint saves, and about 3.5 ms of the
   pose age, are the wrist HUD's per-frame black/white captures.
4. **XR spikes.** `xr_end_frame` p50 0.22 ms but p95 1.4 to 1.5 ms and max 3.2 to 3.5 ms,
   with `xr_copy` p50 0.31 ms. PR 4 territory.
5. **Per-eye guest draw** is 3.9 to 4.5 ms per eye of a 9 to 10 ms pair, the single largest
   term and inherent guest drawing (PR 6).

## Next steps

- **PR 5 slice 2 (design in the plan).** Opt-in `PSX_OPENXR_RESUBMIT=1`: re-present the
  retained pair at the granted cadence with the pose it was rendered from. Needs per-pair
  rendering views retained in `gpu_gl_renderer.c`, a `psx_openxr_resubmit` entry point that
  runs WaitFrame, BeginFrame and EndFrame without locating views or touching the stereo
  budget, `psx_mod_openxr_resubmit` for the plugin, and the cadence policy in
  `vr/psx_vr_stereo.c` from the existing vblank hook. `psx_frame_trace_source` is
  first-writer-wins, so a repeat can label its record with its own source value.
- **Alternate candidate with better measured support:** make the per-frame HUD captures
  reuse the baseline or skip when unchanged. Two thirds of the checkpoint work and about
  3.5 ms of pose age sit there, both inside the frame, and neither is guest drawing.
- PR 4 and PR 6 as scoped in the plan.

## Open questions

1. VDXR reported 11.111 ms early in one run and then 22.222 ms. If the grant can change
   mid-session, a resubmission policy must follow it rather than assume a constant.
2. Does the runtime keep the last submitted layer when `xrEndFrame` carries no projection
   layer? If it already does, part of slice 2's motivation disappears. Worth checking before
   building the rest of it.
3. `build-master` (an upstream-master framework checkout for a true A/B against the perf
   work) was floated but never created.

## Reproducing

```powershell
# framework at the branch tip, then the game build
git -C psxrecomp checkout vr-perf-pr1-pr2
cmake --build build-release --target psx-runtime

# one traced run, then export while it is alive (the ring is only on TCP)
$env:PSX_FRAME_TRACE='1'; $env:PSX_FRAME_TRACE_CAP='1024'
vr\run_vr.ps1 -Runtime vdxr -WeaponPoseDiagnostic -WristHud -WristHudReplace `
  -BuildDirectory build-release -Seconds 100
python vr\trace_frames.py analysis\vr-proof\<name> --max 1024
```

The framework render-pass and trace tests are not targets in `build-release` (that build
uses `runtime.cmake`, not `runtime/CMakeLists.txt`). Compile them directly, for example:

```powershell
gcc -O1 -std=gnu99 -I psxrecomp\runtime\include -o out.exe `
  psxrecomp\runtime\tests\test_frame_trace.c psxrecomp\runtime\src\frame_trace.c
python psxrecomp\runtime\tests\test_frame_trace_guard.py
```

Raw captures live under `analysis/vr-proof/`, which `.gitignore` excludes, so they are
local artifacts rather than tracked evidence.
