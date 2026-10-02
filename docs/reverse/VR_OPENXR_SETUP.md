# Quest 3 / VDXR prototype

Accepted profile: Quest 3 through Virtual Desktop's VDXR runtime, user IPD 67mm,
`WorldScale=3`. This is subjective tuning; a physical world reference has not been
measured. Weapon and enemy size need later tuning. Wrist HUD is explicitly deferred.

## Run

Connect Virtual Desktop with VDXR selected and run from the game repository:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run_vr.ps1 -Build -Slot 3
```

`-Build` configures the pinned framework with `PSX_OPENXR=ON`, builds Release and
refuses to launch after a failed configure/build. OpenXR support is OFF by default
in the framework. First configuration fetches a pinned official SDK/static loader.
The script supplies `--no-launcher`, waits for exit and closes its owned process in
`finally`. Slot 3 is TCP slot 3 (the game's OSD calls it slot 4). Omit `-Slot` to
boot normally. Close the desktop game window when finished.

For a bounded desktop diagnostic without a headset:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run_vr.ps1 -Desktop -Slot 3 -Seconds 5
```

`-CaptureDirectory analysis/vr-proof/<fresh-name>` records two pairs, a verified
slot load, 96 frame fingerprints, TCP stats, image readback, launch environment,
executable SHA256 and actual CMake framework-root provenance. It defaults to slot
3. Bulk images stay ignored. `-Verify` enables expensive restore diagnostics;
use Release without verification for worn-headset tests. The script restores its
parent environment and clears old offset/fault/synthetic-pose controls.

## Controls and inspection

| Control | Meaning |
| --- | --- |
| `-WorldScale 3` | Divides base game units/meter; accepted provisional profile |
| `-IPDmm 67` | Desktop eye separation; headset uses located runtime eye poses |
| `-UnitsPerMeter 716.417910447761` | Provisional base mapping, derived from old 48-unit separation |
| `-HideHud` | Omits measured upper-right text/icon only; compass remains |
| `PSX_VR_AUTHORED_FOCAL=0` | Diagnostic absolute-FOV control; launcher normally sets 1 |

Effective mapping is `base_units_per_meter / WorldScale`. At profile 3 this is
238.805970 game units/meter; desktop half separation rounds to 8 units. Runtime
IPD sampled 66.780mm despite the user's 67mm setting. Device eye poses provide
actual separation; no extra artificial IPD translation is applied.

```powershell
python psxrecomp/tools/debug_client.py openxr_stats
python psxrecomp/tools/debug_client.py openxr_views
python psxrecomp/tools/debug_client.py openxr_control recenter=1
```

Recenter while looking in the intended game-forward direction. The first valid
located pair establishes the initial origin. Diagnostics expose session state,
tracking validity, effective units/meter, real FOV/eye poses, projection matrices,
last submitted pair/cycle/time and errors. Located data can be newer than submitted
data during a render. `openxr_control enable=0|1` disables/retries initialization;
re-enabling is refused during an active frame or eye.

## Physical calibration procedure

Identify a fixed span in the game's world coordinates from its vertex producer
and transform, then explicitly choose a credible physical length for that same
span. Feed those values to `vr/calibrate_scale.py --game-units <span> --meters
<length> --reference <description> --ipd-mm 67`. Use its units/meter with
`WorldScale=1` for that mapping. Retain the measured coordinates, transform, scene
and physical-length assumption. A screenshot disparity alone cannot establish
physical scale. No such reference has been measured for the current profile.

## Measured behavior and remaining work

The user confirmed upright output and expected head tracking after the XR-only
Y flip. Both eyes use one guest checkpoint and one predicted XR display time,
with per-eye asymmetric projection and genuine pre-divide camera transformation.
Desktop synthetic yaw/translation and HUD controls preserve all 96 measured
post-load fingerprint columns/cycles. Ordinary and watchdog full-pose restore
unit tests pass. See VR_PHASE9_STATUS.md and VR_HEADSET_RECEIPT.json.

Native projection traces measure level H=400 and entity-renderer H=133. Absolute
XR focal lengths discarded that distinction and enlarged the held grenade.
Preserving H/400 reduced the weapon region in a fixed desktop FOV control while
the measured wall ROI stayed identical. This is enabled by default; its final
appearance still needs headset assessment and other scene/mode checks. It is
separate from physical calibration or an enemy-size conclusion.

The compass ring/disc still needs a coherent HUD policy. Flat text/icon can be
hidden; held weapon geometry is not removed by the icon switch. Wrist placement
and controller actions are later work, per user direction. The current XR loop
runs at the game's draw boundary, approximately 30Hz here. An independent headset
cadence, culling coverage for large head turns, frame-time tails and comfort remain
unverified. Successful submissions do not establish headset-rate performance.
