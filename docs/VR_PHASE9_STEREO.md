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
