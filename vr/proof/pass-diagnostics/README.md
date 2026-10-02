# Render-pass refusal and no-op capture evidence

## Retention update: 2026-10-02

The no-op PNG generations and gameplay screenshot were removed. The refusal, no-op stats and pixel-comparison receipts remain. The image verification command below requires restored historical images or a fresh capture.

The complete pre-cleanup image bundle is in game commit `231650447bdc219629559a16f0dd7d229ee1ce49`.
See [the proof index](../README.md). The run descriptions below record the original measurements.

Recorded 2026-10-02 on Windows, game SLUS-00974, OpenGL Debug build.
Framework: `257a88b0`. Game base: `b2ac246`; the successful run includes the
`PSX_VR_PASS_PROBE` plugin change committed with this evidence. Original game
generation is unchanged in the final implementation: the new hook is overlay code.

## Original refusal reproduction

Launch from the game root with `--no-launcher --game game.toml --disc
Input/medal-of-honor/medal-of-honor.cue`, and environment:

```powershell
$env:PSX_VR_PROBE='1'
$env:PSX_VR_INTERP='1'
$env:PSX_RENDER_PASS_VERIFY='1'
```

The legacy VSync probe remains selected when `PSX_VR_PASS_PROBE` is absent.
Load slot 1, then query `render_pass_stats` and `video_info` using
`python psxrecomp/tools/debug_client.py <command> ...`.

`refusal_initial.json` and `refusal_after_load.json` retain the same last
failure: attempt 2, plan 2, cycle 231387035, `capture_size`, requested 512x240,
capture 256x240, scales 1, wide=0. No generation textures were allocated.
`video_after_load.json` describes the later 512x240 display and must not be
substituted for the failure-site capture dimensions.

`gameplay.png` confirms the room scene after load. `gameplay_loop.json` is live
disassembly at 0x80090B80; `gameplay_flow.json` is the live flow selection.
`gp1_gameplay_irq_flips.json` retains exact matching GP1 entries from a second
run: opcode 05, frame >= 1000, last twelve matches. The flip stores have
sr=0x40000404 and exception EPCs in the render wait path. A temporary PutDispEnv
probe did not plan passes there; that probe is not in the delivered code.

## Successful no-op reproduction

Environment for the delivered probe:

```powershell
$env:PSX_VR_PROBE='0'
$env:PSX_VR_PASS_PROBE='1'
$env:PSX_VR_INTERP='1'
$env:PSX_RENDER_PASS_VERIFY='1'
.\build-debug\Medal_of_Honor__Recompiled.exe --no-launcher --game game.toml --disc Input/medal-of-honor/medal-of-honor.cue
```

Before loading gameplay, create an output directory and arm the dump. Use an
absolute path in `render_pass_dump`; the runtime can change its working directory.

```powershell
python psxrecomp/tools/debug_client.py render_pass_dump path=<absolute-output-dir> count=2
python psxrecomp/tools/debug_client.py savestate slot=1 op=load
python psxrecomp/tools/debug_client.py savestate_status
python psxrecomp/tools/debug_client.py render_pass_stats
```

The probe runs at `FUN_80090B80` before it enables IRQ flipping. It reads the
upcoming RECT from `0x8009A7A0 + (!read_word(0x8009C824))*20`, following the
live disassembly at 0x80090CCC..0x80090CE8. It gates on the measured 512x240
mode, not startup's 256-wide history. It returns from the no-op callback without
executing guest draw code. The legacy VSync probe is bypassed in this mode.

Saved TCP receipts:

- `noop_stats.json`: 90 completed passes, 89 promotions, 90 verification checks,
  zero mismatches and zero pass-call refusals.
- `noop_stats_final.json`: 727 passes/checks, zero mismatches, faults, dropped
  writes or pass-call refusals. The current sampled status is BUSY; it does not
  override the recorded success counters. `refused=572` counts empty plans.
- `noop_savestate_status.json`: load slot 1 completed successfully.
- `noop_video_info.json`, `noop_gl_interp.json`: contemporaneous presentation
  configuration. They are independent samples, not failure-site inputs.
- `noop/`: two promoted generations, each with baseline phase 0 and pass phase
  32768, 512x240 PNGs.

Verify the images directly:

```powershell
python vr/verify_noop_pass.py vr/proof/pass-diagnostics/noop --output vr/proof/pass-diagnostics/noop_pixel_comparison.json
```

The receipt reports zero changed decoded RGB pixels for both pairs (122880
pixels per pair). The images show the interior room, including level and weapon.
This proves no-op capture/promotion and the enabled state/VRAM verification,
not a replayed scene or stereo. Measured `cost_us=3837` includes verification;
it is not a scene-redraw or stereo performance estimate. The no-op callback
executes zero guest cycles. Live fingerprint equivalence and watchdog rollback
remain separate work before claiming redraw safety.

TCP JSON written through PowerShell may include a BOM; read with `utf-8-sig`.
For ordinary screenshots the client takes a positional filename:
`screenshot_file <absolute-path>`.
