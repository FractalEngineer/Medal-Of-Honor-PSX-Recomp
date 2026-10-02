# Same-checkpoint stereo, including animated enemies

## Retention update: 2026-10-02

Both Debug zero-offset and offset-24 left/right image pairs remain, as does slot3-offset24/presented.png. Duplicate SBS composites, other presentation screenshots, fault-run images and large Release textures were removed. All pair manifests, raw TCP responses, provenance, fingerprints and comparison receipts remain. Debug eye equality/parallax and timeline checks can still be recomputed. Release image and fault-image checks require the historical images or fresh captures.

The complete pre-cleanup image bundle is in game commit `231650447bdc219629559a16f0dd7d229ee1ce49`.
See [the proof index](../README.md). The run descriptions below record the original measurements.

2026-10-02. Framework `0a955971`, game base `6e4ce84` plus the paired plugin and
measurement scripts committed with this evidence. NTSC-U SLUS-00974, windowed
OpenGL Debug, `--no-launcher --game game.toml --disc
Input/medal-of-honor/medal-of-honor.cue`. All control runs use fresh processes.

The save is TCP slot **3**, file `saves/openbios/state_8001DFD4_slot03.pst`.
Its file metadata and SHA256 are recorded per run. The OSD labels this as
"Loaded slot 4" because its display number is one-based. Each receipt verifies
a completed load (`pending=0`, `last_ok=1`, `last_slot=3`) before arming dumps.
The scene contains two enemies outdoors, a held grenade, compass and ammo HUD.

Common environment: `PSX_VR_PROBE=0`, `PSX_VR_PASS_PROBE=0`,
`PSX_VR_INTERP=0`, `PSX_RENDER_PASS_VERIFY=1`; ordinary 4:3, internal scale 1.

| Run | STEREO | EYE_OFFSET | STEREO_FAULT | FAULT_HOLD |
| --- | --- | --- | --- | --- |
| `slot3-off` | 0 | unused | 0 | 0 |
| `slot3-zero` | 1 | 0 | 0 | 0 |
| `slot3-offset24` | 1 | 24 | 0 | 0 |
| `slot3-right-watchdog-held` | 1 | 24 | 2 | 1 |
| `slot3-right-watchdog-recovery` | 1 | 24 | 2 | 0 |

Table names have prefix `PSX_VR_` (FAULT_HOLD is `STEREO_FAULT_HOLD`).
`EYE_OFFSET` is a camera-space half-separation, not a calibrated physical IPD.
The plugin supplies +24 for LEFT and -24 for RIGHT: a camera left of centre sees
scene coordinates translated right. The runtime applies this at RTPS/RTPT's
pre-divide translation stage, including level, object and held-weapon paths.

## Measurements

- The zero-offset pair images are decoded RGB pixel-identical for both captured
  pairs. Both enemies and their exact poses are rebuilt from the same checkpoint.
- Offset-24 pairs differ on 78,026 and 77,961 pixels, across every row band.
  In both captures, explicit wall ROI `[50,100,170,130]` corresponds at -5px
  (NCC 0.9412), and nearby ground ROI `[50,195,200,230]` at -13px (NCC 0.9637).
  These boxes were visually inspected. The method measures image correspondence,
  not world distances. Different shifts within one pair support real parallax.
  The GTE unit test independently measures 12px versus 3px at Z=800/3200.
- TCP records equal entry guest cycles and equal full checkpoint hashes for both
  eyes, with end-of-callback view ambient `[24,0,0]` and `[-24,0,0]`. This offset
  sample is not a per-RTPS producer trace; the GTE test and image correspondences
  independently establish the projection effect. All normal eye
  transactions verify with zero mismatches, VRAM leaks or dropped device stores.
- The first 96 consecutive post-load frame fingerprints match the no-redraw
  control for zero-offset, offset-24, held-watchdog and recovery runs. Every
  recorded judge/locator column and guest cycle count matches; host frame labels
  are excluded. This is a bounded timeline check, not a claim about all modes.
- The held-failure run publishes pair 1, then aborts the right eye of attempt 31
  inside the real level dispatch. TCP records `eye_abort`, `retained_pair_id=1`,
  published pair 1, staging mask 0, one watchdog and four clean verification
  checks. The recovery run subsequently publishes later complete pairs while
  retaining the failure record. No half-pair is published.
- `gl_interp` reports `enabled=0`, and shared temporal plans/promotions remain
  zero. `presented.png` is read from the composed present surface, with the TCP
  readback completion receipt, rather than inferred from a VRAM screenshot.

## Reproduction

After launching a process with the selected environment:

```powershell
python vr/capture_stereo.py analysis/vr-proof/<new-run> --slot 3
# Use --pairs 0 for OFF, --pairs 1 for the held watchdog control.
python vr/verify_stereo_pairs.py analysis/vr-proof/<new-run> --expect different --output <receipt.json>
# Use --expect equal for zero separation.
python vr/compare_frame_fingerprints.py <off>/fingerprint.json <candidate>/fingerprint.json --output <comparison.json>
```

`capture_stereo.py` polls completed load status, arms pair dumps through TCP,
collects the first 96 fingerprints and waits for composed-present readback.
Dumps contain explicit eye IDs, one pair identifier and one guest-cycle value.
The new offset replaces its host ambient instead of accumulating per transform;
normal returns and watchdogs restore it. The plugin clears its entry guard and
eye selector after the paired API returns. Close each test process afterward.

## Limits and follow-up

This establishes paired stereo and monitor SBS for the measured scene. It does
not provide OpenXR, head tracking, world-scale/IPD calibration or HUD comfort.
The 3D compass also gets the projection offset; screen-space ammo text stays
coincident. HUD/weapon handling needs a deliberate policy before headset use.

The sampled Debug pairs cost about 35ms with verification and exceed the
conservative 26.67ms budget for a two-VBlank cadence. The complete-pair scheduler
therefore sheds requests and retries every thirtieth request, visibly holding
old pairs in this configuration. This is correctness evidence, not a headset
performance claim.

## Release cost sample

`slot3-release-noverify/` and `slot3-release-off/` use the existing Release
configuration (`-O3`, `NDEBUG`) with `PSX_DEBUG_TOOLS=ON` for TCP inspection,
VERIFY off, interpolation off and the same slot-3 save. The first Release launch
had TCP tools disabled and produced no measurement; it was closed before
reconfiguring. `video_info` establishes a different renderer configuration:
scale **5**, full drawable 1920x1440, eye textures **2560x1200**. This is not a
same-settings performance comparison against the Debug captures.

The warm receipt records **1,719 pairs**, zero refused/failed/shed, last cost
**12.013ms**, pair-cost EMA **11.584ms**, and 3,437 composed presents. Guest entry
cycles match; checkpoint hashes are zero because VERIFY is off. No checkpoint
hash-verification claim is made for this cost sample. Its 96-frame fingerprints
match the separate Release OFF control at the same scale/settings. The attempted
cross-build Debug/Release comparison differs; it is retained in
`release_timeline_comparison.json` and is not evidence that eye redraw changes
the timeline. The matched Release comparison isolates the added redraw work.

The two Release pairs have real parallax and the same 5/13 display-pixel ROI
correspondences, measured on a documented 5-pixel grid sample of their actual
textures. Full-resolution differing pixels are counted without downsampling.
These bounded samples fit the cadence budget; frame pacing tails and headset
cadence remain unmeasured. Both game build targets pass. Test instances are closed.
