# Slot 2: overload flicker and production performance

2026-10-23. Follow-up to the owner-accepted projective floor/stability fix.
Framework pin: `9b2100fe8d0c37931fba4d8a82fd6a724ece8f0b` on `vr-head-pitch`.
It merges current upstream `9caf4c22` while retaining the validated fork baseline.
All checks in this follow-up were headless; the owner requested continued headless
verification without another headset check. No new physical-headset acceptance
is claimed.

## Delivered changes

- Normal VR launch defaults to `PSX_STEREO_SHED=0`, while preserving an explicit
  environment override. The framework's policy remains available for diagnostics.
  Sustained shedding submits zero XR layers, producing black flashes on the null
  compositor. Rendering complete pairs avoids that overload-induced empty stream.
- Correct an empty-environment bug in the default promotion: the launcher emitted
  `PSX_VR_PROJECTIVE=""`, which native `atoi` interpreted as disabled. The launcher
  now explicitly resolves the normal default to 1, and the game constructor ignores
  empty values. Explicit 0 / `-NoProjectiveDiagnostic` still selects the control.
- Fresh game configurations default detailed debug tools OFF. The normal
  `build-release` was explicitly reconfigured OFF; existing ON caches otherwise
  retain their choice. PGXP/projective rendering remains ON; batching remains LLE.
- Production startup uses an opt-in one-shot `PSX_OPENXR_READY_FILE` receipt instead
  of requiring debug TCP. The receipt waits for the boot-seeded state request to
  finish and for actual XR submissions, and includes state-load success/slot.
- Normal `-Slot` stages the load before guest execution. A missing/incompatible
  slot is rejected by the launcher from the lifecycle receipt. Empty `PSX_LOAD_SLOT`
  no longer accidentally means slot 0.
- `-LogDirectory` records stdout/stderr and the startup receipt without requiring
  screenshot/debug tools. Opt-in `PSX_RUNTIME_PERF_DIAG` now logs VR pair/shedding/
  empty/failure counters, pair-time EMA/last sample, and rollback verification.
  Normal play does not enable the recurring logs.

No changes were retained from the batching or interpreter-inlining experiments.
No game-generated C, GTE arithmetic or new overlay shards were changed here.

## Current save and reproducibility

The owner's slot 2 changed after the earlier floor investigation. This follow-up
used a fresh byte-for-byte copy of the current `saves/` tree at
`analysis/vr-proof/20261023-slot2-fixture/saves`, with no header rebasing.
The original slot-2 SHA-256 is
`96ad21a05a0e03c1b6923012e650957d0381b92420074be0eeb3e3fb5d1bb11e`.
Its ABI is `0x2001e`, execution cookie `0x601abb88`, already compatible with the
accepted rendering build. Original personal states/cards remain untouched.

Headless controls use SteamVR's null driver, recorded Quest 3 eye projection,
1080p internal preset (5x), tracked-weapon/wrist-HUD options, no movement and
projective rendering ON. The current save is an indoor upper-level/crate view.
SDK tracking/grips in this null-HMD test are not a claim about physical controllers.

The accepted diagnostic reference is retained as a runnable copy in
`analysis/vr-proof/20261023-slot2-diag-reference`. It uses the pre-receipt diagnostic
launch helper in `20261023-slot2-boot/launch.ps1`; the current normal launcher
expects the new readiness receipt. For future source profiling, configure a
separate current diagnostic build with `-DPSX_DEBUG_TOOLS=ON`; TCP screenshot,
synthetic-head and trace export tools require that build.

## Black-flicker evidence

The initial explicit-shedding reproduction produced 113 new pairs and 789 empty
XR submissions in 30 seconds: roughly 88% empty. An approximately 18 ms pair
exceeded the null HMD's 11.111 ms deadline; the policy skipped eight requests
before each probe. That run inadvertently had projective rendering disabled by
the empty-value bug, so it establishes the shedding failure, not accepted-path
rendering performance.

