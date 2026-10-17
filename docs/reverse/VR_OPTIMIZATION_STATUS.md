# VR optimization campaign: status and evidence

Companion to [VR_OPTIMIZATION_PLAN.md](VR_OPTIMIZATION_PLAN.md). Everything below is
measured, with the capture it came from named so it can be re-read. Framework changes live
in the `psxrecomp` branch `vr-perf-pr1-pr2`.

## Branches and pins

- Parent branch `vr-perf-pr1-pr2`. Framework pinned at **`0fe8a425`**, and the parent's
  `psxrecomp` gitlink matches it. The parent tip is whatever commit last touched this file:
  naming it here would only be one commit stale, since recording it changes it.
- Framework branch `vr-perf-pr1-pr2`, tip `0fe8a425`.
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
| attribution result | | `f7dae7f`, `ad79640`, `00ebe30` | the split, the stretch, and what the shakier session measured as |
| traversal split | `87a3e822` | `d86a678` | a nested span for the hook's own visibility override |
| traversal attribution | | `2850982`, `60391fa` | the guest work is the framework's dirty-RAM interpreter |
| overlay route | | `bafdba9`, `8e4954b` | the overlay cache compiled and measured |
| load-frame trap | `f5229748` | `ea80e78`, `84c7e07` | the A/B was confounded, and the new saves recorded |
| confound list | `1cb15cf4` | `63bf323` | no readable verdict until the load seed exists |
| seeded protocol | `ce7921f6` | `c92f2c5` | boot seeds pin the load; fork at frame 240, first write named |
| Layer 3 mechanism | `0fe8a425` | `fdeabb1` | publication schedule site, frame 2 corroboration |

## Work package status

| PR | Scope | Status |
| --- | --- | --- |
| PR 1 | Low-overhead frame-time tracing | Done. Trace ring, TCP export, `vr/trace_frames.py`. No frame-time gain by design; it is what makes every later number attributable. |
| PR 2 | Stereo shedding, recovery, stale-pair policy | Done, exit criteria measured, and a latch defect found later and fixed (framework `b6a671c1`). |
| PR 3 | Reuse stereo checkpoints safely | Done, exit criteria measured. Gain: `eye_begin_r` 143 to 36 us p50. |
| PR 5 | Guest/XR pacing | Slice 1 (cadence and pose-age tracing) done and verified. Slice 2 (opt-in resubmission) not started, and the measurements now say why it cannot pay for itself as scoped. |
| PR 4 | GPU capture and submission optimization | Not started as a package, and its first target is now mostly worked: the wrist HUD capture (3.19 ms p50 headless, 2.0 to 2.6 ms on the headset) had its stage cost located, its readback merged, and its checkpoint share removed. What remains on it is change-gated capture, which the headset data puts at 43 us of a slow frame. |
| PR 6 | Mission/sector hotspots | Attributed four layers deep and then handed off: the replay is split by call, the stretch is 97.8% `game_traverse`, the hook's own selection inside it is 0.9%, and the guest work that is left is the dirty-RAM interpreter running this game's overlay code (556,199 instructions per pair, 66% of the frame). The overlay cache was compiled and measured: interpreter share down to 0.517 to 0.526, interpreted instructions per pair down 46.2%, settled pair time down 12 to 14%, every replay dispatch entry off the interpreter. Its fidelity fails on a **six-cycle phase**: with the pair policy pinned (`PSX_STEREO_SHED=0`) both backends are self-consistent (shard pair identical at 18404 accesses, interp pair identical at 18407) and shard against interp diverges at the frame's first access, six cycles apart, with the kernel's interrupted context (`0x909C`, the OpenBIOS exception prologue at `0x27D4`) saved differently. An interp pair read as the reference moving until the shedding policy was pinned: with shedding on the interp sheds 22 to 23 times by frame 340 against the shard's 8 and one run shed inside frame 240, which is why no A/B on this title is readable without that flag. Next is the accounting diff (`psx_cycle_count`, `g_psx_cyc_batch`, `g_psx_cyc_bb_defer`, `psx_get_cycle_count()` per instruction, frames 239 to 240, both backends) to name the charge site. The route stays off and the in-campaign levers stay the fallback. |

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
  **The saves in the tree now load straight into gameplay**, so the resume is not needed
  for them: both slots verify as `submitted_layer: 1` with pairs advancing and no failures
  (slot 0 sha256 `20462E2F636ABDF6320A90F020A31873600000586945C9CE1735387C510B8AE8`,
  slot 1 sha256 `6EC4DCE71AF9DBE5C5546914B236E423D17518994D627A2C87E6EAADF623B1F6`). That
  matters beyond convenience, because `resume_if_paused` is one of the three confounds in the
  fingerprint A/B above: a save that starts in gameplay removes it outright. The save-screen
  save this section was written against has been overwritten by that newer slot 1, so any
  baseline keyed to it has to be re-taken. Tracked-weapon settings are launch-side
  (`-WeaponPoseDiagnostic -WristHud -WristHudReplace`, which all three `RunVR-*.bat` launchers
  pass and which only `RunFlat.bat` clears when it scrubs the `PSX_VR_*` environment), and
  they change presentation rather than the save, so a save taken without them is still usable
  here: the bench applies its own flags on launch.
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
clock, eight slots that sum against `eye_guest`: `game_env` (draw environment, clear,
ordering-table setup), `game_world`, `game_text`, `game_traverse` (the scene traversal:
level groups and entities), `game_icon`, `game_submit` (DrawOTag), `game_laser` (the
hook's own per-eye raycast) and `game_select` (its level visibility override, added later;
it nests inside `game_traverse`, so it is a subset of it rather than a sibling). The record
also carries `prims_l`/`prims_r`,
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
eye selects and how much of the scene the traversal has to process. It turns out to be the
second half of that sentence, which the next measurement settles.

### Where the traversal goes: the hook's own selection is 0.9 per cent of it

The two candidate levers inside `game_traverse` are different in kind, so they were separated
the way the guest calls were. The hook's level visibility override is host work executed
inside the eye's guest traversal, so its span nests in `game_traverse` and what is left of
that stage is emulated machine work on whatever the override selected. Framework `87a3e822`
adds `PSX_TRACE_GAME_SELECT` and lets one span nest in another (an outer span still measures
wall time, so a nested stage is a subset of its parent, and the sandbox test fails against
the single span version that would have truncated the outer one). Parent `d86a678` brackets
the override, which is where `moh_vr_collect_tree` and the leaf queue it writes live.

Headless, save slot 1 resumed to gameplay, one 1024 frame window. This scene is a usable
bench because it is the same every run: per-eye prims 1143 and 1142 and per-eye guest cycles
638657 and 637929, unchanged from the earlier reading of the same save.

| pair total, medians over 1024 frames | us |
| --- | --- |
| `game_traverse` | 8878 |
| `game_select`, inside it | **81** |
| guest work inside the traversal | 8797 |
| `eye_guest` sum, the whole per-eye replay | 9591 |
| `game_env` / `game_submit` / `game_text` / `game_world` / `game_icon` | 73 / 577 / 30 / 7 / 10 |

The override is 0.9 per cent of the traversal and 0.8 per cent of the replay, and the
containment check holds in all 1024 frames, so it is not running anywhere else and the
subtraction is sound. The worst frame of the window says the same at the other end: 14042 us
of traversal, 110 us of it selection.

That retires the first candidate. Sharing one selection across the pair, or tightening the
frustum's conservatism, can return at most 81 us per pair here, which is nothing against a
4.5 ms rise and not worth any fidelity risk. **The lever is the guest work inside the
traversal**: 8797 us of per-object processing of the set the override returns, on a scene
where nothing is even moving. That it is emulated machine work and not host submission is
already established above by the guest cycles rising with it.

How far that goes is bounded by the whole replay: the traversal is 92 per cent of `eye_guest`
at the save and 95 per cent of it on a slow frame, so the pair's guest drawing is where the
frame's 4.5 ms of extra time lives and where anything that wants to move the p95 has to work.
What that guest work actually is turns out to be the biggest finding of this step, and it is
not a game routine at all: it is the framework's interpreter.

### The guest work is the dirty-RAM interpreter, and it is two thirds of the frame

Profiling the same headless gameplay, with the framework's own always-on instrumentation
(`phase_profile`, `gcp_ring` exported as CSV, `dirty_ram_stats`, `overlay_loader_status`):

| measure | value |
| --- | --- |
| execution phases, 15 to 30 s windows | interp **63.7 to 66.6%**, static 30.7 to 34.1%, native overlay **0.0000**, gpu 2.5 to 2.7%, exception 2.1% |
| interpreted instructions, session | 113018 blocks, **459,522,240 instructions** |
| two snapshots 20 s apart | **556,199 interpreted instructions per pair**, 149 blocks per pair, over 510 pairs at 25.5 pairs/s (`avg_pair_ms` 10.88) |
| guest cycle ring, last 500 gameplay frames | interp 65.95%, static 34.05%, and **99.9% of the interpreted weight is at phys >= 0x3A000** (the runtime-loaded overlay region) against 0.10% in the main-EXE text band |

`game.toml` says why: this game loads at `0x80010000` with `text_size 0x2A000`, so the
statically recompiled main-EXE text ends at phys `0x3A000` and everything the game actually
plays from is streamed in above it. The framework interprets runtime-loaded overlay code
unless a compiled variant is registered for the region, and for this game nothing is:
`overlay_loader_status` reports `registered: 0`, `regions: 0`, `loads: 0`, `dispatch_native: 0`,
`dispatch_interp_fallback: 113019`, `file_found: 0` and no candidates, and
`overlay_static_entries` says `no static overlay dispatcher linked (GAME_OVERLAY_STATIC_C
unset)`.

The per-eye replay dispatches that same overlay code twice per frame: the interpreter's per-PC
table shows the traversal `0x6C714` with **1648 hits, which is exactly two per pair**, and
11.27M instructions, and the world pass `0x64550` likewise at 1648 hits. In the gameplay ring
window the hottest interpreted block is `0x8006DAD8` at 18 per cent of all interpreted time,
and it is inside the `0x8006C714` traversal the hook replays; the next three are `0x8006DC48`
(8.9%), `0x8003DD6C` (6.6%) and `0x8006DAC8` (5.6%). For contrast, the hottest statically
dispatched block is `0x80011214` (`vertex_project`, 7.3 per cent of the window), which is the
main-EXE text band and runs compiled.

So the 8797 us of guest work in the traversal is emulated machine work in the sense the earlier
section already established, and now it is named: it is the interpreter running overlay code at
roughly 17 to 20 ns per instruction, about 556k instructions per pair across the frame. The
earlier host profile of the pass says the same from the other side: its top entries are the
interpreter's own machinery (`psx_cyc_step` 13.9%, `exec_one_fetched`, `psx_icache_fetch`,
`ls_at_leader`, `psx_dispatch_impl`, `dirty_ram_dispatch`) and not one hot game function.

