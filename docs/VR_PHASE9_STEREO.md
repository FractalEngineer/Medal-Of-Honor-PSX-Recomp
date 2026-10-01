# Phase 9 — stereo (design + first step)

Stereo is the hard phase: the plan warns "do not build a large VR layer on top of
already-flattened 2D PS1 primitives."

## The constraint (measured, not assumed)

- psxrecomp's renderers are **2-D VRAM rasterizers**. They receive final integer
  screen coords; there is no per-eye projection to "set" on them.
- At the GTE, `RT` is the **combined model × view** matrix (verified), so the
  camera is not separable from the GTE ring alone.
- The GTE ring gives per-vertex `V/RT/TR` and `caller_ra`, but **not primitive
  assembly** (which vertices form a triangle, with UVs/colour). Reconstructing
  primitives from the ring is a large renderer project.

## Strategies

**A. Weak (rejected).** Render once, shift screen-space horizontally per eye.
No true parallax — it is a 3-D-TV effect, not VR. The plan explicitly warns
against it.

**B. Recommended — per-eye view offset at the GTE matrix, render twice.**

The insight: we do **not** need to reconstruct primitives. If we make the game's
own packet builder run twice, once per eye, with a per-eye **view-space
translation** (±IPD/2) folded into the matrix it loads into the GTE, then every
vertex the game projects is already correct for that eye — the game does the
per-eye projection for us.

Requirements:
1. **Find the view/model matrix source** — the 75 `CTC2` `RT`/`TR` load sites.
   The view matrix is the one whose source changes with **camera rotation** but
   not object animation.
2. **A framework mechanism to run the guest render path twice per game frame**
   (update once, render twice — plan Phase 9), with the per-eye offset injected,
   into two targets; present side-by-side for the monitor proof. This is the
   biggest piece and is a framework change.

## First steps

1. **Identify the view matrix** (needed regardless of presentation): `watch` the
   75 `CTC2` sites' source addresses while rotating the camera only, then while
   animating an object only; the view moves for the former and not the latter.
2. **Prototype double-render + per-eye offset**, presented side-by-side, and
   confirm parallax from a screenshot (near objects shift more than far ones).

## Risks / open questions

- If the game bakes the view into each object's matrix (likely), we need the
  view source to inject the offset *before* the per-model multiply — hence step 1.
- Running the render twice doubles GTE/GPU work; the frame-pacing story
  (plan Phase 17) comes later.
- Disc/cutscene/FMV paths must not be stereo-rendered (plan Phase 14/18).

## Status

Not started. Step 1 (view-matrix identification) is the immediate next task and
is pure RE — no framework change.

## Step 1 progress — matrix sourcing (disasm of CTC2 sites)

Around `0x80013BE4` the matrix is loaded from the **scratchpad base**
(`$s3 = 0x1F800000`):

```text
0x80013BD0  LW   $t4, 4($s3)
0x80013BE0  LW   $t5, 8($s3)
0x80013BE4  CTC2 $t3, $RT11RT12      # t3 loaded earlier from 0($s3)
0x80013BE8  CTC2 $t4, $RT13RT21
0x80013BEC  LW   $t6, 12($s3)
0x80013BF0  CTC2 $t5, $RT22RT23
0x80013BF4  LW   $t7, 16($s3)
0x80013BF8  CTC2 $t6, $RT31RT32
0x80013BFC  CTC2 $t7, $RT33
```

- So a **rotation matrix is staged at scratchpad `0x1F800000+0..16`** before
  being loaded into the GTE.
- `TRX/TRY/TRZ` are **computed**, not loaded: `MVMVA` → `MFC2 MAC1/2/3` →
  add `20/24/28($s3)` → shift → `CTC2 $TRX/TRY/TRZ` (`0x80013C98…0x80013CF0`).
- A second source is a **structure pointer** (`$a1`): `LW $t9,0($a1)`,
  `LW $v0,4($a1)`, `LW $t4,16($a1)` → another `CTC2 $RT11RT12…`.
- The `MVMVA` + MAC math here suggests this path is the **lighting** matrix, not
  the view.

### Next probe (decisive)

`watch 0x1F800000` (+4/+8/+12/+16) while **rotating the camera only**, then while
**animating an object only**. The view rotation staging should change for the
former and stay put for the latter. If it does, that scratchpad matrix is the
view (or the view×model staging) and is where the per-eye offset goes.

## Step 1 probe result — NEGATIVE (scratchpad is a general work area)

Read `0x1F800000..0x1F800014` at four points (idle, idle, camera-left, release):

```text
idle t0 : 5000801f c8b70980 dcd9a001 03000000 fa000000
idle t1 : 5000801f c8a70980 dcd9a001 03000000 fa000000
rotL t2 : 5000801f c8b70980 dcd9a001 03000000 fa000000
rel  t3 : a2d01f01 c242f8ff dcd9a001 1a4106fc ead56801
```

