# Locating the camera / projection (procedure)

Goal: identify MOH's camera + player + projection state so it can be driven and
later replaced by an HMD pose. Nothing here has been executed yet — this is the
procedure for the first live RE pass.

## Prerequisite: a debug build (TCP debug server)

The runtime's TCP debug server is compiled only when `PSX_DEBUG_TOOLS` is ON
(`runtime.cmake:70`, defaults ON for Debug, OFF for Release). The MOH release
build (`build-release\`) is Release → **no debug server**. Build a debug tree:

```powershell
cd C:\Users\titan\Desktop\Github_Projects\Mine\Medal-Of-Honor-PSX-Recomp
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPSX_DEBUG_TOOLS=ON
cmake --build build-debug --target psx-runtime
.\build-debug\Medal_of_Honor__Recompiled.exe
```

Server listens on **port 4370** (native runtime). Client: `tools/debug_client.py`
in the framework (`psxrecomp/tools/debug_client.py`).

## Step 1 — Anchor the GTE projection seam

The projection numbers the game feeds the GTE (H / OFX / OFY) are set via CTC2
into `runtime/src/gte.cpp:1512`. Watch them live:

```text
gte_state            # current RT/TR/OFX/OFY/H and SXY/SZ fifos
gte_frame_stats      # per-frame projection activity
display_aspect, ws_aspect, ws_margin
```

Also watch the guest's camera-matrix / GTE config words. Candidate locations are
in guest RAM (main RAM 0x80000000+, scratchpad 0x1F800000):

```text
read_ram  addr=0x1F800000 len=1024      # scratchpad — libgte often puts work here
read_scratch
get_registers                            # a0..v1, ra around the camera updater
```

Use `wtrace_range` (per-frame write watch) to find which address the game writes
once per frame with plausible camera values while turning the view:

```text
wtrace_arm
wtrace_add lo=0x1F800000 hi=0x1F800400
wtrace_dump
```

## Step 2 — Find the camera rotate / translate calls

The guest's libgte wrappers (`RotTransPers`, `RTPT`, `ApplyMatrix`, `CompMatrix`
and the title's own `UpdateCamera`) are ordinary recompiled MIPS, not framework
symbols. Locate them by:

- `fntrace_arm` + `fntrace_dump` around a known frame to see hot function
  addresses during gameplay;
- correlate with `gte_frame_stats` to find the function that issues RTPS/RTPT;
- the boot EXE entry is `0x8001DFD4` (`game.toml`), text `0x2A000`.

Then label them in `symbols.toml` (`docs/SYMBOLS.md`) → `psx_symbols.h`.

## Step 3 — Separate camera from player

The plan needs camera and player identified independently (plan Phase 2 success
criteria). Heuristic: freeze one and vary the other.

- Move the player (input) with the camera code untouched → the player transform.
- Rotate the view (camera) with the player static → the camera transform.

Use `press` / `set_input` (e.g. analog look) and diff RAM with `read_ram` before
and after.

## Step 4 — Camera logging / debug overlay

Once the camera struct address is known, log position/rotation/projection each
frame. No per-frame host hook exists for game plugins (only per-emulation-VBlank);
the safe options are:

- a trusted plugin VBlank callback (`psx_mod_register_vblank_plugin`) that reads
  the known addresses via `psx_mod_read_word`; or
- a debug-build TCP poll (`tools/debug_client.py read_ram …` in a loop).

Both read the same guest addresses — the point is to convert RE findings into a
stable, labelled view of camera state.

## Step 5 — Free camera (plan Phase 3)

Proof that the camera can be detached: write the camera transform each frame from
a plugin VBlank callback (`psx_mod_write_word` / `..._code_word`) and confirm the
render follows while gameplay continues. This is the plan's M2 gate, and it is
achievable **game-side** (no framework change) because it only writes guest state.

Whether the *renderer* then needs the framework seam depends on Step 6.

## Step 6 — Go/No-Go: geometry before projection

See `render-pipeline.md`. Short version: world vertices are only observable at
`gte_rtps_internal` (`runtime/src/gte.cpp:804`). Confirming that, and capturing
vertex + transform there, is the framework fork's first task.

## Open items

- `functions.csv` — populate with confirmed guest addresses (pending Steps 1-3).
- No `[widescreen]` block exists in `game.toml` yet; MOH is still `4:3`.
- Disc is currently a single-track rip; multi-track gates (netplay) unaffected by
  VR work but noted.
