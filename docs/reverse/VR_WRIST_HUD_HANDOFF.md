# Native wrist HUD handoff

Updated 2026-10-15. The three VR batch launchers now select the wrist-HUD candidate
and enable native HUD replacement alongside tracked weapons by default.
The originally requested handoff file was absent; this records the new work.

## Requirements and source

VR launch now explicitly sets `PSX_DITHERING=0`. Upstream added default-enabled
native-grid PS1 dithering during the master sync; its enlarged pattern appeared
across the scene in the user's headset screenshot. This is a game-side VR
presentation preference, not a change to the framework's general default.

Published branch `FractalEngineer/psxrecomp:feat/vr-hud-surfaces` is now based on
upstream/fork master `f9c287f5`, with three grouped commits: `062d6e39` isolated
capture, `890fed78` positioned surfaces, `99dd1eee` head-facing billboards.
After framework PR #666 merged, the game pins framework master `8d1c983b` in
`psxrecomp/` and tracks branch `master`, with compatible upstream UI
`41c88ff` in `recomp-ui/`. Current framework requires the newer launcher fields.
The successful `build-vr-wrist` rebuild used identical commits in separate
worktrees; those worktrees are historical development material. Batch launchers
select that build, and `-Build` now uses the normal pinned dependency directories.

Latest placement override: the user rejected all wrist-side orientations and
requested hand-pinned, always head-facing art. Native HUD surfaces now follow
grip position plus 6cm in LOCAL up; controller rotation cannot roll or flip the
text. The generic FACE_HEAD surface flag constructs an upright billboard aimed
at the predicted midpoint of the two eyes, shared by both eye projections.
The historical side-attachment settings below are superseded. Recalibrate the
fixed compositor pixel ROIs before treating the earlier placement receipts as
acceptance of this new layout. Build and billboard contract tests pass, and the
user accepted the head-facing placement in VDXR.

Preserve the original HUD appearance. Isolate native rendering and place
health/compass on the left grip and ammo/icon on the right. Verify unattended
after loading gameplay before requesting a headset check. SteamVR null startup
fixes were pushed to master as `a1aebe1` before HUD development.

- `vr/psx_vr_stereo.c`: black/white native HUD passes, SP signature gate, owned
  text filtering and exact packet suppression during ordinary eye draws.
- `vr/moh_vr_hud.[ch]`: alpha extraction, native cutouts, predicted grip placement,
  pixel aspect correction, frame invalidation and per-hand fallback.
- `vr/check_headless_mirror.py`: both-eye placement/fallback pixel assertions.
  Diagnostic cards additionally assert translation, rotation and recentering.
- `vr/verify_native_hud.py`: matte reconstruction and unrelated-content rejection.
- `vr/check_native_hud_gameplay.py`: native ammo values and compositor images for
  fire/reload/switch/pause/resume/save reload.
- `vr/test_native_hud.c`: opaque black art, transparency, invalid matte, upload
  rejection, tracking age/loss, stale-frame rejection and LOCAL projection.

Initial framework development used `toolchain/vr-hud-framework`, branch
`codex/vr-wrist-surfaces`, based on `c639301a`. The published commits and pins
above supersede that initial checkout.
The generic APIs are `psx_mod_render_capture_rgba` and `psx_mod_openxr_surface`.
Capture uses existing guest rollback and reads staging without publishing a pair.
Surfaces copy bounded frame-local RGBA, with two IDs, then draw into each host
stereo framebuffer before OpenXR copy, restoring GL state. Framework source must
accompany any future HUD commit. A portable copy of all candidate framework changes
is saved as `vr/framework/wrist-hud.patch`; apply it to upstream base `f9c287f5`
only when the published branch is unavailable. The pinned framework already
includes this patch; do not apply it twice.

