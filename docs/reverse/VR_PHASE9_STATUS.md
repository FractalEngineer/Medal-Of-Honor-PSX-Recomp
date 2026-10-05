
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

# 2026-10-02: Quest locomotion, native analog response and scale-4 trial

User priority changed to movement before HUD: left-stick forward/back/strafe,
right-stick smooth turn. Added opt-in Touch thumbstick actions and a trusted
offline controller source through normal SIO, not writes to player transforms.
Input synchronization occurs at normal host input sampling, never eye replay.
Neutral/invalid/unfocused/inactive/declined samples release axes; TCP override
priority, coherent pad type, post-load suppression and selfcheck recording stay
intact. The game forces analog presentation only for its opted-in input source.
Keyboard/ordinary pad button words remain merged for menus. Framework defaults,
netplay and resim do not acquire this source.

Slot-3 native axis controls establish PSX LX turns, LY moves, RX strafes; RY
pitch is left neutral for head tracking. Initial centered-byte mapping passed
basic direction/release controls but was insufficient: the user reported slower
left turns and weak combined/diagonal movement. Do not label those initial
controls consistent movement. Producer tracing found native FUN_80075E7C reads
calibration rows at 800B74B0 (40-byte stride) and the response lookup at 8009E3AC.
Measured rows have negative threshold 91, positive branch threshold 166,
negative/positive factors 360/199 and native scale from row+32 >> 8. Default
semantic rows for LX/LY/RX are 3/0/1, selecting wire indices 2/3/0. An early
response-mapper draft used the pitch row for forward motion; its scheme guard
correctly delivered neutral and its control failed. It was corrected before
accepted final controls. Other schemes/inverted axes deliberately release.

Heading SW at pc=8007FC10 writes input object 800EEC2C+388 (800EEDB0), then
pc=80080160 copies that heading to player 800EE860+556. Controlled pre-fix native
bytes 45/211 produced per-update deltas -522,240/+1,167,360. This isolates actual
heading increments; older before/after camera snapshots over 30-33 frames were
not equal-duration angular-speed measurements. The replacement reads the live
calibration/curve, selects a response attainable by both signs, and inverses it
to native bytes. Movement uses a radial deadzone, preserving direction before
quantization; turn uses a scalar deadzone and gain on decoded response.

Final producer-bound controls: full Quest turn delivers LX=8/241 with heading
increments -2,457,600/+2,457,600; the +/-600-thousandths turn control delivers
LX=22/216 with -1,290,240/+1,290,240. Forward/strafe alone decode to magnitude
255; diagonal +/-707 axes decode to +/-180 components (vector magnitude
0.998268 relative to a cardinal). Combined movement/turn retains the full turn
increment. These are native curve units and per-update writes, not physical
meters/sec or degrees/sec; diagonal magnitude is decoder evidence, not a
collision-free world trajectory measurement. All nine controls release to
[128,128,128,128]. Mapping tests sweep turn symmetry and movement circle angles.
The user confirmed corrected movement: "movement fully consistent now".

Initial real VDXR input capture: 257 adjacent action/pad snapshots in 30 seconds,
zero errors, synthetic=0, both hands active, sticks spanning both horizontal
directions and forward/back. Its bounded counters record 3,131 submissions,
zero XR failures and zero stereo shedding. Corrected worn-headset capture:
174 adjacent snapshots in 20 seconds, zero errors, synthetic=0, and 2,432 XR
submissions with zero failures/empty frames; user assessment supplies the
consistency confirmation. Action and pad queries are separate snapshots and
are not atomic producer/delivery comparisons. These counters do not establish
independent headset cadence or motion-to-photon performance.

Correction/limitation: restricted launches reported xrGetSystem=-35; a launch
in the user's Virtual Desktop session succeeded. Launch context/timing changed,
so a specific IPC cause or controller disconnection is not established. A later
visibility complaint occurred despite 915 successful XR submissions and zero
failures; restart produced a nonblack read-back eye and the user then confirmed
movement. Its cause was not isolated. Do not equate successful submissions with
what the user sees. The launcher now refuses an occupied debug port so captures
cannot silently inspect an older game; headset captures require live XR startup
submissions and retain refusal diagnostics. Owned games close in finally.

WorldScale 4 actually reached VDXR: units/meter=179.104478, versus 238.805970 at
3, with actual eye poses/IPD=0.066780m. The user could not see a difference; this
is an inconclusive perceptual trial. Keep the accepted default 3, IPD preference
67mm. Physical-reference calibration, weapon/enemy tuning and wrist HUD remain
open/deferred; no calibrated-scale conclusion follows from this trial.

Neutral stereo OFF/ON controls match all 96 guest fingerprint columns/cycles.
A first held-source trial differed in 93/96 rows, first at relative frame 3.
The save-load guard uses 90-700ms host time and those runs did not establish
identical delivery schedules, so that experiment is not replay correctness
evidence. Repeating with the measured native pad [216,7,243,128] held by TCP
priority bypasses that guard and isolates stereo replay: all 96 consecutive
rows match in every judge/locator column and cycle. No faithful guard changes.
Corrected Debug response controls record 38 restore verifications with zero
mismatches, VRAM leaks or dropped stores. The native held control has its own
restore receipt; source polling is skipped under TCP priority by contract.

