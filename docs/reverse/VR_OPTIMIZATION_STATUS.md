# VR optimization campaign: status and evidence

Companion to [VR_OPTIMIZATION_PLAN.md](VR_OPTIMIZATION_PLAN.md). Everything below is
measured, with the capture it came from named so it can be re-read. Framework changes live
in the `psxrecomp` branch `vr-perf-pr1-pr2`.

## Branches and pins

- Parent branch `vr-perf-pr1-pr2`, tip `48fc640`, which pins the framework at `d2554481`.
- Framework branch `vr-perf-pr1-pr2`, tip `d2554481`.
- One commit per PR, one pin bump per PR, no PRs opened. The parent's
  `psxrecomp` gitlink always matches the framework branch tip used for that build.

Branch history, newest first, framework and parent commit per step:

| step | framework | parent | what it is |
| --- | --- | --- | --- |
| PR 1 | `022eb1ed` | `8b77fae` | trace ring and TCP export |
| PR 2 | `cae62891` | | stereo shedding, recovery, stale-pair policy |
| PR 3 | `063b082a` | | reuse one stereo checkpoint baseline across the pair |
| PR 5 slice 1 | `0e0d2de8` | `08334ad` | pose age and repeat tracing |
| shed latch fix | `b6a671c1` | | exit shedding on the deadline, not the band edge |
| HUD stage split | `63271ce7` | `9a146f5` | where the capture's time goes |
| HUD one readback | `a876bae3` | `efa32b4` | both mattes in one capture, one read |
| HUD shared checkpoint | `d2554481` | `48fc640` | one checkpoint for a capture's two mattes |
| status sync | | `6031f0a`, `59dfca0` | the record catching up with the branch |

## Work package status

| PR | Scope | Status |
| --- | --- | --- |
| PR 1 | Low-overhead frame-time tracing | Done. Trace ring, TCP export, `vr/trace_frames.py`. No frame-time gain by design; it is what makes every later number attributable. |
| PR 2 | Stereo shedding, recovery, stale-pair policy | Done, exit criteria measured, and a latch defect found later and fixed (framework `b6a671c1`). |
| PR 3 | Reuse stereo checkpoints safely | Done, exit criteria measured. Gain: `eye_begin_r` 143 to 36 us p50. |
| PR 5 | Guest/XR pacing | Slice 1 (cadence and pose-age tracing) done and verified. Slice 2 (opt-in resubmission) not started, and the measurements now say why it cannot pay for itself as scoped. |
| PR 4 | GPU capture and submission optimization | Not started as a package, but its first target is mostly worked: the wrist HUD capture (3.19 ms p50, readback 2.16 ms of it) had its stage cost located, its readback merged, and its checkpoint share removed. What remains on it is change-gated capture. |
| PR 6 | Mission/sector hotspots | Not started. Guest work is the largest single term in the frame (18.0 ms), so this is where the remaining ceiling is. |

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

## Test harness: headless SteamVR with a save slot

`vr\run_vr.ps1 -Headless -Slot 1` runs a real OpenXR session on SteamVR's null HMD
driver, with no headset and no VDXR, and loads save slot 1. This is the harness for
perf work from here: deterministic, repeatable without wearing anything, and it
reaches actual gameplay instead of boot and menus. SteamVR must be closed first, and
the switch manages its own start and stop.

Two things about it that cost a measurement each to learn:

- **A loaded save sits on the native save screen, not in gameplay.** That sets
  `g_menu_flat`, which skips the stereo pair and the whole HUD capture path, and the
  submissions go out as a quad layer (the readiness dump shows `submitted_layer: 2`,
  `quad_submitted` climbing). Resume through native Start, the way
  `capture_stereo.py --resume` does (`resume_if_paused` in `vr/weapon_state.py`,
  documented as "Existing measured native pause flag; resume through Start, never a
  RAM edit"). Traces taken without that step are save-screen numbers.
  **Check the state, do not trust the helper's return value.** `resume_if_paused`
  reports True whenever the native pause word is already 0, which is also true of the
  save screen in some states, so a run can report a successful resume and still never
  reach gameplay. The check that works is `openxr_stats.submitted_layer`: 2 is the menu
  quad, 1 is the stereo pair. Resume in a loop until it reads 1, then sample. This cost
  two runs: the first measurement of the single-readback capture produced no HUD record
  at all because the capture path was never entered, and the HUD diagnostic added for
  it printed nothing rather than failing.
- **The grant differs from VDXR's.** The null driver asks for 11.111 ms (one 90 Hz
  display frame); VDXR granted 22.222 ms (two). The shed policy compares against this
  deadline, so the same build behaves differently under the two runtimes, and the
  90 Hz grant is the harsher case.

