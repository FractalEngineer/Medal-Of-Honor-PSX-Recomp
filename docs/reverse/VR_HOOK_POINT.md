
## Phase 9 prerequisite: MoH's render-pass hook point is found

**2026-10-02 correction:** the VSync loop below is real but does not describe
the loaded slot 1 gameplay path. Successful legacy planning occurred with a
256-wide startup history. Slot 1 uses `FUN_80090B80`'s DrawSync(-1) wait and an
IRQ-driven PutDispEnv flip. The measured no-op probe now runs at that wait
function's main-thread entry and reads the upcoming DISPENV from the game's
buffer index. See [VR_PHASE9_STATUS.md](VR_PHASE9_STATUS.md) for evidence and
the corrections to the planning and `$ra` claims below.

`RENDER_PASSES.md` says to plan passes from "the entry of the `VSync(0)` that
precedes `PutDispEnv`". For MoH that is now pinned down exactly.

### Where it is

`gp1_dump` (the always-on GP1 ring) shows the display flip directly:

```text
GP1 0x05 writes, alternating:
   0x05000000   -> display origin y = 0
   0x0503C000   -> display origin y = 240   (0x3C000 >> 10 = 0xF0)
```

A **double-buffered 240-line display**, flipping every 2 frames - a 30 Hz game,
matching `FRAME_RATE.md`'s "a 30 Hz game (P = 2)". All 0x05 stores come from
`pc = 0x80018748`, with `ra` 0x8001748C / 0x800178A4 / 0x800178D0, i.e. from
inside `PutDispEnv`.

The main frame-loop tail, from a live disassembly:

```asm
0x8008B254  JAL 0x80016E38   a0 = 0     ; VSync(0)
0x8008B25C  JAL 0x80015DB8   a0 = 0     ; DrawSync(0)
0x8008B288  JAL 0x80017348              ; env build
0x8008B2A4  JAL 0x80017408   a0 = env    ; PutDispEnv
0x8008B2C4  JAL 0x800172D8              ; PutDrawEnv
0x8008B2D8  J   0x8008AED4              ; <- back to the loop head
```

So the loop head is `0x8008AED4` and the tail is this block.

### Function addresses

| Symbol | Address | Evidence |
|---|---|---|
| `PutDispEnv` | `0x80017408` | prologue `ADDIU $sp,$sp,-32`; the GTE-less caller of the GP1 writer at 0x80018748; called at 0x8008B2A4 |
| `VSync` | `0x80016E38` | reads mode in `a0`, gates on a kernel flag at 0x8003DF1A, tail-calls `(*(0x8003DF10)+60)(mode)` through the BIOS vector table - the PsyQ `VSync(mode)` wrapper |
| `DrawSync` | `0x80015DB8` | called with `a0 = 0` immediately after VSync |

Found by reading the whole 2 MiB of live RAM (`read_ram`) and scanning for the
JAL encoding of the target - the technique from `M1_VIEW_MATRIX.md`. Six call
sites for `PutDispEnv`: 0x80089D1C, 0x80089D44, 0x8008AD98, 0x8008B2A4,
0x80090CE4, 0x80090DCC.

### How to hook it selectively

Hooking `0x80016E38` on its own is too broad: **28 call sites** exist, most in
menus and loading code. But a function-entry hook receives the CPU state, so
`$ra` at entry is the return address, which names the exact call site:

```c
/* fires at VSync entry; only the frame loop's own VSync(0) qualifies */
if (cpu->gpr[4] == 0 && cpu->gpr[31] == 0x8008B258u) { plan and run passes }
```

`0x8008B258` is the instruction after the `JAL` at 0x8008B254. A second,
non-gameplay path calls VSync(0) at 0x8008B2E0 (return address 0x8008B2E4);
it does not do a PutDispEnv/PutDrawEnv pair and should not be used.

This is a precise gate rather than a heuristic, and it costs one register
compare per VSync.

### Still open

- The hook is unverified at run time: no pass has actually been executed. Per
  the gates, that needs a **presenter** (OpenGL, FLIP interpolation source) -
  headless returns `NO_PRESENTER`, and every test so far has run headless.
  Verification is `render_pass_dump` plus the band-correlation parallax test.
- Only the graphics half is addressed here. `VSync(0)` also blocks the guest
  clock, and a pass runs with guest time frozen, so pass placement relative to
  the flip needs checking once something can run.
## 2026-10-02 measured hook update

The earlier VSync/RA proposal is superseded. Gameplay replay and paired-eye
capture use the main-thread overlay entry `FUN_80090B80`, before its IRQ-flip
permission stores. The upcoming DISPENV rect is read using the measured inverted
buffer-index calculation. No `$ra` gate is used. VSync remains only a legacy
startup probe and requires a generated entry hook. The paired API needs neither
temporal planning nor interpolation. DrawOTag is `0x800172D8`; PutDrawEnv is
`0x80017348`, as corrected in VR_PHASE9_STATUS.md. See the scene replay and
paired-eye proof bundles for live measurements and exact controls.
