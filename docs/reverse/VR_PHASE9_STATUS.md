
## Phase 9 step 2 status — hook wired, and headless passes confirmed impossible

### Main-EXE functions need a recompile-time hook (new, and it applies to any future hook)

The `0x80016E38` hook fired **zero** times when registered from the plugin alone.
Reason: `dirty_ram_interp.c` only calls `psx_mod_function_entry` on **interpreted**
entries. Overlay code (above the main-EXE text end) is interpreted, which is why
`0x80084718` and `0x8008B3E8` worked. `VSync` at `0x80016E38` is **main-EXE**, so it
is statically recompiled and never interpreted.

`recompiler/src/code_generator.cpp:2925` emits the hook only when the address is
listed:

```cpp
if (config_.mod_function_entry_funcs.count(func.start_addr)) { ... }
```

So `game.toml` gained a second address and the game C had to be regenerated:

```toml
mod_function_entry_funcs = ["0x80084718", "0x80016E38"]
```

After `psxrecomp_cli.py generate`, one shard changed and the hook appeared:

```c
/* generated/SLUS_009.74_full_01.c:37803 */
if (psx_mod_function_entry(cpu, 0x80016E38u)) return;  /* trusted opt-in game-mod hook */
```

**Rule for the rest of this work: an overlay hook is a plugin-only change; a
main-EXE hook is a config change plus a regeneration.** The earlier note that
"no regeneration was required" was true only for overlay addresses.

### Headless passes are confirmed impossible by measurement

With the hook live, planning from the frame loop reports:

```text
vr-pass: mode0 VSync hits=1 status=1 plan=0
vr-pass: mode0 VSync hits=2 status=1 plan=0
vr-pass: mode0 VSync hits=3 status=1 plan=0
```

`status = 1` is `PSX_MOD_RENDER_PASS_NO_PRESENTER`, and `plan = 0`. `gl_interp`
reports `enabled: 0, target_hz: 0.0`. `render_pass_stats` shows `plans: 0`.

That was previously a reading of `RENDER_PASSES.md`; it is now a measurement.
**No pass can be executed in the headless harness, and `render_pass_dump` cannot
produce anything there.** Step 2's remaining half is a presenter, not more
plugin code.

### The `$ra` gate: usable in principle, unmatched in practice

Instrumenting the hook showed both arguments arriving correctly:

```text
vr-vsync: a0=00000000 ra=8005FE90 sp=801FFF10
vr-vsync: a0=00000000 ra=80061E6C sp=801FFEB0
vr-vsync: a0=00000001 ra=800504C4 sp=801FFEF0
```

Two things follow:

- `a0` carries the mode correctly, so `a0 == 0` is a valid filter.
- `$ra` **is** maintained across the native call, contrary to the worry that
  static recompilation discards it. So a call-site gate is possible in principle.

But **not one of the sampled arrivals had `ra == 0x8008B258`** - the frame loop's
own `VSync(0)` that the disassembly shows at 0x8008B254. The observed return
addresses are 0x8005FE90, 0x8005FF0C, 0x8003D7A0, 0x80061E6C, 0x800619A4 and
0x800504C4, all outside the 0x8008B* range.

**Unresolved.** Either the frame loop runs later than the sampled window (only the
first 24 arrivals were logged), or the tail at 0x8008B254 belongs to a mode the
game was not in. This needs a longer capture before the gate can be trusted -
and it matters, because mode-0 VSync is called from several places.

## 90 Hz feasibility probe

Asked whether a VR-appropriate rate is reachable. Answer: the API is fine, the
compute budget is the open question, and one number is already worrying.

**API: 90 Hz and above are supported.** `psx_mod_set_frame_interpolation` accepts
`0` (follow the display) or 60..1000 and rejects anything else
(`main.cpp:1576`), and the presenter self-paces with vsync off. The presenter's
own gate accepts `source_hz` and `effective_hz` up to 1000
(`gpu_gl_renderer.c:5479`). Nothing in the API blocks 90 or 120 Hz.

**Budget: this is the constraint, and the probe is unflattering.** Turbo,
uncapped, headless, from the interior save:

```text
guest frames in a 5.06 s window: 157   ->  31.0 guest Hz
host cost per guest frame:       32.24 ms
```

That is roughly 1.0x realtime **with no rendering at all** (headless), in a debug
build, and `latency` agrees (`frame_period.mean_us = 31794`). At 90 Hz the whole
output frame is 11.1 ms; with two eyes that is ~5.6 ms per eye - against 32 ms for
one guest frame.

