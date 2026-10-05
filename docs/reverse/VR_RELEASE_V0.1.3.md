# v0.1.3 - Native VR Alpha

Windows x64 update with tracking profiles for all ten supplied weapon types:
M1 rifle, pistol, BAR, Thompson, MP40, shotgun, scoped rifle, bazooka,
fragmentation grenade and stick grenade. Their visual tracking controls were
accepted on Quest 3 through Virtual Desktop / VDXR.

Jump now uses **right-stick click**. Left-stick click toggles crouch; the sticks
continue to move and turn. This release retains the accepted headset color and
head-turn visibility fixes, plus the 1080p launcher default.

Download `moh-0.1.3-windows-x64.zip`, extract everything to a writable folder,
connect your OpenXR headset, and run `RunVR.bat`. Use `RunFlat.bat` for flat play.
Supply your own SLUS-00974 CUE/BIN image. Keep your previous `saves/` when updating.
No disc data or retail BIOS is included; bundled OpenBIOS and the overlay
compilation toolchain are included. No development tools are needed to play.

The ten-weapon headset controls include a multiplayer test bench; Thompson's
control uses single-player. Physical damage alignment, scoped/special behavior,
and broader single-player assets remain open. World jitter is still unresolved;
experimental precision/tolerance candidates remain excluded. Wrist HUD,
reconnect/focus behavior and broader mission/runtime coverage remain pending.

The user also accepted the rebuilt normal `RunVR.bat` target on 2026-10-06 after
an old local v0.1.0 executable was discovered. This package uses that rebuilt
Release/OpenXR/OpenGL configuration, with only the release version updated.
Framework pin: `3618bc00`. `BUILD_INFO.json` records exact source revisions and
executable SHA-256. This is an unsigned Windows alpha.

Validation covers the input regression and native jump writers, real-GL color
conversion, native/translated/rotated/unfocused tracked geometry, and MP40's
synthetic multiplayer control with zero rollback mismatches. The extracted
release package is checked for cold boot into Mission 1, bundled overlay
compilation with development tools removed from PATH, isolated save/load and
normal exit. These desktop checks do not constitute a new headset test of the
version-stamped release binary.