Two limits on that reading, both recorded rather than glossed. The ring has no pass filter, so
these shares are every frame's execution, native path and both replays together; what ties the
replay to the interpreter is the per-PC dispatch counts (two per pair on the replay's own
calls) and the pass profile. And the interpreter's cost per instruction here is derived from
the pair rate and instruction counts, not measured directly: 556k instructions inside a
10.88 ms pair bounds it at under 20 ns per instruction, which is the console's own rate, far
from the ~300 ns per instruction the framework's DirtyRam header cites for the worst case.

**This is the framework's own documented work item, not a VR one.** `docs/internal/overlay-status.md`
in the framework tracks the overlay capture, compile and cache feature: it works on Tomba (one
region captured, 88 functions compiled into a DLL and dispatched natively), and its own note
says the payoff there is correctness so far, because only one overlay is compiled and an
invalidated region is not re-registered in the same session: "speed payoff needs broad overlay
coverage + Inc2 reload-on-return". Medal of Honor has no coverage at all, which is what these
counters show. The route from here is the framework's own: capture this game's overlays, compile
them with `compile_overlays.py`, and let the loader dispatch them natively, with the Inc2 work
that keeps them registered across re-visits.

For PR 6 that changes the value of the two levers it had, without changing which they are. Every
object the eyes stop processing also stops being interpreted, so pruning is worth its
instructions at the interpreter's rate rather than at some abstract guest-work rate, and the host
replacement pattern this campaign already uses at `8008B3E8` has a named target list now: the
hottest interpreted blocks are the traversal's own `0x8006DAD8` and `0x8006DC48`. Neither is the
two thirds of the frame that the overlay cache is worth, but both are inside the campaign's
reach, and the second one is the pattern already in the tree.

### The pr6-attr session felt shakier, and what that measured as

The player reported the session above as shakier and less smooth than the earlier runs. The
record agrees with the feel, and rules out the explanations that would have been convenient.

| | live-90s-2 | pr6-attr |
| --- | --- | --- |
| gameplay seconds | 87 | 87 |
| `pair_ms` p50 / p95 / worst second | 9.76 / 15.96 / 19.31 | 10.20 / 16.51 / 17.92 |
| seconds above 12 ms | 22 | 31 |
| frames stalled past 45 ms (`guest_sim`) | 43 of 2441 | 93 of 2409 |
| sheds during gameplay | 1 | 1 |
| empty submissions during gameplay | 1 | 1 |
| `hud_capture` p50 | 2.03 ms | 2.37 ms |
| `hud_readback` p50 | 0.87 ms | 1.29 ms |

Two things follow from that table.

- **Shedding is not the cause, and the raw counters are misleading here.** `shed` and
  `empty` are cumulative, and both sessions already read 15 or 16 and 2 or 3 in the
  recorder's *first* row: the difference is the load and save-screen phase before gameplay,
  not the session. Through the 87 s of gameplay each session shed once.
- **The degradation is spread across the frame, and its largest relative rise is the HUD
  readback, which is the driver's.** The pair is 4.5% slower at p50, the HUD capture 17%,
  and the readback 48%, while stalled frames double. A synchronous `glReadPixels` taking
  half as long again reads as the machine or its GPU being busier, not as this change: the
  instrumentation adds fourteen clock samples per frame (under a microsecond) and a trace
  export once per ten seconds that is 40% larger than before, which cannot account for 50
  extra stalled frames spread through the session, and the stalls show no ten-second period
  that an export artefact would produce.

So the position is: the session was heavier, the feel is corroborated by the stall count,
and the cause is not in the record. What the record cannot separate is the route (only the
guest frame counter ties a frame to a position) and the state of the machine the second time
round, which followed a VDXR restart and a headless run on the same GPU that also drives the
headset streamer.

The campaign's own rule is that instrumentation must not disturb what it measures, so the
instrument was measured rather than guessed at. The in-game half of it is the debug server's
export, and `handle_frame_trace` builds it twice, once to size the text and once for real,
every time the recorder asks for a window. Timing that exact path offline, filled with 1024
records through the module's own API, costs **5.4 ms** of CPU (2.7 to measure, 2.7 to build,
for 0.94 MB of text), and the recorder's own share is 6.1 ms to parse the same window plus
the file write. So a window costs the machine about 12 ms of CPU per ten seconds, split
across two processes.

That bounds the harness. At worst one frame per window is pushed past the 45 ms stall line,
so about nine of the 50 extra stalled frames in the table above, and realistically fewer,
because the block has to land on a frame that was already heavy for it to cross the line. It
cannot be the other 41, and it is not the 4.5 per cent slower median pair either.

The tracing-off session that was planned to settle this is dropped, for a plain reason:
`guest_sim` is a trace field, so the stall count, which is the number the acceptance was
written against, does not exist without the trace, and the pair-time numbers it could still
measure are already bounded above. What replaces it is a rule for reading the record. The
spread between sessions is 43 to 93 stalled frames and 4.5 per cent on the pair median for
the same build on the same route, which is larger than the change any of this work is
chasing, so a comparison that is going to be believed has to be within a single session
(slow frames against fast frames, which is how the attribution above is read) or repeated
across sessions before it is trusted.

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

## The overlay cache is the route, and this is the ask

Decided after the interpreter measurement above. The largest lever in this frame is not in it:
63.7 to 66.6 per cent of wall time is the framework's dirty-RAM interpreter running this game's
overlay code (556,199 interpreted instructions per pair, 99.9 per cent of interpreted weight
above phys 0x3A000), because the statically recompiled image is this game's text, 0x2A000 bytes
from 0x80010000, and every scene the game plays from is DMA'd in above it. So this goes to the
framework that owns the overlay cache, with the counters as evidence, rather than being spent on
levers worth tens of microseconds or on one interpreted block at a time.

Nothing in the route is new machinery, and the capture half is already done. From the artifacts
themselves: `build-release/overlay_captures.json` holds 6 capture records, 340 KB in total
(schema `psxrecomp overlay capture v3`), and between them they cover every hot interpreted
address in the profile above:

| capture | covers | seeds |
| --- | --- | --- |
| `0x66000` + 143364 | the traversal `0x6C714`, and the hot blocks `0x6DAD8` and `0x6DC48` | 46 |
| `0x8A000` + 20484 | the level blocks `0x8BE6C`, `0x8BFCC`, `0x8CDB4` | 10 |
| `0x3D000` + 163844 | the world pass `0x64550` and `0x3DD6C` | 23 |

(`0x80011214` `vertex_project` is not captured, correctly: it is main-EXE text and already runs
compiled.) What has not happened is the compile: `build-release/cache/SLUS-00974/` contains an
empty `datashards/` directory and no DLL, which is why the loader reports `registered: 0` and
`dispatch_native: 0` while falling back 113019 times. **So the first increment is to point the
overlay compiler at the capture that already exists**, not to go and capture again.

The route from there, from the framework's own docs and tools:

1. **Capture.** The config gate is already open here (`[runtime] overlay_cache = true` in
   `game.toml`) and the capture above already exists, so this step is done for the ranges that
   matter; more playthroughs only widen coverage (the stretch is already in it).
2. **Compile.** The compiler and its CLI are already in the tree, and it can take one region at a
   time, which is how to start:
   `python psxrecomp/tools/compile_overlays.py --captures build-release/overlay_captures.json
   --game-toml game.toml --recompiler psxrecomp/recompiler/build/psxrecomp-game.exe
   --runtime-include psxrecomp/runtime/include --out-dir build-release/cache --only-region 0x66000`,
   with `--check` first for a dry run. It writes `<cache>/<game-id>/<crc32>.dll`, namespaced by
   codegen version so new emitter output reads and writes a fresh directory instead of a stale DLL.
   Region `0x66000` is the traversal and its two hot blocks, so it is the one to try first.
3. **Register and run.** The loader `LoadLibrary`s on the first dispatch into the region and
   registers the exported `func_X` entries ahead of the interpreter. `overlay_loader_status` is
   the instrument: `registered`, `loads`, `dispatch_native` against `dispatch_interp_fallback`.
4. **Clear the two documented blockers if they bite.** `docs/internal/overlay-status.md` records
   OV-1, stale registration when a region is reused, fixed by write-invalidation (Inc1-D), and
   names Inc2, reload-on-return, as the increment that turns coverage into speed: without it an
   invalidated region is not re-registered in the same session, which is why the Tomba run there
   shows `dispatch_native` of order 100 against 31M interpreted dispatches.

Acceptance, in this document's own terms, so the same readings compare:

- **The direct measure is the phase share.** Interpreter down from 63.7 to 66.6 per cent, and
  interpreted instructions per pair down from 556,199, read exactly as the baselines above were
  (15 to 30 s `phase_profile` windows, two `dirty_ram_stats` snapshots against the pair counter).
- **The frame.** `pair_total` p50 9.9 to 10.9 ms headless, and on the headset the p95 with the
  worst spikes at the stretch: worst pair 19.42 ms and 641 frames above 12 ms in `pr6-attr`.
- **Correctness.** The campaign's pixel-compare harness at a fixed guest cycle against the current
  baseline, plus the accepted look on the headset (no missing tiles, no head-turn pop-in, no
  near-player floor gaps). The overlay path changes which backend executes code, not what is
  drawn, so byte identity of the pair is the right bar until something else is deliberately
  changed.

While that is with the framework, the VR-side items left in this document are all small against a
66 per cent interpreted share: host-replacing the hottest interpreted block (`0x8006DAD8`, 18 per
cent of interpreted time), the HUD capture's change gating (43 us of a slow frame), PR 5 slice 2
and PR 4's XR spikes. One consequence to carry forward: every baseline in this document was taken
with the interpreter inside the frame, so if the overlay work lands they all have to be re-taken
before any VR comparison is believed again.

## The overlay cache measured: a large win, blocked on fidelity

The route above was run, end to end, on the same headless bench. Two things came out of it, and
the second one decides what happens next.

**First, it works.** The overlay compiler refused at first, with its own guard: the recompiler
binary was stale against the runtime tree's cache tag (emitter hash `baa243b3` against runtime
`1b6950cb`), so it was rebuilt (`cmake --build psxrecomp\recompiler\build --target
psxrecomp-game`, a gitignored build directory) and then compiled the capture that already
existed: 41 shards for `0x66000` alone, then all six regions for 67 shards, 4 skipped as
data-only or cached. Measured against the baselines above, on the deterministic save:

| measure | baseline | `0x66000` only | all six regions |
| --- | --- | --- | --- |
| interpreter share | 0.637 to 0.666 | 0.580 | **0.517 to 0.526** |
| static share | 0.307 to 0.341 | 0.350 | 0.370 to 0.377 |
| native overlay share | 0.0000 | 0.039 to 0.043 | **0.075** |
| interpreted instructions per pair | 556,199 | 376,990 | **299,445 (-46.2%)** |
| `avg_pair_ms`, settled headless | 10.88 to 11.13 | 9.44 to 9.79 | **9.52 to 9.59 (-12 to -14%)** |
| loader | `registered 0`, `dispatch_native 0`, 113019 interpreter fallbacks | `registered 29`, native 275232 | **`registered 89`, native 1,874,950**, 280846 fallbacks, 0 invalidations, 0 stale-blocked |