A render pass is cheaper than a frame (guest time is frozen; only the draw code
runs, no AI, audio, CD or input), so 32 ms is not the pass cost. But it is the
only number available until a presenter exists, because `cost_us` in
`render_pass_stats` is measured from real passes and headless never runs one.
Caveats that cut both ways: debug build is much slower than release, and headless
has no GPU work.

**What this implies.** Two full scene re-renders per output frame at 90 Hz
almost certainly does not fit. If VR at 90 Hz is the target, the likely shape is
**one scene traversal producing both eyes** - one GTE run, two projections - rather
than two independent passes. That is a deeper change than either the pass system
or a second pass call, and it argues for settling the API design question raised
in `VR_DOUBLE_RENDER_SCOPE.md` (step 4) before building presentation.

**Also worth noting:** `psx_mod_set_native_vblank_rate` would raise the rate but
speeds up the entire machine, and `FRAME_RATE.md` explicitly warns against using
it for this.

## RETRACTION: the 90 Hz cost claim above is not supported

The "Budget" section above concludes that two passes per frame at 90 Hz "almost
certainly will not fit". **That conclusion is withdrawn.** It rests on a
measurement that does not say what I claimed it said, and the user was right to
push back.

### What was wrong

**1. The run was never uncapped.** The 32.24 ms figure came from a run where I
sent `turbo enabled=1` and then measured. Querying `turbo_state` later reports:

```json
{ "id": 0, "ok": true, "enabled": 0 }
```

Turbo never engaged from the TCP command. So the number is a **paced** frame
time, not a capacity limit - and I presented it as "turbo, uncapped". That was
the error.

**2. The host was not saturated.** In the same debug build, process CPU was
13.2 s over 19.1 s of wall time - about **69% of one core**, and the earlier run
was ~14.8 s over ~30 s (about half a core). A workload that is compute-bound
pins a core. This one was idle half the time, waiting on the pacing clock.

**3. It is a debug build.** Debug C with no optimisation is several times slower
than release.

**4. The frame counter runs at 48.9 Hz against a 59.94 Hz nominal** - the debug
build is at roughly 0.8x realtime, and paced.

### Why the underlying intuition was wrong

Modern games at 144 Hz spend almost nothing on the second view per frame: the
extra work is GPU rasterisation, which is cheap and massively parallel.

Here the second eye is **not** extra pixels. It is a re-execution of the
PlayStation's own CPU code for the whole scene - GTE transforms and packet
building, instruction by instruction, through the emulator. That is why the
question is not "can the hardware draw 512x240 twice" (of course it can) but
"can we afford to emulate the PS1's scene draw twice".

The PS1 CPU is 33.87 MHz; a frame is ~565k cycles. Code recompiled to C on a
modern host typically runs that 10-50x realtime. A second full scene draw should
therefore cost single-digit milliseconds, not tens. **90 Hz stereo is plausibly
achievable, and nothing measured so far contradicts that.**

### Still unmeasured

To be honest about the remaining gap: I do **not** have a clean uncapped number.
Turbo does not engage from the TCP command, and the release build never reached
gameplay - it sat at boot (`PC=0xBFC00000`) and its log shows overlay gaps
falling back to the interpreter ("tcc tier active but no bundled toolchain"),
so it is not a fair performance sample either.

The number that actually decides this is `cost_us` in `render_pass_stats` - the
**measured per-pass cost**, taken from real passes. It does not exist headless
and only exists once a presenter runs a pass. So the presenter does not merely
unblock verification; it is also the only way to answer the rate question.

Until then the burden of proof is on the pessimistic claim, and it failed.


## Phase 9 step 2: the presenter is up and passes are AVAILABLE

Two blockers cleared this round.

### 1. The windowed binary opens a GUI launcher, not the game

Running without `--headless` sat forever with no debug server. It was showing the
shared **recomp-ui Dear ImGui launcher** (`PSX_RECOMP_UI:BOOL=ON` in the build).
`main.cpp:14335` documents the escape:

```text
Skip the GUI (boot straight in) when ANY of: PSX_NO_LAUNCHER=1 env,
--no-launcher, or the persisted [launcher] skip_launcher setting
```

With `--no-launcher` the game boots windowed with the OpenGL presenter and the
debug server comes up. **Every windowed run must pass `--no-launcher`.**

### 2. Passes go from unavailable to READY and planning

The plugin now calls, from its activation callback (opt-in via `PSX_VR_INTERP`):

```c
psx_mod_set_frame_interpolation_source(PSX_MOD_FRAME_SOURCE_FLIP);
psx_mod_set_frame_interpolation_blend(PSX_MOD_FRAME_INTERPOLATION_HOLD);
psx_mod_set_frame_interpolation(g_interp_hz);   /* 0 = display refresh */
```

All three return 1. `gl_interp` then reports:

