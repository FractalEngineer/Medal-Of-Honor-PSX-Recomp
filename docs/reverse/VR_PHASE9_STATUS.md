
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