## Shedding latch defect: found, fixed, A/B measured

Found with the harness above, and fixed in framework `b6a671c1`.

`stereo_shed_policy_should_shed` enters shedding when the pair-time estimate exceeds
the budget, and leaves it when the estimate drops below `budget * hysteresis`. Between
those two it holds whatever state it is in. The recovery probe's exit test used the
band's lower edge as well, so a pair time that settles *inside* the band is a latch: a
streak entered during any heavy moment never ends, because every probe resets the
streak timer and then fails its own exit test. The probe's exit test now uses the
deadline itself.

Identical scenario, `-Headless -Slot 1`, 1024 traced frames, 42 to 54 s live window:

| measure | pre-fix | post-fix |
| --- | --- | --- |
| shed frames of 1024 | 895 | 62 |
| published pairs per second | 3.8 | 28.4 |
| sheds per second | 26.2 | 1.6 |
| `avg_pair_ms` | 10.33 | 10.64 |
| budget (headset deadline) | 11.111 ms | 11.111 ms |
| `shedding` state | 1 (latched) | 0 |
| XR empty submissions | 895 | 86 |

Captures: `analysis/vr-proof/shed-prefix-paused`, `analysis/vr-proof/shed-fixed-paused`.
`test_stereo_shed_policy.c` gained the case that reproduces this; it fails against the
previous policy and passes against the new one. The heavy-scene behaviour PR 2 targeted
is unchanged: an over-deadline estimate is still required to enter shedding, and a
persistent overload still probes on the same streak and time bounds.

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

## Frame budget, with the HUD term finally measured

The HUD capture cost was invisible until `PSX_TRACE_HUD_CAPTURE` existed (framework
`5c67dced`). It is the wrist HUD's two per-frame capture transactions, one per matte
colour, each a full render transaction with its own checkpoint. Gameplay on the null
HMD, save slot 1 resumed, 1024 frames:

| term | p50 | p95 |
| --- | --- | --- |
| frame interval (one guest frame) | 33.4 ms (3.005 granted periods) | |
| guest logic and native present (derived) | 18.0 ms | |
| stereo pair, both eyes | 10.35 ms | 11.04 ms |
| wrist HUD captures, both transactions | 3.41 ms | 6.91 ms |
| ... of which the draw half (checkpoint, guest slice, verify) | 1.30 ms | 1.58 ms |
| ... of which the readback half (blit plus synchronous read) | 2.10 ms | 5.43 ms |
| XR acquire, copy, release, end frame | 0.86 ms | |
| submitted pose age | 16.01 ms | 22.85 ms |

Framework `63271ce7` splits the capture into those two halves, and the split says
where the cost is: the readback, 62% of the capture at p50 and 79% at p95, is the
driver's, not the guest's.

**The obvious fix is already ruled out by measurement.** The pair target is
2560x1200 (5x internal scale), so each capture blits 2560x1200 down to 512x240
before reading. Re-running with the internal scale clamped to 1, same scene class
(pair_total 10.41 vs 10.54 ms, so a fair comparison):

| | scale 5 | scale 1 |
| --- | --- | --- |
| capture | 3.41 ms | 2.80 ms |
| draw | 1.30 ms | 1.24 ms |
| readback | 2.10 ms | 1.56 ms |

The five-times downscale costs about 0.5 ms, and the guest HUD draw does not move
at all, so it is not pixel-bound. The readback is per-call overhead, about 0.78 ms
for a 512x240 blit and a 491 KB read at either scale. Rendering the capture at the
display's own resolution would therefore buy 0.5 ms and cost the capture its 5x
supersampling.