The sharpest structural check agrees: the interpreter's per-PC table no longer contains any of
the replay's dispatch entries. Before, the traversal `0x6C714` and the world pass `0x64550` had
1648 hits each, two per pair, at 11.27M and 0.43M instructions, and `0x8BFCC` had 15694 hits. With
all six regions compiled, every one of them is absent: those dispatches run native now. So the
prize is at the top of the estimate, and the per-eye replay that PR 6 chased is largely off the
interpreter.

**Second, the A/B reports a difference between the backends, and that difference is not yet
attributable.** The acceptance is byte identity of the guest work, so the runtime's own judge was
run, `frame_fingerprint` (per-frame write, MMIO, SPU and memcard hashes and counts, plus per-frame
guest cycles) through `vr/compare_frame_fingerprints.py`:

- Two shard-free runs on the same bench: **equal, 0 of 96 frames differ**
  (`fingerprint_nocache2` against `nocache3`).
- Shard-free against shards: **all 96 frames differ** (`fingerprint_nocache2` against
  `shards2`), in `wc`, `ws`, `wr`, `pc`, `sp` and `sc`, while per-frame cycles, quiet counts,
  MMIO and the memcard columns are equal. The magnitudes are small: write count +0.04 per cent,
  SPU count -0.2 per cent.
- All three runs loaded the same savestate, and their windows sit on the same guest frame labels,
  203 to 298, so the ordinal alignment is right.

Read as "the shards fork the guest", that would be the project's own standard for a fidelity
failure, and it is not the known cycle-model fork either: shards are already built with
`PSX_ENABLE_BLOCK_CYCLES=1` to match the interpreter, and the per-frame cycle counts do match.
The interpreter's deferred load-delay writeback (`dirty_ram_interp.h`) has no counterpart in the
shard compile line and was the first thing to suspect. But the two confounds below mean the
difference cannot be assigned yet, and the honest status is unattributed rather than a shard bug:
the load frame was matched by luck rather than by construction, the sub-frame phase at the load
was never controlled, and the resume pressed Start at backend-dependent frames.

Three confounds decided whether that difference was a shard bug or a measurement artifact. The
recipe below closes all three, and the verdict then came out readable.

- **The load frame.** Measured: two runs of one backend that loaded on frames 606 and 914 diverge
  from ordinal 69 (`ws`, `wr`) and 72 (`wc`, `pc`, `mmio`, `mc`, `sp`, `sc`), differing in 107 to
  110 of 179 compared ordinals in every write column, with `cyc` and `qc` equal throughout and
  4926 writes apart in a single frame. That is the signature a real fork leaves, which is exactly
  why it is dangerous. The bullet-list runs all loaded on frame 3, right after boot, so they
  matched by luck. Worse, the frame cannot be matched deliberately: `savestate_poll` applies a load
  at the next block-leader boundary, so the request-to-apply lag is host wall-clock and therefore a
  different number of guest frames per backend. Gating both runs on the frame counter, with a
  4 ms poll against a measured 7.6 ms per command, still produced 8306 on the native at 58 fps
  against 5004 on the interp, after 606 against 615 and 617 against 620. And it cannot be pinned
  with a synchronous control, because `pause`, `continue`, `step` and `run_to_frame` are
  deliberately removed in this runtime and answer `send_err`. So a matched load frame is luck, and
  even when it happens it does not guarantee a matched sub-frame phase, which means the
  bullet-list difference is the campaign's best candidate evidence and not proof.
- **The sub-frame phase.** `savestate.c` applies a load at a safe point inside a guest frame, so
  the guest executes a variable slice of a frame between the restore and the first ring snapshot,
  and ordinal 1 is not automatically the same guest state. A slower backend reaches any gate at a
  different phase, and the interp reference boots the kernel in the dirty interp, so its debug
  port only answers around guest frame 4200 here.
- **The resume.** The bullet-list A/B resumed through `resume_if_paused`, which presses Start when
  the submitted layer changes, and that event is backend-dependent, so input timing was not
  eliminated as a source either.

The void attempts are recorded so nobody re-derives them: a native and an interp run whose loads
completed on frames 606 and 615 showed a first divergence at ordinal 4, and a repeat pair, 617
against 620, showed the same early pattern; with parity unverified, neither means anything. The seed
that removes all three confounds turned out to be the runtime's own `PSX_LOAD_SLOT`, which has been
there all along and which this campaign simply was not using: the probe-driven load was the path
under test and the wrong one to measure with. With it, the protocol is a boot-seeded load, a
boot-seeded recorder frame, a no-input window, and a same-backend pair required to be bit-identical
before any cross-backend difference is read as a fork. The framework's copy of this rule and the
worked example are in `psxrecomp/tools/divergence/README.md`. Separately, no pixel or headset check
was attempted, deliberately: measuring the look of a backend that forks the guest's own write stream
would be premature.

**The recipe, and it is launch flags rather than framework work.** The probe cannot stage a
comparable load, because `savestate_poll` applies at the next block-leader boundary and the
request-to-apply lag is host wall-clock, but the runtime's boot seeds can. `PSX_LOAD_SLOT=1` stages
the load during init, so it applies at the first safe block boundary before frame 1 in every run,
and `PSX_RECORD_FRAME=N` seeds the Layer-2 recorder at an absolute guest frame. Both are needed:
attempting to arm frame 240 from the probe recorded zero accesses because the frame had already
passed, and seeded at boot it recorded 18404. Everything below is on that protocol, which needs no
probe load, no resume and no input.

**The verdict, and the two corrections it took to reach it.** Two runs of the shard backend agree on every
column across 393 compared frames, and shard against the interp reference forks at **frame 240**, in `wc`,
`ws`, `wr`, `pc`, `sp` and `sc` with `cyc`, `qc`, `mmio` and `mc` equal. Layer 2 named `0x0000909C` from
store PC `0x000027DC` as the first divergent RAM write, and Layer 3 read that store sequence as the OpenBIOS
exception prologue (`k0 = *(*(0x108)) + 8`, then `SW $at/$v0/$v1`, `SW $ra,124($k0)`), so the divergent word
is the interrupted context's `$v1` in the kernel save frame at `0x9090`, with the shard six cycles ahead
from the frame's first access while the absolute cycle at every frame boundary is equal.

An interp pair then disagreed with itself, which looked like the reference moving and got the fork
retracted. It was a shedding artifact: with the pair policy pinned (`PSX_STEREO_SHED=0`) both backends are
self-consistent and the fork reproduces exactly.

| pair at frame 240 | shedding on | `PSX_STEREO_SHED=0` |
| --- | --- | --- |
| shard against shard | identical, 18321 accesses | identical, 18404 accesses |
| interp against interp | **not identical**, match 13690 then diverge | **identical**, 18407 accesses |
| shard against interp | match 13652 then diverge | diverges at index 0: 6-cycle lead, 18404 against 18407 |

`stereo_stats` says why the interp wobbles with shedding on: by frame 340 it sheds 22 to 23 times against
the shard's 8, renders 148 pairs against 164, and can be sampled mid-pair with `eye_cycles [x, 0]`. With
that pinned, the frame-240 fork is reproducible and belongs to the backends, and it is small and
cycle-shaped: same guest work, same addresses, same store PCs, six cycles apart, with the kernel's
interrupted context saved differently. **So the route's fidelity fails on a six-cycle phase between the
compiled shard and the interpreted path, not on a wrong instruction, and the next measurement is the
accounting diff**: log `psx_cycle_count`, `g_psx_cyc_batch`, `g_psx_cyc_bb_defer` and
`psx_get_cycle_count()` per guest instruction across frames 239 to 240 in both backends with shedding
pinned, and diff the two streams. The first divergence in that sequence is the charge site. The
publication asymmetry in `psx_cyc.h` (the shard build compiles `psx_cyc_bb_defer_begin()` to a no-op while
compiled game code defers per block and the interp charges per instruction) stays the leading candidate,
now with a reference that can be trusted.

**First bracket from the accounting diff.** Recording frames 239 and 240 in both backends with
shedding pinned: at the last recorded access of frame 239 the shard is 14 cycles *behind* the interp
(24327394526 against 24327394540) and at the first access of frame 240 it is 6 cycles *ahead*
(24327394594 against 24327394588), so the relative gain across that window is 20 cycles and the window
is bracketed by pc `0x80010EE0` (shard-executed game code) and the vblank ISR entry at pc `0x27D4`.
The ordered streams also differ in *delivery order* already inside frame 239: identical for 4047
accesses, then one backend is still in game code while the other is saving the interrupted context.
That is why the per-frame fingerprint columns can match while the ordered stream does not, and it
reprices the earlier reading: the columns are per-frame sets, the recorder is a sequence, so the fork's
first *countable* effect (frame 240, three extra accesses) is later than its first *observable* effect.
Next: arm `PSX_CYC_WATCH` region anchors across that window (`PSX_CYC_WATCH="0xA-0xB"`,
`PSX_CYC_WATCH_N`) in both backends and compare the charged deltas to name the block, then the
instruction.

**That ran, and it killed the charge-model candidate.** `PSX_CYC_WATCH="0x6C714-0x64550"` (the
traversal entry to the world pass, inside the eye replay), 200 traversals, both backends: the charged
deltas are **identical element for element** (first twelve 567105, 551918, 572089, 551963, 577809,
558218 and so on, same min and max). So the compiled shard and the interp charge the same cycles for
the same overlay code, and the per-block-versus-per-instruction hypothesis does not explain the offset.
The offset therefore sits around the frame boundary and interrupt delivery, which is also where the
recorder shows its order divergence (frame 239, after 4047 identical accesses). That fits the two
architectures as they are written: the dirty interp checks interrupts at its gates and "every 4096
insns internally" (`dirty_ram_interp.c:2610`), while compiled code checks at block edges (the emitted
`psx_check_interrupts` sites, for example `generated/OpenBIOS_dispatch.c:9073`), so a pending vblank can
be delivered at a different guest instruction in the two backends and the kernel's save frame then
records a different interrupted context.

**Next measurement, narrowed.** Compare the cycle at which the vblank ISR is entered, per frame, in both
backends: `PSX_CYC_WATCH="0x27BC"` as a single anchor (the interp stamps block leaders too, see the
comment at `dirty_ram_interp.c:86`), `PSX_CYC_WATCH_N` high enough to cover several frames, then read
the per-frame entry cycles. A constant few-cycle difference there is the interrupt-delivery granularity
and nothing else, and it decides whether the fix is in the emitted check sites or an accepted phase.

**And the offset is a pure guest-clock phase, proven by the timer reads.** The recorder's `mmior` stream
(device *reads*, which no fingerprint column covers) shows the first real difference at read 11 of frame
240: `0x1F801120`, Timer 2's counter, at the same pc `0x80023F38`, shard `0x00000885` against interp
`0x0000087F`, exactly **6** apart. Every later Timer 2 read differs by exactly 6 as well
(`0x00000D00`/`0x00000CFA`, `0x00000D70`/`0x00000D6A`, `0x00000E34`/`0x00000E2E`), while everything that
is not clock-derived is identical: `0x1F801070` I_STAT, `0x1F801074` I_MASK, `0x1F801044` and
`0x1F80104A` SIO, `0x1F801814` GPU status, all at the same pcs and the same values. So the two backends
execute the same code, see the same device state, and charge the same cycles per span, and the entire
difference is that the guest's own clock reads 6 higher at the same instruction in the shard. That is the
whole fork: this title's kernel reads a timer, its interrupted context carries the difference, and the
frame's access count follows.

