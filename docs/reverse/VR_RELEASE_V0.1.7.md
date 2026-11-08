# v0.1.7 - Full 6dof VR + Mouse/Keyboard Alpha

Adds mouse and keyboard campaign gameplay alongside the existing optional VR
mode in the Windows x64 package.

- Direct mouse aiming and WASD movement, including movement while scoped.
- Editable gameplay and menu bindings, mouse buttons and mouse-wheel weapon
  cycling. R reloads; menu defaults are E to choose and Q to go back.
- Independent 1-10 global and aiming sensitivity sliders, defaults 5 and 4.
- Smooth aiming zoom from 1.00x to 2.50x, default 1.25x. Firearms and grenades
  retain their held weapon; the sniper retains its native scope.
- Mouse aiming for mounted machine guns, including horizontal traversal.
- Existing optional OpenXR VR and runtime launchers remain available.

Download `moh-0.1.7-windows-x64.zip` and extract the whole archive. Use
`RunFlat.bat` for flat play, then enable **Mouse and keyboard** in **Mods**.
Connect a headset and use `RunVR-VDXR.bat`, `RunVR-SteamVR.bat` or
`RunVR-Oculus.bat` for VR. Mouse and keyboard defaults off and targets local
single-player campaign play.

Supply your own **SLUS-00974 (NTSC-U)** CUE/BIN image. No game data or retail
BIOS is included. Keep your existing `saves/` folder when updating; memory-card
saves are separate from executable-specific savestates. See
[ALPHA_README.md](../../ALPHA_README.md) for setup and remaining VR limitations.

This remains a Windows alpha. VR performance, broader weapon and mission
coverage, and headset/runtime compatibility need further playtesting. The
earlier v0.1.6 VR build was accepted on Quest 3 with Virtual Desktop VDXR;
this release has not had a separate physical-headset acceptance test.
