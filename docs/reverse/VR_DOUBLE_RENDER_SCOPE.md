# VR: scope of the per-eye double render

**Headline: I was wrong that this is a framework change. The redraw-and-restore
machinery already exists, is documented, and is tested. What is genuinely
missing is the stereo *presentation* path, not the emulation plumbing.**

## What already exists: render passes

`docs/RENDER_PASSES.md` and `runtime/include/mod_plugins.h:276-350` define:

```c
uint32_t psx_mod_render_pass_plan(uint32_t period_vblanks,
                                  uint32_t shown_after_vblanks,
                                  uint32_t *alpha_q16, uint32_t max);
int psx_mod_render_pass(struct CPUState *cpu, const PSXModRenderPass *pass,
                        PSXModRenderPassFn fn, void *user);
uint32_t psx_mod_render_pass_status(void);

typedef int (*PSXModRenderPassFn)(struct CPUState*, void*, uint32_t alpha_q16);
```

It is built for "true in-between frames": it re-runs the game's own draw code
between two logic ticks, with guest time frozen, and restores everything
afterwards. The parts we would otherwise have had to build:

- **Re-execution of the game's draw code** - `fn` may write guest RAM and call
  guest functions with `psx_dispatch_call()`.
- **Full bit-for-bit restore** - CPU with the GTE, all main RAM at live size,
  scratchpad, I-cache tags, I_STAT/I_MASK, timers, DMA and GPU registers, the
  VRAM rect, the renderer's coherency bookkeeping, every clock value.
- **Runaway protection** - an 8 M guest-cycle watchdog rolls the pass back by
  longjmp, with the host nesting restored from a checkpoint.
- **Cost control** - a per-frame host-time budget with shedding, and a
  re-warm mechanism when an estimate goes stale.

Verified by real tests: `render_pass_plan_test`, `render_pass_freeze_test`,
`render_pass_sandbox_test`, `render_pass_abort_test`, plus `PSX_RENDER_PASS_VERIFY`
(hash CPU/RAM/scratchpad/timers/DMA/GPU before and after every pass) and a frame
fingerprint check that a run with passes matches one without.

## It composes with the injection point we found

`RENDER_PASSES.md` states that entry hooks "also fire for the guest functions a
pass itself calls, the plugin's own included". So the `FUN_8008B3E8` hook that
currently applies the TRX offset **will fire inside a pass**. The eye offset
becomes a plugin variable set before `psx_mod_render_pass()` is called - the
existing parallax mechanism plugs straight in.

## The real gap: presentation is time-multiplexed, stereo needs simultaneity

A pass image is shown at a **phase** (`alpha_q16`) - a point *between* the game's
frames, mapped onto host time during the VBlanks frame N stays on screen. The
whole design assumes one image visible at a time, chosen by a time deadline.

Stereo needs the opposite: **two images at the same instant**, tagged per eye,
delivered together (side-by-side, or one per eye to a headset). Nothing in the
framework does this. A search for `stereo`, `side-by-side`, `anaglyph` and
`openxr` across the sources and docs returns only audio and unrelated widescreen
hits - there is no stereo output path at all.

So rendering eye L and eye R as two passes would produce two images shown at
*different* phases: flicker, not stereo.

There is also a budget problem. Passes are budgeted at `PSX_RENDER_PASS_BUDGET`
percent (default 80) of the presenter's idle time, with shedding. Two eyes at a
VR cadence is a much larger ask than two in-between frames at 30 Hz, and the
shedding logic is designed to *drop* work under load - the opposite of what a
headset needs, where a dropped eye is worse than a dropped in-between frame.

## Practical obstacle: the current harness cannot run a pass at all

Passes require the OpenGL presenter with FLIP-source interpolation. The gates
return `NO_PRESENTER` (1) when that is not the case, and the Gates section lists
"when no VBlank was presented since the last plan (**headless**)" among the
conditions that make a plan return 0.

**Every test in this investigation so far has run `--headless`.** Nothing we have
built can exercise a render pass until we move to a windowed (or offscreen-GL)
session.

The compensating tool already exists:

```text
render_pass_dump path=<dir> count=<n>   (TCP)
```

which writes PNGs of the game's own frame and then each pass in phase order -
exactly the artifact needed to verify per-eye images, and measurable with the
same band-correlation parallax test already written.

## What is actually left, in cost order

1. **Find MoH's hook point** - the `VSync(0)` entry preceding `PutDispEnv`, i.e.
   after logic for frame N+1 but before the flip to frame N. RE in the game, the
   same shape of work as everything so far. Cheap, and it is a prerequisite for
   anything else.
2. **Move testing to a presenter** - a windowed OpenGL session, then use
   `render_pass_dump` to capture per-eye images. Modest, mostly harness work.
3. **Build the stereo presentation path** - the only genuinely new piece.
   Two sub-options:
   - *side-by-side composite* in the presenter: small, testable, gives the
     monitor proof and a path to any SBS-aware display;
   - *a real VR backend* (OpenXR): a new renderer, an order of magnitude larger,
     and it cannot reuse the pass scheduler's time-multiplexed model.
4. **Decide the pass model for VR.** For a headset the correct model is two eyes
   per output frame at the *same* guest tick, both presented - which is a
   different contract from "in-between frames". This probably means a new API
   alongside `psx_mod_render_pass` rather than a reuse of it, with the existing
   save/restore internals shared.

Steps 1 and 2 are cheap and independent of the big decision, and they produce
the artifact that makes step 3 testable. Step 3 is where the real cost is, and
step 4 is a design question that should be settled before writing step 3.

## Correction to an earlier note

An earlier summary of mine called the double render "the remaining structural
piece" and "a framework change". That was wrong: the structural piece exists.
The honest statement is that the *redraw* half is solved and reusable, and the
*output* half is missing.
