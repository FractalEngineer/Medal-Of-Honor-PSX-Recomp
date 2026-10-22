This release fixes the missing nearby floor and world shakiness during head movement, and carries the VR performance improvements into the normal Windows build.

- **Stable world and nearby floor:** precise per-eye poses and projective near-plane clipping preserve geometry close to the player. Accepted on Quest 3 / Virtual Desktop VDXR.
- **No overload black-frame shedding by default:** demanding scenes continue submitting complete eye pairs instead of alternating with empty submissions.
- **Faster production build:** heavyweight debug instrumentation is disabled. Matched headless slot-2 controls measured 23–27% higher guest throughput than the diagnostic build.
- Retains stereo checkpoint reuse, paired native-HUD capture, tracked weapons, hand-anchored HUD, spectator mirror, barrel laser and the existing controls.
- Normal VR startup works without the developer TCP server. Build metadata records the exact source revisions, executable hash and production feature flags.

The owner considers this build playable. Performance optimization continues: demanding scenes can still slow down, and these changes do not promise headset-refresh-rate rendering. The final performance/flicker checks were headless; earlier Quest 3 / VDXR acceptance covers the floor and stability fixes.

Extract the entire Windows ZIP and use `RunVR-VDXR.bat`, `RunVR-SteamVR.bat` or `RunVR-Oculus.bat`; use `RunFlat.bat` for ordinary flat play. Keep your existing `saves/` folder when updating. Older emulator savestates may be incompatible with the precision-enabled executable; memory-card saves are separate.

Supply your own Medal of Honor **SLUS-00974 (NTSC-U)** disc image. No game data or retail BIOS is included. See `ALPHA_README.md` for setup, controls and remaining alpha limitations.