Framework 5a1264b7 is committed/pushed, inventoried in UPSTREAM_PENDING.md and
pinned in the game. Final OpenXR-enabled Debug/Release builds again use the pinned
submodule (initial measurements used the external framework working tree).
Controller-source and compiled-out XR-input C tests, game response C test,
controller lifecycle/render guards, debug-less syntax and generated TCP index
checks pass. Local compiler launchers were cleared after ccache children stalled;
the underlying stall mechanism was not isolated. Configure still emits existing
BIOS emitter-stamp warnings; generated BIOS/game C was not manually edited.
Final bounded pinned Release desktop capture completed two equal-cycle eye
manifests; VERIFY off is not restore proof. Build hashes and actual framework
root/revision are in VR_MOVEMENT_RECEIPT.json. Full traces/images remain ignored.
Real focus-loss/reconnection, other input schemes, controller buttons/poses/aiming,
head-relative movement and physical sensitivity calibration remain untested.

## 2026-10-02 - Touch combat actions and native gameplay controls

Framework ac6f84ba is pushed and pinned. It adds trigger/grip float actions and
Touch click actions to the existing opt-in offline source. Activity is independent
per action; stale/unfocused/failed samples release values. New TCP fields and
synthetic controls are in the existing OpenXR input handlers, with no diagnostic
printf path. UPSTREAM_PENDING.md inventories the exact framework changes.

Native slot-3 action table: required masks at 800B7790 are
[40,10,20,1,80,80,800,200,2] hexadecimal; forbidden masks at 800B77E0 are zero.
func_8007570C tests held input; func_80075778 additionally compares prior input
for a press edge. Their game button word at 80093B7C swaps native pad byte order.
The game-owned source now validates this table as well as the analog mapping;
other schemes decline to a neutral sample. Movement's curve/deadzone is unchanged.

Current controls: right trigger Cross/fire (also native menu confirm), right A
Square/use, right B Circle/next weapon (native back), left X Square/reload/use,
left Y Triangle/jump, left stick click L2/crouch toggle, right grip R2/native aim,
left Menu Start/pause. Analog actions press at 0.55; native edges/hold/release are
retained. Slot-3 weapon id3 is the visible grenade and fires on Cross release.
No controller pose or head-to-shot aiming is implemented.

**Correction recorded:** preliminary commentary and ignored native control labels
called Circle reload. That label was inferred too early from state/animation
changes and is retracted. Producer-bound controls switch equipped index1/id3 to
index0/id12 with Circle: SB writers 8007A960 at 800EEC80 and 8007DDFC at 800EEC81.
With the resulting visible firearm, Cross decrements clip8->7 at 800EEC92 through
SH pc8007DC24. Square reload restores clip7->8 through pc80046184 and decreases
reserve38->37 at 800EEC82 through pc800461A8. The partial-grenade reload control
could not establish this. Do not reuse the old ignored label as a finding.

Desktop synthetic controls in both Debug and Release pass weapon selection,
fire/reload writer assertions, crouch bit2 at SW pc8007ACD8, release, independent
action inactivity, synthetic focus loss, combined move/fire/turn and pause/resume.
Combined pad axes remain [241,7,243,128]. Source pause PNG shows PAUSED/objectives;
resume PNG shows the level again. Jump/use/native aim have verified pad routing;
an actual interaction target and a measured jump trajectory are separate tests.
These are synthetic controls, not new hardware measurements. Adjacent action/pad
queries are not atomic; actual guest writer records support gameplay claims.
Eye restore verification remains clean; counts and run provenance are in
VR_COMBAT_RECEIPT.json. Raw traces/screenshots stay in ignored analysis/vr-proof.

Compiled-out XR input and game mapping C tests, existing controller/render guard
checks, debug-less syntax and TCP index pass. OpenXR-enabled Debug/Release builds
pass. Rebuilt consumers are required for the enlarged sized input struct. Neither
this change nor its tests establish actual Touch binding acceptance, hardware
focus-loss/reconnect or headset menu visibility; these remain pending.

Aim investigation started with live disassembly: func_8007B350's event calls
8007D5C8 at 8007B488; id12's live jump-table entry at 8003D270 is 8007D770, which
calls 80045A78 at 8007D7C0, then 8004532C. Spawn code contains orientation logic,
but it must be tied to actual projectile/damage evidence before an aim hook is
selected. A muzzle/particle spawn is not damage-ray proof. No shot-direction
conclusion follows merely from changing the displayed camera.

Desktop verify-enabled sessions had 594 (Debug) and 328 (Release) restore checks,
zero mismatches/leaks/dropped stores. They also shed 4284 and 394 pairs respectively;
these verified runs are not a headset throughput benchmark. Initial functional
controls used the edited external framework checkout; the final pinned build is
separately recorded. Debug controls preceded the action-table guard, while
Release controls include that guard. No hardware action proof is inferred from
successful synthetic input or build completion.