Native health producer: `0x80060afc`; generic text: `0x8005f86c`; icon:
`0x8008019c`. Owned ammo text buffers: active input+264/+268. Health packet base:
`0x800ab970`, buffer stride `0xc00`, tag count 11; compass: `0x800ad170`, stride
40, tag count 9. Check signatures/opcodes when unlinking. Keep unrelated text
and notifications; never omit whole scene callbacks or the generic text renderer.

Native canvas is 512x240, cutouts `(16,12,96,96)` and `(374,8,128,36)`.
Dual mattes preserve opaque black artwork. Initial width is 12cm. The attachment
frame rolls the left grip -90 degrees and right grip +90 degrees around their
forward axes. Local offsets are `(+.045,0,.06)` and `(-.045,0,.06)` metres: the
inward-facing sides, as when clapping. The earlier underside placement was
rejected. Local tilt remains -35 degrees around the attachment X.
Pixel proportions use the eye focal ratio. The initial dorsal placement was
visually accepted for rendering, then moved inside at the user's request.

Weapon tracking was disabled in the assistant's manual test launch because it
omitted `-WeaponPoseDiagnostic`; it was not a rendering change. All VR batch
launchers retain that option and now explicitly select `build-vr-wrist`, with
`-WristHud -WristHudReplace`. Explicit build selection takes precedence over any
older root-folder executable. `-Build` now preserves the candidate framework for
wrist launches. Headless mirror checks with weapon pose enabled also execute
the existing translated/rotated tracked-weapon regression script.

## Run the candidate

`build-vr-wrist` is configured against the candidate framework with OpenXR and
debug tools. It reuses dependencies from `build-release`.

```powershell
cmake --build build-vr-wrist --target psx-runtime -j 8
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run_vr.ps1 -Headless -HeadlessMirrorCheck -WristHud -WristHudReplace -BuildDirectory build-vr-wrist -Slot 0 -CaptureDirectory analysis/vr-proof/<fresh-name> -Seconds 2 -Verify
```

SteamVR must be closed first. The launcher temporarily forces null HMD and
disables competing drivers/dashboard/Home, restoring settings in `finally`.
The 1920x1080 null window keeps both eyes inside the compositor dump. VDXR was
tried first but returned `XR_ERROR_FORM_FACTOR_UNAVAILABLE` without a headset.

`-WristHud` duplicates native art for comparison; `-WristHudReplace` additionally
suppresses the corresponding ordinary HUD. `-WristHudTest` uses colored diagnostic
cards only, never product art.

## Evidence and limits

Final run `analysis/vr-proof/wrist-native-gameplay-20261015-c` passed all eight
visibility scenarios plus fire, reload, weapon switch, pause/resume and save
reload. Native ammo changed 20/453 -> 19/453 -> 20/452, then weapon 11 -> 8
with 7/146; save reload restored 20/453. Both-eye wrist pixels changed with
these transitions. It completed 3,160 verified render passes with zero aborted,
discarded, mismatch, leak, nesting-repair or watchdog counts. SteamVR settings
were restored and the runtime stopped. Six harness tests and the standalone
native cutout/fallback/projection and framework input tests also passed.

`analysis/vr-proof/wrist-native-replace-20261015-c` passed eight visibility cases
and 1,244 verified restored passes without mismatch/leak/nesting/watchdog events.
Both compositor eyes were visually inspected. Matte reconstruction error was zero;
left/right cutouts contained 1,256/809 opaque pixels with transparent surroundings
and no unrelated content outside the owned regions.

`wrist-native-gameplay-20261015-a` passed fire/reload/switch pixel checks, then
rejected SteamVR's empty raw projection during the pause quad. Its compositor
showed the correct menu. The harness now permits an empty raw projection only
for an explicitly checked menu-quad case. Blank gameplay projection or blank
compositor output always fails.

Compositor dumps are asynchronous, not atomic guest-pair captures. Receipts also
contain separate complete stereo pairs and save provenance. Synthetic controls
are marked. `wrist_hud_verified` stays false pending the broader acceptance gates
in `VR_WRIST_HUD_PLAN.md`. Do not claim full mission or hardware comfort coverage.
