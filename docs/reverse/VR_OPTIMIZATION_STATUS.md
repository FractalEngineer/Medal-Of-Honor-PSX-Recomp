# VR optimization campaign: status and evidence

Companion to [VR_OPTIMIZATION_PLAN.md](VR_OPTIMIZATION_PLAN.md). Everything below is
measured, with the capture it came from named so it can be re-read. Framework changes live
in the `psxrecomp` branch `vr-perf-pr1-pr2`.

## Branches and pins

- Parent branch `vr-perf-pr1-pr2`. Framework pinned at **`d2554481`**, and the parent's
  `psxrecomp` gitlink matches it. The parent tip is whatever commit last touched this file:
  naming it here would only be one commit stale, since recording it changes it.
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
| headset recording tools | | `ed4a0d1` | session recorder and hitch report, in `vr/` |
| per-eye replay split | `571a8f10` | `3c4d769` | guest sub-stages, per-eye prims and cycles |
| attribution report | | `e416d45` | the split read out of the raw windows |
| status sync | | `6031f0a`, `59dfca0` | the record catching up with the branch |

## Work package status

| PR | Scope | Status |
| --- | --- | --- |
| PR 1 | Low-overhead frame-time tracing | Done. Trace ring, TCP export, `vr/trace_frames.py`. No frame-time gain by design; it is what makes every later number attributable. |
| PR 2 | Stereo shedding, recovery, stale-pair policy | Done, exit criteria measured, and a latch defect found later and fixed (framework `b6a671c1`). |
| PR 3 | Reuse stereo checkpoints safely | Done, exit criteria measured. Gain: `eye_begin_r` 143 to 36 us p50. |
| PR 5 | Guest/XR pacing | Slice 1 (cadence and pose-age tracing) done and verified. Slice 2 (opt-in resubmission) not started, and the measurements now say why it cannot pay for itself as scoped. |
| PR 4 | GPU capture and submission optimization | Not started as a package, and its first target is now mostly worked: the wrist HUD capture (3.19 ms p50 headless, 2.0 to 2.6 ms on the headset) had its stage cost located, its readback merged, and its checkpoint share removed. What remains on it is change-gated capture, which the headset data puts at 43 us of a slow frame. |
| PR 6 | Mission/sector hotspots | Attributed: the per-eye replay is split by call and the stretch is 97.8% `game_traverse`, with guest cycles, primitives and `tex_bytes` saying it is emulated per-object work. Next: the selector control run that says whether the lever is the per-eye selection or pruning what it returns. |

## Environment

Headset is 90 Hz. VirtualDesktopXR grants the app one submission per **22.222 ms**, that is
two display frames per app frame, though the grant is worth reading per session rather
than assuming: one headset session reported **11.111 ms** (see the headset section). Guest
vblank is 60 Hz. Runs use `RunVR-VDXR.bat` (`build-release`, `-WeaponPoseDiagnostic
-WristHud -WristHudReplace`), with `-Slot 1 -Seconds 90` for a bounded session window,
tracing on (`PSX_FRAME_TRACE=1`, `PSX_FRAME_TRACE_CAP=1024`), and `vr/record_session.py`
started first so the whole window is recorded.

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
  display frame); VDXR granted 22.222 ms (two) in the headset runs below, though one
  session reported 11.111 ms, so the grant is read per session rather than assumed
  (the trace carries `target_period_us`). The shed policy compares against this
  deadline, so the same build behaves differently under the two runtimes, and the
  90 Hz grant is the harsher case.

### Watching a headset session

`vr/record_session.py` records a session from before the game starts to after it exits:
one row a second (wrist surface submissions and uploads, XR submissions, losses, pairs,
shedding, checkpoint saves and reuses, rates), a trace window every ten seconds, the raw
records of every window, and stalls recorded rather than retried away. `vr/hitch_report.py`
then attributes a stutter from the raw windows. Both read stats only and never drive input.

Four things they taught, each of which cost a run or a reading:

- **Start the recorder before the game.** Recording after launch loses the boot and, in
  the case that prompted this, the inside of the only slow stretch that mattered.
