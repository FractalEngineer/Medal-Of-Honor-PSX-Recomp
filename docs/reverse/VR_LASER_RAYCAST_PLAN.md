# VR laser: raycast endpoint + impact dot

## Outcome (implemented)

The box cast proved too coarse — the sector boxes are 500-800 units (several
metres). A live-RAM Ghidra pass (`analysis/ghidra`, gitignored) mapped the real
geometry:

- `FUN_8008b3e8` is only a **box selector**: it runs the RTPS corner tests and
  writes the selected node pointers to a queue (`0x800aa1a0`, count
  `0x800aa19c`). It never touches vertices.
- `FUN_80089d68` is the drawer. A leaf node (`+0xc` = triangle count, `>= 1`)
  points at a 16-byte-per-triangle index list at `+0x18`; word0 = `[idx0|idx1]`,
  word1 low = `idx2`, indexing an 8-byte-per-vertex table (3 x int16 + pad).
- The level is **4 structs** at `0x80098d8c` (stride `0xb0`, count
  `0x80098984`), each owning a tree root (`*(*(struct+8)+0x14)`) and a vertex
  table (`*(struct+0x2c)`).

`vr/moh_vr_raycast.h` now casts those triangles (`moh_vr_ray_tree_tris`, with the
leaf boxes still pruning) so the beam ends on the exact surface; the box path is
kept as the `PSX_VR_LASER_TRI=0` fallback. Verified offline against the dump
(a leaf's 12 triangles decode to vertices inside its box) and live.

## Goal

Replace the fixed-length (`PSX_VR_LASER_RANGE_M`) beam with a real raycast: the beam
ends at the first level surface it hits and a dot marks the impact. Falls back to the
current fixed beam when nothing is hit.

## Findings that shape the design

- **No depth buffer.** `gpu_gl_renderer.c` disables `GL_DEPTH_TEST`; the eye pass is
  painter's-order only and primitives carry `z = 0`. No per-pixel Z exists.
- **No geometry/depth in the mod API.** `mod_plugins.h` has no per-primitive, depth, or
  Z getter; the GP0/GTE/census rings are debug-server only.
- **No callable hitscan.** MoH is projectile-based; `moh_vr_aim_shot` only produces an
  origin + direction. `docs/reverse/VR_WEAPON_TRACKING.md` warns against assuming firearm
  ray logic.
- **One usable geometry source: the level visibility AABB tree**, already walked by the
  frustum feature (`vr/moh_vr_frustum.h`, `docs/reverse/VR_HEAD_VISIBILITY_FIX.md`):
  - Measured SLUS-00974 node layout: bbox `int16` at `+0..+10` (min xyz at `+0/+2/+4`,
    max at `+6/+8/+10`), leaf flag/count at `+12`, PVS masks at `+16/+20`, children at
    `+24/+32/+28`. Root arrives in `$a0` at the selector `0x8008B3E8`.
  - Bounds are in the engine's camera-relative world space; `RT` (`gte_ctrl[0..4]`) and
    `TR` (`gte_ctrl[5..7]`) map that space to camera space. TR is **zero** on the level
    pass (camera-relative storage, `docs/reverse/M1_VIEW_MATRIX.md`).
- **We already hold everything needed at laser-draw time:** `g_camera_inverse` /
  `g_camera_tr` (cached in `vr_lvtx_entry`, the same pair the weapon path uses) and the
  eye's render view `g_eye_view[eye]` (`R_view = rotation_q12/4096`, `T_view`).

Chosen fidelity: **cast the AABB tree.** The dot lands on the nearest sector box; it can
sit slightly off irregular chunks. Triangle-exact would need the leaf geometry/vertex
format reversed first — a documented follow-up, not in scope.

## Files

- `docs/reverse/VR_LASER_RAYCAST_PLAN.md` — this plan, committed into the repo docs.
- `vr/moh_vr_raycast.h` — pure ray/AABB + BVH traversal (header-only, mirrors
  `moh_vr_frustum.h`; depends only on `psx_mod_read_*` + `math.h`).
- `vr/test_moh_vr_raycast.c` — strict-gcc source-owned fixture.
- `vr/psx_vr_stereo.c` — capture the tree root + camera RT; cast; draw endpoint + dot.
- `vr/run_vr.ps1` — pass-through for the new knobs.
- `ALPHA_README.md`, `docs/reverse/VR_WEAPON_TRACKING.md` — document the raycast + caveat.

## Implementation

### 0. Save this plan into the repo

Write this document to `docs/reverse/VR_LASER_RAYCAST_PLAN.md` before touching code, so
the design and its caveats live with the feature (matching the other `VR_*` plans).

### 1. `vr/moh_vr_raycast.h`

```c
/* Slab test for the int16 AABB, ray o + t*d (t >= 0). Writes the entry t. */
static inline int moh_vr_ray_box(const double o[3], const double d[3],
                                 const int16_t bounds[6],
                                 double tmin, double tmax, double *t_enter);
/* Nearest leaf hit along the ray. 1 on hit, 0 on miss. */
static inline int moh_vr_ray_tree(uint32_t root, const double o[3], const double d[3],
                                  const uint32_t masks[2], int bypass_masks,
                                  double tmax, double *t_hit, uint32_t *leaf);
```

- Reuse the exact node validation from `moh_vr_collect_tree`: `node & 3` free, offset
  bounds, `visited <= 8192`, stack `<= 256`, children offsets `{24,32,28}`, leaf when
  `psx_mod_read_word(node+12) > 0`, masks `+16/+20` unless `bypass_masks`.
- **Internal node:** skip subtree only when the ray misses the box; descend when hit
  *or* the origin is inside.
- **Leaf:** accept only a real entry (`t_enter > 1e-3`); skip leaves whose box contains
  the origin (that box is the player's own container, not a surface). Keep the smallest
  `t_enter` with `t_enter <= tmax`.
- Degenerate rays (`d[i] == 0`) use the inside/outside test per axis.

### 2. `vr/psx_vr_stereo.c` — capture

- Globals: `g_laser_ray=1`, `g_laser_dot=4` (px half-size), `g_level_root`,
  `g_level_root_frame`, `g_camera_rt[9]`.
- In `vr_cache_camera` store the un-inverted rotation too: `g_camera_rt[i] =
  (int16_t)(cpu->gte_ctrl[i/2] >> ((i%2)*16))/4096.0` (the matrix it already inverts
  into `g_camera_inverse`).
- In `vr_lvtx_entry`, capture the root once per frame: if `!g_level_root_frame` and
  `cpu->gpr[4]` passes the header checks (bounds ordered, child offsets in range, same
  validation as the traversal), set `g_level_root` and the latch. Reset the latch at the
  top of `vr_wait_entry` (frame start). The first valid capture in a frame is the
  outermost selector call = the root. If capture fails, raycast stays off (fallback).

### 3. `vr/psx_vr_stereo.c` — cast in `vr_laser_draw`

After `o` (muzzle, render-pose camera) and `dir` are computed, and after the existing
`moh_vr_weapon_shot_id` gate:

```
R = g_eye_view[g_stereo_eye].rotation_q12/4096; T = g_eye_view[g_stereo_eye].translation
gc = Rᵀ·(o - T); gd = Rᵀ·dir                      # render-pose -> guest camera
wo = g_camera_inverse·(gc - g_camera_tr)          # guest camera -> world
wd = normalize(g_camera_inverse·gd)

if (g_laser_ray && g_level_root &&
    moh_vr_ray_tree(g_level_root, wo, wd, NULL, 1,
                    g_laser_range_m*g_units_per_meter, &t, NULL)) {
    wh = wo + wd*t;                                # world hit
    gh = g_camera_rt·wh + g_camera_tr;             # world -> guest camera
    rh = R·gh + T;                                 # -> render-pose camera
    p1 = rh;                                       # beam end == dot centre
    have_dot = 1;
} else {
    p1 = o + dir*(g_laser_range_m*unit);           # existing fixed fallback
    have_dot = 0;
}
```

`R` is orthonormal so its inverse is its transpose; `g_camera_inverse`/`g_camera_tr`
are already the level pass's camera (the weapon path depends on the same pair). Then
project `p1` with the existing `vr_laser_project` + `vr_laser_clip` (unchanged).

### 4. `vr/psx_vr_stereo.c` — dot

When `have_dot && g_laser_dot >= 1`, emit a small solid quad centred on the hit's screen
point (reuse the same GP0 write style as the beam): `0x28` POLY_G4, four corners at
`±g_laser_dot` px, same bright-red colour (`0x0000ff`) at all four. Size it against the
clipped screen point so it never leaves the 11-bit range. The beam ends at the same
point, so head and dot coincide. No depth test — the dot can rarely show through nearer
geometry; accept.

### 5. Env + launcher

- `PSX_VR_LASER_RAY=0` — disable the cast (fixed beam, current behaviour).
- `PSX_VR_LASER_DOT=N` — dot half-size px (`0` = beam only).
- Validate both in the existing parse block (clamp dot 0..32). Add
  `PSX_VR_LASER_RAY='1'; PSX_VR_LASER_DOT='4'` to `$vrVariables` in `vr/run_vr.ps1`
  (after the `PSX_VR_LASER_*` region), with a matching optional switch if a diagnostic
  override is wanted.
- Extend the file-header env comment block.

### 6. Tuning aid (temporary)

Env-gated, off by default: `PSX_VR_LASER_RAY_DEBUG=1` writes a throttled
(`<= 400` lines) `analysis/laser_ray.txt` with `root`, `wo`, `wd`, `t`, `wh`, and the
hit/miss reason — the same shape that unblocked the first laser pass. Remove before the
final commit once the beam reads correctly.

## Verification

1. **Fixture:** add `vr/test_moh_vr_raycast.c` (stub `psx_mod_read_word/half` like
   `test_moh_vr_frustum.c`); cover forward hit, behind, parallel-inside, entry distance,
   nearest-leaf-wins, masks bypass, malformed node -> miss, range limit, origin-inside
   leaf skipped. Compile strict:
   `gcc -std=c11 -Wall -Wextra -Werror -I psxrecomp/runtime/include vr/test_moh_vr_raycast.c -lm -o /tmp/raycast_test && /tmp/raycast_test`
2. **Build:** kill any running game (the exe locks the link), then
   `cmake --build build-release --target psx-runtime`.
3. **Headset (VDXR launcher):** get into gameplay with a firearm, hold the right grip.
   Aim at a near wall, a far wall, the floor and an open doorway:
   - beam stops on the surface and a dot sits at the impact;
   - aiming at sky/nothing within range keeps the fixed beam with no dot;
   - dot tracks the gun, no flicker, both eyes coherent.
4. **Controls:** `PSX_VR_LASER_RAY=0` restores the fixed beam;
   `PSX_VR_LASER_DOT=0` suppresses the dot; a large dot confirms the size clamp.
5. **Regression:** vision/menu surfaces, tracked weapon, combat input and the existing
   laser colour/parity are unchanged; no new rollback/watchdog faults with the per-eye
   cast in the pass.

## Risks / notes

- **Root capture** across nested selector calls is the main unknown; the per-frame latch
  plus header validation makes it fall back safely, and the debug log confirms it live.
- **Box granularity** is a known, accepted limit: the dot is on the sector AABB, not the
  exact triangle. If it reads poorly on large chunks, the follow-up is to map the leaf's
  geometry pointer/vertex format and cast real triangles.
