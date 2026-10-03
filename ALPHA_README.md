# Medal of Honor Recompiled v0.1.0 — Native VR Alpha

This is a Windows x64 testing release. Flat play is the default; VR is optional.
Quest 3 through Virtual Desktop with VDXR is the tested headset/runtime setup.

## Install and play

1. Download `moh-0.1.0-windows-x64.zip` and extract the entire ZIP to a writable
   folder. Keep the executable, assets, mods and overlay toolchain together.
2. Supply your own **Medal of Honor SLUS-00974 (NTSC-U)** CUE/BIN image. The
   expected Track 01 size is 742,258,272 bytes, SHA-1
   `faebe91db711f804e1a6a38613c804b2c370f340`. No game data or retail BIOS dump
   is included. The package includes the redistributable OpenBIOS and its notice.
3. For **flat play**, run `RunFlat.bat` and select your disc in the launcher.
4. For **VR**, connect your headset, activate the intended OpenXR runtime
   (VDXR for the tested setup), and run `RunVR.bat`. Select your CUE when asked.
   The BIN files stay beside your CUE in their existing location.

The launchers remember the disc beside the executable. Normal VR boot includes
videos, menus and briefing; gameplay switches to per-eye rendering. No test save,
Python installation or development tools are required. A bundled Python/TCC
toolchain handles streamed code from your disc automatically; initial compilation
can take time. Saves are stored in `saves/`; keep them when replacing a build.

For an explicit disc path:

```powershell
.\RunVR.bat -DiscPath "C:\Games\Medal of Honor\medal-of-honor.cue"
```

Run until you close the game. If VR startup fails, check that the headset is
connected and the intended OpenXR runtime is active. Startup errors remain visible
in the console. These binaries are unsigned.

## Current Touch controls

| Control | Gameplay |
| --- | --- |
| Left stick | Move / strafe |
| Right stick | Smooth turn |
| Right trigger | Fire |
| A | Use |
| B | Cycle weapon |
| X | Reload / use, according to game context |
| Y | Jump |
| Left stick click | Toggle crouch |
| Left Menu button | Pause / Start |
| Right grip | Temporary legacy native aim binding |

Menus: left stick navigates; A or right trigger confirms; B goes back.

## Alpha limitations

- Tracked mesh and shot override currently cover the measured rifle only.
  Physical barrel-to-shot alignment and other weapons need validation.
- Reported world shaking, missing nearby floor, pop-in, missing/transparent
  Mission 1 ruins tiles and brighter headset color remain under investigation.
- Wrist HUD and an in-game VR options menu are pending. World scale is provisional.
- Headset cadence, focus/tracking loss, reconnect, other headsets/runtimes and
  broad mission coverage need more testing.
- Windows OpenGL is the alpha rendering target. This release does not advertise
  Vulkan or Linux/macOS VR support.

## Help test and contribute

Report issues at
[FractalEngineer/Medal-Of-Honor-PSX-Recomp](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/issues).
Include build version, headset/runtime, connection method, mission/location,
reproduction steps and optional captures. Do not attach disc images or BIOS dumps.

Start with normal boot, menu navigation, movement, rifle combat, pause/resume,
save/load and exit. Tell us whether the problem also occurs in flat mode.
The [alpha backlog](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/blob/v0.1.0/docs/reverse/VR_ALPHA_TODO.md)
lists open tasks. Game-specific work belongs in the game repository; generic
framework changes are being prepared for separate upstream review.

`BUILD_INFO.json` records source revisions and the executable SHA-256. Dependency
licenses are in `licenses/` and `bios/OpenBIOS.LICENSE`; the framework uses
PolyForm Noncommercial 1.0.0.

Release verification: an extracted ZIP in a path containing spaces booted through
briefing into Mission 1 with an empty overlay cache and development tools removed
from PATH. The bundled TCC toolchain produced native overlays; flat save/load and
normal exit passed. Quest 3/VDXR acceptance is from the earlier gameplay baseline;
live headset play was not rechecked during packaging. These are bounded alpha
checks, not a full playthrough or validation of every weapon/runtime.
