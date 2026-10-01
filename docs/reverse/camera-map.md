# M1 — camera map (progress + blocker)

Method: headless Debug build, driven over TCP 4370 (`gte_state`, `gte_frame_stats`,
`gte_ring_dump`). See `live-capture.md` for the boot/commands.

## Reached gameplay-ish rendering

| t | frame | nproj/frame | scene |
|---|---|---|---|
| 0-40s | 13-2097 | 0 | boot / 2-D |
| 60-80s | 3741-6113 | 356 | **title/menu** (small scene) |
| 100-160s | 7426-9554 | 1301-1302 | **gameplay-scale 3-D** |

## Projection config observed

- `OFX = 0x01000000` → 256.0, `OFY = 0x00780000` → 120.0 (16.16) → **512x240 display**.
- `H` differs per submission path: 400 (world) vs 133 (secondary).

## Geometry submitters seen (RTPT callers in the ring)

| ra | n / 512 | note |
|---|---|---|
| `0x80010D64` | 294 | dominant gameplay caller; large-TR (view*model) cluster |
| `0x80080F2C` | 203 | secondary; `TR=[0,0,0]`, `H=133` |
| `0x8007C758`, `0x800110C0`, `0x8008BF84` | 1-12 | rare |
| `0x80053FF8` (+ `0x80053F94/F0C/E84/E00`) | 500 at title | sibling title/menu submit routines |

Example gameplay entry (`ra=0x80010D64` cluster):

```text
RT=[-1484,-2,-8072, -50,5128,8, 8056,82,-1480]  TR=[29280,-497,31618]
V0=[-10768,361,16846]  S0=[339,187]  SZ=[578,1268,4359]  H=400
```

## BLOCKER for the ring-only approach

The GTE `RT` is the **combined model * view** matrix — the game multiplies the
object's model matrix into `RT` before RTPS. The ring therefore **cannot
separate the camera from per-object transforms**; `RT` here has a non-unit scale
(~2.0) because object scale is folded in. Computing a "camera" from `RT/TR`
alone is invalid.

**Consequence:** the camera must be found as the game's own **view matrix /
camera struct in guest RAM**, not derived from the GTE ring.

## Next approaches (in order)

1. **`wtrace_range`** over the scratchpad (`0x1F800000..0x1F800400`) and a small
   main-RAM window while rotating the view, to catch the per-frame view-matrix
   write.
2. **`fntrace_arm`/`fntrace_dump`** to find the once-per-frame camera updater
   near the submitters above.
3. Once a candidate address exists, **`watch`** it: it must change with view
   rotation but not with object motion.
4. Label confirmed routines in `symbols.toml` (see `functions.csv`).

No `disasm` command exists on the debug server, so static confirmation needs
Ghidra (out of band).