The origin is still open but tightly bounded: the rate is identical (the traversal span above), the
absolute cycle at every frame boundary is identical, and the offset appears as a 20-cycle swing across
one frame boundary (14 behind at the last access of frame 239, 6 ahead at the first of frame 240). With
the charge rate equal, the offset has to be created by where the runtime's own frame flip and pacing
land relative to guest execution rather than by any per-instruction charge. The idle-loop cycle skip is
not it by default: it is opt-in (`psx_cycles.c:569`, `PSX_IDLE_SKIP=1` or the `idle_skip` command), so a
default run does not have it. Next measurement: watch the guest cycle around the flip itself,
`PSX_CYC_WATCH="0x27BC"` single-anchor at the vblank ISR entry plus the `frame_fingerprint` `cyc` for the
same frames, and read whether the runtime advances the guest clock by a different amount in the two
backends before the guest resumes.

**The flip probe found the onset, and it is delivery lag, not extra charge.** `PSX_CYC_WATCH="0x27BC"`
(the exception entry, which the interp also observes, `dirty_ram_interp.c:3281`) with 282 hits, both
backends: the vblank entry cycles are **identical for hits 0 through 261**, every vblank from frame 2 to
frame 238, with the frame boundaries on the same absolute 564,480-cycle grid in both runs. The first
difference is hit 262: the interval from hit 261 is **96,217 cycles in the shard against 94,092 in the
interp**, and the very next interval reverses it, 223,652 against 225,767. The two cancel, and the net
over hits 258 to 270 is **minus 4 cycles**. So the shard takes the same vblank *event* up to about 3,000
cycles later than the interp in some frames (another lag event of 3,062 shows at hit 269), the interval
difference is that lag and not charge, and the pairs of intervals net out. The residual tens of cycles
(plus 10, minus 6, plus 6, minus 8) are the sub-frame remnant, which is what the timer reads expose as 6.

**Where it shows, and where it does not.** In the onset frame's store stream the first divergence is index
4047: both backends are identical through pc `0x8003DD54` at cycle 24,326,924,151, then the interp's next
store is the exception save at pc `0x27D4` (cycle 24,326,924,216) while the shard runs on through
`0x8003DDAC`, `0x8003DDB0`, `0x8003DD10`, writing a display-list walk, and takes the interrupt later. That
code is **static**, not an overlay: no `.ranges` manifest in the cache lists `0x8003DDxx` or `0x8003E5xx`,
so those pcs are exe code compiled in both backends. They are where the *delivery* lands, which is exactly
what a delivery lag moves, and not where anything is charged.

**The fidelity position that leaves.** The two backends' guest clocks are identical at every vblank entry
for the first 238 frames and the per-frame totals net out to a handful of cycles; the difference is that a
shard frame can run up to about 3,000 cycles (0.09 ms) past a pending vblank before the check delivers it,
so the interrupt lands at a different instruction in that frame and the kernel saves a slightly different
context. That is a sub-frame delivery-timing difference, not a diverging guest state or a diverging rate.

**The check-granularity explanation does not survive the code, either.** The emitter places an interrupt
check at *every block leader*, with the comment "Include an interrupt check so tight loops (backward
branches) can service vblank and other hardware interrupts" (`full_function_emitter.cpp:925`), and the
overlay shim flushes its pending cycle batch *before* forwarding (`overlay_dispatch_preamble.c.inc:48`
and `:52`), so a shard never holds a stale clock at a check. The checks are also dense: one shard fragment
carries 19 in 1676 lines, the same order as the static generated code. So the lag does not come from the
shard's code missing a check or from a deferred batch being invisible at the check.