Final Release launch uses clean submodule ac6f84ba, verified slot 3 and two complete
paired captures with source enabled. VERIFY is off for this smoke capture; the
earlier verify-enabled writer controls provide restore evidence. Launch/build/save
provenance is included in VR_COMBAT_RECEIPT.json. All owned games closed.

## 2026-10-02 - Quest combat acceptance and capture limits

The user tested the pushed Release build with Quest 3/VDXR and reports all
controls coming through. The pause menu appears too close. Right-grip native
aim is accepted as temporary until 6DoF weapon aiming replaces it; no shot-aim
implementation or direction claim follows from this acceptance. World scale
remains 3. Pause/menu placement is separate from world-scale calibration.

The first bounded launch ended before action sampling: the attempted capture
has zero rows and connection errors, so supplies no button evidence. The restart
verified slot 3 and actual XR submissions using the clean pinned Release binary
SHA256 5451506606fdd6c6bb453ed52facbb64c57cd388ffa29df26e44a9012ff16cd1.
The restart's startup snapshot records 50 submissions, zero failures; this is
startup evidence, not whole-session throughput or visibility proof.

The separate 55-second requested sample retained 246 adjacent action/pad rows:
synthetic=0 throughout, 44 focused rows with right squeeze=1.0 and pad FDFF (R2),
then 202 unfocused rows with neutral actions and pad FFFF. First/last frame
233/787; last focused frame329 and first unfocused frame332. Left/right active
click masks are 15/11 when focused. All captured click values and triggers are
zero: the other button presses were missed. User acceptance is distinct from
trace evidence; adjacent TCP queries are not atomic delivery proof. The physical
cause of focus change and reconnection were not controlled or established.

Trailing status queries encountered connection refusal after the owned process
closed. No final complete XR/performance/restore receipt exists for this sample.
The capture utility now saves trailing errors with its partial-capture status
instead of dropping out with only a traceback. Both owned launches have exited;
no game process remains. Compact samples/provenance and exact user feedback are
retained in VR_COMBAT_RECEIPT.json; bulk files remain ignored.

Next desktop work follows VR_COMBAT_PLAN.md: establish an actual shot-direction
producer before controller pose/weapon aim changes. The previously found spawn
is still a candidate, potentially a particle/muzzle effect. Wrist HUD remains
deferred; menu depth and independent headset cadence are still open.

Follow-up framework 26281aac is pushed and pinned: validation documentation only,
including UPSTREAM_PENDING.md; runtime source is unchanged from tested ac6f84ba.
The utility's partial-disconnect path passes a simulated closed-server check
(one retained synthetic row, errors saved, later diagnostics attempted, exit 1).
That check is tooling validation, not device evidence. Python syntax and diff
checks pass. No new game/runtime binary or guest behavior changes in this commit.

## 2026-10-02: native rifle shot and damage / tracked pose foundation

Slot 0 controls establish a moving id5110 shot actor, with constructor speed
800454C4, origin from player input+408/+412/+416 at 800450C8/CC/D0, angles
computed by 80044D18 and first movement stores 8006CC10/2C/48. Body turn changes
that trajectory. Native auto-aim uses input+420 target, so player angles alone
are not the shot producer. Slot 5 forward fire reduces enemy actor+244 at
8004ACC0 (6 -> 3.5); idle/turned-away controls leave it unchanged.
VR_WEAPON_AIM_RECEIPT.json records the exact writer and controls.

Correction: an unpublished ignored slot-0 results field was named
player_pitch_yaw_roll_hex, but read player+524 position, not +552 angles.
It is renamed player_position_q16_hex. No angle conclusion relies on it.
Early broad/truncated trace replies and empty function traces do not establish
absence of shot code; complete narrow address controls are used for damage.

Framework 734977c9 adds read-only grip/aim snapshots with shared eye predicted
time/recenter origin and age/validity, inverse pose math, synthetic TCP controls
and post-hoc producer-PC filtering. Tests and live synthetic controls pass;
no actual Quest poses or controller-to-shot override are established yet.
The game test is closed. Continue VR_WEAPON_AIM_PLAN.md.

## 2026-10-03: experimental controller-to-shot aiming

A guarded hook at native basis builder 8006A76C redirects player-owned id5110
shots from a fresh valid right aim pose. Native ammo/speed/collision/damage remain
game-owned. Full camera matrix inverse is required: the native level RT carries
scale, so transpose-as-inverse is invalid. Native entry snapshots are excluded
from eye replay and expire after four NTSC VBlanks; hand poses require <=150ms,
focus/activity and both valid bits. Invalid poses fall back to native aim.

Eight Debug synthetic controls passed: native, straight, +/-45deg aim, translated
origin, partial validity, unfocused and body turn. Rotation changes measured
native first-movement direction; origin translation changes constructor position;
NPC constructors have no pose override. Straight/translated controls damage the
near enemy while +/-45deg controls miss it (NPC return fire is recorded separately).
134 frozen-eye verify checks have zero restore mismatches/aborts/dropped stores.
Both pinned Debug and Release builds pass. See weapon plan/receipt.

