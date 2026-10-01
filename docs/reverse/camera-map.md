# M1 — camera map (progress + blocker)

Method: headless Debug build, driven over TCP 4370 (`gte_state`, `gte_frame_stats`,
`gte_ring_dump`, `wtrace_range`/`wtrace_dump`, `read_ram`). See `live-capture.md`.

## Reached gameplay-scale rendering

| t | frame | nproj/frame | scene |
|---|---|---|---|
| 0-40s | 13-2097 | 0 | boot / 2-D |
| 60-80s | 3741-6113 | 356 | **title/menu** |
| ~70-80s | 6821+ | **1266-1302** | **gameplay-scale 3-D** |

## Projection config observed

- `OFX = 256.0`, `OFY = 120.0` (16.16) → **512x240 display**; `H` = 400 (world) / 133 (secondary).

## Geometry submitters (RTPT callers in the ring)

| ra | n/512 | note |
|---|---|---|
| `0x80010D64` | 294 | dominant gameplay caller |
| `0x80080F2C` | 203 | secondary; `TR=[0,0,0]`, `H=133` |
| `0x8007C758`, `0x800110C0`, `0x8008BF84` | 1-12 | rare |
| `0x80053FF8` (+ `…F94/F0C/E84/E00`) | title | title/menu submit family |

## BLOCKER: the ring cannot give the camera

The GTE `RT` is the **combined model * view** matrix (object scale folded in), so
the ring cannot separate camera from per-object transforms. The camera must come
from the game's **own view matrix / camera struct in guest RAM**.

## wtrace (step 1) — matrix staging found

Armed scratchpad `0x1F800000-0x1F800400` + globals `0x8003A000-0x80060000`.

- **Every scratchpad write came from `ra = 0x80010D64`** (same routine as the
  dominant RTPT caller), code `pc = 0x8001121C … 0x8001133C`, `s3 = 0x1F800000`,
  `sp = 0x801FFEA0`.
- Writes cluster at `0x1F80001A, 0x22, 0x2A, 0x2C, 0x30, 0x34, 0x42` — packed
  pairs like `0x02CD03FF`=(717,1023) → a **PsyQ `MATRIX` staging slot around
  `0x1F80001A`**. `0x1F8003C0/3C2/3C4` = small counters.
- Globals `0x8003A000-0x80060000`: **no** writes → state is elsewhere.

## read_ram of the routine's live pointers (step 2)

The `wtrace` entries carried registers; reading where they point:

| reg | value | what it is |
|---|---|---|
| a0 | `0x80156FCC` | packed vertex-like data (16-bit pairs) |
| a2 | `0x80098D8C` | struct `{0, 30, ptr(0x8009898C), …, ptr(0x8013650C)@+48, 0x5070}` — **not** a matrix |
| a3 | `0x8009A7C8` | table of ascending pointers `0x0009A7CC, D0, D4, …` — not a matrix |
| s1 | `0x8013650C` | fixed-point value pairs (`0x00BFE326, 0x00682CF2, …`) — not a matrix |

So the scratchpad `0x1F80001A` slot remains the **only** matrix located so far.
The `a2` struct is the most promising to follow (`0x80098D8C → 0x8009898C`,
`+48 → 0x8013650C`).

## Conclusion / next

Dynamic-only RE has hit its practical limit: there is **no `disasm` command** on
the debug server, so we cannot see where `0x8001121C` sources its matrix.

Highest-leverage next step: **add a `disasm` TCP command** to our fork (the repo
already has `recompiler/src/mips_decoder.cpp` and the runtime already links
recompiler sources — `runtime.cmake:437-438`), then read `0x8001121C` to find the
matrix/camera source. That unblocks this and all later RE. (Ghidra out of band is
the alternative.)
