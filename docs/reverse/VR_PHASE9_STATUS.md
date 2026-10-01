
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