Correction/refinement: input+64 was provisionally called a shot lifetime. Its
producer 80045124 computes distance/speed+1, but consumer 800444C8/D8 decrements
it and 800444F0 restores saved actor flags+20; it is a transition countdown,
not a proven destruction timer. That policy and long-range behavior remain open.
A too-narrow initial EC000..EF000 shot arena query missed variable allocations;
complete B0000..EF000 controls resolve the producer without absence claims.

This remains opt-in via -WeaponAimDiagnostic. The held rifle is still visually
native, and actual Quest hand alignment is untested. Inspecting the held model
uses the existing fn_filter/fn_entry_dump commands: arm fn_filter first; a zero
reply while inactive is not proof of no function calls. The root node path in
rifle slot 0 is 80084718 -> 80013AE4, RA 800847BC, render entity 800EEF3C.

## 2026-10-03: tracked rifle mesh and nested rollback correction

The opt-in -WeaponPoseDiagnostic now enables the render-only held-rifle mesh
and the existing guarded native shot override. Native animation vertices are
mapped after the node walk and before RTPS; inverse scaled weapon matrix W
and object-scoped H=133 compensation are required. Grip position and aim
orientation drive the rifle+arms. Provisional model units/meter 850 and pivot
(80,150,100) still need actual headset calibration; shots originate at aim
space, not a calibrated muzzle. Other weapons remain native.

Corrections recorded explicitly:
- Initial visual captures stayed native because registering an observer and
  filter separately for the same plugin/address rejects the second hook. One
  filter per address now calls the existing observer. Those first captures
  are excluded from tracked mesh evidence.
- Earlier asynchronous captures did not await a new complete pair and PNGs;
  they cannot prove a pose-specific image. check_weapon_pose.py now waits for
  newer manifests and complete decoding while holding the pose.
- A nested weapon watchdog initially passed guest restore and later pair
  recovery, but mod callback depth remained elevated and the next save load
  stayed pending. That was not complete rollback. Framework 6d44e21d fixes
  depth/plugin-owner checkpointing; docs-cleanup head 35b209d4 is pinned. Live
  repair reports mod entries +2 and a slot-5 generation 1 -> 2 reload. Tests
  also cover changed owner and nonzero outer context. UPSTREAM_PENDING updated.

Fresh final controls establish mesh translation/rotation/focus fallback, with
producer vertices and complete paired PNGs. A selected left-eye world region
has zero changed pixels out of 90,000; the rotated right-eye weapon overlaps
that rectangle, so its 14,336 differences do not isolate world geometry. Slot-5
mesh+shot test damages enemy 800B7B08 from 6 to 3.5 at SW8004ACC0. Eight aim
regressions pass; the final run has 192 restore checks/zero mismatch, one
intentional right-eye watchdog, one nesting repair and no dropped stores/leaks.
Subsequent slot load and pairs recover. Both SDK Debug/Release builds pass.
Compact evidence in VR_WEAPON_POSE_RECEIPT.json; raw files remain ignored.
Owned desktop game closed. Real Quest alignment/pose validity, muzzle offset,
range, weapon sizes and native animation acceptance remain open.


## 2026-10-03: calibration tooling, hardware alignment pending

Exposed existing rifle mesh scale/pivot inputs as launcher parameters without
changing defaults or rebuilding. The TCP collector now optionally retains
read-only grip/aim snapshots and trailing restore diagnostics. Locale checks
and a three-second desktop capture passed: 24 samples, zero query errors.
All sampled poses were inactive/unfocused/origin-invalid with predicted time
zero and age UINT32_MAX; synthetic=0 is not evidence of device tracking.
Verification was disabled (zero checks); no new restore proof is claimed.
Compact evidence: VR_WEAPON_CALIBRATION_CHECK.json. Owned game is closed.

Documentation clarification: older OpenXR overview sentences said controller
aiming was unimplemented. The prototype is implemented and desktop verified;
ordinary launches retain native aim, while Quest alignment remains untested.
Updated that overview explicitly. No framework source changes this checkpoint.


## 2026-10-03: first Quest tracked-rifle delivery check

User confirmed Virtual Desktop stream active. Launched the unchanged Release
binary with WeaponPoseDiagnostic, slot 5, WorldScale 3 and a 240-second bound;
no synthetic controls or injected faults. VDXR reported live submissions.
A 30-second capture retained 242 adjacent samples: every right grip/aim was
non-synthetic, focused, origin-valid, active and position/orientation-valid,
with age 0..29ms. Trigger reached 1.0. Native held-rifle RTPS RA80080F2C was
observed; a fresh complete paired capture shows the rifle. A large black
polygon is visible along the lower edge; cause/alignment remains unestablished.

Final snapshots: 6,805 XR submissions, zero XR failures; zero failed stereo
pairs, watchdogs or reported leaks. Verify was off (zero checks), so this is
not a new restore proof. Adjacent hand/producer/image requests do not prove
pose-to-mesh or pose-to-shot alignment. The enemy-health trace was armed after
health was already zero; its empty damage slice proves no controlled firing
result. No attribution of that earlier death is made.

