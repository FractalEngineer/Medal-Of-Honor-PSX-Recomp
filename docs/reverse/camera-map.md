# M1 — camera map (progress + blocker)

Method: headless Debug build, driven over TCP 4370 (`gte_state`, `gte_frame_stats`,
`gte_ring_dump`, `wtrace_range`/`wtrace_dump`). See `live-capture.md` for boot.

## Reached gameplay-scale rendering

| t | frame | nproj/frame | scene |
|---|---|---|---|
| 0-40s | 13-2097 | 0 | boot / 2-D |
| 60-80s | 3741-6113 | 356 | **title/menu** (small scene) |
| 80s+ | 6821+ | **1266-1302** | **gameplay-scale 3-D** |

## Projection config observed

- `OFX = 256.0`, `OFY = 120.0` (16.16) → **512x240 display**.
- `H` differs per path: 400 (world) vs 133 (secondary).

## Geometry submitters (RTPT callers in the ring)

| ra | n/512 | note |
|---|---|---|
| `0x80010D64` | 294 | dominant gameplay caller |
| `0x80080F2C` | 203 | secondary; `TR=[0,0,0]`, `H=133` |
| `0x8007C758`, `0x800110C0`, `0x8008BF84` | 1-12 | rare |
| `0x80053FF8` (+ `…F94/F0C/E84/E00`) | 500 at title | title/menu submit family |

## BLOCKER for the ring-only approach

The GTE `RT` is the **combined model * view** matrix (object scale folded in), so
the ring **cannot separate the camera from per-object transforms**. The camera
must come from the game's **own view matrix / camera struct in guest RAM**.

## wtrace findings (step 1 done)

Armed: scratchpad `0x1F800000-0x1F800400` + globals `0x8003A000-0x80060000`,
then rotated/moved the view.

- **Every scratchpad write came from `ra = 0x80010D64`** — the *same* routine as
  the dominant RTPT caller. Code at `pc = 0x8001121C … 0x8001133C`,
  `s3 = 0x1F800000` (scratchpad base), `sp = 0x801FFEA0`.
- Writes cluster at `0x1F80001A, 0x22, 0x2A, 0x2C, 0x30, 0x34, 0x42`
  (halfword/word), values are matrix-like packed pairs (e.g. `0x02CD03FF` =
  `(717, 1023)`, `0x011802AB` = `(280, 683)`) → a **PsyQ `MATRIX` staging slot in
  scratchpad around `0x1F80001A`**.
- `0x1F8003C0/3C2/3C4` = small changing counters (e.g. `0x315→0x316`).
- The globals window `0x8003A000-0x80060000` had **no retained writes** → game
  state is not in that range.

## Interpretation / next

- `0x80010D64` (code ~`0x800112xx`) is an object/geometry submission routine: it
  stages a matrix in scratchpad and issues RTPT. The scratchpad matrix is likely
  the **per-object** matrix, not the view.
- Confirm: `watch 0x1F80001A` while rotating the camera only vs animating an
  object only — the view matrix must change with the former and not the latter.
- To find the **view** matrix: widen the write window
  (`0x80060000-0x80100000`, and heap `0x80100000+`), or use `fntrace` to find the
  once-per-frame camera updater and then filter `wtrace_dump` by its `ra`.
- Label confirmed routines in `symbols.toml` (see `functions.csv`). No `disasm`
  on the debug server, so static confirmation needs Ghidra out of band.