```text
enabled: 1  source: "flip"  flip_period: 2  host_hz: 60.0  captures: 510  duplicates: 443
```

`flip_period: 2` independently confirms the 30 Hz double-buffered structure found
from the GP1 ring, this time from the presenter's own flip tracker.

And at the frame-loop `VSync(0)`, planning works:

```text
vr-pass: mode0 VSync hits=1 status=6 plan=0     <- BUSY, first frame
vr-pass: mode0 VSync hits=2 status=0 plan=1     <- READY, plan succeeded
render_pass_stats: plans=2 planned=2 wanted=2 refused=0
```

`status` 6 is BUSY (no frame captured yet), then 0 = READY. **This is the first
time a plan has succeeded**, and it is the empirical confirmation that the
hook point chosen in `VR_HOOK_POINT.md` is the right one.

It also retires the earlier `$ra` puzzle: gating on `a0 == 0` alone produces a
valid plan at the frame loop, so the hook is in the right place and the
`$ra == 0x8008B258` question is moot for planning purposes.

### 3. Running a pass: still refused, and where

Calling `psx_mod_render_pass()` with a 512x240 rect at (0,0):

```text
vr-pass-call: alpha=32768 rect=512x240+0+0 ret=0 status_after=0 runs=0
```

`ret=0` is "refused or rolled back", `status_after=0` is READY, and `runs=0`
means the pass body never executed. So the refusal is not the status gate and not
the argument validation in `render_pass.c:494-497` (w/h nonzero, `0 < alpha_q16 <
65536` - ours is 32768). It is `gl_renderer_pass_begin` returning 0
(`render_pass.c:516`), and inside it there are only two candidates:

```c
if (!gl_renderer_pass_ready() || s_pass_active) return 0;            /* :6177 */
if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > VRAM_W || ...)     /* :6178 */
if (open_gen) { if (tw != s_interp_w || th != s_interp_h) return 0; } /* :6192 */
```

The rect is in range and the size looks right: `video_info` reports
`display_x/y = 0/0`, `display_w/h = 512/240`, `hr_scale = 1`,
`effective_scale = 1`, `fbo 1024x512`. With `S = 1`, `tw = 512 = display_w`,
which is what `s_interp_w` should be. So the size check is probably passing and
the refusal is more likely `gl_renderer_pass_ready()` - or the rect's `y` is
wrong: MoH double buffers, so the *next* flip's DISPENV origin is 0 or 240
depending on the buffer, and `pass.x/y/w/h` is documented as "the display rect
the **next flip** shows". Passing a constant `y = 0` ignores that alternation.

