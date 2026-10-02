# Bounded guest scene replay

## Retention update: 2026-10-02

The draw, clear-only and level-only PNGs were removed. Raw stats, timeline fingerprints, image-comparison receipts and reproduction controls remain. Commands below that inspect PNGs require the historical images or a fresh capture; fingerprint comparisons still use retained JSON.

The complete pre-cleanup image bundle is in game commit `231650447bdc219629559a16f0dd7d229ee1ce49`.
See [the proof index](../README.md). The run descriptions below record the original measurements.

2026-10-02, SLUS-00974 slot 1, OpenGL Debug, framework runtime `04b39513`,
game base `93be7f2` plus the draw/coverage changes committed with this bundle.
All launches use `--no-launcher --game game.toml --disc
Input/medal-of-honor/medal-of-honor.cue` from the game root.

Common environment: `PSX_VR_PROBE=0`, `PSX_VR_PASS_PROBE=1`,
`PSX_VR_INTERP=1`, `PSX_RENDER_PASS_VERIFY=1`; no entity/TRX offset.
Use a fresh process for each control:

| Control | PASS_DRAW | PASS_WATCHDOG | Saved results |
| --- | --- | --- | --- |
| Full bounded scene | 1 | 0 | `draw/`, `draw_stats.json`, `fingerprint_draw.json` |
| Clear only | 2 | 0 | `clear/`, `clear_stats.json` |
| Level callback only | 3 | 0 | `level/`, `level_stats.json` |
| Abort inside level dispatch, then full scene recovery | 1 | 2 | `nested_watchdog_stats.json`, `fingerprint_nested_watchdog.json` |

Environment names in the table have prefix `PSX_VR_`.
Before loading slot 1, arm `render_pass_dump path=<absolute-dir> count=2`
(count=1 for clear/level), then `frame_fingerprint reset_on_load=1` for timeline
runs and `savestate slot=1 op=load`. Check `savestate_status`, allow at least
96 guest frames, and collect `render_pass_stats` / `frame_fingerprint count=96`.
All observations are through TCP; no new printf instrumentation is used.

## What ran

At the proven pre-wait hook the callback sets the game's current drawing
environment and clears the complete capture rect to black. It reconstructs the
calls before `0x8005047C` in the game's frame constructor: setup, conditional
scene helpers, ordering-table links/text, entity scene traversal and post-scene
packets. It then submits the rebuilt 512-entry OT with DrawOTag, without calling
the wait/flip helper. Each guest call uses dispatch's return-address contract.

The full captured image rebuilds the room, weapon, compass and ammo counter.
Both baseline/replay pairs match all decoded RGB pixels. Clear-only captures
are entirely black, establishing that a capture uses the modified rect rather
than retaining baseline pixels. The level-only control resolves its entity by
the callback pointer in the current scene list; it retains the room and compass
but omits the weapon/ammo counter. It differs on 11,095 pixels in this capture.
This is a coverage control, not a stereo parallax measurement.

TCP full-draw receipt: 87 successful passes/checks, zero mismatches, faults,
leaks or dropped device stores. Last sampled guest work: 389,779 cycles;
last pass 14.340 ms, EMA 16.147 ms, including verification on this Debug run.
These sampled timings are not a throughput benchmark or a stereo budget.

The nested test injects once at the level-transform entry while a pass's real
guest dispatch is active. Receipt: 1 watchdog/abort, 388 successful subsequent
draws, 389 checks, zero mismatches/leaks/dropped stores, disabled=0. Both full
draw and nested-abort runs exactly match the control's first 96 post-load frame
fingerprints/cycles. A zero `nesting_repairs` counter is recorded as observed;
do not infer a missing abort from it or claim a particular native stack depth.

Reproduce artifact checks:

```powershell
python vr/verify_noop_pass.py vr/proof/scene-replay/draw --output vr/proof/scene-replay/draw_pixel_comparison.json
python vr/verify_scene_coverage.py vr/proof/scene-replay --output vr/proof/scene-replay/coverage_comparison.json
python vr/compare_frame_fingerprints.py vr/proof/replay-scope/fingerprint_off_1.json vr/proof/scene-replay/fingerprint_draw.json --output vr/proof/scene-replay/fingerprint_draw_comparison.json
python vr/compare_frame_fingerprints.py vr/proof/replay-scope/fingerprint_off_1.json vr/proof/scene-replay/fingerprint_nested_watchdog.json --output vr/proof/scene-replay/fingerprint_nested_comparison.json
```

## Limits and next work

The proof covers the static room scene, its weapon and HUD. Animated enemy/world
object coverage and other scene modes still require independent captures. The
scene traversal contains conditional entity work, so zero dropped stores in this
scene does not prove all possible callbacks are render-only. The replay is opt-in
and game-specific. No simultaneous eye pair, calibrated IPD, stereo presenter or
headset submission is claimed. The next contract should share checkpoint/capture
internals while publishing two same-state, explicitly tagged eyes atomically.
