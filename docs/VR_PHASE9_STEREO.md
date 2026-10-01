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