**Next measurement, and it moves to the scheduler.** The lag has to come from the runtime's own event
scheduler, since the per-block checks are there and prompt: the guest is advanced in chunks and a device
event is only noticed at a chunk boundary (`psx_cycles.c` around lines 318 to 370, "no chunk ever skips
OVER a device event boundary"), and the compiled path's chunked advancement is a different caller from the
interpreted path's. Instrument that boundary for one lag frame: record `psx_advance_cycles` chunk sizes and
the device deadline across the frame that shows the 2,125-cycle lag, and read which side notices the event
late. That is a runtime-side measurement, no guest code involved.

**The event ring then named the site, and it is neither the flip nor the pacing.** The always-on ring exists
for exactly this (its header: "diff two runs of the SAME route (native-overlay OFF vs ON) and find the FIRST
interrupt/DMA ordering divergence"). Diffing with a timing-only key (kind, detail, cycle, aux, i_stat,
i_mask; `pc` and `func` are descriptive and `mode` is expected to differ): the two backends are
**bit-identical for 12,974 events**, every interrupt, DMA and device event in the session up to cycle
24,326,919,142, the divergence frame. The first real difference is the CD interrupt. Both backends see the
`CD_IRQ` dequeue at 24,326,919,142 and the I_STAT raise (bit 2) at 24,326,924,142, and then the shard
**delivers it at 24,326,926,292, resuming at `0x8003E548`, while the interp delivers at 24,326,924,167,
resuming at `0x8003DD74`**. Every later event in the ring carries the plus 2,125.

**The site is an interrupt check that does not happen.** At that moment the shard's guest is inside overlay
code (physical `0x3DDxx`, `mode=NATIVE_OVERLAY` against `INTERP`) and runs a straight-line stretch of about
2,064 bytes, roughly 500 instructions and 2,125 cycles, before the next emitted check, so the pending CD
interrupt waits for the block to end; the interp charges one instruction at a time and delivers promptly.
`PSX_EVENT_STEP_CONSERVATIVE=1`, the framework's own diagnostic for per-block event ordering
(`psx_cycles.c:193`), does **not** change it: its ring is identical to the default shard's for all 13,932
entries, because splitting an advance into single cycles does not put a check inside the block.

**What the route's fidelity delta therefore is.** One narrow thing: a long straight-line shard block with no
interrupt check delays a device IRQ by up to the block's length, here 2,125 cycles or 0.063 ms, and the guest
resumes at a different pc. Everything downstream follows from that one resume pc: the kernel saves a
different interrupted context, the timer reads differ by 6, and the frame-240 access count changes. The
emitter already checks at every block leader "so tight loops (backward branches) can service vblank"
(`full_function_emitter.cpp:925`), and the gap is a block long enough to hold an event.

**Fix to try, and its test.** A device-deadline or instruction-budget check inside long blocks, so a pending
IRQ does not wait for the block to end. The test is the event ring: the first divergence must move past this
frame, and the frame-239 store-stream split (index 4047) must disappear, before the route can be turned on.

**The delivery latency is now quantified, and so is the price of fixing it.** Pairing each `ISTAT_RAISE` with
the `IRQ_DELIVER` that follows it across roughly 250 frames: the interp delivers in 4 cycles at the median, 35
at p90, worst 135, and never above 256. The shard is identical at the median (5) and p90 (38) and separates
only in the tail: **4 deliveries of 294 above 1,024 cycles, worst 3,059** (about 0.09 ms).
`PSX_EVENT_STEP_CONSERVATIVE` changes nothing (same distribution, 290 deliveries). Neither ring contains a
single `IRQ_GATE`, so the interrupt is never blocked, only unchecked. The route's fidelity delta is therefore
1.4 per cent of interrupts, bounded by the longest straight-line shard block.

**That also fixes the shape of the fix.** Per-instruction checks would make the shard bit-exact and destroy
the win, since they are exactly what makes the interp slow. A check *budget* inside long blocks (128 guest
cycles, say, which is below the interp's own worst case of 135) bounds the tail at the price of one forwarded
check per budget spent. Accepting it leaves a sub-frame delivery latency in overlay rendering code. The budget
is the one to price: emit a check when a block's running cycle count crosses the budget in the emitter,
rebuild the recompiler, recompile the shards, then read the ring tail (target: worst case at or below the
interp's 135) and the A/B pair times for the cost. Nothing in that path touches static generated code, so the
baseline stays comparable.

**The budget was implemented, and its real cost turned out to be the cache tag, not the check.** The emitter
change is small and env-gated: with `PSX_IRQ_CHECK_BUDGET=<guest cycles>` set at generate time, a check is
also emitted inside a block once the accumulated cost since the last such check crosses the budget, at an
instruction boundary that is not a delay slot; default 0 emits byte-identical code, and the recompiler's own
tests pass (10 passed). It is preserved on framework branch `vr-irq-check-budget` (commit `89dade95`).

Three costs came out of trying to measure with it, and they matter more than the check itself.

1. Any emitter change moves the codegen source hash, so the overlay cache tag moves with it (`1b6950cb` to
   `25935d6f` here). Every existing shard is stale by construction, so the route needs a **full re-capture**
   under the new tag, not just a recompile. The capture file in the tree is a subset: compiling it built 21
   shards where the shipped set is 67, and the full set came from a longer session.
2. Bumping the tag also broke the session: with the new tag the runtime no longer loads slot 1 (the
   harness's probe load returns `last_ok: 0` with no shards present at all, so it is not shard-related).
   The savestate is bound to the build the shards belong to, which makes an emitter change the opposite of a
   cheap experiment on this title.
3. Building refreshes the framework's `overlay_toolchain/` mirror (53 modified files plus 49 new ones here),
   so a clean-tree claim needs `git -C psxrecomp checkout -- overlay_toolchain` and
   `git clean -fdq overlay_toolchain` after any build.

The workspace is restored: framework back on `59c3fb3d` with the change safe on its branch, tag back to
`1b6950cb` with its 67 shards, and a verification run reaches frame 186 with the ring populated.

**Recommendation, therefore: take the tail, do not pay for the budget.** The measured tail is 4 deliveries
of 294 above 1,024 cycles, worst 3,059 (0.09 ms), with no gates anywhere and the guest continuing correctly
from a different pc; the price of removing it is a full re-capture plus a fresh savestate on top of a
forwarded check per budget spent, which is a large fraction of what the route buys. The budget stays on its
branch for whenever a re-capture is cheap anyway. Next step is the campaign's own acceptance for the route
as it stands: pixel compare at a fixed guest cycle against the current baseline, then the headset look.

**The acceptance's correctness half ran, and what it finds is ordering-level, not state-level.** Four
fingerprint windows with the pair policy pinned and the load seeded at boot, compared by
`vr/compare_frame_fingerprints.py` over 300 frames from the load frame: the same-backend control pair is
byte-identical on **both** sides (interp equal, shard equal), so the cross-backend reading is readable.
Against that reference, the shard is byte-identical to the interp for the **first 238 frames**, and from
relative frame 238 (guest frame 240, where the late IRQ lands) **62 of 300 frames** differ. The differing
columns are `ws` and `wr` in all 62, `wc` in 58, `pc` in 61, and `sp`/`sc` in 24 each. **`mmio`, `mc`, `qc`
and `cyc` never differ**, so device writes and the cycle timeline stay equal throughout, and the magnitudes
are one guest write per frame (`wc` 3,058,456 against 3,058,455) plus at most a dozen scratchpad accesses
(`sc` 3,243,271 against 3,243,283). That is the delivered-late-interrupt signature and nothing else: the same
writes happen, one of them belongs to a different frame, and no device state or timing column moves.

Read strictly, the campaign's bar for this route ("byte identity of the pair is the right bar") is met for
238 frames after the load and not afterwards, exactly where the delivery lag begins. The remaining item is
the visual tiebreak, the accepted look on the headset (no missing tiles, no head-turn pop-in, no near-player
floor gaps), which the fingerprint columns cannot speak to.

**The visual half passed headlessly, with byte-identical eyes at matched guest cycles.** The mirror harness
(`vr/run_vr.ps1 -Headless -HeadlessMirrorCheck -Verify -Slot 0`, shedding pinned) was run twice, once with the
shard directory moved aside and once with it present, and both runs are `passed: true`: advancing stereo
projection submissions, application focus, nonblank compositor images, and zero `verify_mismatch`,
`vram_leaks`, `nesting_repairs` and `watchdog` in `render_pass_stats.json`. Both runs captured the **same two
guest pairs**, `p000033` at guest cycle 4,140,319,241 and `p000034` at 4,141,453,436, with identical metadata
(pair id, 2560x1200, the same view offsets). All six PNGs, left, right and the composed SBS for both pairs,
are **SHA256-identical** between the two runs. Their 96-frame fingerprint receipts differ in one frame (a
one-write boundary shift, `wc` off by one at frame 11), the same ordering-level signature as the 300-frame
window.

The two runs demonstrably executed differently, so this is not a comparison of a run against itself:
`stereo_stats` reports the route run at `avg_pair_ms` 19.107 against 20.763 and `checkpoint_ram_bytes`
111,149,056 against 115,343,360. (Both are far above the 11.111 ms budget because mirror mode adds the dumps,
so that is a relative signal, not the campaign's benchmark.)

So the correctness bar is met at pixel level at a fixed guest cycle: the route changes which backend executes
code, and the rendered stereo output at matched cycles is byte-for-byte the same. The residual is the
write-attribution shift, which changes no pixel in the captured pairs. The accepted look on the headset
remains, which is the owner's run.

**A scripted look sweep shows the route renders identical still frames and diverges under camera motion.**
The headset report was geometry shifting and stretching while moving, plus empty tiles looking down and
near-plane floor gaps. Both are view and motion dependent, so the look was scripted instead of relying on a
headset: launch headless on the same save with the load seeded at boot, drive the look axes with
`openxr_input_override`, and take one screenshot per pose, twice, once with the shards present and once with
them moved aside (`analysis/vr-proof/vr_sweep.py`, receipts in `sweep_route/` and `sweep_baseline/`).

Nine poses, same script both times. Five are **byte-identical** between the two backends: `level`,
`bob-down-a`, `bob-down-b`, `look-down`, `look-up`. The four that differ are the ones with **camera motion**:
`turn-left` (7,752 pixels differing by more than 24), `turn-right` (12,191), and `level-again` (3,271, the
turn residue after release). In every differing case the difference is spread across the whole frame rather
than localized, which is the signature of an edge-wise view offset, not of a deformed mesh.

So the route renders the same still frame and a different moving one. That is a real, reproducible,
route-specific motion difference, and it corrects the method: "byte identity of the pair" was checked on
pairs captured in a scene at rest, and a still-pair check cannot see this at all. The bar has to include
pairs captured while the camera moves.

Harness facts worth keeping: `openxr_input_override`'s `ry` axis does nothing here (five pitch cases share
one image and the pad stayed centred) while `rx` works, and the game's slot 1 save is
`state_8001DFD4_slot01.pst`, the save with the roof deformation.

**Next measurement.** Dump the camera/player state per pose in both runs and diff the numbers. If the view
angles differ under identical scripted input and identical frame counts, the route is changing the camera
math, and the camera code is overlay code, which is exactly what the shards replace.

**And the guest-side view matrices say where it is not: not the camera, but movement.** The same sweep was
re-run with the level producer's GTE entries recorded per pose (`ra` `0x8008BF84`, the numbers
`check_movement.snapshot` reads: `RT`, `TR`, `H`). In every case without forward movement the two backends'
view matrices are **identical to the last digit**: `level`, `bob-down-a`, `bob-down-b`, `look-down`,
`look-up`, `turn-left`, `turn-right` all agree (`RT` `[288,-2,-8242,-52,5140,-2,8208,82,288]` for `level`, and
`[2316,...]`/`[5972,...]` for the turns). The divergence appears only where the player **moves forward**:
`bob-mix` (`lx` 707, `ly` 707) and the `level-again` that follows it differ in `RT`, `TR` and `H` outright
(`H` `-1230` against `2381`, then `3822` against `6854`). So the route agrees with the baseline on the
camera and disagrees on movement, which puts the difference in guest movement or physics code, and movement
code is overlay code, exactly what the shards replace.

Two method notes, one of them a correction of my own reading. First, the screenshots in the earlier sweep
are not a trustworthy instrument on their own: `screenshot_file` returns the last presented frame, and the
two backends' presentation timing relative to the guest differs, so in animated cases the images can differ
from capture timing alone. The view matrices above are guest-side and immune to that, which is why they are
the reading. Second, the camera struct hash differs in every case and means nothing here: the 256 bytes
follow a pointer that includes volatile fields, and it is not a view signal.

**Next measurement.** Sample the view-matrix series across a scripted forward move in both backends rather
than one value per pose, and read where the series first parts. Then attribute that guest pc to a shard or to
the exe through the `.ranges` manifests, and check it against the host-replaced helper list, since a
host-replaced fixed-point helper is the shape of bug that deforms geometry.

**A head bob, scripted, and what it reports.** The pose the renderer uses comes straight from
`xrLocateViews`, so a headless run cannot move the head at all: the null HMD reports a fixed pose, and
`openxr_input_override`'s axes drive the game camera rather than the head. That is why the reported motion
defects had no headless test. The missing knob is now in the framework next to the other test affordances,
debug-gated and translation only (`openxr_pose_override`, framework `2ed12640`): it offsets both eye
positions after the origin is taken, and it is runtime-side, so the codegen hash and the cache tag are
unchanged and no re-capture is needed. `vr/headbob.py` traces two full circles, the frontal one
right, down, left, up `[60,0,0] [55,-23,0] ...` and the sagittal one forward, down, back, up
`[0,0,-60] ...`, capturing each step, and it acks every offset it sets.

Both backends ran the same requested circle (`vr/headbob.py bob2_route` against `bob2_baseline`). The guest
view matrices stay `H=400` throughout, which is correct: the bob is render-space, so the guest camera does
not move. And the **per-frame render traces are identical at every one of the 32 steps**, including the pose
age stage that the header describes as "the pose the compositor reprojects from". So the route does not
change the pose or reprojection timeline at all, which rules out reprojection as the explanation for
"geometry shifts and stretches when moving" and leaves the divergence where the look sweep put it: guest
movement code.

Method note: `vr_sweep.py`'s screenshots use `screenshot_file`, which is the flat guest view and is
byte-identical under head translation, so head-motion checks have to read the trace or the stereo pair
path, not that screenshot.

**The route is not active in a savestate-seeded headless run, so today's A/Bs compared the
interpreter against itself.** `overlay_loader_status` reports, from frame 1 and for as long as it is
asked, `registered: 0`, `regions_checked: 1`, `file_found: 0`, `loads: 0`, `dispatch_native: 0`, with
`dispatch_interp_fallback` climbing in step with the guest. Slot 1, slot 0, `PSX_LOAD_SLOT` seeding
before frame 1, and a plain boot with no savestate at all all agree. The plain boot is the informative
one: `regions_checked` rises to 3 during the level load, so the game does load overlays, and
`file_found: 0` for every one of them, so the loader finds no shard for any region the game actually
loads even though 67 shards are present. The consequence is that every comparison run today, the head
bob circles, the look sweep and the mirror pairs included, was executed by the interpreter on both
sides. The head bob's "traces identical at all 32 steps" and the look sweep's matrix readings are
retracted as evidence about the route; they are evidence about the interpreter only.

Two numbers point at the cause. `build-release/overlay_captures.json` holds a single capture while the
full set sits in `analysis/vr-proof/overlay_captures.full.json`, and `file_found: 0` for every region
the game loads. Restoring the full index, recompiling the shards from it, and then gating every harness
on `overlay_loader_status.registered > 0` before sampling is the prerequisite for any further A/B, and
the gate belongs in the harnesses rather than in a note.

Method corrections found the same way: `screenshot_file` returns the flat guest view and carries no
head pose, so the head bob it does not show is expected; and `openxr_input_override`'s `ry` axis is
unwired, which means every vertical look pose in the sweep, `look-down` and `look-up` and the two
`bob-down` cases, reproduced the level pose exactly, byte for byte. A vertical look cannot be driven at
all today. The black near-player tiles and the bright untextured patch are present in a frame captured
with the route off, so they are baseline renderer behaviour and not a shard regression.

**The cause of that, and the fix.** `overlay_cache = true` in `game.toml`, and the loader records a
capture when the game DMA-loads an overlay, but it writes the JSON only on an explicit
`overlay_capture_dump` or a clean window close. Compares with the killed runs then: everything the
loader saw was thrown away, and the index next to the exe had been left holding a single capture. So the
loader checked regions, found a shard for none of them, and registered nothing. Recapturing on the
seeded level with the route off, dumping every block (21 captures), merging that with the full set kept
aside (16), and recompiling gives **85 shards where there were 67**, and registration now works from a
savestate-seeded headless run: `registered=41`, `loads=14`, `file_found=1`, `dispatch_native` climbing to
418,187 by frame 1267 and `dispatch_interp_fallback` to 145,343, against 0 and 0 before. The gate that
would have caught this is now in all three harnesses: each refuses to run when
`overlay_loader_status.registered` is zero.

For the next capture: roaming the seeded level produced no new regions (`regions_checked` stayed at 1),
because the level's overlays are already resident in the savestate, so a capture taken from a savestate
only sees what a boot loads. Regions loaded before the savestate have to come from a merge, or from a
capture taken at a load boundary.

**Why no shard was found: the mod flags change the guest code.** Registration is keyed by region and by
content. The same seed run without `-WeaponPoseDiagnostic -WristHud -WristHudReplace` registers 41 shards
and dispatches natively (`dispatch_native` 24,522 by frame 352 and climbing); with them the loader reports
`registered` 0, `file_found` 0, `loads` 0, and the same single checked region `0x0003A000`, for as long as
it is asked. `-WristHudReplace` replaces guest HUD code, so that region no longer matches the captured
content and no shard can match it. Every harness in this campaign passes those flags, which is the second
reason their A/Bs measured the interpreter, the first being the single-capture index. For the route to be
live in the runs the harnesses actually make, a capture taken with the same flags has to be merged and
recompiled; the alternative is to drop the flags from measurement runs. The harness gate now makes that
choice explicit rather than silent, and takes a baseline mode for runs with the shards deliberately moved
aside.

**The capture is the same with those flags, so the mismatch is not just the capture.** A recapture with
`-WeaponPoseDiagnostic -WristHud -WristHudReplace` present produced the same 21 entries and the same single
checked region, and the loader still reported `file_found` 0. So the flag variant does not add a matching
capture for the region the loader checks. Either that region is not among the 21, or the live content
differs from the captured content in a way the recapture does not change. That is the next thing to
determine, and it is cheap now: run the loader check with and without the flags and print `checked`
alongside `file_found` in both, rather than only the count.

**The right way to look at VR: the eye pairs.** The flat guest view is not the VR path and must not be
used for VR checks. The launcher forwards arguments to `vr/run_vr.ps1`, so a headset run with
`-Verify -CaptureDirectory <dir> -Slot 1` records the stereo eye pairs (`p######_left.png`, `_right.png`,
`_sbs.png`) plus `presented.png` into that directory. Two traps cost real time there: `-CaptureDirectory`
must be relative to the repo root, because `run_vr.ps1:137` joins it with the VR root and an absolute path
throws so the launcher exits; and `capture_stereo.py` defaults to `--slot 3`, which does not exist, so
`-Slot 1` is required. That combination produced the pair used below, on save state 1, in the user's own
headset run, and it is the only capture method that shows what the headset shows.

**The floor wedges are produced by the stereo offset, not by dropped geometry.** Measured on that captured
pair (`analysis/vr-proof/headset-capture/p000011_left.png` and `_right.png`, 2560x1200): black coverage in
the bottom-left quadrant is 64.5% in the left eye against 67.8% in the right, bottom-centre 45.1% against
41.0%, and bottom-right 29.3% against 28.9%. The eye separation measured off the floor texture is 20, 22
and 28 pixels. A polygon clipped at the near plane would land identically in both eyes; these move with the
eye offset, so they are made when the pair is composed. The composition site is the renderer's stereo path:
`StereoPair s_stereo_pair[2]` at `gpu_gl_renderer.c:7901`, `gl_renderer_stereo_begin` at 8656, and the pair
branch of the present at 8471, with the crop correction at 8456 explicitly skipped for the stereo pair. The
fix is to fill what the offset vacates instead of leaving it black, or to render each eye from its own
camera. The VR stereo is a declarative plugin (`mods/preloaded/packages/moh.vr.stereo/0.1.0/manifest.toml`,
plugin id `moh.vr.stereo`) carrying no source path, so its code is compiled into the runtime and the
composition is the renderer code above.

**Correction to the above, from reading the code: the eyes are replayed guest renders, not copies with a
2D offset, and the ±24 is a view offset.** Each eye gets its own FBO (`StereoPair::fbo[eye]`, `tw[eye]`,
`th[eye]`), and `render_pass.c` drives them: `stereo_eye_draw` at 1104, the loop that renders both eyes at
1200 to 1230, and `gl_renderer_stereo_publish(s_stereo.attempts, cycle, s_stereo.eye_view)` at 1226. The
per-eye view offset is sampled from the projection ambient at `render_pass.c:969`
(`gte_render_view_get(s_stereo.eye_view[eye])`), and the unit test pins its value:
`test_render_pass_sandbox.c:771` asserts `eye_view[0][0] == -24` and `eye_view[1][0] == 24`. So each eye is
the guest's own render replayed with the view translated ±24 guest units laterally.

That means a per-eye difference in a clipped region is expected after all: the two eyes are offset in
opposite directions, so the near geometry at the player's feet falls behind the projection origin in one eye
before the other. The evidence I measured still stands (the wedges move with the eye offset, 64.5% against
67.8% bottom-left, 20 to 28 px separation), but it is consistent with near geometry being clipped by the
guest's own projection under the offset, not only with a composition fill. This is why `-24` and `+24` are the
numbers to work from, and why the fix has to be decided at the eye draw: either clip the near geometry in the
eye replay instead of letting the guest's 2D clip flags drop the polygon, or stop the offset from pushing
geometry behind the projection origin.

**The launcher's wrist flags and the shard route are mutually exclusive, measured.** Recapturing the overlays
in the launcher's own configuration (`vr\recapture_overlays.py 1 14 mods`, which passes the wrist flags)
reports `regions 1`, `checked None`, `found 0` on every one of the 21 entries: with `-WristHudReplace` the
guest never presents the overlay code that a shard is keyed to, so no capture exists to key against and no
shard can ever register. That is why the live headset run sits at `registered 0` with 385,910 interpreter
fallbacks, and it is not fixable by recapturing: it is the flag set, not the capture. The pacing win and the
replaced wrist HUD cannot both be had while shards are keyed by content.

**So the distortion has a known fix, and it is a choice.** The campaign's own A/B, taken while the route was
live, measured `avg_pair_ms` 10.88 to 11.13 without the route and 9.52 to 9.59 with it, against an 11.111 ms
budget for 90 Hz. Dropping `-WristHudReplace` (and `-WristHud`) for runs where pacing matters puts the route
live and the pair under budget, which is what removes the reprojection warp. Keeping the wrist HUD keeps the
interpreter pace and the warp.

**The live eye offset in a null run is 8 units, not 24.** `stereo_stats.json` reports `eye_view`
`[[8,0,0],[-8,0,0]]` in the headless capture, because the null HMD reports a small IPD; the game's own
`g_ipd_mm` of 67 with `g_units_per_meter` 716.4 gives the 24 seen in the sandbox fixture and on real
hardware. The headless pair cost is also startup dominated, `avg_pair_ms` 19.27 over 7 pairs with `shed 49`,
because a `-Verify` run is fixed at a 96 frame receipt, so headless pair timing is not a steady state
measurement. Use the live headset run for pacing numbers and the headless capture for image evidence.

**Where this stands, after the headless hold and the pitch override landed.** What is landed and working:
the SteamVR settings repair (the forced null driver was why the headset was never driven), the eye
projection reverted to the headset's so both eyes are geometrically correct, `vr/headless_hold.bat` as a
stable way to hold a null-HMD session for probing and `stereo_dump` captures, the gated `pitch_deg` on
`openxr_pose_override` (framework `vr-head-pitch` at `a3714fad`, pinned as `3f04d6f`), and a real pacing
measurement on the real headset of 6.888 ms per pair with zero shed against an 11.111 ms budget.

Open, in order of value. One, the black wedge over the floor near the player. It is visible looking
straight ahead, in both eyes, on slot 1, and it is not pacing, not the per-eye tree cull, and not the eye
projection, all three of which were tested and cleared. The standing explanation is that the eye renders
more frustum than the guest frame draws, so the candidate is widening the guest render rather than
replacing the eye projection, and the `PSX_VR_AUTHORED_FOCAL=0` lever was only ever tested against a stale
binary. Two, the master A/B: the worktree at `.master-ab` is a full master checkout with the slots copied
in, and its build is the one remaining step. Three, the interrupt-check budget stays parked, because
`boot_state.c:976` rejects any savestate whose header hash differs from the build, so landing it costs the
existing slots.

Retractions to carry forward, because earlier entries in this file say otherwise. Every pacing figure
taken before the settings repair (11.165 ms, 7.15 ms) came from the null HMD and is void. The eye
projection change recorded as a confirmed fix made both eyes wrong and is reverted. The per-eye cull is
not the cause. The pitch override was built to reach a defect thought to be pitch-gated, which it is not:
the wedge is visible straight ahead, and `p000926` only gave a cleaner view of it.

**The margin is set by one number, and it was measured by sweeping it.** The eye projection override scales
the projected focal lengths by `(fov-scaled guest H) / projection_h_ref`, so `projection_h_ref` decides how
much of the eye surface the guest render covers. It was a hardcoded 400; it is now `PSX_VR_AUTHORED_H_REF`,
default 400, so the pinned behaviour is unchanged and the value can be swept. Measured with `rows_diff.py` on
straight-ahead pairs from a held headless session on slot 1, bands in percent black, left eye:
`margin-a/margin-b/bottom` are 29.81/42.67/92.65 at ref 400, 5.11/16.84/21.94 at 256, and 3.25/1.46/0.69 at
160, with the eyes within 0.8 points of each other everywhere except the bottom band at 400. Lower ref spreads
the guest render wider across the eye surface and the undrawn margin collapses toward nothing, so the pinned
400 is an under-cover value, not a neutral one. The earlier claim that this lever was dead came from the
harness: `vr/run_vr.ps1:165` hardcodes `PSX_VR_AUTHORED_FOCAL = '1'` into the process environment at line 221,
so a shell export of it never reached the game and the two runs could only be byte-identical. Sweep frames are
in `analysis/vr-proof/href-sweep`. Settled, and it kills the lever: the user confirmed on the headset that a
lower ref zooms the image in, which is what the numbers already said once read properly, since coverage rose
while the scene grew. It hides the shard by cropping it out of view and draws nothing, so the code is back to
the pinned constant and the knob is removed. What the sweep does establish is that the shard is not a
projection scale error: at the correct scale (ref 400) the lower bands are 30, 43 and 93 percent black and no
scale correction fills them without magnifying.

**The shard is clipped triangles, not a margin.** The presented frame from a held headless run on slot 1
(`analysis/vr-proof/upside-check/presented.png`, 1856x1392) is upright, so the submission flip is intact, and
it shows the defect as two large black wedges with straight diagonal edges in the lower half of each eye, over
the floor and beside the player. Straight edges are the signature of geometry clipped at the guest's own
screen boundary: the game clips polygons against its 512x240 rect, so the parts that fall outside it are never
drawn, and the eye's wider frustum sees that boundary as a diagonal. That is why the user describes it as
unrendered rather than black, why it moves with the eye offset, and why magnifying hides it: magnifying stops
the eye from seeing past the guest frame. The fix has to make the guest draw past that boundary, by widening
the frame the eye replay is rendered into, rather than changing any focal.

**RESOLVED: the world hung upside down because the origin pose and the live pose disagreed by a half turn.**
The user reported the world inverted while the HUD and the weapon were correct and tracking was fine, on
SteamVR, and that it had been right before. The eye view the world is drawn with is `conj(origin) * pose`, and
that matrix was measured live over the debug surface at session start as
`rotation_q12 = [-4073,110,415, -112,-4094,-13, 415,-24,4075]`, a diagonal of approximately (-1,-1,+1): a 180
degree roll, not a head orientation. The world render obeys it, so the world hangs upside down. The 2D HUD
never passes through that matrix and the hand and head deltas stay correct, which is exactly why the HUD and
the weapon looked right and only the world did not. `openxr_control recenter=1`, which re-captures the origin,
changed the same matrix to `[4056,-277,499, 275,4087,29, -500,5,4065]`, near identity, and the captured mirror
went from ceiling-lights-at-the-bottom to an upright corridor, ceiling and light at the top, floor at the
bottom, both HUD elements upright, in the same run seconds apart. Evidence: `analysis/vr-proof/mirror-now.bmp`
(inverted) against `analysis/vr-proof/mirror-fixed.bmp` (upright), both taken through
`psxrecomp/tools/window_capture.py`, with the eye views before and after from `openxr_views`.

Why no headless run ever showed it: the null HMD reports an identity pose, so the origin and every later pose
agree by construction and the half turn cannot arise. Every headless pair in this repo is therefore blind to
it, which is why this survived a long stretch of capture work, and it is the same trap as the symmetric null
FOV: a null device cannot exercise a defect that needs a real pose.

The fix is in the runtime where the origin is captured (`psxrecomp/runtime/src/psx_openxr.c`, the locate path
before the `if(!s_origin_valid)` capture): if the head's up axis in origin space comes back pointing down, the
stored origin is not a head orientation, so it is dropped and the current pose becomes the reference. A real
turn keeps the up axis up and is untouched, and a ten frame counter means a one frame glitch cannot reset the
origin. The keyword `recenter` remains the manual escape hatch when a session is already inverted.

Still open after this: the black wedge over the floor beside the player, which is unchanged by the fix and is
visible in `mirror-fixed.bmp`; the master A/B build; and the parked interrupt-check budget.

**The black wedge is not the eye offset, and the numbers now say so.** `PSX_VR_EYE_SCALE=0.0` collapses the
pair onto one camera so the eye offset cannot contribute at all. The wedge is unchanged: margin-a / margin-b
/ bottom read 30.23 / 42.74 / 91.54 percent black against 29.81 / 42.67 / 92.65 at 1.0, and mid, floor and
near-floor agree to within 0.03 points (23.48 / 2.99 / 3.22). Same pose, slot 1, straight ahead, dumps in
`analysis/vr-proof/eye-scale/scale1.0` and `scale0.0`, measured with `rows_diff.py`. The only difference
between the runs is the bottom band's inter-eye spread, 2.22 points at 1.0 against 0.00 at 0.0, so the offset
does move near-floor content slightly and removes none of the black. The missing floor is inside the guest's
own render. Run order matters as usual: the first attempt reported "never reached the scene" because the
readiness check required a non-zero eye translation, which is precisely what collapsing the offset sets to
zero, so the probe was blind to the run it was measuring.

The shape of the black explains every earlier attempt: 23.5 percent mid, 3 percent floor, 30 percent
margin-a, 43 percent margin-b, 92 percent bottom, so the black grows toward the bottom edge. It is also why
every `PSX_VR_AUTHORED_H_REF` value only magnified the image: scaling the focal fills the surface out of the
same 240 rows of content instead of adding rows.

Two readings remain and one probe separates them. If the guest's 240 rows are mapped to the eye surface
without padding, the black is vertical field of view the guest frame does not contain. If they are stretched
to fill the surface instead, the same black is the guest's own bottom rows holding no geometry, and the
frame is also anamorphic, which would be a second defect wearing the same symptom. Render the same slot and
pose with the native presentation, that is with no eye replay, and read the guest's own bottom scanlines:
content there against black in the eye image blames the replay, black in both blames the game's own cull or
the collector in `vr_head_frustum_filter`, which is the only place the mod can widen what the game submits.
Do that before writing any fix, because the two readings want opposite changes.

**Selection is excluded too, and that is the strongest evidence yet.** `PSX_VR_HEAD_FRUSTUM` is unset by
default, so the mod's eye-aware tree collector was never in the path, and turning it on changes the image not
at all. A new test mode, `PSX_VR_HEAD_FRUSTUM=2`, forces the collector to ignore both the PVS masks and the
screen planes, so every node of the tree is submitted. The bands come back bit for bit identical to the
collector off run, 23.49 / 2.96 / 3.24 / 11.94 / 29.81 / 42.67 / 92.65 with the same inter-eye deltas, in all
three runs. Nothing in our selection, our eye offset or our projection filter is removing these polygons,
and no amount of relaxing a filter that is already passing everything can put them back.

Three independent mechanisms leaving an identical image is the signature of a region that never had content,
not of geometry being dropped. That points at the uncovered padding reading above: the eye surface is 1200
rows against a guest frame whose content reaches about 88 percent of it, so the bottom band is simply not
covered by the guest render, and the earlier authored-H-ref measurements only ever filled it by magnifying
the picture until it overran the gap, which is exactly what the user saw and rejected. The instrument stays
gated and off by default; it is how selection was excluded and it is worth keeping for the next time this is
suspected.

**The interrupt-check budget invalidates every savestate, and that is why the load failed.**
`psxrecomp/runtime/src/boot_state.c:976` refuses a load when the state header's `codegen_hash` differs from the
running build's `PSX_OVERLAY_CODEGEN_HASH`, reporting `codegen_hash=%08X(want %08X)`. The emitter change moves
that hash (the build produced `0x25935d6f`), so every state saved on the previous build is rejected and
`capture_stereo.py` aborts at the slot load with `last_ok 0`, which is what looked like a crash. This is a
guard, not a defect: any emitter change invalidates savestates. That is a second cost of landing the
interrupt-check budget, larger than staling the shard cache, because the user's own slots have to be re-saved.
The pin bump and the regenerated tree were reverted as `c8e1a3e` and `38ee058`, and the framework candidate is
parked as `e96a4fb1` on `vr-perf-pr1-pr2`. To land it: pin the framework, regenerate `generated/`, then
re-save the slots on the new build, and only then recapture the overlays.

**Reverted: the eye projection change was the wrong fix, but it proves the margin tracks the eye frustum.**
Forcing `projection` to 0 for both eyes filled the eye surface but threw away the headset projection, so both
eyes were geometrically wrong on the real headset; reverted in `5be5f13`. What it established is the
relationship: with it the lower margin measured 2.37% against 2.35 left and right (margin-a), 5.20 against
5.17 (margin-b) and 3.28 against 2.91 (bottom edge), against 29.81 against 30.61, 42.67 against 42.97 and
92.65 against 90.43 without it. The user's own reading from the headset is that these are unrendered
triangles seen through rather than black polygons, that they show when looking down, and that the look-down
case cannot be captured headlessly because `openxr_pose_override` only translates and `ry` is unwired. The
proper fix is to widen the guest render so it covers the headset frustum, not to replace the eye projection.

**The harness skips its build, so a switch experiment can run against a stale binary.** `vr/run_vr.ps1` runs
`cmake -S . -B build-release -DPSX_OPENXR=ON ...` and `cmake --build build-release --target psx-runtime`, but
several capture runs completed in seconds with no compiler output. Forcing `cmake --build build-release
--target psx-runtime` by hand recompiled `vr/psx_vr_stereo.c` and relinked, which is when the fix took
effect. Force the build before trusting any mod change, and check for the `Building C object
... psx_vr_stereo.c.obj` line.

**The scene interior is fine in both eyes, and that reframes the whole defect.** Measured with
`analysis/vr-proof/scene_diff.py` on the captured pairs: floor-near 2.85% black left against 2.84% right,
floor-mid 3.34 against 3.23, floor-far 7.36 against 7.27, wall-right 23.49 against 23.50, longest black run
81/32/51 against 81/31/39. The floor is drawn in both eyes at the same coverage. The 64.5 against 67.8
bottom-left figures that drove the earlier conclusions were dominated by the area below the guest's drawn
frame, which is constant black in every configuration and therefore cannot distinguish anything: exporting
`PSX_VR_AUTHORED_FOCAL=0`, and re-running with `-NoHeadFrustumDiagnostic` so the host tree cull is off, both
produced byte-identical pairs. Three hypotheses are now dead: the cull, the focal switch, and per-eye
clipping. What the user sees as a large missing area bottom-left is the undrawn margin of the eye surface,
where the eye is projected with a wider frustum than the guest's frame covers.

**The fix, therefore, is to stop replacing the guest projection for the eye view.** The eye keeps the pose
and the plus/minus 24 unit view translation, which is correct IPD stereo, and keeps the guest's own
projection so the drawn scene covers the whole eye surface. Concretely, in `vr/psx_vr_stereo.c` after
`psx_mod_openxr_view(eye, &g_eye_view[eye])` at 853, force `projection` to 0 for both eyes, which makes the
cull and the eye render use the guest focal (400) instead of the headset's, and leave `projection_h_ref`
irrelevant. That is a one line change and it is verifiable headlessly with the capture and `scene_diff.py`.
The harness must then rebuild the mod, since the environment table overrides anything exported in the shell.

**The wedge mechanism, located: it is the per-eye tree cull.** (Superseded: the measurement above shows this
is wrong. Disabling the host cull with `-NoHeadFrustumDiagnostic` produces a byte-identical pair, so the cull
selects the same nodes and is not what leaves the margin black. Kept only as the trail.)
`vr/psx_vr_stereo.c:447`
`vr_head_frustum_filter`, enabled by `PSX_VR_HEAD_FRUSTUM=1` which the harness sets, hooks the guest's
visibility traversal at `0x8008B3E8` and, inside each eye's replay, replaces the guest's own selection with a
host traversal: it reads the cull centre from the GTE projection registers (`gte_ctrl[24]`, `[25]`), calls
`moh_vr_collect_tree(cpu->gpr[4], queue, &count, matrix, tr, &g_eye_view[g_stereo_eye], cx, cy, masks,
bypass_masks)` and writes the resulting node queue back. A node the cull does not select is never drawn for
that eye, so the black wedges are culled nodes, not clipped or unfilled geometry, and not a pair composition
artefact. That is consistent with every measurement: the covered region depends on the eye (opposite plus and
minus 24 offsets, 64.5 against 67.8 bottom-left), the wedge edges are the cull boundary meeting the
perspective of the floor, and the near floor is what a too-tight cull loses first. The fix belongs in that
cull: a margin for the eye offset when selecting nodes. Not the pair composition, not the near plane, not the
projection focal, and not the eye separation, which is correct IPD maths at 24 units per eye for 67 mm.

**The harness forces its own environment, so shell exports are ignored.**
`analysis/vr-proof/*/launch_config.json` records what actually reached the game, and for the headless pair it
is `PSX_VR_AUTHORED_FOCAL=1`, `PSX_VR_DESKTOP_FOV=0`, `PSX_VR_HEAD_FRUSTUM=1`, `PSX_VR_IPD_MM=67`,
`PSX_VR_UNITS_PER_METER=716.417910447761`. Exporting `PSX_VR_AUTHORED_FOCAL=0` in the shell and re-running
produced a byte-identical pair, because the harness table overwrites it: that run proved nothing about the
focal switch, and any experiment has to change the harness table itself.

**The wedges reproduce headlessly, exactly, which takes the headset out of the loop.**
`vr\run_vr.ps1 -Headless -HeadlessMirrorCheck -Verify -Slot 1 -CaptureDirectory analysis/vr-proof/headless-eye
-Seconds 2` forces the SteamVR null HMD, then restores the settings and stops SteamVR. It needs SteamVR down
first, and the capture directory is relative to the repo root. It writes two verified eye pairs plus
`presented.png`, and those pairs measure identically to the headset pair: bottom-left 64.5 against 67.8,
bottom-centre 45.1 against 41.0, bottom-right 29.3 against 28.9, floor-mid-left 14.1 against 16.1, and the
same 20, 22 and 28 pixel separations. So the wedge can be changed and verified with no headset and no user in
the loop: capture, edit, re-capture, compare with `analysis/vr-proof/eye_diff.py <dir> <pair>`. The mirror
check inside the same run fails on a blank null compositor capture, which is unrelated to the eye pairs.

**Live numbers, headset run, save state 1.** `avg_pair_ms` 11.165 against an 11.111 ms budget for 90 Hz,
`shed` 133 of 606 pairs, 133 empty frames, `openxr` running with `ipd_m` 0.063 and `units_per_meter` 238.8.
The frame misses its deadline, the compositor reprojects, and that warp is the shift and stretch on head
motion. The same run reports `registered` 0, `loads` 0, `dispatch_native` 0, `file_found` 0 with
`dispatch_interp_fallback` 385,910, so the compiled shards are not active in the launcher's configuration
and the pacing win is not in play.

So the route's win is measured and its fidelity verdict is in, and it is narrower than any earlier reading of
it: the two backends' guest event timelines are **bit-identical for 12,974 events** (every interrupt, DMA and
device event up to the divergence frame), and the whole of the difference is one device IRQ delivered late
because a long straight-line shard block holds no interrupt check: the CD interrupt lands 2,125 cycles later
in the shard and the guest resumes at `0x8003E548` instead of `0x8003DD74`. That one resume pc is what the
kernel's save frame, the 6-cycle timer reading and the frame-240 access count all follow from. It is
reproducible only with the pair policy pinned (`PSX_STEREO_SHED=0`, without which this title's A/B cannot be
read at all), and it is not the accounting (identical over a measured span), not the flip or pacing
(identical at every exception entry for 238 frames), and not the conservative event-step diagnostic (its ring
is identical to the default shard's). One fix is queued and it is a small emitter change, not a guess: give
long blocks a device-deadline or instruction-budget check so a pending IRQ does not wait for the block to
end, and prove it by the ring (first divergence moves past this frame, the frame-239 store split at index
4047 disappears). Only then should the route be turned on, with the campaign's own acceptance after it.
To turn the route off, move `build-release/cache/SLUS-00974` aside. Everything needed to reproduce all of
this is in the tree: `build-release/overlay_captures.json`, the cache directory, the compile command above,
the seeded protocol, and the receipts under `analysis/vr-proof/`.

## Next steps

- **The instrument is measured, and the reading rule is a within-session comparison.** The
  harness costs about 12 ms of CPU per ten second window (5.4 ms of in-game export, 6.1 ms of
  recorder parsing), which bounds it at roughly one stall per window, so it cannot explain the
  session that felt shakier. The tracing-off session planned for this is dropped because the
  stall count it was written against is a trace field and does not exist without the trace.
  What matters now is that cross-session comparisons have to clear a spread of 4.5 per cent on
  the pair median and 43 to 93 stalled frames on the same build and route: read the record
  within a session, or repeat it before believing it.
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
  2. **Inside the traversal, the hook's own selection is retired as a lever.** DONE: the
     override is 81 us of an 8878 us traversal (0.9 per cent), with containment holding in
     every frame, so sharing the selection across the pair or tightening the frustum can
     return 81 us and no more. The lever is the 8797 us of guest per-object work that is
     left, and the `-NoHeadFrustumDiagnostic` control run now has one job instead of two:
     it sizes the guest cost of the objects the eye frustum adds over the native body
     camera, which is the whole prize the pruning route plays for.
  3. **Where that 8797 us goes is answered, and it reprices both levers.** DONE: it is the
     dirty-RAM interpreter executing this game's runtime-loaded overlay code, 63 to 67 per cent
     of the frame, 556,199 interpreted instructions per pair, with the replay's own calls
     (`0x6C714` traversal, `0x64550` world pass) dispatched into it exactly twice per pair. The
     in-campaign levers keep their shape: pruning now removes interpreted instructions at the
     interpreter's rate, which is money, and the hottest interpreted blocks to host-replace are
     named (`0x8006DAD8` at 18 per cent of interpreted time and `0x8006DC48` at 8.9 per cent,
     both inside the traversal the hook already brackets).
  4. **DECIDED, RUN, AND NOW MEASURED: the overlay cache forks this game's guest.** It was
     compiled and measured on this bench (section above): interpreter share 0.637 to 0.666 down to
     0.517 to 0.526, interpreted instructions per pair 556,199 down to 299,445, settled
     `avg_pair_ms` 10.88 to 11.13 down to 9.52 to 9.59, 89 functions registered and every one of
     the replay's dispatch entries gone from the interpreter. The acceptance then failed in a way
     that is now readable: with the seeded protocol (`PSX_LOAD_SLOT=1`), two runs of the shard
     backend are identical on every column across 393 frames while shard against interp forks at
     frame 240, first at the RAM write from store PC `0x000027DC` to `0x0000909C`, the OpenBIOS
     exception prologue saving the interrupted `$v1`, with the shard six cycles ahead from the
     frame's first access. An interp pair disagreed with itself and nearly had this retracted;
     the cause was the shedding policy, not the shards (with shedding on the interp sheds 22 to 23
     times by frame 340 against the shard's 8, and one run shed inside frame 240). With
     `PSX_STEREO_SHED=0` both pairs are self-consistent and the fork reproduces, so next is the
     accounting diff per instruction across frames 239 to 240 to name the charge site. The route
     stays off and the in-campaign levers below stay the fallback. To turn the route off, move
     `build-release/cache/SLUS-00974` aside.
  5. **Then the change, held to its acceptance.** Pruning or a cheaper per-object path must
     clear the pixel-compare harness at a fixed guest cycle against the current baseline and
     the accepted look on the headset (no missing tiles, no head-turn pop-in, no near-player
     floor gaps). Anything that only moves bytes must keep the pair byte-identical.
  6. The stretch reproduces in the same part of the level in every session, so the route can
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
6. Answered, and replaced by the choice it implies: the traversal's 8797 us is the dirty-RAM
   interpreter running this game's overlay code, so the in-campaign question is now which route
   to build, pruning the objects the eyes process or host-replacing the hottest interpreted
   blocks (`0x8006DAD8`, `0x8006DC48`). Pruning has a measured price per object and is a
   fidelity trade; host replacement is this campaign's existing pattern, but each block is
   bespoke work and the blocks are inside interpreted overlay code rather than at a stable
   entry.
7. Whether this port's overlay cache can be brought up for Medal of Honor, and what it is worth
   once it is. `overlay_loader_status` shows the loader active with nothing registered, no
   candidates, and no static overlay dispatcher linked into this build, while the framework's
   own `docs/internal/overlay-status.md` says the feature works on Tomba and that its speed
   payoff needs broad coverage plus reload-on-return. If it can be brought up here, it is worth
   most of the two thirds of the frame the interpreter holds, which is more than every other
   item in this document put together.

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

That save is also the composition bench. The scene is identical every run (per-eye prims
1143 and 1142, per-eye guest cycles 638657 and 637929), so stage compositions compare
directly there even though frame timing does not. Start the recorder first (it only needs the
debug port), resume to gameplay until `openxr_stats.submitted_layer` reads 1, let the ring
fill, then read the split with `vr/hitch_report.py`, which prints `game_select` against the
traversal it nests in.

The same session is the profile bench, and its readings come from the framework's own
always-on instrumentation over the debug socket (port 4370), with no instrumentation of ours
involved:

- `phase_profile` tells which backend is executing: 1 the dirty-RAM interpreter, 2 native
  overlay shards, 3 static dispatch of recompiled code, 4 the GPU path.
- `psxrecomp/tools/host_profile.py --port 4370 --exe build-release\Medal_of_Honor__Recompiled.exe
  --frames 900 --pass 1` names the hot host functions inside a render pass, which is how the
  pass profile above was read.
- `gcp_ring` with no arguments gives the retained frame range; with `frame_lo`, `frame_hi` and
  a `path` it writes the ring as CSV, whose `phase`, `block` (a basic-block leader, phys) and
  `fn` columns are what the phase-by-block view above aggregates. Read `block`, not `fn`, for
  interpreted samples: the function stamp is a static-dispatch stamp and goes stale there.
- `dirty_ram_stats` has the interpreter's cumulative blocks and instructions (two snapshots
  plus the pair counter give interpreted instructions per pair) and its per-PC dispatch table,
  and `overlay_loader_status` says whether any overlay variant is registered.

Anything that changes which backend executes guest code wants the runtime's own fingerprint
judge. Use the boot seeds, not the probe: `PSX_LOAD_SLOT=1` stages the save load during
runtime init so it applies at the first safe block boundary before frame 1 in every run, and
`PSX_RECORD_FRAME=N` seeds the Layer-2 recorder at an absolute guest frame. Then poll
`frame_fingerprint count=4096` and slice the ring by frame label. Compare with
`vr/compare_frame_fingerprints.py`, and always compare two runs of the *same* backend against
each other first, on **both** sides of the comparison, because the method only means
something once both pairs are equal.

**Pin the stereo pair policy or the A/B is unreadable.** `PSX_STEREO_SHED=0` on both sides.
Measured: with shedding on, an interp pair disagrees with itself (one run sheds a pair inside
frame 240, 18359 accesses against 18407) and reports 22 to 23 sheds by frame 340 against the
shard's 8, so a cross-backend difference is indistinguishable from the reference moving. With
it pinned both pairs are bit-identical (shard 18404 accesses, interp 18407) and the shard
against interp fork reproduces at the frame's first access, six cycles apart. `stereo_stats`
is the probe that shows the shedding, including `eye_cycles [x, 0]` for a pair whose second
eye never ran.

**Copy `overlay_captures.json` aside before a run if the shard set has to be rebuilt.** The
runtime rewrites that file every run, so a rebuild after a session compiles only the overlays
that session touched. That is what shrank the cache from 67 shards to 21 mid-experiment.

The load-frame trap itself, with its measurements, is recorded in
`psxrecomp/tools/divergence/README.md` and above; the boot seeds are what removed it.

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
# the sandbox test needs the target's own sources (from runtime/CMakeLists.txt):
gcc -O1 -std=gnu99 -I psxrecomp\runtime\include -o sandbox.exe `
  psxrecomp\runtime\tests\test_render_pass_sandbox.c psxrecomp\runtime\src\render_pass.c `
  psxrecomp\runtime\src\frame_trace.c psxrecomp\runtime\src\stereo_shed_policy.c `
  psxrecomp\runtime\src\render_pass_plan.c psxrecomp\runtime\src\psx_cycles.c `
  psxrecomp\runtime\src\timers.c psxrecomp\runtime\src\psx_video_timing.c -lm
python psxrecomp\runtime\tests\test_frame_trace_guard.py
```

Raw captures live under `analysis/vr-proof/`, which `.gitignore` excludes, so they are
local artifacts rather than tracked evidence.