- **`-Seconds N` bounds a headset run too**, not just a headless one: `RunVR-VDXR.bat
  -Slot 1 -Seconds 90` waits N seconds and then kills the process, which is what a
  reproducible session window wants.
- **`frame_trace` nests its export under a `trace` key and returns a small window unless
  `max` is passed.** Asking for `max=1024` is what makes a window cover the whole ring.
- **A window median hides a burst.** A trace window is about 34 seconds, so a few seconds
  of slow frames does not move its median: a per-window view of a session the player
  describes as stuttering reported "no slow windows" at `p50 pair 6.3 to 9.2 ms`. The
  hitches only appear when the raw frames are ranked, which is what `hitch_report.py`
  does and why the raw windows are kept. It now also prints the per-eye replay split (the
  seven `game_*` sub-stages, the per-eye prims and guest cycles) against the fast frames,
  plus the remainder the measured calls leave, so a mis-mapped entry cannot hide.

## Headset sessions: panels submit, and where the stutters are

Three sessions on the real headset through VirtualDesktopXR, recorded with
`vr/record_session.py`, all from save slot 1: a 185 s session, and two 90 s sessions
bounded by `-Seconds 90`. Captures: `analysis/vr-proof/live-headset-1`, `live-90s`,
`live-90s-2`.

**The wrist panels work on hardware, and this is the verification headless could never
give.** In the 90 s runs, 89 of 89 gameplay samples reported `submitted_surfaces: 2`,
`surface_uploads` climbed to 5200 and 5282 with `surface_failures: 0`, `tracking: 1`, and
the native HUD capture ran every frame (`hud_capture` p50 2.0 to 2.6 ms). So the panels a
null-HMD run cannot submit are submitted and uploaded on a headset with no failures, and
the capture feeding them costs the same on hardware as it does headless.

Headset frame terms, gameplay, per frame unless noted:

| term | headset | headless, same build |
| --- | --- | --- |
| `pair_total` | p50 6.3 to 15.2 ms across windows | p50 10.4 ms |
| `guest_sim` | p50 33 to 38 ms, p95 47 to 50 | p50 34 ms |
| `hud_capture` | p50 2.00 to 2.56 ms | p50 3.19 ms |
| ... `hud_draw` | p50 1.12 ms | p50 1.09 ms |
| ... `hud_readback` | p50 0.87 to 1.17 ms | p50 2.16 ms |
| `xr_wait_frame` | 2 us | 2 us |
| `pose_age` | p50 11.9 ms, 16.8 on slow frames | p50 14.1 ms |
| checkpoint saves / reuses per pair | 1.99 / 1.99 | 1.945 / 1.945 |

The readback being cheaper on the headset than on the null driver is the one term that
does not carry across runtimes, so headless readback numbers are the conservative case.
The checkpoint shape carries exactly, which is the shared-capture-checkpoint change
behaving in the product path.

### The stutter, attributed to the eyes' guest replay

The player reported slowdowns in the first 90 s session and not in the second. The record
says they were present in both, at nearly the same level, in the same parts of the level:

| | live-headset-1 (185 s) | live-90s | live-90s-2 |
| --- | --- | --- | --- |
| `pair_ms` median | 9.47 | 9.48 | 9.76 |
| seconds above 12 ms | 24 of 141 | 25 of 89 | 22 of 87 |
| worst second | 18.58 | 19.03 | 19.31 |
| slow stretch | t 44 to 93 s | t 44 to 93 | t 45 to 76, 82 |
| frames above 12 ms (raw) | | | 591 of 2441, p95 16.4, max 22.6 |

From `vr/hitch_report.py` over the raw windows of `live-90s-2`, medians of the slow frames
against the rest:

| stage | fast | slow | delta |
| --- | --- | --- | --- |
| `pair_total` | 8727 | 13658 | +4931 |
| `eye_guest_l` | 3786 | 6129 | +2343 |
| `eye_guest_r` | 3839 | 5975 | +2136 |
| `eye_end_l` / `eye_end_r` | 238 / 284 | 403 / 511 | +165 / +227 |
| `hud_capture` / `hud_draw` / `hud_readback` | 1989 / 1113 / 880 | 2032 / 1152 / 851 | +43 / +39 / -29 |
| `eye_begin_l` / `eye_restore_l` / `xr_end_frame` | 153 / 149 / 232 | 155 / 164 / 234 | +2 / +15 / +2 |
| `guest_sim` | 33000 | 35000 | +2000 |

