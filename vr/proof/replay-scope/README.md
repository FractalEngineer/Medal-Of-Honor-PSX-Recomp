# No-op timeline, watchdog and draw-scope evidence

Recorded 2026-10-02, SLUS-00974, slot 1, OpenGL Debug, `--no-launcher`.
Framework pin `04b39513`; game base `c3dd4a3`. This commit contains the one-shot
watchdog probe. No framework runtime changes were needed for these measurements.

## Timeline comparison

All runs: `PSX_VR_PROBE=0`, `PSX_VR_INTERP=1`, `PSX_RENDER_PASS_VERIFY=1`.
Control: `PSX_VR_PASS_PROBE=0`; enabled: `PSX_VR_PASS_PROBE=1`.
The watchdog run also sets `PSX_VR_PASS_WATCHDOG=1` (unset otherwise).
Before each load, run:

```text
frame_fingerprint reset_on_load=1
savestate slot=1 op=load
savestate_status
```

Allow at least 96 guest frames, then save `frame_fingerprint count=96` and
`render_pass_stats`. `off_1`/`off_2` are two loads in the control process;
`on_1`/`on_2` are two loads in a new enabled process. Watchdog uses a third
process built with this commit's opt-in fault injection. All use the same saved
guest scene and no controller input changes. Fingerprints reset on successful
load, not on the asynchronously queued load request. First post-load host frame
labels differ; compare by ordinal after load and check consecutive labels.

```powershell
python vr/compare_frame_fingerprints.py vr/proof/replay-scope/fingerprint_off_1.json vr/proof/replay-scope/fingerprint_off_2.json vr/proof/replay-scope/fingerprint_on_1.json vr/proof/replay-scope/fingerprint_on_2.json vr/proof/replay-scope/fingerprint_watchdog.json --output vr/proof/replay-scope/fingerprint_comparison.json
```

All four comparisons match every measured column for 96 frames. Enabled stats
confirm real passes (238 and 842 respectively), not a disabled-probe comparison.
`stats_watchdog.json` records 1 abort/watchdog, 487 successes, 488 checks, zero
mismatches and no disable. The injection changes checkpointed state then charges
frozen cycles; it executes no guest draw. These are bounded no-op/synthetic-abort
results, not proof of stereo or nested draw rollback.

## Draw-scope reads

`*_calls.json` contain filtered `dirty_flow_log` entries (target ranges are
half-open). Disassembly JSON contains live words and decoded instructions.
`graphics_debug_strings.json` is `read_ram addr=0x800148D0 len=96` and confirms
DrawOTag/PutDrawEnv names at the actual producers. Failed/empty trace queries
were not used as evidence. See the appended correction in VR_PHASE9_STATUS.md.

Important candidates: frame constructor `0x800503D4`; setup `0x80050548`;
scene traversal `0x8006C714`; level list `0x80053C20`; level-group builder
`0x8008BF00`; vertex transformer `0x8008B3E8`; submission `0x800172D8`.
The enclosing constructor ends through a wait/flip/submission helper, so a
bounded replay must reconstruct the draw calls and submit without that helper.
This evidence set does not include a replayed draw image yet.
