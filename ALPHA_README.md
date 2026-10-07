# Medal of Honor Recompiled — Native VR Alpha

This is a Windows x64 testing release. Flat play is the default; VR is optional.
Quest 3 through Virtual Desktop with VDXR is the tested headset/runtime setup;
SteamVR/OpenXR also renders correctly. Oculus / Meta Quest Link is untested.

v0.1.4 adds a barrel laser sight and one-stick aiming for the mounted machine
gun, and swaps the stick clicks (left clicks jump, right clicks crouch).
v0.1.3 added tracking profiles for all ten supplied weapons, accepted
visually in the Quest 3 / VDXR batch on 2026-10-06. `BUILD_INFO.json` identifies
the exact source commit and executable hash.

v0.1.1 corrects headset brightness/contrast and prevents head turns from losing
world tiles in the tested scene. Visibility now uses each eye's view to select
world geometry; eye FOV and the game's sector visibility remain unchanged.

## Install and play

1. Download the Windows x64 ZIP and extract the entire ZIP to a writable
   folder. Keep the executable, assets, mods and overlay toolchain together.
2. Supply your own **Medal of Honor SLUS-00974 (NTSC-U)** CUE/BIN image. The
   expected Track 01 size is 742,258,272 bytes, SHA-1
   `faebe91db711f804e1a6a38613c804b2c370f340`. No game data or retail BIOS dump
   is included. The package includes the redistributable OpenBIOS and its notice.
3. For **flat play**, run `RunFlat.bat` and select your disc in the launcher.
4. For **VR**, connect your headset and run the launcher for the runtime you
   want — the runtime is chosen by which launcher you run, and the system's
   active runtime is left unchanged. Select your CUE when asked; the BIN files
   stay beside your CUE in their existing location.

   ```powershell
   .\RunVR-VDXR.bat        # Virtual Desktop (VDXR)
   .\RunVR-SteamVR.bat     # SteamVR
   .\RunVR-Oculus.bat      # Meta Quest Link / Air Link
   ```

   VDXR and SteamVR are tested; **Oculus / Meta Quest Link is untested** — it
   needs the Quest connected through the Quest Link app. This build renders with
   OpenGL, so a Direct3D-only runtime (for example Windows Mixed Reality) will
   not start.

The launchers remember the disc beside the executable. Normal VR boot includes
videos, menus and briefing; gameplay switches to per-eye rendering. No test save,
Python installation or development tools are required. A bundled Python/TCC
toolchain handles streamed code from your disc automatically; initial compilation
can take time. Saves are stored in `saves/`; keep them when replacing a build.

For an explicit disc path:

```powershell
.\RunVR-VDXR.bat -DiscPath "C:\Games\Medal of Honor\medal-of-honor.cue"
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
| Right stick click | Toggle crouch |
| Left stick click | Jump |
| Left Menu button | Pause / Start |
| Right grip | Hold to show the barrel laser (ends on the surface it hits) |
| Left grip | Hold to aim a mounted machine gun with one stick |

Menus: left stick navigates; A or right trigger confirms; B goes back.

The barrel laser is a laser sight on the tracked weapon: **hold the right grip**
and the beam runs from the muzzle to whatever it hits, with a dot marking the
impact. It appears for firearms only — the passport and other non-firing items
show none. The muzzle alignment is provisional and needs a headset pass.

A mounted machine gun aims with a single stick: **hold the left grip** and one
hand covers both traverse and elevation. Let go and normal movement returns.

## Alpha limitations

- Tracked mesh and shot profiles cover ten supplied weapon types. Their visual
  tracking was accepted in the headset batch; physical damage alignment,
  scoped/special behavior and broader single-player assets need further validation.
- World shaking remains under investigation. Experimental precision/tolerance
  candidates are not included in this release.
- Missing nearby floor, pop-in and missing/transparent Mission 1 ruins tiles
  need separate validation. Visibility acceptance covers the tested scene;
  broader sector coverage and near-plane clipping remain open.
- Wrist HUD and an in-game VR options menu are pending. World scale is provisional.
- Headset cadence, focus/tracking loss, reconnect, broad mission coverage and
  Oculus / Meta Quest Link (untested) need more testing.
- Windows OpenGL is the alpha rendering target. This release does not advertise
  Vulkan or Linux/macOS VR support.

## Help test and contribute

Report issues at
[FractalEngineer/Medal-Of-Honor-PSX-Recomp](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/issues).
Include build version, headset/runtime, connection method, mission/location,
reproduction steps and optional captures. Do not attach disc images or BIOS dumps.

Start with normal boot, menu navigation, movement, rifle combat, pause/resume,
save/load and exit. Tell us whether the problem also occurs in flat mode.
The [alpha backlog](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/blob/master/docs/reverse/VR_ALPHA_TODO.md)
lists open tasks. Game-specific work belongs in the game repository; generic
framework changes are being prepared for separate upstream review.

`BUILD_INFO.json` records source revisions and the executable SHA-256. Dependency
licenses are in `licenses/` and `bios/OpenBIOS.LICENSE`; the framework uses
PolyForm Noncommercial 1.0.0.

Release verification: an extracted ZIP in a path containing spaces booted through
briefing into Mission 1 with an empty overlay cache and development tools removed
from PATH. The bundled TCC toolchain produced native overlays; flat save/load and
normal exit passed. Quest 3/VDXR acceptance covers the merged color and visibility
fixes, including movement-enabled visibility gameplay on 2026-10-05. Live headset
play was not repeated during packaging. These are bounded alpha
checks, not a full playthrough or validation of every weapon/runtime.
