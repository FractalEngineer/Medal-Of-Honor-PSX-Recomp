> Historical design and measurements. Current status/builds: [VR_HANDOFF.md](VR_HANDOFF.md).
> Current tasks: [VR_ALPHA_TODO.md](VR_ALPHA_TODO.md). Current controls: [ALPHA_README.md](../../ALPHA_README.md).

# Calibration, HUD and OpenXR execution

Date: 2026-10-02. Quest 3 through VDXR; user IPD 67 mm.

1. Add explicit units-per-meter and IPD controls, preserving the old integer
   offset as a diagnostic override. Keep the initial scale labelled provisional.
   Derive a reference scale from identified game coordinates and an explicitly
   chosen physical reference; do not infer physical size from screenshot disparity.
2. Identify HUD producers separately from world geometry. Provide measured,
   reversible HUD/weapon policies inside the restored draw, with no live-game
   state changes. Compare zero-separation and known-offset controls, plus guest
   fingerprints and restore checks. Keep captures in ignored analysis/vr-proof.
3. Extend the scoped GTE view to a rigid camera transform and per-eye asymmetric
   projection. Apply head rotation after the game's world-to-camera transform;
   retain an exact identity path. Check near/far translation, rotations, projection
   and normal/watchdog restoration before using device poses.
4. Add an opt-in OpenXR backend using the existing OpenGL context. Locate both
   views for one predicted display time, retain their pose/FOV with the pair,
   render both from one guest checkpoint and submit matching projection views.
   Handle session events, tracking validity, swapchain acquire/wait/release,
   recenter and shutdown. Reject stale/mismatched metadata; do not label a
   stretched desktop pair as native headset projection.
5. Measure VDXR initialization and, when a connected headset is available, real
   session/tracking/submission. Desktop synthetic tests do not establish headset
   comfort, calibrated physical scale or motion-to-photon performance. Document
   any hardware-dependent checks still requiring a worn headset.

Each completed increment gets relevant tests/builds, framework documentation in
UPSTREAM_PENDING.md, appended results/corrections in VR_PHASE9_STATUS.md, a commit
and immediate push. Push framework first, then the game submodule pin. Close the
owned game process after each measurement. No bulk proof assets are committed.

## Progress

- [x] Hardware/runtime preference recorded and execution plan written.
- [x] Explicit IPD/units/world-scale controls and reproducible reference procedure.
- [x] Measured text/icon exclusion and separate held-weapon focal policy.
- [x] Rigid head pose and asymmetric projection with ordinary/watchdog restore tests.
- [x] OpenXR lifecycle, poses, swapchains and matching per-eye submission.
- [x] Connected Quest 3/VDXR validation; user confirmed orientation and tracking.
- [ ] Physical-reference calibration and final weapon/enemy size tuning.
- [ ] Coherent compass/HUD projection; wrist placement deferred by user.
- [ ] Independent headset-rate draw scheduling and wider-FOV culling coverage.
- [x] User-prioritized locomotion actions and desktop movement/turn/release controls.
- [x] Real Quest/VDXR controller action sampling; direction assessment recorded separately.

Accepted provisional profile: WorldScale 3, user IPD 67mm (runtime measured
66.780mm). Initial 3.3 GL refusal, XR Y flip and invalid half-scale launch are
recorded in VR_PHASE9_STATUS.md. Usage: VR_OPENXR_SETUP.md. This phase establishes
native projection submission and tracking, not physical calibration or full HUD
comfort. Bulk captures remain in ignored analysis/vr-proof.

User priority update: movement before HUD; left-stick move/strafe and right-stick
smooth turn. See VR_MOVEMENT_PLAN.md. WorldScale stays 3. Wrist HUD and further
weapon/enemy scale tuning remain deferred.

Movement correction: live guest analog-curve inversion and radial movement
deadzone; per-update heading magnitudes match left/right and the user confirmed
movement fully consistent. Scale 4 reached its configured metric conversion;
the user could not distinguish it visually. Default 3 remains provisional.
