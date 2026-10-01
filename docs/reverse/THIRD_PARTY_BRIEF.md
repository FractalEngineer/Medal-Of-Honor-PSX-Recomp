# Third-party brief: MoH PS1 stereo/VR — injection point found, render pass blocked

You are picking up a live reverse-engineering effort. Everything below is
established by measurement unless explicitly flagged as unresolved. Please read
it as a peer brief and tell us how to proceed.

## 1. Goal

Add stereo (VR) rendering to **Medal of Honor** for PlayStation 1, running on a
static-recompilation framework. The requirement is a **genuine per-eye viewpoint
change with real parallax** — near geometry must separate more than far geometry.
A flat 2D screen-space shift ("3-D-TV" style) is explicitly *not* acceptable.

## 2. Repositories and environment

Two repos, both on branch `vr-dev`, both pushed:

- **Game / project repo**: `C:\Users\titan\Desktop\Github_Projects\Mine\Medal-Of-Honor-PSX-Recomp`
  (remote `https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp.git`).
  Contains the VR plugin (`vr/psx_vr_stereo.c`), `game.toml`, and the
  investigation docs under `docs/reverse/`.
- **Framework repo**: `C:\Users\titan\Desktop\Github_Projects\psxrecomp`
  (the recompiler/runtime, also on `vr-dev`). The game repo has a pinned
  `psxrecomp/` submodule.

Game: Medal of Honor, SLUS-00974 (NTSC-U). Disc at
`Input/medal-of-honor/medal-of-honor.cue`.

**Build:** `cmake --build build-debug --target psx-runtime` in the game repo.
**Regenerate game C:** `python psxrecomp/psxrecomp_cli.py generate --config game.toml --project-root . --disc Input/medal-of-honor/medal-of-honor.cue`

**Runtime tooling:** a JSON-over-TCP debug server on port **4370**, driven by
`tools/debug_client.py` (framework repo): `python tools/debug_client.py <cmd> k=v`.
Useful commands: `ping`, `frame`, `savestate slot=N op=load`, `read_ram`,
`disasm addr=0x... count=N`, `wtrace_range`/`wtrace_dump`, `gp1_dump`,
`gpu_state`, `video_info`, `gl_interp`, `render_pass_stats`, `render_pass_dump`,
`render_pass_refuse`.

**Two gotchas that cost real time:**
- Windowed runs **must** pass `--no-launcher`. Without it the binary opens a
  shared Dear ImGui launcher (`PSX_RECOMP_UI=ON`) and never boots the game.
  Headless runs use `--headless` and cannot run render passes at all.
- The `grep` tool is unreliable on absolute Windows paths in this environment;
  `git grep` from a repo root always works. `read_ram` responses are JSON with a
  `BOM` - read with `utf-8-sig`.

## 3. Established: the game's render pipeline

```text
FUN_8006d2e4 / FUN_8006d460          scene/entity render drivers
        v
FUN_800824d0                          per-MODEL render entry  (DYNAMIC OBJECTS ONLY)
        v
FUN_80082948                          render dispatcher (vtable on entity+0x54)
        v
FUN_800814c4 / FUN_80080dd4            geometry / primitive renderers
        v
FUN_80084718(entity, model, x, y)      per-entity transform setup
        v
FUN_80013ae4(node, matrix, out, ent)   recursive node transformer
```

Hooking `FUN_800824d0` over 1400+ calls across load -> idle -> strafe showed
**only three entities ever reach it**: `800EEF3C`, `800ECD94`, `800EC1DC`. That
set is disjoint from what `FUN_80084718` sees (`800BDAEC`, `800BB2D8`,
`800B8AC4`). So this whole branch renders **dynamic objects only**; the level
geometry goes elsewhere.

### The level path (the important one)

`FUN_8008B3E8` - prologue `ADDIU $sp,$sp,-288` at `0x8008B3E8`, ends
`0x8008BEF8`. It contains **twelve unrolled `RTPS` sites**:

```text
0x8008B5B8  0x8008B65C  0x8008B71C  0x8008B7B8  0x8008B84C  0x8008B8E8
0x8008BAD4  0x8008BB78  0x8008BC38  0x8008BCD4  0x8008BD68  0x8008BE04
```

