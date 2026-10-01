# GTE API + projection control (M1)

Identified by scanning the EXE text for `CTC2` (method below) and confirmed by
`disasm`.

## PsyQ libgte entry points (confirmed)

| addr | function | evidence |
|---|---|---|
| `0x8001BA98` | **`InitGeom`** | `ZSF3=341`, `ZSF4=256`, `H=1000`, `DQA=-4194`, `DQB=0x140`, `OFX=OFFY=0` |
| `0x8001BA68` | **`SetGeomScreen(h)`** | `CTC2 $a0, $H ; JR $ra` |
| `0x8001BA78` | **`SetGeomOffset(ofx, ofy)`** | `SLL $a0,16 ; SLL $a1,16 ; CTC2 $a0,$OFX ; CTC2 $a1,$OFY ; JR $ra` |

Game-side `H` writes (projection changes, not the library setter):
`0x80013768`, `0x80013A84`.

## Why this matters for VR (Phase 6)

`SetGeomScreen(H)` is the **projection-distance / FOV knob** the plan needs:

```text
fov = 2 * atan(screen_w / (2 * H_scaled))     # H is GTE's projection distance
```

Intercepting `SetGeomScreen` (or the game's `H` writes at `0x80013768` /
`0x80013A84`) gives arbitrary FOV / aspect control without touching the
renderer. This is framework-reusable and matches the existing widescreen
"squash at the GTE" approach.

## Matrix (RT/TR) sites — view-matrix hunt

The scan found **75 `CTC2` matrix-load sites**. The *view* matrix is one of them;
distinguish it by `watch`/`wtrace` — the view's source changes with **camera
rotation** but not with object animation. Clusters seen: `0x80013BE4–0x80014348`.

## Scan method (reusable)

```text
read_ram addr=0x80010000 len=0x2A000      # EXE text (load_address 0x80010000)
# CTC2 match: (w & 0xFE000000)==0x48000000 && ((w>>21)&0x1F)==6
#   cop2 ctrl = (w>>11)&0x1F : 0-4 RT, 5-7 TR, 24 OFX, 25 OFY, 26 H, 27 DQA, 28 DQB
```

Script: `scan_ctc2.py` (session scratchpad). Result: 102 CTC2 sites total — 75
matrix, 8 projection.