4.5 ms of the 4.9 ms extra pair time is the two eyes drawing more, about 2.2 ms per eye.
The worst frame is seq 2102: 22.6 ms of pair of which 20.7 ms is the two eyes' replay,
with a 2.5 ms HUD capture, the same as a median frame's. The HUD capture, the checkpoint
begin and restore, and the XR path are flat to within tens of microseconds, so the stutter
is guest draw workload per eye and not the capture path this campaign has been working on.

The same windows carry the per-frame record counters, and they say what that extra drawing
is: **947 primitives on slow frames against 669**. One caveat on that number: the trace
writes `prims` once per eye transaction and the last eye of the pair therefore owns the
field, so it is one eye's count, not the pair's, and the two eyes are not separated in the
record today (see the open questions). `tex_bytes` is 0 on both, so it is more geometry per
frame rather than texture uploads, and not a VRAM leak, which points the PR 6 work at what
the eyes are drawing rather than at asset streaming.

It is also not a missed submission: `empty` held at 3 for the whole run, so the runtime
always presented something and covered the late frames by reprojecting a slightly older
pose (`pose_age` 11.5 ms on fast frames against 16.8 on slow). That is why 24% slow frames
can read as softness rather than a stall, and why the two sessions felt different while
measuring the same.

## Where the per-eye replay goes: instrumented, and the first reading