Each is preceded by `LWC2 $zero, 0($t0)` / `LWC2 $at, 4($t0)` (vertex XY, Z from
`$t0`) and followed by `SWC2 $t6, 0($t1)` (projected SXY to `$t1`). It also writes
`H`:

```text
0x8008B4DC  CTC2 $s0, $H  <- ADDIU $s0,$zero,400    constant 400
0x8008B9D4  CTC2 $s1, $H  <- ADDIU $s1,$zero,400    constant 400
0x8008B9B0  CTC2 $v0, $H  <- LW    $v0, 8($v0)      from memory
0x8008BED4  CTC2 $v0, $H  <- LW    $v0, 8($v0)      from memory
```

Found by write-tracing the 41 KB per-frame region `0x800A0100-0x800AA200`
(26,217 writes); the dominant writers sit at `ra=0x8008BF84` / `ra=0x8008BF34`,
i.e. in this code.

`TR` at this function's entry was observed as `(-17893,-768,28854)` - the level
transform carries a large translation, unlike the dynamic-object path where `TR`
stayed `(0,0,0)`. Watch out: an earlier measurement of `H = 133` was a *sampled
GTE register* read at a probe point, not at the write site; the level path's own
value is 400. That mistake is documented as a retraction.

## 4. Established: the stereo injection point (verified)

`RTPS` computes `RT*V + TR` and only **then** perspective-divides. So adding a
per-eye delta to `TRX` (GTE control register 5) at the top of `FUN_8008B3E8`
shifts every level vertex in **camera space, before projection** - a real
viewpoint offset, not a screen shift.

Measured in the interior save, hooking `FUN_8008B3E8` and adding the delta:

| run | result |
|---|---|
| `TRX +200` | **75.58%** of pixels changed, **every** row band (rows 0-30 included) |
| weapon-only candidates (`FUN_80084718` etc.) | 10.89% changed, rows 0-120 exactly **zero** |
| `TRX +20` | per-band shift: top rows 2.5 px, bottom rows 6.5 px, band correlations 0.87-0.92 |

**Displacement grows toward the camera** - that is parallax. A flat 2D shift would
give one `dx` for every band. At `+200` the frame is not a translated copy of the
baseline, so the perspective genuinely changes.

Left/right eye captures (`TRX -24` / `TRX +24`) differ on 62% of pixels across
every row band. Proof images are committed at `vr/proof/` (`stereo_SBS.png`,
`stereo_anaglyph.png`, `eye_L.png`, `eye_R.png`). The anaglyph shows correct
depth ordering.

**Note:** `+/-24` is an uncalibrated first guess, not a tuned IPD. World scale has
not been established.

## 5. Established: the framework already has the double-render machinery

The framework documents a **render pass** system (`docs/RENDER_PASSES.md` in the
framework repo; API in `runtime/include/mod_plugins.h:276-350`):

```c
uint32_t psx_mod_render_pass_plan(uint32_t period_vblanks, uint32_t shown_after_vblanks,
                                  uint32_t *alpha_q16, uint32_t max);
int psx_mod_render_pass(struct CPUState *cpu, const PSXModRenderPass *pass,
                        PSXModRenderPassFn fn, void *user);
uint32_t psx_mod_render_pass_status(void);

typedef int (*PSXModRenderPassFn)(struct CPUState*, void*, uint32_t alpha_q16);
typedef struct PSXModRenderPass {
    uint32_t struct_size;   /* sizeof(PSXModRenderPass) */
    uint32_t alpha_q16;     /* a phase returned by the plan */
    uint16_t x, y, w, h;    /* VRAM display rect the pass draws */
} PSXModRenderPass;
```

It re-runs the game's own draw code with guest time frozen and restores CPU+GTE,
RAM, scratchpad, I-cache, interrupts, timers, DMA, GPU registers and the VRAM
rect **bit-for-bit**; it has an 8M-cycle watchdog with longjmp rollback and a
host-time budget with shedding. It was built for "true in-between frames" (frame
interpolation above the guest rate), not for stereo, but the redraw+restore half
is exactly what a per-eye render needs.

`psx_mod_render_pass_status()` values: `0 READY`, `1 NO_PRESENTER`, `2 BACKEND`,
`3 DISABLED`, `4 SESSION`, `5 FAST_FORWARD`, `6 BUSY`.