After correcting the default and verifying live `vr.projective.draw` counters,
`20261023-slot2-accepted-base` produced 555 submissions in its quiet 30-second
window, zero empty submissions, zero sheds and zero XR failures. All three actual
compositor/application-layer captures were nonblank without rejected retries.
The frame-trace export retained 555 rows; separately sampled pair-counter delta
was 554 because the counters and export are not one atomic query.

Two production runs also captured three nonblank compositor frames each, with
verified owned rendering and no rejected blank dumps. The official production
launcher reported slot 2 generation 1, successful load, tracking/running and two
actual XR submissions. A fresh empty diagnostic save directory produced the
expected `VR slot load failed` error and clean owned SteamVR shutdown.

Three isolated snapshots alone are not a proof against all temporal flicker;
the continuous zero-empty counters and known policy change establish the specific
overload mechanism. Tracking loss or a real failed eye may still intentionally
submit no layer. Future motion/mission checks remain separate.

## Rendering attribution and production comparison

Corrected diagnostic build, accepted projective path:

| Stage | p50 | p95 |
| --- | ---: | ---: |
| Stereo pair | 27.195 ms | 29.058 ms |
| Guest scene traversal, both eyes | 24.485 ms | 26.235 ms |
| Host visibility selection, nested in traversal | 0.174 ms | 0.208 ms |
| Ordering-table submission | 0.964 ms | 1.100 ms |
| HUD capture | 3.955 ms | 10.577 ms |
| Host interval between traced frames | 52 ms | 68 ms |

Source: `20261023-slot2-accepted-base/trace.json`. The dominant cost is the
interpreted scene traversal. Scoping or pruning the host box selector cannot
recover most of it. HUD readback has a separate tail. Do not sum nested stages.

For the production comparison, both binaries used LLE batching, the same source
rendering implementation, compatible execution identity, copied slot, startup
seed, scene and settings. Both enabled the same opt-in once-per-second frame and
runtime-cadence logs. Compare the quiet 30–60 second windows before screenshots:

| Build | Weighted guest-frame-counter rate | Median one-second rate |
| --- | ---: | ---: |
| Diagnostic (`PSX_DEBUG_TOOLS=ON`) | 37.95/s | 37.9/s |
| Production (`PSX_DEBUG_TOOLS=OFF`) | 47.88/s | 47.45/s |
| Production repeat with official readiness/load checks | 46.59/s | 46.9/s |
| Final normal `build-release` | 48.24/s | 48.65/s |

This is a repeatable approximately 23–27% throughput improvement. The counter is
guest frame/VBlank cadence, not headset display FPS. This scene produces roughly
one new stereo pair per two counter increments: it is not 48 or 90 unique stereo
world frames/s. Production remains below the full 60 Hz guest rate in this test;
CPU-heavy overlay traversal and audio underruns are still visible in the logs.
The improvement is real but does not close the whole performance campaign.

Final normal-build counters reached **2,183 pairs, zero sheds, zero empty
submissions and zero XR failures** across the bounded run. The quiet window's
median sampled pair EMA was 24.365 ms (23.827–26.263 ms); this is not a p50 of
individual pair times. The separate 15-second `-Verify` run logged **968 checks
and zero mismatches**, with zero sheds/empties/failures. Both owned runs cleaned
up SteamVR. Post-window compositor captures deliberately introduce stalls and
are excluded from the timing window.

Receipts: `20261023-slot2-boot/comparison.json`, the same directory's per-build
stdout logs, `20261023-slot2-production-final/timing.json`, and
`20261023-slot2-release-final/timing.json`. Final correctness logs are in
`20261023-slot2-release-verify`; the missing-slot receipt is in `20261023-slot2-missing`. Compositor image
comparison to the diagnostic reference is visually coherent, with mean absolute
RGB difference about 0.93/255 in the backbuffer and 1.02/255 in the application
layer. Over half the pixels have small differences; do not call this bit-identical
fidelity or infer their cause without a separate controlled comparison.

## 2026-10-31 upstream optimization update

The game now pins framework `cdfdb73c` (upstream `a7b37e07` plus the three shared
VR commits) and recomp-ui `a956157`. The production configuration is Release /
OpenGL / OpenXR with `PSX_EXECUTION_PROFILE=ENHANCED`, generated native CPU spans,
ordered HLE GL batching, PGXP enabled and debug tools disabled.