**That per-call estimate was wrong, and the fix built on it underdelivered.** Both
mattes now stage into one capture call and come back from a single read
(framework `a876bae3`, pinned here as `efa32b4`), verified byte-identical: the two
dumped mattes and the two reconstructed panels hash the same as the per-eye build.

| same scenario, 1024 frames | per-eye build | one readback | shared checkpoint |
| --- | --- | --- | --- |
| hud_capture | p50 3.41 ms | p50 3.26 ms | p50 3.19 ms, p95 5.10 |
| hud_draw | p50 1.30 ms | p50 1.22 ms | p50 1.09 ms, p95 1.27 |
| hud_readback | p50 2.10 ms, p95 5.43 ms | p50 2.05 ms, p95 4.66 | p50 2.16 ms, p95 4.00 |
| checkpoint saves per pair | 2.79 | 2.79 | **1.945** |
| checkpoint reuses per pair | 1.0 | 1.0 | **1.945** |

The p50 readback does not move, so at the median the two blits and the transfer
dominate and the per-call overhead is small; halving the number of calls shows up
only in the tail, where one stop-the-world read replaces two. That change is kept
because it is byte-identical, simpler and no slower, not because it moves the
budget.

The checkpoint share is where the measurable win was, and the capture's second matte
now reuses the baseline the first one captured (framework `d2554481`, pinned here as
`48fc640`), which is one 2 MiB RAM copy per frame less. `hud_draw` is the stage that
holds it, and it drops 1220 to 1087 us at p50, 1455 to 1265 at p95, while
`checkpoint_reuses` rises to match `checkpoint_saves` at 1.945 per pair: one save for
the capture and one for the pair, each reused once. Both changes were held to the
same acceptance test, byte identity, and the two dumped mattes and two reconstructed
panels hash the same as before in both.

`pose_age` is now fully explained: pair plus HUD captures plus the XR tail, with about
1 ms left for view maths and HUD preparation. The checkpoint work per pair is now one
save for the capture and one for the pair, down from three saves, and `hud_draw`'s
remaining 1.09 ms is the guest slice plus the blit setup rather than the RAM copy.

## What the trace says is left

1. **Wrist HUD captures, 3.19 ms per frame at p50 and 5.10 ms at p95.** Two
   transactions that re-run the same guest slice with a different matte colour. This is
   the largest measured non-guest cost and it is inside the frame rather than guest
   drawing. Its checkpoint share is gone (one save per frame, reused by the second
   matte), which leaves the draw's guest slice and the readback's two blits.
2. **Cadence.** One rendered pair serves about 1.5 granted periods under VDXR and 3.0
   under the null HMD's 90 Hz grant, so a large share of display periods carry nothing
   new and rely on reprojection of a pose up to 33 ms old.
3. **The grant itself is the constraint.** Guest work alone is 18.0 ms against an
   11.111 ms 90 Hz grant, and the full frame is 33.4 ms against either grant. Added
   presentation work cannot be free here: `xr_wait_frame` is p50 11 us, so the app
   never blocks today and any extra `xrWaitFrame` is time taken from the guest.
4. **XR spikes.** `xr_end_frame` mean 0.80 ms with a long tail, `xr_acquire` mean
   0.46 ms and `xr_copy` mean 0.06 ms. PR 4 territory, but smaller than the HUD term.
5. **Per-eye guest draw** is the bulk of the 10.35 ms pair, and inherent guest drawing
   (PR 6).

## Next steps