Gates: passes need the **OpenGL presenter**, **frame interpolation ON**, and the
**FLIP** source. Headless returns `NO_PRESENTER`. `render_pass_dump path=<dir>
count=<n>` writes PNGs of the game's own frame and each pass in phase order.

Also relevant: entry hooks fire for guest functions a pass itself calls, so the
`FUN_8008B3E8` `TRX` hook fires inside a pass too - the eye offset can simply be a
variable set before the pass call.

## 6. Established: the hook point, and a framework subtlety

The game is **double buffered and 30 Hz**: GP1 `0x05` alternates `0x05000000` /
`0x0503C000` (display origin y=0 / y=240) every 2 frames, all stores from
`pc=0x80018748`. Independently confirmed by `gl_interp` reporting
`flip_period: 2`.

The frame-loop tail:

```asm
0x8008B254  JAL 0x80016E38   a0 = 0     ; VSync(0)
0x8008B25C  JAL 0x80015DB8   a0 = 0     ; DrawSync(0)
0x8008B2A4  JAL 0x80017408              ; PutDispEnv
0x8008B2C4  JAL 0x800172D8              ; PutDrawEnv
0x8008B2D8  J   0x8008AED4              ; back to the loop head
```

Symbols: `VSync = 0x80016E38` (verified: reads the mode in `a0`, gates on a kernel
flag at 0x8003DF1A, tail-calls `(*(0x8003DF10)+60)(mode)` - the PsyQ wrapper),
`DrawSync = 0x80015DB8`, `PutDispEnv = 0x80017408`, loop head `0x8008AED4`.

Per `RENDER_PASSES.md`, passes are planned from "the entry of the `VSync(0)` that
precedes `PutDispEnv`". **28 call sites reach `VSync`**, so gating matters. The
plugin hooks `0x80016E38` and acts only when `cpu->gpr[4] == 0` (mode 0).

**Framework subtlety worth knowing:** the interpreted-entry hook path
(`dirty_ram_interp.c`) only fires for **overlay** code. `VSync` is main-EXE, so it
is statically recompiled and a plugin-only registration does nothing - the address
must be listed in `[recompiler] mod_function_entry_funcs` in `game.toml` and the
game C regenerated (`code_generator.cpp:2925` emits the hook). Current value:

```toml
mod_function_entry_funcs = ["0x80084718", "0x80016E38"]
```

An unresolved detail we no longer care about: the frame-loop `VSync(0)` call site
(`$ra` would be `0x8008B258`) did not appear among sampled `$ra` values, so the
earlier `$ra`-based gate was dropped in favour of `a0 == 0`, which works.

## 7. Current state: the presenter works, planning works, a pass is REFUSED

The plugin (`vr/psx_vr_stereo.c`, package `moh.vr.stereo`) is env-gated and inert
without env. Relevant vars: `PSX_VR_PROBE`, `PSX_VR_INTERP`, `PSX_VR_INTERP_HZ`,
`PSX_VR_RECT="x,y,w,h"`, `PSX_VR_RECT_ALT`, `PSX_VR_TARGET`, `PSX_VR_AXIS`,
`PSX_VR_OFFSET`.

With `--no-launcher` plus `PSX_VR_PROBE=1 PSX_VR_INTERP=1`:

```text
vr-interp: source(FLIP)=1 blend(HOLD)=1 rate(0)=1
gl_interp: enabled=1  source="flip"  flip_period=2  host_hz=60.0

vr-pass: mode0 VSync hits=1 status=6 plan=0     <- BUSY, first frame
vr-pass: mode0 VSync hits=2 status=0 plan=1     <- READY, plan SUCCEEDED
render_pass_stats: plans=2 planned=2 wanted=2 refused=0
```

So passes are **available** (`status 0`) and **planning succeeds**. But calling
`psx_mod_render_pass()` is refused:

```text
vr-pass-call: alpha=32768 rect=512x240+0+0    ret=0  status_after=0  runs=0
vr-pass-call: alpha=32768 rect=512x240+0+240  ret=0  status_after=0  runs=0
```

`ret=0` means "refused or rolled back"; `status_after=0` is READY; `runs=0` means
the pass body never executed.