The owner created a fresh state in UI slot 2. In this menu, UI slot 2 maps to the
backend file `state_8001DFD4_slot01.pst`; its SHA-256 is
`63f9d66a63c8fdc148d2190d5607ea6d5d67e8b0fb71da0e5bf2315771382ded`.
The snapshot header is version 9 with codegen `0xe060fa62`, ABI `0x00020023`,
execution cookie `0x534bcb13`, and 19 sections.

The isolated 45-second production run produced 1,337 stereo pairs with zero
sheds, empty submissions or XR failures. After excluding five startup samples,
the median sampled guest rate was **59.90 Hz** and the mean was **59.34 Hz**.
The final average pair-time EMA was **14.384 ms**. This is a 23.1% increase over
the previous 48.65 Hz median, and the pair-time EMA is 41.0% below the previous
24.365 ms.

The separate debug verifier loaded the same state and captured two complete,
nonblank eye pairs. It reported 52 pairs, zero refusals/failures/sheds, and a
17.261 ms average pair time. The pair receipts retain distinct left/right images
at the expected +/-8 view offsets.

An identical production repeat was cleaner: 1,348 pairs, zero
sheds/empty submissions/XR failures, a **59.90 Hz** median and **59.95 Hz** mean,
with every post-warmup one-second sample at or above **59.6 Hz** and a final
**14.441 ms** pair-time EMA. The first run's one two-second dip was not reproduced.

Raw evidence:

- `analysis/runtime/upstream-792-ui-slot2-perf/runtime.stdout.log`
- `analysis/runtime/upstream-792-ui-slot2-perf-repeat/runtime.stdout.log`
- `analysis/runtime/upstream-792-ui-slot2-perf/startup.json`
- `analysis/runtime/upstream-792-ui-slot2-capture/p000001_sbs.png`
- `analysis/runtime/upstream-792-ui-slot2-capture/p000002_sbs.png`
- `analysis/runtime/upstream-792-ui-slot2-capture/stereo_stats.json`

The previous slot-2 state remains intentionally rejected by the snapshot guard:
its codegen/execution/ABI identity is older. The exact reason is
`enhancement_memory_layout_or_execution_profile_or_host_provider`; this is an
identity mismatch, not a damaged save file. A snapshot format migration would
need separate validation before old states could be accepted across framework
updates.

## Rejected/invalid experiments

`20261023-slot2-baseline`, `slot2-noshed`, `slot2-batch` and `slot2-inline` predate
the empty-value correction. They are invalid as accepted-projective performance
results. Batching and inlining failed to clearly beat their own control and were
not promoted. The framework interpreter source is restored. The F7 geometry fix
remains the accepted implementation.

## Commands

Normal production headless reproduction, without touching personal saves:

```powershell
$env:PSX_VR_EYE_PROFILE = 'quest3'
$env:PSX_FRAME_REPORT_MS = '1000'
$env:PSX_RUNTIME_PERF_DIAG = '1'
$env:PSX_RUNTIME_PERF_DIAG_MS = '1000'
.\vr\run_vr.ps1 -Headless -Slot 2 `
  -MemcardDirectory analysis/vr-proof/20261023-slot2-fixture/saves `
  -WeaponPoseDiagnostic -WristHud -WristHudReplace `
  -LogDirectory analysis/vr-proof/<fresh-run> -Seconds 90
```

Use `-Verify` in a separate correctness run; do not mix its hashing overhead into
performance comparisons. `-CaptureDirectory` and the Python debug clients require
a separately configured `PSX_DEBUG_TOOLS=ON` build. Normal launcher readiness and
`-Slot` work without that debug surface.

## Final validation

Normal Release/OpenGL/OpenXR build passed with PGXP ON, debug tools OFF and LLE
batching. Launcher syntax and empty/0/1 environment selection checks passed,
including the explicit projective opt-out. Valid-slot readiness, missing-slot
refusal, the continuous VR counters, compositor captures and the separate rollback
run are recorded above. Source GTE execution identity remains unchanged by this
follow-up. No game/SteamVR test or compiler process remains running.
