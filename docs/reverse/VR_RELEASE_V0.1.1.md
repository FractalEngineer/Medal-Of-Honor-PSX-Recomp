# v0.1.1 — Native VR Alpha

Windows x64 update with two accepted Quest 3 / Virtual Desktop VDXR fixes:

- Correct headset brightness and contrast to match flat rendering. The XR
  copy now handles sRGB/linear encoding correctly and preserves presentation gamma.
- Keep world tiles visible when turning your head in the tested scene. World
  geometry selection follows each eye's view, with unchanged eye FOV and native
  sector visibility. Movement-enabled headset gameplay was accepted.

Download `moh-0.1.1-windows-x64.zip`, extract everything to a writable folder,
then run `RunFlat.bat` or connect your OpenXR headset and run `RunVR.bat`.
Supply your own SLUS-00974 CUE/BIN image; no game data or retail BIOS is included.
Keep your previous `saves/` when updating. See `ALPHA_README.md` for setup and controls.

World jitter remains open. Experimental continuous-pose and one-pixel-tolerance
builds are excluded. Other weapons, wrist HUD, broader sector/mission coverage,
near-plane clipping and other headset/runtime combinations still need work.

The release builds the merged master fixes with framework `3618bc00`, bundled
OpenBIOS, Windows OpenGL and optional OpenXR. `BUILD_INFO.json` identifies the
source revisions and executable SHA-256. This is an unsigned testing release.