### What has been ruled out

- **The status gate.** `status_after=0` is READY.
- **Argument validation.** `render_pass.c:494-497` requires non-null, `w/h != 0`,
  `0 < alpha_q16 < 65536`. Ours: `512x240`, `alpha = 32768`.
- **Rect range.** `x+w <= VRAM_W (1024)`, `y+h <= VRAM_H (512)` - both candidates
  are in range.
- **The double-buffer origin.** Tested `y=0` and `y=240`; both refused, so the
  flip-origin hypothesis is retired.
- **`gl_renderer_pass_ready()`.** It is
  `gl_renderer_pass_unavailable() == READY` (`gpu_gl_renderer.c:5880`), and status
  is 0, so it is true.

### Where it must be

Inside `gl_renderer_pass_begin` (`runtime/src/gpu_gl_renderer.c:6173`):

```c
if (!gl_renderer_pass_ready() || s_pass_active) return 0;            /* 6177 */
if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > VRAM_W || ...)     /* 6178 */
if (open_gen) { if (tw != s_interp_w || th != s_interp_h) return 0; } /* 6192 */
...
if (!pass_gen_reserve(gi, 1u, tw, th)) return 0;                     /* 6193 */
if (!pass_make_color_fbo(...)) ...
```

The size check at 6192 **looks satisfied on paper**: `tw = w * s_hr_scale`, and
`s_hr_scale` / `s_out_scale` are both 1 (confirmed in source at lines 344/345 and
via `video_info` reporting `hr_scale: 1, effective_scale: 1`), the display is
512 wide, and `game.toml` has `aspect_ratio = "4:3"` so the `wide` path should be
inactive. The presenter's captured size is `hiw_capture_size(w * s_out_scale, ...)`
= 512x240. So `tw == s_interp_w` should hold.

Relevant facts: `video_info` reports `display_x/y = 0/0`, `display_w/h = 512/240`,
`fbo_w/h = 1024/512`, `hr_scale = 1`, `effective_scale = 1`.
`gpu_state` reports `display 512x240 at (0,0)`, `hres1: 2, hres2: 0`.

That points at the **resource path past line 6206** (`pass_gen_reserve` /
FBO creation) - which a plugin cannot observe.

## 8. The question

**How should we proceed?**

Specifically:

1. **What is the most likely cause** of the refusal, given the above? If it is the
   `pass_gen_reserve` / FBO path, what commonly makes that fail?
2. **How should we get visibility?** The framework's convention is explicit:
   instrumentation goes in the TCP debug server, not printf - "If an inspection
   need isn't covered by the existing commands, do not fall back to printf or log
   files. Instead: add a handler in `runtime/src/debug_server.c`". We were about
   to add a refusal-reason field alongside the existing `refused` counter in
   `render_pass_stats`. Is that the right call, or is there an existing command or
   a better approach we have missed?
3. **Is there a simpler route to a first pass image** that we are overlooking -
   for example a different hook point, driving passes from somewhere other than
   `VSync`, or a way to exercise `render_pass_dump` that does not need our plugin
   to succeed?
4. **Given the API, is the pass system even the right vehicle for stereo?** A pass
   image is shown at a time *phase* between the game's frames; stereo needs two
   images at the *same* instant, tagged per eye. We suspect a new API sharing the
   existing save/restore internals is needed rather than reusing the
   in-between-frame contract. Is that right, and what would you do first?

## 9. Constraints and conventions to respect

- **Measurements must be real.** Several earlier conclusions were drawn from a
  value detached from its producer and had to be formally retracted (a sampled
  `H=133`; a screenshot-hash signal; a "turbo, uncapped" frame time where turbo
  had in fact reported `enabled: 0`). Please do not add to that pile - prefer a
  measurement that isolates the thing claimed.
- **Corrections are recorded, not quietly dropped.** Retractions are appended to
  `docs/reverse/VR_PHASE9_STATUS.md`.
- **Push after every commit.** No `Co-authored-by` / bot-attribution trailers.
- Docs live in `docs/reverse/`: `M1_VIEW_MATRIX.md`, `VR_HOOK_POINT.md`,
  `VR_DOUBLE_RENDER_SCOPE.md`, `VR_PHASE9_STATUS.md`.