The eye term is the one that matters, so it is now split rather than reported whole. The
hook brackets each guest call it replays and the runtime times the spans with the eye's own
clock, seven slots that sum against `eye_guest`: `game_env` (draw environment, clear,
ordering-table setup), `game_world`, `game_text`, `game_traverse` (the scene traversal:
level groups and entities), `game_icon`, `game_submit` (DrawOTag) and `game_laser` (the
hook's own per-eye raycast). The record also carries `prims_l`/`prims_r`,
`tex_prims_l`/`tex_prims_r` and `cycles_l`/`cycles_r`, the guest cycles each eye's replay
executed, which separates emulated machine work from host submission time. All of it is off
unless `PSX_FRAME_TRACE` is on, and the runtime refuses the spans outside a traced stereo
eye, so a HUD capture's replay of the same scene cannot write them.

First reading, from a headless verification run (a 256-frame window at the save, not the
busy stretch): per-eye prims 1143 and 1142, per-eye guest cycles 638657 and 637929, and of
9662 us of per-eye replay, **`game_traverse` is 8943 us**. `game_submit` is 576 us,
`game_env` 72, `game_text` 31, `game_icon` 10, `game_world` 7 and `game_laser` under a
microsecond in that scene. The worst frame of the window (pair 12684 us) has the same
shape: 10491 us of its 11286 us of replay is the traversal.

So the replay is dominated by the scene traversal even where nothing is moving, and the
ordering-table submission and the draw environment are minor. That makes the traversal the
hypothesis for the stretch, and the headset session below is what decides it: the split is
only useful where the frames are heavy.

### The stretch, attributed

`analysis/vr-proof/pr6-attr`: 92 s of gameplay on the headset, 2409 frames, 641 above 12 ms,
worst pair 19.42 ms, same stretch as the earlier sessions (t 45 to 80 s from the save).
Medians of the slow frames against the rest, and each call's share of the rise in the two
eyes' replay:

| stage | fast | slow | delta | share of the eye rise |
| --- | --- | --- | --- | --- |
| `game_traverse` | 7019 | 11548 | **+4529** | **97.8%** |
| `game_submit` | 382 | 496 | +114 | 2.5% |
| `game_laser` | 14 | 19 | +5 | 0.1% |
| `game_env` / `game_world` / `game_text` / `game_icon` | 69 / 7 / 11 / 0 | 70 / 7 / 11 / 0 | +1 / 0 / 0 / 0 | 0% |
| `eye_guest` sum | 7528 | 12161 | +4633 | |
| remainder | 26 | 10 | -16 | |

Per eye, `prims_l` rises 640 to 864 and `prims_r` 660 to 860, `tex_bytes` stays 0, and the
guest cycles rise with the traversal: 458380 to 733349 (left) and 460155 to 713016 (right),
about 55 to 60 per cent. The slow frames' `pair_total` is +4839 us and the two eyes' replay
is +4608 of it, so the finding from the earlier sessions holds and is now located.

Two things the split adds. First, the time is **emulated machine work inside the scene
traversal**, not host submission: the guest cycles rise with it, and the primitive count
rose 30 per cent while the traversal's own cost rose 64 per cent, which points at per-object
processing (the level's node and group iteration, the entity transforms, the ordering-table
insertion) rather than at per-primitive or per-pixel work. Second, the remainder is -16 us,
so the seven calls account for the whole eye replay and nothing is hiding between them.

That selects the first branch of the decision gate in the next steps: the lever is what the
eye selects and how much of the scene the traversal has to process.

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

1. **Wrist HUD captures, 3.19 ms per frame at p50 and 5.10 ms at p95 headless, 2.0 to
   2.6 ms p50 on the headset.** Two transactions that re-run the same guest slice with a
   different matte colour. This is the largest measured non-guest cost and it is inside
   the frame rather than guest drawing. Its checkpoint share is gone (one save per frame,
   reused by the second matte), which leaves the draw's guest slice and the readback's two
   blits. It is not what the stutters are made of: on the headset it contributes 43 us of
   the 4.9 ms a slow frame carries, against 4.5 ms from the eyes' guest replay.
2. **Cadence.** One rendered pair serves about 1.5 granted periods under VDXR and 3.0
   under the null HMD's 90 Hz grant, so a large share of display periods carry nothing
   new and rely on reprojection of a pose up to 33 ms old.
3. **The grant itself is the constraint.** Guest work alone is 18.0 ms against a
   11.111 ms 90 Hz grant, and the full frame is 33.4 ms against either grant. Added
   presentation work cannot be free here: `xr_wait_frame` is p50 11 us, so the app
   never blocks today and any extra `xrWaitFrame` is time taken from the guest.
4. **XR spikes.** `xr_end_frame` mean 0.80 ms with a long tail, `xr_acquire` mean
   0.46 ms and `xr_copy` mean 0.06 ms. PR 4 territory, but smaller than the HUD term.
5. **Per-eye guest draw** is the bulk of the pair: median 4.3 ms per eye on the headset,
   6.1 and 6.0 ms on the slow frames, which is where the stutters come from (PR 6).

## Next steps

- **PR 6 has its first measured target, and it is the only thing on the critical path
  for what the player feels.** The stutter is the pair's per-eye guest replay: 4.3 ms per
  eye at the median, 6.1 and 6.0 ms on the frames above 12 ms, and 4.5 ms of the 4.9 ms
  extra pair time on a slow frame. The counters say the extra drawing is geometry: 947
  primitives against 669 (one eye's count, see the caveat above), with `tex_bytes` 0 in
  both.
  1. **The replay is split per call, and the split is done for the stretch.** DONE: the
     headset session above puts 97.8% of the eye rise in `game_traverse`, with the guest
     cycles rising with it, the primitive count up 30% and the traversal cost up 64%, and
     `tex_bytes` 0. The submission, the draw environment and the host remainder are all
     under 3%.
  2. **Narrow it inside the traversal.** `game_traverse` is the guest's scene traversal, so
     the two candidate levers inside it are different in kind: the hook's own per-eye
     AABB/PVS selection (`moh_vr_collect_tree`, called from `vr_head_frustum_filter`, host
     work that a pair-shared selection could do once per frame), or the guest's per-object
     processing of whatever the selection returns (fewer objects, which needs pruning). The
     cheapest way to weigh them is the existing selector control: a run over the same route
     with `-NoHeadFrustumDiagnostic` (the launcher switch that turns `PSX_VR_HEAD_FRUSTUM`
     off) bounds the whole per-eye selection, and needs its own session because the switch is
     launcher scoped. If the traversal stays heavy with the selector off, the cost is the
     guest's own per-object work and the lever is pruning; if it collapses, the selection's
     conservatism is the lever and it can be tightened without touching what is drawn.
  3. **Then the change, held to its acceptance.** Pruning or a tighter frustum must clear the
     pixel-compare harness at a fixed guest cycle against the current baseline and the
     accepted look on the headset (no missing tiles, no head-turn pop-in, no near-player
     floor gaps). Anything that only moves bytes must keep the pair byte-identical.
  4. The stretch reproduces in the same part of the level in every session, so the route can
     be walked again deliberately. The two things not to do: treat the HUD capture as a
     suspect (it moves 115 us of that 4.8 ms), and treat it as a submission problem (the
     runtime never missed one: `empty` held through the whole session, and the slow frames
     were covered by reprojection at a pose 5 ms older). Both are already measured, so
     neither needs a run.
- **Wrist HUD captures, the top measured non-guest cost.** 3.19 ms p50 and 5.93 ms p95
  per frame headless, 2.0 to 2.6 ms p50 on the headset, of which the readback is 2.16 ms
  p50 headless and 0.87 to 1.17 ms on the headset.
  1. **One readback for both mattes. DONE** (framework `a876bae3`, pin `efa32b4`).
     Byte-identical output; the saving is in the tail, not at p50, and the per-call
     overhead hypothesis behind it was wrong. See the frame-budget table above.
  2. **Let the second matte reuse the first capture's checkpoint. DONE** (framework
     `d2554481`, pin `48fc640`). Saves per pair 2.79 to 1.945 and reuses 1.0 to 1.945,
     `hud_draw` p50 1220 to 1087 us, byte identity held, and confirmed in the product path
     on the headset at 1.99 saves and 1.99 reuses per pair. The scope the change
     introduced is the part worth keeping in mind: the capture's shared baseline is
     cleared before it returns, so the pair that follows in the same frame still captures
     its own first eye, and the sandbox test now asserts exactly that alongside the shared
     checkpoint.
  3. **Skip the capture pair when the HUD has not changed.** Now the only lever left on
     this term, and worth the whole 2 to 3 ms on the frames it would skip, but it needs a
     trustworthy change signal from the guest HUD state, and a stale HUD would be a
     visible bug. The counters are no longer the argument for it: they are already at
     their floor of one save per capture. Lower priority than it looked: the headset data
     puts it at 43 us of a slow frame.
  Ruled out by measurement: rendering the capture at display resolution (0.5 ms, and it
  costs the capture its supersampling).
  **Panel rendering stays out of the headless harness, and it is verified now.** The
  wrist panels are OpenXR surfaces gated on a tracked grip: `moh_vr_hud_prepare` needs the
  hand session focused with a valid origin and an age under 100 ms, and each side needs an
  active grip with all four tracking flags, while `native_ready` is rebuilt every frame by
  the capture. A null-HMD run has no controllers, so it submits nothing and the game's own
  HUD is what you would be looking at. The capture stays checkable headless through
  `PSX_VR_WRIST_DUMP_DIRECTORY`, which is how the byte identity above was proved, and the
  panels themselves were verified on the headset instead: 89 of 89 gameplay samples with
  `submitted_surfaces: 2`, 5200 and 5282 uploads, no surface failures. That closes the
  panel question this campaign had been carrying.
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
4. Why the same slow stretch appears at t 44 to 93 s in every session, at the same level
   position from the same save. If it is a fixed part of the level rather than the player's
   route, the route can be walked deliberately for profiling instead of played.
5. The primitive counter used to report only the last eye of the pair. `prims_l`/`prims_r`,
   `tex_prims_l`/`tex_prims_r` and `cycles_l`/`cycles_r` are recorded now, so a change to
   what one eye selects can be checked on that eye. Closed in `571a8f10`.

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

**A recorded headset session wants the recorder started first**, because it waits for the
debug port rather than missing the boot, and `-Seconds` works outside `-Headless` too:

```powershell
$env:PSX_FRAME_TRACE='1'; $env:PSX_FRAME_TRACE_CAP='1024'
Start-Process python -ArgumentList 'vr\record_session.py analysis\vr-proof\<name>'
cmd /c RunVR-VDXR.bat -Slot 1 -Seconds 90
python vr\hitch_report.py analysis\vr-proof\<name>
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