- **Wrist HUD captures, still the top measured non-guest cost.** 3.26 ms p50 and
  5.93 ms p95 per frame at the current tip, of which the readback is 2.05 ms p50 and
  4.66 ms p95.
  1. **One readback for both mattes. DONE** (framework `a876bae3`, pin `efa32b4`).
     Byte-identical output; the saving is in the tail, not at p50, and the per-call
     overhead hypothesis behind it was wrong. See the frame-budget table above.
  2. **Let the second matte reuse the first capture's checkpoint. DONE** (framework
     `d2554481`, pin `48fc640`). Saves per pair 2.79 to 1.945 and reuses 1.0 to 1.945,
     `hud_draw` p50 1220 to 1087 us, byte identity held. See the frame-budget table
     above. The scope the change introduced is the part worth keeping in mind: the
     capture's shared baseline is cleared before it returns, so the pair that follows
     in the same frame still captures its own first eye, and the sandbox test now
     asserts exactly that alongside the shared checkpoint.
  3. **Skip the capture pair when the HUD has not changed.** Now the only lever left on
     this term, and worth the whole 3.19 ms on the frames it would skip, but it needs a
     trustworthy change signal from the guest HUD state, and a stale HUD would be a
     visible bug. The counters are no longer the argument for it: they are already at
     their floor of one save per capture.
  Ruled out by measurement: rendering the capture at display resolution (0.5 ms, and it
  costs the capture its supersampling).
  **Panel rendering stays out of the headless harness, by decision.** The wrist panels
  are OpenXR surfaces gated on a tracked grip: `moh_vr_hud_prepare` needs the hand
  session focused with a valid origin and an age under 100 ms, and each side needs an
  active grip with all four tracking flags, while `native_ready` is rebuilt every frame
  by the capture. A null-HMD run has no controllers, so it submits nothing and the
  game's own HUD is what you would be looking at. The capture itself is still checkable
  headless through `PSX_VR_WRIST_DUMP_DIRECTORY`, which is how the byte identity above
  was proved, but the panels are verified on a headset instead.
- **PR 5 slice 2, with a caution that the measurements now support.** Re-presenting the
  retained pair at the granted cadence needs an extra `xrWaitFrame` per added frame, and
  `xr_wait_frame` is p50 11 us, so the app never blocks today and any added frame's wait
  and copy come out of guest time. Guest work is already 18.0 ms against an 11.111 ms
  90 Hz grant, so the added frames cannot fit for free. Build it only as a measured
  prototype, with the guest frame interval as a first-class reported metric.
- **The grant arithmetic is the real ceiling.** 33.4 ms of frame against an 11.111 ms
  grant cannot be fixed from the presentation side; it needs less work in the frame
  (HUD, pair, XR tail) or fewer guest cycles (PR 6).
- PR 4 (XR spikes) and PR 6 (per-eye guest draw) as scoped in the plan.

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

# headless: SteamVR null HMD, save slot 1, no headset. SteamVR must be closed first.
$env:PSX_FRAME_TRACE='1'; $env:PSX_FRAME_TRACE_CAP='1024'
vr\run_vr.ps1 -Headless -Slot 1 -WeaponPoseDiagnostic -WristHud -WristHudReplace `
  -BuildDirectory build-release -Seconds 200

# with a headset on VDXR instead: drop -Headless and -Slot, or keep the slot
vr\run_vr.ps1 -Runtime vdxr -Slot 1 -WeaponPoseDiagnostic -WristHud -WristHudReplace `
  -BuildDirectory build-release -Seconds 200

# export while the game is alive (the ring is only reachable over TCP)
python vr\trace_frames.py analysis\vr-proof\<name> --max 1024
```

A headless `-Slot` run loads the save onto the **native pause screen**, so resume
before sampling or the trace will describe the pause screen. The published tools do
this through `vr/weapon_state.py`'s `resume_if_paused`, which presses native Start and
refuses a RAM edit; the session harness did the same and then waited 36 s so the ring
(1024 records, about 34 s at 30 frames/s) held gameplay.

The framework render-pass and trace tests are not targets in `build-release` (that build
uses `runtime.cmake`, not `runtime/CMakeLists.txt`). Compile them directly, for example:

```powershell
gcc -O1 -std=gnu99 -I psxrecomp\runtime\include -o out.exe `
  psxrecomp\runtime\tests\test_frame_trace.c psxrecomp\runtime\src\frame_trace.c
python psxrecomp\runtime\tests\test_frame_trace_guard.py
```

Raw captures live under `analysis/vr-proof/`, which `.gitignore` excludes, so they are
local artifacts rather than tracked evidence.
