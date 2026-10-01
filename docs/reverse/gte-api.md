# GTE API + projection control (M1 / Phase 6)

## PsyQ libgte entry points (confirmed)

| addr | function | evidence |
|---|---|---|
| `0x8001BA98` | **`InitGeom`** | `ZSF3=341`, `ZSF4=256`, `H=1000`, `DQA=-4194`, `DQB=0x140`, `OFX=OFY=0` |
| `0x8001BA68` | **`SetGeomScreen(h)`** | `CTC2 $a0, $H ; JR $ra` |
| `0x8001BA78` | **`SetGeomOffset(ofx, ofy)`** | `SLL <<16` → `CTC2 $OFX/$OFY ; JR $ra` |

## Where MOH's `H` comes from

| addr | code | role |
|---|---|---|
| `0x80013764` | `ADDIU $t8, $zero, 400 ; CTC2 $t8, $H` | **world path — H hardcoded 400** |
| `0x80013A84` | `LW $t8, 8($v0)` → `CTC2 $t8,$H` | H from RAM `0x80096974` (not the world path) |

## Dead end (important): guest-code patches do NOT work here

Two attempts to change world FOV at the guest all failed:

1. **`write_ram` poke** of `0x80096974` persisted but never affected the ring's
   `H` — that site isn't the world projection.
2. **A trusted plugin** calling `psx_mod_write_code_word(0x80013764, …)` had **no
   effect**: `0x80013764` is inside **statically recompiled** code, where `400`
   is baked into the generated native C. A guest-RAM code write only changes what
   the *interpreter* would execute.

**Consequence:** projection changes must be made at the **GTE seam** (framework),
exactly like the existing widescreen squash. (The game-side plugin was reverted.)

## Phase 6 implemented — GTE FOV scale (framework), VERIFIED

`runtime/src/gte.cpp`:

- `extern "C" void gte_set_fov_scale(int num, int den)` — identity by default.
- Applied as `H * num / den` at the perspective divide in `gte_rtps_internal`
  (`gte_divide(gte_h_scaled(gte), SZ3, FLAG)`).
- `PSX_GTE_FOV_SCALE` = **FOV multiplier** (v>1 widens; maps to `(1000, v*1000)`).
- [video] fov_scale in game.toml sets the same value (default 1.0); env overrides.
- The GTE ring now records the **effective** (scaled) H, so the change is
  observable: `gte_ring_dump`.

**Verified live (headless):**

| `PSX_GTE_FOV_SCALE` | ring `H` values |
|---|---|
| unset (default) | `133, 400` |
| `2` (FOV ×2) | `200` (i.e. `400/2`) |

Fork commits: `82695b75` (scale), `5633e868` (ring effective H), `bbd01ccf`
(multiplier semantics — >1 widens).

This is the first real VR-facing behavior and it matches plan Phase 6
(`fov = 2·atan(w/(2H))`). It is a framework change on the `vr-dev` fork.

## Matrix (RT/TR) sites — view-matrix hunt (open)

75 `CTC2` matrix-load sites; identify the *view* by `watch`-ing which source
changes with camera rotation but not object animation. Clusters
`0x80013BE4–0x80014348`.

## Scan method (reusable)

`read_ram` the EXE text (`0x80010000`, `len=0x2A000`), match
`(w & 0xFE000000)==0x48000000 && ((w>>21)&0x1F)==6`; cop2 ctrl = `(w>>11)&0x1F`
(0-4 RT, 5-7 TR, 24 OFX, 25 OFY, 26 H). Script: `scan_ctc2.py`.