The owned game is closed. Physical grip/pivot, size, barrel direction and
muzzle calibration still await user feedback. Compact receipt:
VR_WEAPON_QUEST_RECEIPT.json; full samples and paired PNGs remain ignored.
Framework runtime unchanged at 35b209d4; upstream inventory records actual
pose delivery separately from outstanding weapon alignment.


## 2026-10-03: rifle visual isolation and pause menu surface

User accepts the previous Quest grip size and alignment, while reporting a
super-garbled rifle and a pause menu too near the eyes. Keep WorldScale 3,
model scale 850 and pivot (80,150,100). Acceptance of grip alignment does not
establish native shot/barrel alignment or visual mesh correctness.

Native 80013DB0..80013DC0 output tracing identifies node 22 as the gun: 136
vertices, XYZ bounds [61..92,76..153,107..744]. Complete output slice contains
609 XYZ stores for 203 total vertices. The 28-byte GT3 bank has 280 faces;
174 reference node 22 in all three node bytes (23/25/27). Index bytes are
22/24/26. Other faces skin the original first-person arms/fingers. The
prototype now validates this exact asset and compacts the 174 gun faces inside
each eye sandbox, falling back to native drawing on any guard failure. Native
faces and asset RAM restore afterward. Original arms no longer move with the
controller. This is rifle-only policy, not a generalized weapon mesh decoder.

Uniform camera-coordinate/eye-translation scale 16 improves packed coordinate
precision without retuning physical scale/pivot. Isolated scale-1/16 controls
retain translation/rotation/focus fallback, but folding remains: this is not
a proven explanation or complete repair. A temporary two-sided branch bypass
at 800812A0 exposed more arm faces without fixing the rifle; removed. An
experimental authored-axis correction also failed and was removed. No edited
generated C or permanent guest instruction patch is included.

Raster experiment correction: the starting renderer already had geometry,
PGXP and texture correction disabled. Disabling them again is not a comparison
against an enabled baseline; the final so-called restore case enabled them.
That process is closed. Do not claim a controlled on/off PGXP exclusion from
those labels. A native image under the initial disabled settings is coherent;
tracked captures still show sharp stock/receiver shapes. A large lower black
polygon also persists after arm filtering; its producer/cause is not isolated.

Native pause flag 8009A61C changes 0->1 and 1->0 with recorded write-PC fields
80062760 and 8006407C. Pause eyes are exactly identical decoded images, unlike
the normal/resumed stereo pairs. Framework 04714837 adds a frame-local VIEW
quad API using the fresh pair's left image for both eyes. Native paused frame,
including its background, is a flat menu surface; gameplay resumes real stereo.
Only pause is measured; other menus and wrist HUD remain separate work.
Launcher defaults to distance 2m and width 2m (height 1.5m), with bounded knobs.

Desktop candidate passed 408 restore checks. A separate right-eye nested
weapon fault, after face compaction, yielded one watchdog/nesting repair and
zero mismatches across 932 final checks. Original header count is again 280;
slot-0 reload reached generation 2, pending=0, last_ok=1, and pairs recovered.
The saved fault snapshot has its own earlier check count; distinguish it from
the final console snapshot. Both pinned SDK Release/Debug builds pass, as do
strict off-XR tests, debug-less syntax, render guards and the 332-command index.

Quest startup correction: three tool-sandbox launches returned system -35 and
zero submissions despite the user's active stream. The otherwise identical
external launch succeeded. Do not attribute those failures to user readiness.
The bounded actual Quest test submitted 89 quads at 2m distance, 2 x 1.5m size,
then projection on resume; the late snapshot has 6,218 submissions and zero
XR failures. Sixty adjacent actual hand samples are focused/origin-valid,
non-synthetic, age 1..27ms. Verify was off in Quest: no new rollback proof.
Late trailing queries failed after the automatic bound closed the game; the
late snapshot is not a final whole-run total. The owned game is closed.
Headset assessment of remaining rifle garbling and menu comfort is pending;
actual barrel/shot alignment remains open. Compact evidence:
VR_WEAPON_VISUAL_RECEIPT.json. Raw captures/OBJ/plots remain ignored.


Headset acceptance follow-up: user reports "both the rifle and menu are fixed
now perfect" for this candidate. Keep rifle-only faces, precision scale 16,
menu distance/width 2m and the accepted physical calibration. This supersedes
the pending visual/comfort assessment above. Shot/barrel alignment and the
lower world polygon remain separate, unvalidated items. Owned game is closed.

Framework final pin 6134f8b8 adds the user acceptance documentation to the
04714837 API build; no subsequent runtime source changes. Both commits pushed.


## 2026-10-03: double-click VR launcher

