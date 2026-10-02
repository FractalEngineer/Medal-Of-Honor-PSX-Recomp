# VR proof index

For normal session context, read
[VR_EXECUTION_PLAN.md](../../docs/reverse/VR_EXECUTION_PLAN.md) and the relevant
section of [VR_PHASE9_STATUS.md](../../docs/reverse/VR_PHASE9_STATUS.md).
This directory is supporting evidence; read individual receipts or images only
when the question requires them. Future bulk captures belong in the ignored
`analysis/vr-proof/` directory.

## Current stereo evidence

[stereo-pairs/README.md](stereo-pairs/README.md) records the controls, save identity,
measurements, reproduction steps and limits. Both zero-offset and offset-24
Debug left/right pairs remain, together with one actual composed SBS screenshot.
Raw TCP, timeline, fault/recovery and Release measurement receipts remain.

## Earlier work

- [pass-diagnostics](pass-diagnostics/README.md): refusal and no-op capture.
- [replay-scope](replay-scope/README.md): guest timeline, watchdog and draw scope.
- [scene-replay](scene-replay/README.md): bounded scene reconstruction.
- [scene-replay-slot2](scene-replay-slot2/README.md) and
  [scene-replay-slot3](scene-replay-slot3/README.md): exploratory results and
  scene-identity correction; not proof of animated-object coverage.

The four original root images remain: `eye_L.png`, `eye_R.png`,
`stereo_SBS.png` and `stereo_anaglyph.png`. They show the earlier isolated level
TRX experiment described in [M1_VIEW_MATRIX.md](../../docs/reverse/M1_VIEW_MATRIX.md),
not the subsequent simultaneous paired API.

## Image retention

On 2026-10-02, 57 redundant, exploratory or large Release PNGs were removed after
user confirmation. All raw JSON receipts and evidence READMEs remain. Historical
images and their complete original bundle are available at game commit
`231650447bdc219629559a16f0dd7d229ee1ce49`. Image checks against removed PNGs require
an isolated historical checkout or a newly measured run. See
[VR_PROOF_CLEANUP_PLAN.md](../../docs/reverse/VR_PROOF_CLEANUP_PLAN.md).