**Next step, precisely:** instrument which of those two returns fires - easiest is
to try `y = 240` on alternate frames (or read the game's own DISPENV buffer) and
see whether `ret` becomes 1. That is a small, bounded change, and it is the last
thing between here and the first real pass image.

### Note on the pass body

The body is deliberately a no-op for now (`vr_pass_fn` returns 1 and logs). A
no-op pass still exercises freeze, capture and restore, and it is what proves the
mechanism end to end. The per-eye redraw goes in that function once a pass runs.


## Phase 9 step 2b: pass refusal narrowed to one function; needs framework visibility

### The rect is not the cause

Made the rect configurable (`PSX_VR_RECT="x,y,w,h"`, `PSX_VR_RECT_ALT=1` to add
240 to `y` on alternate frames) and tried both candidates:

```text
rect=512x240+0+0     ret=0  status_after=0  runs=0
rect=512x240+0+240   ret=0  status_after=0  runs=0
```

Both refused. So the double-buffer origin alternation is **not** the blocker, and
the earlier hypothesis is retired.

### Where it must be

`gl_renderer_pass_ready()` is `gl_renderer_pass_unavailable() == READY`, and the
status is 0, so it is true. The argument validation in `render_pass.c:494-497`
passes. The rect is in VRAM range. That leaves, inside
`gl_renderer_pass_begin` (`gpu_gl_renderer.c:6173`):

```c
if (!gl_renderer_pass_ready() || s_pass_active) return 0;            /* 6177 - ready is true */
if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > VRAM_W || ...)     /* 6178 - in range */
if (open_gen) { if (tw != s_interp_w || th != s_interp_h) return 0; } /* 6192 - see below */
...
if (!pass_gen_reserve(gi, 1u, tw, th)) return 0;                     /* 6193 */
if (!pass_make_color_fbo(...)) ...
```

**The size check looks like it should pass.** `tw = w * s_hr_scale`, and both
`video_info` (`hr_scale: 1`) and the source (`s_hr_scale` defaults to 1, set from
`s_out_scale`, which is also 1 here) give `S = 1`, so `tw = 512`. The presenter's
captured width is `pw` from `hiw_capture_size(w * s_out_scale, ...)` with the
display width 512 and no windowed hi-res - also 512. `aspect_ratio = "4:3"`, so
`wide` should be false. On paper it matches.

So the refusal is probably **past** line 6206, in the resource path
(`pass_gen_reserve` / FBO creation), not in the validation. That is not
observable from a plugin.

### Next step: get visibility the sanctioned way

The framework is explicit that instrumentation belongs in the TCP server, not in
printf - "If an inspection need isn't covered by the existing commands, do not
fall back to printf or log files. Instead: add a handler in
`runtime/src/debug_server.c`".

So the next move is a small framework change: a diagnostic that reports, at the
`psx_mod_render_pass` refusal, which internal check failed -
`s_interp_w`/`s_interp_h` vs the requested `tw`/`th`, `s_pass_active`, and the
resource-allocation result. `render_pass_stats` already has the counters
(`refused`, `discarded`) to hang it off; what is missing is the *reason*.

This is a change to the shared `psxrecomp` framework rather than to the game, so
it is worth agreeing before making it.

### What is solidly established

- Windowed runs need `--no-launcher`; the presenter then works and the debug
  server is up.
- `PSX_VR_INTERP=1` enables interpolation (FLIP + HOLD); `gl_interp` confirms
  `enabled=1, source=flip, flip_period=2`.
- Passes go from `NO_PRESENTER` to READY, and **planning succeeds** at the
  frame-loop `VSync(0)`: `plans=2 planned=2 wanted=2 refused=0`.
- Whatever blocks `psx_mod_render_pass` is downstream of the status gate and of
  all argument validation, and is independent of the rect origin.

## 2026-10-02: source audit corrections and execution plan

The next steps are tracked in [VR_EXECUTION_PLAN.md](VR_EXECUTION_PLAN.md).
The following are source findings, not new live measurements:

- The assertion that the refusal must be inside `gl_renderer_pass_begin` was
  too strong. `checkpoint_save` can fail after GL begin succeeds but before the
  callback executes; this also returns 0 with zero callback runs.
- `status_after=READY` reports current availability, not a latched failure-site
  status. It does not by itself prove which earlier gate accepted the call.
- `render_pass_stats.refused` counts empty plans with wanted phases, not rejected
  `psx_mod_render_pass` calls. Its zero value does not establish pass acceptance.
- `pass_gen_reserve` checks the slot cap and texture names, but does not inspect
  `glTexImage2D` errors. A texture-storage failure is not directly detected there.
- Display dimensions/scales sampled by TCP do not establish capture-size equality
  at the rejecting branch. Actual requested and history dimensions will be latched
  there before ranking resource failures above size/history mismatch.
- `render_pass_dump` arms dumping at generation promotion; it does not create a
  pass. Existing `image_textures` can narrow the allocation stage, but does not
  prove valid texture storage.

No runtime was listening on TCP port 4370 during the initial source audit.

## 2026-10-02: live refusal identified; gameplay no-op passes captured

Framework `257a88b0` adds failure-site diagnostics to `render_pass_stats`,
including separate pass-call counters and latched rejection inputs. Sandbox,
watchdog rollback and source guards pass. The game Debug build passes.

### Refusal: actual history was 256 pixels wide

The original probe reproduced two refused calls. Both are classified as
`capture_size`: requested 512x240, capture/history 256x240, scales both 1,
wide=0, open_gen=1, active=0, status=READY. `image_textures=0` and no resource
stage was reached. The last failure record is attempt 2, plan 2, guest cycle
231387035, alpha 32768. Raw responses are in
`vr/proof/pass-diagnostics/refusal_initial.json` and `refusal_after_load.json`.

**Correction:** the previous conclusion that the size comparison should pass
and resource creation was the likely culprit is retired for this reproduction.
The size check correctly rejected the request. A later `video_info` reading of
512x240 describes the current display, not the history at that earlier branch.
The diagnosis does not claim that all other runs have the same refusal.

### Slot 1 has a different wait/flip path

After loading the interior room, the old failure record and pass-attempt count
remained unchanged while gameplay continued. Live disassembly and flow tracing
show slot 1 executing `FUN_80090B80`, which polls DrawSync(-1). The alternate
loop at `0x8008B254` remains real, but its VSync(0) probe did not establish a
gameplay pass boundary for this save.

GP1 traces show the gameplay flip at `0x80018748`, ra=0x8001748C, with
sr=0x40000404 and exception EPCs in the wait loop. A PutDispEnv probe produced
no planned passes there: the transaction's exception gate is appropriate.

**Correction:** "planning succeeded, therefore the gameplay hook is right" was
too broad. The recorded initial successes belonged to the 256-wide startup
history. VSync(0) availability cannot establish the gameplay timing path.

### No-op proof at the main-thread render-wait entry

The plugin now has opt-in `PSX_VR_PASS_PROBE=1` at `FUN_80090B80` entry,
before its stores enable the IRQ flip. The upcoming DISPENV is read using the
game's own calculation at 0x80090CCC..0x80090CE8:
`0x8009A7A0 + (!read_word(0x8009C824))*20`. The RECT supplies x/y/w/h; hook
count parity is not used. The legacy VSync probe is bypassed in this mode.
The host recursion guard is cleared after the pass API returns, including
watchdog rollback. The hook is an overlay entry and needs no added static hook.

With `--no-launcher`, `PSX_VR_PROBE=0`, `PSX_VR_PASS_PROBE=1`,
`PSX_VR_INTERP=1`, `PSX_RENDER_PASS_VERIFY=1`, and loaded slot 1:

- First saved sample: 90 passes, 89 promotions, 90 verification checks,
  zero verification mismatches, zero pass-call refusals.
- Final saved sample: 727 passes and 727 verification checks, zero mismatches,
  no aborted passes, watchdog overruns, VRAM leaks or pass-call refusals.
- Two dumped generations contain baseline phase 0 and no-op phase 32768 images,
  each 512x240. Direct decoded RGB comparison reports **zero changed pixels**
  for both pairs. The images show the interior-room scene.
- The final `cost_us=3837` is measured no-op capture/restore with verification
  enabled. Guest draw cycles are zero. It is not a scene-redraw or stereo cost.
- The nonzero `refused=572` is temporal plan shedding, not pass-call refusal.
  `status=BUSY` in the final asynchronous TCP sample is current availability;
  all 727 actual attempts succeeded.

The verification runs were stopped after recording evidence. Details and
reproduction commands: [pass-diagnostics README](../../vr/proof/pass-diagnostics/README.md).

**Still open:** live timeline fingerprint comparison, live watchdog rollback,
complete replayable draw slice, paired per-eye capture, dynamic/weapon/HUD
coverage, eye calibration and stereo presentation. No redraw-safety or stereo
performance conclusion follows from the no-op proof.

# 2026-10-02: no-op timeline comparison and bounded watchdog injection

Five slot-1 loads were measured: two no-op probe-disabled control loads, two
probe-enabled loads, and one probe-enabled load with a one-shot synthetic abort.
All kept interpolation enabled and `PSX_RENDER_PASS_VERIFY=1`; existing geometry
offset/probe controls were unset or zero. `frame_fingerprint reset_on_load=1`
was armed before each load. The first 96 consecutive guest frames match exactly
in cycles, RAM write counts/sums, MMIO hashes/counts, scratchpad hashes/counts,
quiet counts and ordered RAM/PC locator hashes. Only host frame labels differ.
The two control loads also match each other. This establishes repeatability and
no-op timeline equivalence for this bounded scene window, not arbitrary redraws.

`PSX_VR_PASS_WATCHDOG=1` injects one deliberate CPU/GTE/RAM/scratchpad change and
advances frozen cycles in bounded chunks until the existing watchdog aborts.
TCP receipt: 488 attempts/checks, 1 watchdog/abort, 487 subsequent successful
passes, zero verification mismatches, zero leaks, disabled=0. The host one-shot
selector is set before the longjmp; `g_in_pass` is cleared after API return.
This tests synthetic callback rollback/recovery; nested guest dispatch remains
to be checked when the draw slice is exercised. Evidence and comparison script:
`vr/proof/replay-scope/` and `vr/compare_frame_fingerprints.py`.

## Correction: DrawOTag and PutDrawEnv addresses

Earlier notes and the peer brief identified `0x800172D8` as PutDrawEnv. Live
disassembly ties its debug-string producer to `0x800148FC`, which reads
`DrawOTag(%08x)...`. `0x80017348` refers to `0x80014910`, containing
`PutDrawEnv(%08x)...`, and builds a drawing-environment packet. The corrected
mapping is **DrawOTag=0x800172D8, PutDrawEnv=0x80017348**. This matters: the call
at `0x8005052C` submits the completed ordering table, after the wait returns.
The historical notes are retained; use this corrected mapping for replay.
Raw disassembly and the referenced RAM strings are preserved in replay-scope.

The live render-wait caller is `0x800504E0` (return `0x800504E8`). Source inspection
shows `FUN_800503D4` constructs the frame then calls `FUN_800504F8`, which waits,
flips and submits. Do not replay that enclosing function under frozen time.
The level list renderer is `0x80053C20`, called indirectly at `0x8006C744` from
the scene entity traversal with observed a0=0x801693E0, a1=0x8009A620. It iterates
level groups and calls `0x8008BF00`, which invokes `0x8008B3E8` and primitive
construction. These observations locate a candidate slice; no complete redraw
has yet been claimed.
# 2026-10-02: first complete slot-1 redraw and nested abort proof

The plugin now provides opt-in `PSX_VR_PASS_DRAW=1`: reconstruct the calls before
`0x8005047C` in `FUN_800503D4`, then submit the rebuilt OT using the corrected
DrawOTag address. It excludes the wait/flip helper. The pass first sets the
current game draw environment and clears its full rect. Two promoted replay
images match the baseline decoded RGB pixels exactly and contain the room,
weapon, compass and ammo counter. Nonzero work is measured at 389,779 guest
cycles in the sampled pass; 87 checks have zero mismatches or dropped stores.

Clear-only mode 2 produces an entirely black image; the capture therefore reflects
the modified rect. Level-only mode 3 resolves the current level entity by callback
pointer instead of pinning an observed RAM address. It retains the room and compass
and omits the weapon/ammo counter (11,095 changed pixels). Do not call this a
HUD-free level pass: the compass is retained.

`PSX_VR_PASS_WATCHDOG=2` injects one abort at the level-transform hook inside a
real pass dispatch. TCP records 1 watchdog/abort, 388 subsequent successes,
389 verification checks, zero mismatches/leaks/dropped stores and no disable.
The full-draw and nested-abort runs both match all measured fingerprint columns
and cycle counts for the first 96 consecutive post-load frames against the
repeatable probe-disabled control. Recorded `nesting_repairs=0` is not used to
infer a particular native dispatch depth.

Evidence: `vr/proof/scene-replay/`, with PNGs, raw TCP responses, exact controls,
comparison receipts and reproduction commands. All changes here are game plugin
and proof/docs changes; no framework transaction behavior was changed. This
establishes a bounded complete scene replay for slot 1, not animated-object
coverage, other game modes or a simultaneous stereo pair. Debug verification
timings are recorded as samples, not a stereo performance claim.
# 2026-10-02: scene selection correction and simultaneous eye proof

The user corrected the requested scene to slot 3. Earlier exploratory slot-2
and initial slot-3 captures are retained with caveats, and are not evidence for
animated coverage or timeline equivalence. Completed TCP slot-3 loads now record
`pending=0`, `last_ok=1`, `last_slot=3`, plus save file metadata and SHA256. TCP
slot 3 maps to `state_8001DFD4_slot03.pst`; its OSD uses one-based "Loaded slot 4".
Ordinary temporal baseline/replay dumps in the additional scenes differ slightly;
their cause has not been isolated and is not attributed to draw completeness.

Framework `0a955971` adds explicit paired-eye transactions, atomic publication,
TCP inspection/dumps and SBS, independent of interpolation. Its scoped GTE offset
replaces camera-space translation before RTPS/RTPT division without writing TR
or accumulating at transform entries. Existing snapshot/watchdog protections are
shared. Framework changes are inventoried in docs/UPSTREAM_PENDING.md.

TCP view offsets are sampled from the projection ambient at callback end, not at
each RTPS instruction. They describe callback scope; the GTE translation test and
the two image ROI correspondences independently establish the projection effect.

The fresh slot-3 zero-offset control captures both enemies and their exact poses
with zero differing decoded RGB eye pixels in both pairs. Offset-24 pairs differ
on 78,026/77,961 pixels across all bands. Named, inspected wall/near-ground ROIs
match at -5px/-13px (NCC .9412/.9637). These are image correspondences, not world
distances. GTE tests independently measure 12px/3px at Z=800/3200, preserve TR and
confirm repeatability. Eye entry cycles and VERIFY state hashes match. The first
96 consecutive post-load fingerprints/cycles match the no-redraw control for
zero, offset, held-abort and recovery runs. All verification mismatches, VRAM
leaks and dropped stores are zero in these samples.

A watchdog inside real right-eye level dispatch aborts attempt 31 while published
pair 1 remains valid, with staging mask zero and one watchdog. A second run permits
subsequent successful pairs and retains the failed-eye/retained-pair record. Host
selector and view ambient recover; native nesting depth is not inferred from the
live `nesting_repairs` counter. Nested unit tests cover skipped native exits.

Composed-present readback confirms SBS with interpolation disabled and no temporal
plans or promotions. Debug pairs sampled around 35ms exceed the 26.67ms cadence
budget and trigger whole-pair shedding; no real-time headset claim is made. Eye
offset 24 remains uncalibrated. Compass geometry also receives the offset; flat
ammo text stays coincident. Scale/IPD, HUD comfort, head poses and OpenXR remain.
Evidence and commands: vr/proof/stereo-pairs/README.md. All test processes are
closed after measurements at the user's request.
# 2026-10-02: Release measurement configuration and matched control

The first Release launch had `PSX_DEBUG_TOOLS=OFF`; no TCP measurement was
available and it was closed. The local Release build was reconfigured with
`PSX_DEBUG_TOOLS=ON`, retaining `-O3`/`NDEBUG`, and run with VERIFY/interpolation
off. `video_info` measures scale 5 and 2560x1200 eye textures, unlike Debug's
scale 1. The warm receipt records 1,719 pairs, zero refused/failed/shed, 12.013ms
last pair and 11.584ms EMA, with equal eye entry cycles. Zero state hashes mean
VERIFY was off; they are not a hash-verification signal. This is a bounded pair
cost sample, not a Debug/Release speedup ratio or headset throughput guarantee.

A preliminary cross-build fingerprint comparison differs and is retained with
its mismatches. It does not isolate the effect of stereo: build and renderer
settings differ. A separate Release OFF run at the same scale/settings matches
all 96 recorded fingerprint columns and guest cycles against Release stereo.
Evidence: vr/proof/stereo-pairs/release_matching_timeline_comparison.json. Two
Release image pairs retain 5/13 display-pixel correspondences on a documented
scale-5 grid sample. All measurement instances have been closed.

# 2026-10-02: Proof retention cleanup

After user confirmation, removed 57 redundant, exploratory or large Release PNGs
from the current checkout: 118.86 MiB becomes 6.51 MiB. Thirteen representative
images and all 170 JSON receipts remain. Both Debug zero-offset and offset-24
eye pairs can still be checked directly, along with one composed SBS screenshot.
Measurements, limitations and recorded corrections are unchanged. Historical
image recomputation for removed captures requires game commit
`231650447bdc219629559a16f0dd7d229ee1ce49` or a fresh measured run.

New sessions should start with VR_EXECUTION_PLAN.md and the relevant status
section, then consult targeted proof files only as needed. The proof index and
evidence READMEs state the retention boundaries. Future bulk captures go to the
already ignored analysis/vr-proof/ directory. See VR_PROOF_CLEANUP_PLAN.md.
No framework changes or new game measurements were needed for this cleanup.

# 2026-10-02: metric controls, rigid views and measured HUD producers

Added explicit IPD, base units/meter and WorldScale controls. The base mapping
48/.067 preserves the previous 48-unit separation at user IPD 67mm; it is a
provisional starting point, not a measured physical reference. The new calibration
utility requires a coordinate span and an explicit physical-size assumption.

Framework scoped views now include Q12 rigid rotation, translation and per-eye
asymmetric focal/centre terms. RTPS/RTPT apply the camera transform before division;
guest TR is untouched. Full host view state restores on ordinary return and nested
watchdog abort. The identity/native-projection path retains the GTE oracle.
Synthetic desktop yaw +10 degrees and position X +0.1m change 87,962 and 79,786
pixels respectively against the identity left eye. All 96 recorded timeline rows
match identity across judge and locator columns; no restore mismatches/leaks or
dropped stores in these verified controls. Identity +/-24 reproduces the previous
78,026/77,961 changed-pixel pairs and wall/ground 5/13px correspondences.

At zero eye separation, excluding FUN_8005F86C changes 81 pixels at
x=392..486,y=18..24; excluding FUN_8008019C changes 310 pixels at
x=417..445,y=15..33. Combined controls change 391 pixels and leave the held
grenade and measured world unchanged. Correction: the earlier weapon label for
FUN_8008019C was too broad; this measured call produces the upper-right HUD icon
in slot 3. The held weapon comes through entity rendering. Excluding FUN_80089A0C
removes 42,281 pixels across a large upper world region and retains the compass;
it is not a HUD-only policy. Text/icon visibility controls are now independent.
Wrist HUD placement is explicitly deferred by the user; compass treatment remains.

# 2026-10-02: native Quest 3 / VDXR submission and corrections

The opt-in Win32/OpenGL OpenXR backend locates both eyes for one predicted time,
uses runtime poses/FOV, renders one restored checkpoint and submits a fresh
complete pair. Failed/shed redraws end with zero layers rather than attaching new
poses to old images. LOCAL recenter, validity checks, session events, swapchain
ownership and teardown are implemented. Diagnostics are TCP openxr_stats/views/
control; framework changes are inventoried in docs/UPSTREAM_PENDING.md.

Initial real VDXR initialization refused the existing GL 3.3 context at
opengl_version. A later producer sample records actual GL 4.6, required minimum
4.0 and advertised maximum 5.0. The opt-in host environment PSX_OPENXR=1 now
requests 4.6; ordinary launches retain 3.3. Actual runtime IPD sampled 0.066780m.
A Debug sample submitted 13 frames out of 370 waits with 357 empty frames; its
cost exceeded the existing budget. Release scale-5 eye textures are 2560x1200;
a separate sample submitted 483 of 484 waits without empty frames or failures.
One frame can be in progress at a TCP snapshot. These samples are not a headset
refresh-rate or motion-to-photon measurement.

The first direct XR blit was upside down in the headset. Flipping Y only during
copy into acquired XR images corrected it; the user confirmed upright output
and expected head-tracking direction. Desktop dumps retain their row convention.

Scale trials: 1, 0.5, 0.25, then user-reversed direction to 2 and accepted 3.
The user reported smaller settings looked bigger; this subjective observation is
retained without inferring physical scale from it. Correction: the directory
openxr-scale-half is not valid half-scale evidence: its build failed and the
following shell launched an older binary. A guarded successful rebuild/rerun is
openxr-scale-half-valid. The launcher now refuses after build/configure failure.
A stale async request for 0.125 was superseded by explicit "try 2", then "try 3".

The accepted scale-3 live snapshot records 5,806 waits, 5,805 submissions, no empty
frames or XR failures. Stereo records 5,806 completed pairs, no shedding/failures;
6.098ms EMA / 5.857ms last cost is a bounded sample in its scene, not a speedup
ratio against earlier scenes. A snapshot taken during the next left eye has a
zero right-eye entry cycle; do not treat it as a completed-pair mismatch or as
equal-entry proof. The user accepted scale 3 while reporting an oversized weapon
and possibly small enemies, and explicitly deferred fine tuning.

# 2026-10-02: preserve authored entity focal length

New producer-bound native projection tracing (frame 135, render=0) records
620 H=400 entries at ra=8008BF84 and 175 H=133 entries at ra=80080F2C among
1,401 native entries. This is actual RTP producer evidence for the entity path;
it does not undo the earlier retraction of a detached H=133 sample or alter the
established H=400 level path. GTE ring inspection now distinguishes native and
host-view projections and supports pagination/full-frame count.

Absolute XR focal terms discarded the game's different entity focal length.
The optional projection_h_ref=400 policy multiplies focal terms by effective
H/400, retaining world draws at H=400 and the authored entity ratio. A fixed
Quest-FOV desktop control at zero separation/pose changes 10,021 pixels in
[336,110,511,216] and visibly reduces the held grenade. Wall ROI
[50,100,170,130] changes zero pixels. Correction: the exploratory ROI labelled
enemy_right overlaps the weapon and its 1,067 changed pixels do not establish an
enemy-size change. Both focal controls match all 96 recorded timeline rows;
restore checks have zero mismatches/leaks/dropped stores. The policy is enabled
by default; final weapon appearance needs headset assessment and other modes.

The final bounded Release launcher capture completed two pairs with equal eye
entry cycles, 51 completed pairs, no shedding/failures and 11.317ms EMA at the
measured 2560x1200 eye size. VERIFY is off, so zero hashes are not verification.
All owned measurement processes were closed. Complete pose/GTE/sandbox/watchdog
tests, render guards, generated TCP index and opt-in Debug/Release builds pass.

Evidence: compact VR_HEADSET_RECEIPT.json plus ignored analysis/vr-proof captures.
Measurements before pin update used the framework working tree at external
PSXRECOMP_ROOT, not the older pinned submodule reported by early launch receipts.
Future launcher captures record actual build-root revision/changes and executable
SHA256. No bulk images are added to Git. Current scheduling is the game's ~30Hz
draw boundary; independent headset cadence, wide-FOV culling, physical calibration,
controller actions and wrist HUD remain open. Setup: VR_OPENXR_SETUP.md.

# 2026-10-02: delivered pin and matched native-XR control

Framework commit 22bdc9e9 was pushed, then pinned in the game. Both Debug and
Release build from the pinned submodule with OpenXR ON; the local rewind build
setting is restored to ON. A fresh bounded Release native-XR capture records
VirtualDesktopXR running/tracking, view_flags=15, effective units/meter=238.805970,
55 submissions / 56 waits, zero empty frames/failures, and two completed eye
manifests. Its executable SHA256 and clean actual framework revision are recorded
in build_provenance.json. A separate desktop control uses the exact same binary
and scene/settings; all 96 consecutive fingerprint rows match across every judge
and locator column. This isolates headset pose/projection/submission from live
guest timeline effects in the measured window. VERIFY is off, so it does not
substitute for the separately verified Debug restore controls. Both processes
closed automatically. Compact receipts include these producer values and the
comparison; bulk captures remain ignored. Wrist placement remains deferred.