Added root RunVR.bat wrapping vr/run_vr.ps1 with WeaponPoseDiagnostic enabled
and the existing accepted defaults. It boots normally, runs until game exit,
forwards optional arguments (-Slot 0/5, -Build), and keeps startup errors
visible. README records usage. A bounded -Desktop -Seconds 3 invocation from
the framework folder passed with exit 0; no owned game remains. This verifies
wrapper paths/argument forwarding/closure, not another headset check. No
framework changes or calibration changes.


## 2026-10-03: batch startup correction

User reports RunVR.bat loads flat. Original wrapper omitted the saved-state
entry used by successful headset runs. Native boot measured at guest frame
935: XR stage=requested, initialized=0, submitted=0, stereo attempts=0.
The prior three-second Desktop wrapper check established path/closure only;
it did not validate actual VR startup. Corrected batch passes QuickStart;
script selects slot 0 unless Slot is explicitly supplied, then the existing
loader checks live submissions. No gameplay/render/framework changes.

External bounded batch test from the framework folder loads slot 0 (generation
1, pending=0, last_ok=1). VDXR snapshot: running/tracking=1, submitted=551,
failures=0, layer=projection. Actual XR submission is measured; headset
visibility still requires user observation. This snapshot is not a whole-run
total. Intro/main menu remains outside the measured gameplay VR hook. Normal
double-click has no timeout; only this test uses a 45-second bound.
Compact receipt: VR_LAUNCH_RECEIPT.json; raw evidence remains ignored.

The bounded corrected test ended with exit 0; owned game is closed.


## 2026-10-03: normal VR boot and movie pacing (local, uncommitted)

User requested full normal boot without any save state and withdrawal of the
last two game commits. Removed 7ef7a20 and 15d9f09 from local/remote vr-dev with
force-with-lease, retaining their files locally. Current game HEAD is 4613331;
framework HEAD/pin remains 6134f8b8. No further commits or pushes until explicit
user instruction. The prior QuickStart/save-slot workaround described above is
superseded; RunVR.bat now starts normally with Slot=-1.

Framework native-surface API copies only fresh native presentation before host
OSD and tags successful XR sources. Game vblank/scene activity selects native
boot/menu/video versus genuine gameplay pair rendering. Four VBlanks without
the scene hook re-enable native mode; prolonged gameplay stalls remain an
activity-policy limitation to revisit. Menu input uses a table-independent
D-pad/Cross/Circle/Start map; gameplay controls retain measured calibration.

Correction: the first native attempt submitted zero layers because drawable
1856x1392 was passed into bounded PSX view math (stage=view_math). Changed only
unused projection dimensions to 512x240; the full drawable still feeds the quad.
Successful snapshot: 554 native quads, zero empty/failures, save generation 0,
last slot -1. Natural gameplay handoff: source 1, 4029 total submissions, zero
empty/failures. User confirmed "Visible and controls work" and all menus fixed.
These are inspections, not whole-run totals or new guest rollback verification.

User then reported laggy sound/video from intro through briefing. Controlled
native MDEC intervals, with turbo off and equal display dimensions per interval:
actual desktop swap interval 1: 25 intervals / 13.502s / 669 guest frames / 159
decodes = 49.547 guest Hz / 11.776 decode Hz. Actual interval 0: 28 intervals /
15.052s / 891 frames / 206 decodes = 59.195 / 13.686 Hz. Launcher now sets
PSX_VSYNC=0 only for XR; deadline guest speed cap is intact. User confirmed
"sound/framerate fixed". These adjacent TCP samples do not isolate audio
underruns or headset compositor cadence; movie decode rate is not guest VBlank.

Image trial: native 320x240 movie texels reconstructed with bicubic only during
recent MDEC/native surface. Source-owned real GL at 1x/4x passed 168 checks each,
including texel/orientation retention, unchanged canonical VRAM, GL bindings,
and diagonal smoothing. Live trial: 58.462 guest Hz / 13.736 decode Hz, actual
swap interval 0; filter activation 3 during video, -1 afterward. Inspection:
2914 submissions, 2892 native, zero failures, save generation 0. User preferred
the earlier presentation (little visible improvement). Removed the filter and
restored prior visuals, retaining the accepted pacing fix. Grain/compression
remains unresolved; do not infer decoder correctness from this screenshot.

SDK Debug/Release candidates, strict menu/XR input tests, render guards and TCP
index passed. Filter removal rebuilt for Release; final checks recorded in
VR_LAUNCH_RECEIPT.json. Framework edits listed in both UPSTREAM_PENDING.md files;
owned game closed. Raw traces remain ignored under analysis/vr-proof/.

Final checkpoint: both SDK Release and Debug rebuilt after filter removal.
Existing source-owned GL controls pass 157 checks each at 1x/4x. Both worktree
diff checks pass; six framework runtime/header mirrors match exactly. No game
process remains, and no new commits/pushes were made.

## 2026-10-03: authorized commit/push checkpoint

User explicitly authorized commit and push. Framework 5bafeebf committed and
pushed to fork/vr-dev, with native boot/menu surfaces, source metadata, actual
swap interval inspection, tests and UPSTREAM_PENDING.md inventory. Game
submodule now pins that commit cleanly; verified temporary runtime mirrors
were replaced by exactly matching committed sources. Reran game generation
with the real disc: 25 C shards, decls, ranges and dispatch unchanged. No new
headset claim; accepted earlier visuals and pacing retained.