`idle t0` decodes to `{0x1F800050, 0x8009B7C8, 0x01A0D9DC, 3, 250}` — a
**pointer/work record**, not a rotation matrix. The contents swing arbitrarily
(identical while the camera rotates, then entirely different a moment later), so
`0x1F800000` is a **general transient staging area** each routine uses for its
own matrix/scratch — not a stable view matrix.

**Consequence:** the "watch the scratchpad matrix" probe does not identify the
view. The staged matrix belongs to whichever routine is running (often the
lighting/object matrix). Step 1 needs a different probe — e.g. `fntrace` to find
the once-per-frame camera updater and disassemble *its* call path — or accept the
view is baked into per-object matrices.

## Step 1 probe 2 — fn trace finds the once-per-frame function

Method: send `fn_filter` (this **activates** the global fn-entry ring —
`fn_stats.active` goes 0→1; without it the ring stays empty), wait, then
`fn_entry_dump count=2048`.

Results (2048 entries spanning 2 frames, ~165k entries over ~2 s):

| func | calls | note |
|---|---|---|
| **`0x800154EC`** | **exactly 1 per frame** (n=2, frames=2) | caller `ra=0x800178E4` / `0x8008D874` → prime candidate for the per-frame game/camera update |
| `0x80015DB8` | ~672/frame | likely the frame render/loop body |
| `0x80011214` | 355/frame | the per-primitive vertex routine (matches nproj≈356) ✓ |

## Next

`disasm addr=0x800154EC` → follow to the camera update and the view matrix it
builds. That matrix is where the per-eye offset is injected for stereo.

### CORRECTION — `0x800154EC` is `memcpy` (probe 2 false positive)

Disassembly of `0x800154EC`:

```text
0x800154EC  BEQ   $a0, $zero, 0x80015518
0x800154F0  ADDU  $v0, $zero, $zero
0x800154F4  BLEZ  $a2, $zero, 0x80015514
0x800154FC  LBU   $v0, 0($a1)
0x80015504  ADDIU $a2, $a2, -1
0x80015508  SB    $v0, 0($a0)
0x8001550C  BGTZ  $a2, $zero, 0x800154FC
```

That is a byte-copy loop = **`memcpy`**; the neighbours (`0x8001552C` PRNG,
`0x8001556C` GTE transform helper, `0x8001559C` packet builder) are library
routines. The "exactly once per frame" reading (n=2 over 2 frames) was
**coincidence**.

**Method limit:** at gameplay the fn ring produces ~165k entries / 2 s, so a
2048-entry dump spans only ~2 frames — per-frame statistics are meaningless.
`depth` is 0 for many functions here, so it is not a usable call-depth signal.

**Fix:** sample a quieter scene (title: `nproj≈356`/frame ⇒ far fewer calls per
frame ⇒ 2048 entries span many frames), or narrow the trace with
`fn_filter lo=… hi=…` to the world-render subtree, then re-run the once-per-frame
analysis.

## Step 1 probe 3 — title screen, 8-frame window (method works)

Same probe at the **title** (no input; `nproj≈356`): 2048 fn entries now span
**8 frames** (vs ~2 at gameplay), so per-frame statistics are meaningful.

Result: **47 once-per-frame candidates** — a per-frame *task system*, not a single
camera function. Highlights:

| func | calls | caller `ra` |
|---|---|---|
| `0x8001E3FC` | 1/frame | `0x80025DA0` |
| `0x80024274` → `0x800253B8` / `0x80025464` / `0x80025528` / `0x80025570` | each 1/frame | `0x80024274` (a once-per-frame task group) |
| `0x800223E8`, `0x80022E20`, `0x8002307C`, `0x80023D0C`, `0x80023D74` | 1/frame | various |
| `0x00002818`, `0x0000296C`, `0x00002970` (low RAM) | 1/frame | kernel/scheduler (BIOS RAM, not the game) |

**Still not identified:** which candidate is the camera/view update. The camera
must be separated from the task set.

### Next targeted probe

Cross-reference the candidate set with the **75 `CTC2` matrix sites**: the camera
update is the once-per-frame function on the path to an `RT`/`TR` `CTC2`. Or
`fntrace_arm target=<candidate>` to get its caller/callee chain and walk up to the
function that builds the view matrix.

## Step 1 probe 4 — world projection function found; caller trace blocked

**Found:** the world `CTC2 H` site (`0x80013764`) lives in the function at
**`0x80013698`** — prologue `ADDIU $sp, $sp, -400` with `SW $ra, 372($sp)`.
That function is the world projection setup.

**Caller trace blocked by tooling coverage:**

