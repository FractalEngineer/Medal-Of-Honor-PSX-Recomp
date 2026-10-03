# Medal of Honor  Recompiled

<!-- retcomm-readme-metrics -->
[![GitHub downloads (all assets, all releases)](https://img.shields.io/github/downloads/FractalEngineer/Medal-Of-Honor-PSX-Recomp/total)](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/releases)
[![Alpha v0.1.0](https://img.shields.io/badge/alpha-v0.1.0-orange)](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/releases/tag/v0.1.0)
<!-- /retcomm-readme-metrics -->

<!-- retcomm-readme-boxart -->
<p align="center">
  <img src="launcher_assets/img/boxart.png" alt="Medal of Honor box art" width="280">
</p>
<!-- /retcomm-readme-boxart -->

Static recompilation of **Medal of Honor** built on
[psxrecomp](https://github.com/mstan/psxrecomp) and
[recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui).

Play the original game on a flat display, or opt into the experimental native
VR alpha with head tracking, per-eye rendering and tracked-rifle controls.
Flat play is the default; VR is enabled through a separate launcher.

The **v0.1.0 Windows native alpha** is available from this fork's
[releases](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/releases/tag/v0.1.0).
See [alpha installation and controls](ALPHA_README.md) before testing.

| | |
|---|---|
| Players | 2 |
| Region | USA |
| Publisher | - |
| Year | - |

Scaffolded with the New Project Layout. See
`psxrecomp/docs/GAME_PROJECT_SETUP.md` for the full flow.

<!-- retcomm-readme-launcher -->
## Retro Launcher

You can run this title **standalone** (download the release zip, point it at
your disc, play), or manage installs, updates, and disc/BIOS wiring with
**[Retro Launcher](https://github.com/RetroPortingToolKit/Retro-Launcher)** —
the Retro Compilation Manager hub for self-compiling recomps.

[Downloads](https://github.com/RetroPortingToolKit/Retro-Launcher/releases) ·
[Full README & features](https://github.com/RetroPortingToolKit/Retro-Launcher#readme)

<p align="center">
  <img src="https://raw.githubusercontent.com/RetroPortingToolKit/Retro-Launcher/main/docs/screenshots/hub-and-game-launcher.png" alt="Retro hub with a background build, next to a title’s recomp-ui launcher" width="720">
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/RetroPortingToolKit/Retro-Launcher/main/docs/screenshots/queue-and-background-build.png" alt="Background cmake build with titles queued" width="720">
</p>

Retro checks for updates, installs the prebuilt release zips, and automates
BIOS/ROM/save plumbing so you are not stuck repeating each game’s first run by hand.
<!-- /retcomm-readme-launcher -->

## Legal

You must own the original game. Disc images under `disc/` are gitignored and
must never be committed. Retail BIOS dumps are not redistributed and no C
derived from one may be committed; releases run on the bundled MIT OpenBIOS.

`generated/` (the recompiled game C) **is committed**: releases ship the
compiled game, built by CI from that tree. Regenerate and commit it whenever
seeds or the framework pin change.

Default app icon: `assets/psxrecomp.ico` (and `.png` / `.svg`) — Retro-themed controller mark from `psxrecomp/assets/`. Windows builds embed it via `APP_ICON`.

Optional box art under `launcher_assets/img/` may come from
[libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails)
(`Named_Boxarts`); see `BOXART_SOURCE.txt` when present.

## Flat play

Launch `Medal_of_Honor__Recompiled.exe` normally and select your own disc image
in the launcher. No headset is required. The supported disc is **SLUS-00974
(NTSC-U)**; game data is not included.
The Windows release also includes `RunFlat.bat`, which clears VR overrides.

For a local Windows build, launch from the project root in a fresh terminal:

```powershell
.\build-release\Medal_of_Honor__Recompiled.exe --game game.toml --disc "C:\Games\Medal of Honor\medal-of-honor.cue"
```

Replace the example disc path with your own. Add `--no-launcher` to boot
directly. OpenXR support can be compiled into the same executable while ordinary
launches retain flat rendering and normal controller input. VR environment
overrides should be absent for flat play; the VR wrapper restores its process
environment when it exits.

`RunVR.bat -Desktop` is a stereo diagnostic mode, not the ordinary flat launch.

## Experimental VR alpha (Windows)

The gameplay baseline is game `28b0c55` with framework `5bafeebf`; release
`v0.1.0` adds portable launchers, alpha documentation and packaging. Its framework
pin is `9976567e`, which refreshes the OpenBIOS generation stamp without changing
generated BIOS C and fixes Windows dependency packaging. The release links only
OpenBIOS.

### Setup and launch

For the local Quest 3 / VDXR build, connect Virtual Desktop and double-click
`RunVR.bat` in the project folder. It starts normally in VR: boot videos,
main menu and briefing use a comfortable native-screen surface, and gameplay
switches to genuine per-eye rendering. No save state is required. Left stick
navigates menus; A or right trigger confirms, B goes back. Accepted world scale,
tracked rifle, movement/combat controls and pause distance remain enabled.
It runs until you close the game; startup errors stay visible in the console.

For the extracted release, no development tools or Python installation are
required. `RunVR.bat` uses PowerShell, selects your CUE through a file picker and
remembers its location. Pass `-DiscPath "C:\Games\Medal of Honor\medal-of-honor.cue"`
to select a path explicitly. Local builds can still use the existing
`Input/medal-of-honor/medal-of-honor.cue` layout. Quest 3 through Virtual
Desktop with **VDXR** is the tested setup; other headsets, controllers and OpenXR
runtimes remain unverified.

Optional: `RunVR.bat -Slot 5` explicitly loads an existing local test save;
test saves are not included or required for normal boot.
`RunVR.bat -Build` rebuilds before normal boot. The default executable is
`build-release/Medal_of_Honor__Recompiled.exe`. VR disables desktop VSync to
avoid a second wait; the guest real-time speed cap remains active. The
`-DesktopVSyncDiagnostic` option restores VSync only for comparison.

Source-only diagnostic slot/capture options require Python and the development
helpers. They are not part of the packaged player workflow.

### Current Touch controls

| Control | Gameplay |
| --- | --- |
| Left stick | Move / strafe |
| Right stick | Smooth turn |
| Right trigger | Fire |
| A | Use (native Square action) |
| B | Cycle weapon |
| X | Reload / use, depending on native game context |
| Y | Jump |
| Left stick click | Toggle crouch |
| Left Menu button | Pause / Start |
| Right grip | Legacy native aim binding, retained temporarily |

In menus, use the left stick to navigate, A or right trigger to confirm, and B
to go back. Controller assignments are provisional.

### Known limitations and contributing

- Tracked weapon mesh and shot override are a **rifle prototype**. Other weapons
  and physical barrel-to-shot alignment need validation.
- Stationary world shaking, missing nearby floor, pop-in, missing/transparent
  Mission 1 ruins tiles and brighter headset color have been reported. Their
  causes are still under investigation.
- Wrist HUD and an in-game VR options menu are pending. World proportions are
  not physically calibrated, and headset cadence needs further measurement.
- Tracking loss/reconnect, other headset/runtime combinations and broader
  mission coverage need testing.

Report problems in this fork's
[issue tracker](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/issues).
Include the game/framework version, headset, OpenXR runtime, connection method,
mission/location, reproduction steps and any relevant captures. Do not attach
disc images or retail BIOS dumps.

See [the alpha backlog](docs/reverse/VR_ALPHA_TODO.md) for testing tasks and
[the VR handoff](docs/reverse/VR_HANDOFF.md) for development context.
Game-specific weapons, controls and HUD changes belong here; generic framework
work is tracked in [the upstream inventory](docs/UPSTREAM_PENDING.md).

## Development build

```bash
git submodule update --init --recursive
./psxrecomp/tools/ci/build_emitters.sh
python3 psxrecomp/psxrecomp_cli.py generate \
  --config game.toml --project-root . --disc disc/<your>.cue
git add generated && git commit -m "Regenerate game C"
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSXRECOMP_BIOS_STEMS=OpenBIOS
cmake --build build-release --target psx-runtime
```

For Windows VR, configure the Release build with `-DPSX_OPENXR=ON`, or use
`RunVR.bat -Build` after preparing the build tools and generated game C. Compiling
OpenXR support does not automatically enable VR when launching the executable.

Release `v0.1.0` uses the locally verified Windows alpha package. CI builds its
platform matrix without replacing that tested asset. Subsequent tags `vX.Y.Z`
(or the *Release builds* workflow) build the
committed `generated/` C on Linux, Windows and macOS and attach
`moh-<version>-<platform>.zip`, the compiled game. Locally:
`scripts/package_release.sh build-release linux-x64`.
Windows packaging requires OpenXR and TCP startup inspection to be enabled
(`-DPSX_OPENXR=ON -DPSX_DEBUG_TOOLS=ON`), and includes both launchers.

## Symbols

Progressive map: `symbols.toml` → `python3 tools/sync_symbols.py` →
`psx_symbols.h` (`PSX_FN_*`). See `psxrecomp/docs/SYMBOLS.md`.

## Framework pins

Submodule gitlinks (`psxrecomp`, optional `recomp-ui`, nested `recomp-net`)
are authoritative. `framework_pins.txt` is an optional scaffold snapshot;
release CI logs SHAs with `record_pins.sh` but builds whatever the gitlinks
resolve to. Bump submodules deliberately — do not float on `main`/`master`
in release CI.

<!-- retcomm-readme-raid -->
---

<p align="center">
  <sub><b>R.A.I.D. — Retro AI Development</b> · a Discord for AI-assisted retro reverse-engineering, decomp &amp; recomp</sub>
</p>

<p align="center">
  <a href="https://discord.gg/Ad9BwSzctP"><img src=".github/raid-discord.png" alt="Join the Retro AI Development (R.A.I.D.) Discord" width="200"></a>
</p>
<!-- /retcomm-readme-raid -->