Clean-pin checkpoint checks passed: Release build, strict menu input tests and
diff check. Earlier Debug/XR/GL checks remain applicable to identical runtime
source bytes. Framework submodule is clean; raw proof remains ignored.

## 2026-10-03: alpha follow-up backlog

User considers game 28b0c55 / framework 5bafeebf the alpha checkpoint. Added
VR_ALPHA_TODO.md combining current unresolved implementation/validation gaps
with new user reports: stationary world corruption/shake, missing nearby floor,
pop-in, headset color/contrast difference, Mission 1 ruins missing/transparent
tiles, wrist HUD placement, legacy aim removal, optional right-button remapping
and all-weapon testing. These reports do not establish root causes. Accepted
rifle/menu/locomotion/pacing behavior remains the baseline; no runtime changes
or new measurements were made for this documentation-only task.

Alpha backlog follow-up: added other-headset/OpenXR-runtime compatibility
coverage and a user-facing VR options menu. Native in-game integration versus
a long-grip overlay remains a design choice; no bindings or runtime behavior
changed. Settings persistence, input isolation and per-device checks included.

Alpha handoff: added VR_HANDOFF.md as the entry point for a new chat/agent,
with current revisions, doc reading order, accepted baseline, remaining work,
launch/debug gotchas and measurement/checkpoint rules. Reverse README links it.
Documentation only; no game run, runtime changes, commit or push.

User authorized committing and pushing the alpha backlog/handoff documentation.
This checkpoint includes those files and reverse-doc index/status updates only;
runtime and framework pin remain at the accepted alpha. Local handoff links and
Git whitespace checks pass.

## 2026-10-03 — v0.1.0 native alpha release preparation

User authorized pushing the game and publishing the 0.1.0 alpha, with upstream
framework PRs to follow. Portable RunVR.bat workflow now picks/remembers the CUE,
uses the packaged executable and performs normal startup inspection in PowerShell.
RunFlat.bat clears VR activation flags. VR mod support ships as experimental;
its previous developer-only feature would have been filtered out of public builds.

Framework 9976567e includes verified OpenBIOS stamp refresh (all generated BIOS C
unchanged) and literal .exe selection in the shared packager. Git Bash's unsuffixed
alias had skipped DLL/signing gates, producing a missing-zlib 0xC0000135 startup
failure under stripped PATH. Corrected packaging stages the import and passes the
real clean-dependency launch. Retail BIOS was not regenerated; releases select
OpenBIOS only. Required game generation leaves 25 C shards and dispatch unchanged.

Release build, render guards, 332-command index and 29 shared layout tests pass.
An extracted ZIP in a directory containing spaces boots through briefing to visible
Mission 1 with an empty cache and no developer tools on PATH. Bundled TCC produced
32 native images; flat XR-off/no-stereo state, save generation 1, load generation 2
and normal exit 0 verified. PowerShell's new TCP status reader was exercised against
the actual runtime. Prior black capture was a loading transition; later pixels show
the level. TCP may close before quit acknowledgement, so owned-process normal exit
was checked directly. These are bounded package checks, not cadence/full-playthrough
proof. No new hardware assessment; Quest 3/VDXR acceptance remains the earlier
gameplay baseline. Compact receipt: VR_ALPHA_RELEASE_RECEIPT.json; raw local results
and captures are ignored under analysis/alpha and dist. Owned games closed.


## 2026-10-05 - accepted headset color correction, master PR

The user accepted the brightness/contrast correction on Quest 3 / Virtual
Desktop VDXR and requested a PR into game master. The framework fix was extracted
as one commit, `3618bc00381588b7e9ea9b5173e8872470ed0379`, directly over the
released `9976567e` pin, then pushed to fork/fix/vr-headset-color-release before
updating this gitlink. Runtime color code matches the headset-tested candidate;
development GTE trace instrumentation and all game world experiments are excluded.

PSX source RGB is already display encoded. Prefer GL_SRGB8_ALPHA8; linear-only
XR targets receive an exact sRGB decode. Gameplay respects desktop post_gamma;
native GL_BACK is already gamma corrected and is not corrected twice. Alpha,
vertical orientation and incoming GL state are preserved. No global brightness
multiplier or guest timing, projection, simulation or generated-code change.

Validation: all eight real-GL combinations pass for gameplay and native copies
(two swapchain formats, two gamma values, incoming framebuffer-sRGB on/off),
including pixel tolerance <=1 LSB, flip/alpha and GL state restoration. Off-XR
input controls and render-pass guards pass. A fresh Release build uses this
master-based game worktree and the extracted framework worktree, with cached
SDL/OpenXR/UI/rbengine dependencies and OpenBIOS only. OpenBIOS passes the emitter
stamp check; game and BIOS generated sources are unchanged. The exact built
executable passes ordinary flat slot-0 load, 96 guest fingerprints, presented
readback and clean owned-process closure with XR and stereo disabled.

