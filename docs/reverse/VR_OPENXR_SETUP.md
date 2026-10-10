> Historical design and measurements. Current status/builds: [VR_HANDOFF.md](VR_HANDOFF.md).
> Current tasks: [VR_ALPHA_TODO.md](VR_ALPHA_TODO.md). Current controls: [ALPHA_README.md](../../ALPHA_README.md).

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
`finally`. It refuses to launch when debug port 4370 is already occupied;
headset captures also require actual OpenXR submissions. Slot 3 is TCP slot 3 (the game's OSD calls it slot 4). Omit `-Slot` to
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

### Other OpenXR runtimes

VDXR is the default and fully tested runtime. The launcher can pin one installed
runtime for a single launch through the loader's process-scoped `XR_RUNTIME_JSON`
override, without changing the system's active runtime or the registry:

```powershell
.\RunVR-VDXR.bat        # Virtual Desktop (VDXR)
.\RunVR-Oculus.bat      # Meta Quest Link / Air Link
.\RunVR-SteamVR.bat     # SteamVR
```

Each launcher runs `vr/run_vr.ps1 -Runtime <name>` with the accepted VR settings
and tracked-weapon controls; any other arguments still pass through. The names
resolve to the runtimes' default install manifests, and `-RuntimeJson <path>`
selects an explicit manifest. The runtime is chosen by which launcher you run
(`RunVR-VDXR.bat`, `RunVR-SteamVR.bat` or `RunVR-Oculus.bat`), so the system's
active runtime is left untouched. The build renders with Windows OpenGL, so the
chosen runtime must expose `XR_KHR_opengl_enable`; a Direct3D-only runtime (for
example Windows Mixed Reality) will not start.

Runtime status: **SteamVR/OpenXR** launches, delivers controller poses and
renders correctly with the compiled submission (no orientation override needed).
**Oculus / Meta Quest Link is untested**: the runtime loads and passes the
OpenGL check, but `xrGetSystem` returns `-35`
(`XR_ERROR_FORM_FACTOR_UNAVAILABLE`) while the Quest is streamed through Virtual
Desktop rather than Quest Link. Connect with the Meta Quest app's Link/Air Link
and retest. Runtimes other than VDXR/SteamVR, and their color handling, remain
unverified — see the compatibility-matrix task in the backlog.

### Headless (no headset)

`-Headless` runs the VR path with no headset attached. SteamVR ships a null HMD
driver and serves a real OpenXR session without a device; the launcher enables
that driver for the run (it implies `-Runtime steamvr`), starts SteamVR, then
restores `steamvr.vrsettings` and stops the SteamVR it started. Steam and
SteamVR must be installed, SteamVR must be closed first, and `-Desktop` cannot
be combined with it.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run_vr.ps1 -Headless -Seconds 60
```

A headless run reaches the same state as a headset run: `openxr_stats` reports
`running` with `submitted > 0` and `runtime` = `SteamVR/OpenXR`, and the
spectator window shows the single left eye. Confirmed 2026-10-10 with no headset
connected. The null HMD has a fixed origin and near-static eye poses, so head
motion and device timing are not exercised; treat it as a functional check
(session, view location, stereo render, submission, spectator mirror), not as
headset evidence. The system's active runtime and the normal VDXR path are
untouched.

## Controls and inspection

| Control | Meaning |
| --- | --- |
| `-WorldScale 3` | Divides base game units/meter; accepted provisional profile |
| `-IPDmm 67` | Desktop eye separation; headset uses located runtime eye poses |
| `-UnitsPerMeter 716.417910447761` | Provisional base mapping, derived from old 48-unit separation |
| `-HideHud` | Omits measured upper-right text/icon only; compass remains |
| `PSX_VR_AUTHORED_FOCAL=0` | Diagnostic absolute-FOV control; launcher normally sets 1 |
| `-NoMovement` | Disable the Quest input source and retain ordinary game input |
| `-MoveDeadzone 0.2` | Radial movement deadzone; scalar turn deadzone, rescaled |
| `-TurnGain 0.65` | Right horizontal axis gain; native game controls the turn rate |
| `-Runtime vdxr\|oculus\|steamvr` | Pin an installed OpenXR runtime for this launch; default `current` |
| `-RuntimeJson <path>` | Explicit OpenXR runtime manifest; overrides `-Runtime` |
| `-Headless` | No headset: SteamVR's null HMD driver serves the session (implies `-Runtime steamvr`) |
| `-Desktop -MovementDiagnostic` | Enable the source for explicitly synthetic TCP input controls |

Effective mapping is `base_units_per_meter / WorldScale`. At profile 3 this is
238.805970 game units/meter; desktop half separation rounds to 8 units. Runtime
IPD sampled 66.780mm despite the user's 67mm setting. Device eye poses provide
actual separation; no extra artificial IPD translation is applied.

## Movement

Quest left stick moves forward/back and strafes; right horizontal stick turns
smoothly. Right vertical stick is neutral so head tracking controls the view.
Movement follows the game's body-forward direction. The slot-3 native default
scheme uses PSX left X for turn, left Y for movement and right X for strafe;
the plugin swaps horizontal stick channels for the requested Quest layout.
The game applies its own asymmetric thresholds and nonlinear response curve.
The mapper reads that live calibration and selects pad bytes with matched
positive/negative responses, then applies a radial movement deadzone. This
keeps turning symmetric and diagonal movement magnitude nearly constant,
within native byte/curve quantization. TurnGain scales the decoded response,
rather than the distance from byte 128. Unsupported schemes/inverted axes
release the source until their mappings have been measured.
It presents a DualShock through normal SIO, without directly writing player
transforms. Other in-game control schemes require rechecking this mapping.

Actions synchronize outside eye redraws. Unfocused/unavailable/inactive action
samples release movement; the post-load input guard still applies. Keyboard and
ordinary pad button words remain merged for menu access. Controller buttons,
aiming and head-relative movement are separate future work.

```powershell
python psxrecomp/tools/debug_client.py openxr_input
python psxrecomp/tools/debug_client.py pad_status
```

`openxr_input` reports real focus, action activity, raw axes and synthetic=0
for device input. `pad_status` reports SIO axes/type. They are separate snapshots.
Desktop diagnostics can inject signed-thousandth Quest axes with
`openxr_input_override lx=1000 ly=0 rx=0`; use `clear=1` to release. Injected
samples are tagged synthetic=1 and are not headset evidence.

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
is later work; the experimental tracked rifle is described below. Locomotion
is now prioritized over further HUD work. The current XR loop
runs at the game's draw boundary, approximately 30Hz here. An independent headset
cadence, culling coverage for large head turns, frame-time tails and comfort remain
unverified. Successful submissions do not establish headset-rate performance.

The scale-4 trial reached 179.104478 effective units/meter, but the user could
not distinguish it visually; the accepted default remains 3. On 2026-10-02 the
user confirmed movement "fully consistent" after the native-curve correction.
Producer-bound heading writes and response controls are retained in
VR_MOVEMENT_RECEIPT.json; full traces and images stay ignored.

## Combat controls (desktop checked; Quest accepted)

The normal headset launcher enables movement and combat through the same offline
source. NoMovement disables that source. The game must use the measured default
scheme; other action/axis tables decline to neutral. Head tracking remains a
rendered view change; ordinary launches still follow native game aim. The
experimental tracked-rifle launcher below enables independent controller aim.

| Quest control | Native game control |
|---|---|
| Right trigger | Cross: fire / menu confirm |
| Right A | Square: use / reload context |
| Right B | Circle: next weapon / menu back |
| Left X | Square: reload / use context |
| Left Y | Triangle: jump |
| Left stick click | L2: crouch toggle |
| Right grip | R2: native aim mode |
| Left Menu | Start: pause / resume |

Trigger/grip activate at 0.55. Native weapon logic keeps press/hold/release;
the slot-3 grenade throws on release. Pause/resume was observed on desktop;
the user subsequently confirmed all controls in Quest 3/VDXR. The pause menu is
visible but appears too close. Full menu navigation, a controlled focus/reconnect
test and target-specific use behavior remain open. Right grip is temporary native
aim; the tracked-rifle prototype is desktop verified and awaits Quest alignment.

Desktop synthetic checks: `python vr/check_combat.py analysis/vr-proof/<fresh-dir>`
against a windowed Desktop MovementDiagnostic run. They establish weapon-id,
clip/reserve and stance writers, plus neutral-release and combined movement.
TCP `openxr_input` now reports trigger/squeeze and separate click activity/value
masks. Synthetic example: `openxr_input_override right_trigger=1000` or
`openxr_input_override left_buttons=1`; clear=1 releases. These values always
say synthetic=1. See VR_COMBAT_PLAN.md and VR_COMBAT_RECEIPT.json.

A separate real input capture caught right squeeze/R2 and a later unfocused
neutral state, but missed the other button presses. Its trailing status queries
failed after bounded shutdown; do not treat it as a complete button trace or
whole-test performance sample. The utility now preserves failed trailing queries
in capture_status.json and returns a partial-capture exit status. Owned games
are closed after tests.

## Experimental tracked rifle (desktop verified; Quest alignment pending)

`./vr/run_vr.ps1 -WeaponPoseDiagnostic -Slot 5 -Seconds 180` enables the rifle
mesh and native shot pose override for a bounded headset alignment test.
Ordinary launches retain native aiming. Fresh valid right grip position and
aim orientation drive the mesh; the aim pose drives player rifle shots.
Unusable tracking falls back to native handling. WorldScale remains 3.
The arms move with the rifle, physical mesh scale/pivot are provisional, and
shot origin is not yet a calibrated barrel tip. Right-grip native aim is still
temporary. See VR_WEAPON_AIM_PLAN.md and VR_WEAPON_POSE_RECEIPT.json for desktop
producer evidence and remaining hardware checks. Pause/wrist HUD and independent
headset cadence remain deferred. The launcher closes its owned game afterward.

The launcher exposes `-WeaponModelUnitsPerMeter 850` and
`-WeaponPivot @(80,150,100)` for physical calibration without rebuilding.
The pivot is in native mesh coordinates; it is not a distance in meters.
Increasing model units/meter makes the weapon smaller at fixed WorldScale;
1700 gives half the linear size of 850. Leave WorldScale 3 unchanged while
tuning the weapon. These controls affect the mesh; shots still start at the
aim-space origin, so pivot adjustment does not calibrate the muzzle.

During the bounded headset test, a separate terminal can retain real hand
poses and their validity/predicted time through existing TCP commands:

```powershell
python vr/capture_movement_input.py analysis/vr-proof/<fresh-dir> --hands --seconds 30
```

Hand/action/pad queries are adjacent requests, not an atomic shot or mesh
producer receipt. Check synthetic=0, focus/origin validity, right grip/aim
activity, both validity bits and pose age; synthetic=0 alone does not prove a
device sample. Hardware alignment and shot direction still need their own checks.