- `fntrace_arm 0x80013698` → registers (`armed: 1, targets:[0x80013698]`) but
  `fntrace_dump` records **nothing**. `fntrace_*` needs per-function entry hooks,
  which are not emitted for this address.
- `fn_entry_dump addr_lo=0x80013698 …` → **0 entries**: the global fn ring
  instruments only a subset of functions (≈62k of 31M `direct_seen` entries are
  logged), and this one is not covered.

**Tooling notes (learned the hard way):**
- `fntrace_arm` / `fntrace_dump` take **positional** args (`fntrace_arm 0xADDR`),
  not `key=value`.
- `fn_filter` must be sent first to **activate** the global fn ring.
- `disasm`, `read_ram`, `gte_ring_dump` take `key=value`.

## Recommendation — switch to static analysis

Dynamic tracing of the view-matrix caller is now blocked by instrumentation
coverage. The efficient path is **Ghidra** (CLAUDE.md's primary RE tool): load the
EXE at `0x80010000` and read `0x80013698` and its callers directly. The
alternative in-framework route is to add `0x80013698` to
`[recompiler] mod_function_entry_funcs`, regenerate, and re-run the trace — heavier
but fully local.

## Step 1 probe 5 — built xref tooling + OVERLAY discovery

**Ghidra is not installed here** (no JDK either), so it can't be used directly.
Instead we built the equivalent for call/pointer questions and **validated it**:

| tool (session scratchpad) | purpose |
|---|---|
| `jal_xref.py` / `xref2.py` | scan a dump for `J`/`JAL` to a target |
| `ptr_scan.py` | scan for literal pointer words == target |

Validation: `0x80011214` → exactly one caller `0x80010D5C` (matches the GTE ring's
`caller_ra` ✓); `memcpy` `0x800154EC` → 6 callers ✓.

### Discovery: the render path is largely OVERLAYS

`SetGeomScreen` (`0x8001BA68`) has a `JAL` caller at **`0x8005FAF0`** — an address
**above the declared text end** (`0x8003A000`, `text_size=0x2A000`). That is
**overlay code streamed from disc into RAM and executed at runtime**.

Consequences:
- A static scan of the EXE text **misses overlay callers**. Callers must be
  searched in **live RAM** (which holds the loaded overlays) — which is how the
  `0x8005FAF0` hit was found.
- Any VR render interception must account for overlay code. The framework already
  captures/sd shards overlays, so this is compatible — but it explains why several
  static probes came back empty.

### Still open

`0x80013698` (world projection) has **no** `J`/`JAL` caller and no pointer
reference in text or full data/BSS → most likely entered by **fall-through** from
an earlier entry, or via `jalr`. The guessed entry `0x80013698` may be
**mid-function**; the real entry is earlier.

## Recommendation

Our xref tooling now answers the call/pointer questions Ghidra would, locally and
for free. For genuine **decompilation and data-flow** (structs, `jalr` targets,
switch tables) Ghidra remains stronger — installing it (it needs a JDK) would pay
off if this RE continues.

## Step 1 probe 6 — SUPERSEDED

The "blocked / honest stall point" narrative that used to end this file is
retired. Three things it did not know:

1. **The pad is active-low** (`0xFFFF` idle, 0 bit = pressed), so the
   button-mashing sessions had been pressing everything at once including Start.
   See `reverse/LIVE_TESTING.md`.
2. **The camera path is reachable without capturing overlays.** A full live-RAM
   image imported into Ghidra at `0x80000000` analyzes main EXE *and* resident
   overlays together, with complete xrefs - retiring the overlay-capture
   bottleneck.
3. **Step 1 is answered**: there is no standalone view matrix. The view is
   composed upstream and stashed per entity; the pipeline is
   `FUN_800824d0 -> FUN_80082948 -> {FUN_800814c4 | FUN_80080dd4} ->
   FUN_80084718 -> FUN_80013AE4`. See `reverse/M1_VIEW_MATRIX.md`.

## Step 1 status — DONE

- No discrete view matrix exists to override; the premise this phase started
  from is retired.
- `FUN_80084718` is the per-entity transform builder and is the injection point.
- A **working function-entry hook** at `FUN_80084718` is built and firing
  (`vr/psx_vr_stereo.c`, package `moh.vr.stereo`), exposing the live entity
  table: `800EEF3C`, `800BDAEC`, `800BB2D8`, `800B8AC4`, with positions.
- It fires via the dirty-RAM interpreter, so overlay-resident addresses need no
  main-EXE regeneration.

## What step 2 needs

1. Identify which entity is the camera (the hook makes this a short experiment:
   watch the table across a pure turn).
2. **Render twice per frame** with the per-eye offset between the passes. This
   is the remaining framework change - the hook is the injection point, not the
   whole mechanism. `psx_mod_render_pass` runs guest drawing with state restored
   and is the closest existing primitive, but it is built for frame
   interpolation, not a second full eye pass.
