# VR reverse-engineering notes (`vr-dev`)

These notes ground `docs/VR_PLAN.md` in how **psxrecomp actually works**. The plan
was written generically; this directory records the real seams, so we do not
build a VR layer on wrong assumptions.

Game: **Medal of Honor, SLUS-00974 (USA)**.
Framework: psxrecomp master `3505f2a0` (pinned as the `psxrecomp/` submodule).

## The one fact that changes the plan

The plan assumes a modern renderer receives **world/model vertices** and can
re-project them per eye (its Phase 7 "Geometry Interception"). In psxrecomp that
is **not** where geometry exists.

- The PS1 has no camera/view/projection matrix hardware. The **guest** does all
  modelview x projection math on the **GTE**, in fixed point.
- The GTE emits **integer screen-space** `SXY`/`SZ`. The guest writes those into
  GP0 packets.
- psxrecomp's three renderers (Software / OpenGL / Vulkan) are **2-D VRAM
  rasterizers**. They never see a world-space vertex, and hold no camera matrix.

So the only place a camera-space vertex is visible to us is the **GTE
projection seam** (`runtime/src/gte.cpp`). That is where VR geometry
interception has to happen — a **framework** change, not a game-repo change.

## Go / No-Go verdict (plan's key checkpoint)

> Can we obtain stable world-space or camera-space geometry before final PS1
> screen projection?

**Partially — only at the GTE seam.** `gte_rtps_internal()` receives the
camera-space input vector and is the single funnel for all RTPS/RTPT. It can be
instrumented to emit world/camera-space vertices. The renderers cannot supply
them. This means true stereo depth requires intercepting the GTE, and the
"modern projection" work is a GTE-side capture plus a new framework render path —
**not** a swap of the existing renderer's matrices (there are none).

## Constraint: the VR engine work must live in a framework fork

A game repo can, without touching the framework:

- compile trusted plugins into the runtime target and register **VBlank /
  activation / function-entry / function-filter** hooks;
- read/write guest RAM and code (`psx_mod_read_*` / `write_*`);
- set display aspect, run host-timed render passes, drive controller policy;
- carry config (`game.toml` / `game_options.toml`), symbols (`symbols.toml`), and
  installable mod packages.

A game repo **cannot** add a new `psx_mod_*` service, a new TOML key, a new TCP
command, or any renderer/OpenXR behavior. All of that needs
`runtime/` + `recompiler/` changes → **fork psxrecomp** (origin is
`RetroPortingToolKit/psxrecomp`, not ours) or upstream PRs.

## Contents

- `render-pipeline.md` — the real data flow (GTE → GP0 → VRAM rasterizer →
  present), where widescreen already intercepts, and the seam index.
- `camera.md` — procedure to locate the camera/player/projection state at
  runtime, with the TCP debug commands to use.
- `functions.csv` — **pending live RE** (guest addresses for camera/GTE
  routines); not yet populated.

## Plan adaptations

| Plan phase | Reality in psxrecomp |
|---|---|
| 4 (Render abstraction) | Renderers are 2-D; a `RenderView` belongs on the **GTE capture path**, not the rasterizers. |
| 5 (Resolution decoupling) | Already true: renderers rasterize VRAM at arbitrary internal scale; display is mapped by `letterbox_rect`. |
| 6 (Aspect/FOV) | Widescreen already rewrites projection at the GTE (`gte_set_display_aspect`, squash) and widens guest cull sites. Reuse this. |
| 7 (Geometry interception) | **The crux.** Only possible at the GTE seam. |
| 8 (Primitive submission) | Already implemented (GP0 → `gr_*` → draw calls). Reuse; don't rebuild. |
| 9-14 (Stereo/OpenXR/HUD/cutscene) | Framework fork. HUD/2-D primitives (SPRT/TILE) are screen-space and must be classified out to a panel. |