Separate native-menu perceptual acceptance and additional headset/runtime
combinations remain unverified; their source-owned GL paths passed. This is a
PR for the accepted fix, not a merge or new alpha release.


## 2026-10-05: jitter shelved; all-weapon tracking candidate

The user moved active work from jitter to weapons. Preserve the rebased jitter
experiment at game `fix/vr-jitter-tolerance` / `04624df`; the movement test's
distant-enemy flicker was better in the integer-view control. No jitter candidate
is accepted or included in v0.1.1. Weapon work starts from master `b3033cc` on
`feature/vr-all-weapon-tracking`, with framework `3618bc00`.

Copied-save native inventory covers slots 0?6 and 9: multiplayer 1?5, single-player
0/6/9. Ten weapon IDs are observed. `player+256` resolves the first-person model
in both modes; `input+84` is loadout index, `input+85` is weapon ID. The candidate
adds measured node/face/vertex profiles, retains animated gun parts, removes arms
inside eye rollback, and extends the shared single-player shot seam to validated
weapon/actor pairs. Local captured-RAM Ghidra analysis confirms native dispatch
and grenade launch behavior. Source saves remain untouched.

Strict OpenBIOS Release build passes. Compiled profiles match 60 captured native
meshes for all ten IDs and reject malformed indices. Synthetic mesh motion and
zero-mismatch restoration pass for rifle/Thompson/fragmentation grenade. Native
shot/first-motion controls pass for rifle/Thompson and delayed grenade release,
including unfocused fallback. Grenade synthetic samples must refresh throughout
the delayed launch; a one-time sample expires before release. No physical hit,
all-weapon gameplay, scoped-mode or headset acceptance claim follows from these
controls. Other seven live single-player paths and hardware calibration remain
open. See VR_WEAPON_TRACKING.md and VR_WEAPON_SET_RECEIPT.json for exact receipts.


## 2026-10-06: master refresh and ten-weapon batch controls

User requested controls for all weapons to batch-test later in the headset.
Fetched master `1b4f8ae` (v0.1.2) and rebased the local
`feature/vr-all-weapon-tracking` branch. Framework pin remains `3618bc00`; the
new master's 1080p launcher default is retained. Jitter stays shelved at
`04624df`, excluded from this candidate. No PR, merge, push or release requested.

Local Ghidra headless analysis of copied multiplayer RAM establishes the native
first-view chain, player-one owner `8009943C`, embedded first-person object
`input+2452`, transform/geometry producers and native projectile basis call.
Opt-in `PSX_VR_WEAPON_MP_CONTROL=1` registers the multiplayer hooks and reconstructs
player one's native 512x120 view per eye, excluding wait/flip, second view and
split-screen HUD. Native two-player simulation, weapon switching, ammo, reload,
spread, grenade release, flight/collision and damage code remain active. This
is a weapon test bench; multiplayer head-turn visibility/menu support and all
single-player asset variants are not established. Accepted single-player color
and head-frustum behavior remain the baseline.

All ten IDs have native/straight/translated/rotated/unfocused desktop mesh
controls and native/straight/45-degree/unfocused constructor/first-motion
controls. Shotgun controls include at least six native pellet constructors.
Tracked constructors write six pose fields; native/unfocused constructors write
none. Streaming filtered traces reject overwritten/truncated intervals and count
constructor instances rather than reused actor pointers. Raw failures caused by
truncated traces were not accepted. Single-player rifle/Thompson/fragmentation
grenade regression and a tracked right-eye watchdog/rollback recovery control
are included. Exact executable hashes and final measurements are recorded in
VR_WEAPON_CONTROLS_RECEIPT.json, with per-weapon accepted evidence paths.

Native firing was not observed from slot 3 MP40 or slot 5 Thompson during their
controls; both mesh controls passed. Causes remain unestablished. The batch uses
verified multiplayer slot 4 MP40 and single-player slot 6 Thompson, rather than
bypassing native fire logic. Source save hashes are unchanged. Earlier failed
multi-slot inventories are not blanket acceptance of their partial captures.

RunVRWeaponBatch.bat prepares all ten weapons, with movement enabled and 30
seconds per case. `-Mode compare` runs native then tracked; `-Weapon <key>`
selects one. Each case owns TCP 4372 and an isolated save copy. Input and hands
are neutral during preparation; overrides clear before live headset tracking.
Model readiness is checked exactly, and zero-ammo edits are verified after guest
frames. TCP reads can observe temporary compacted eye-model faces, and a write
serviced during a pass can roll back; bounded readiness/persistence checks avoid
accepting either as native state. Expensive restore verification is desktop-only
by default. The full launcher comparison exercises twenty synthetic desktop
cases; final acceptance and measurement details are in the compact receipt.

No new headset run was requested or launched. Per-weapon grip/size, physical
barrel alignment and damage, scoped behavior, recoil/reload, grenade/rocket
collision, tracking loss and comfortable gameplay remain hardware gates. The
weapon TODO stays unchecked. See VR_WEAPON_BATCH.md for the next user batch.
