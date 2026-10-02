# Quest locomotion and smooth turning

User priority on 2026-10-02: movement before further HUD work. Left stick moves
forward/back and strafes; right stick turns smoothly. WorldScale stays 3;
wrist HUD and further weapon/enemy size tuning remain deferred.

1. Add opt-in OpenXR Touch thumbstick actions, attached before session start.
   Synchronize at host input sampling, independently of stereo eye replay.
   Focus loss, inactive actions, disconnect and failed synchronization release
   movement. Expose action samples and final pad delivery through TCP.
2. Provide a trusted game-owned controller source at the existing offline input
   boundary, retaining coherent SIO pad-type requests and selfcheck recording.
   Debug input overrides retain priority; render transactions never poll input.
3. Measure Medal of Honor's native DualShock axis meanings in slot 3 with bounded
   controls. Map Quest axes to those meanings without directly writing player
   transforms or accumulating a synthetic head yaw. Use explicit deadzone/gain.
4. Test neutral, forward/back, strafe, proportional turn and release controls,
   including identical input routes with stereo disabled/enabled. Measure actual
   pad delivery and guest camera/position changes; visual change alone is not a
   displacement or rotation measurement.
5. Build Debug/Release, document framework changes in UPSTREAM_PENDING.md, append
   game measurements/corrections, commit and push framework first, then game pin.
   Close all owned game processes. Keep bulk captures ignored. A real controller
   test must remain labelled pending until its actions and directions are seen.

Movement initially follows the game's body-forward direction. Head-relative
locomotion and controller buttons/aiming are separate choices after this baseline.

## Results (2026-10-02)

Steps 1-3 are implemented. Step 4 has native-axis controls, synthetic action
controls, equal per-update heading writes in both directions, radial/curve
movement checks and neutral-release controls. The user confirmed the corrected
movement is fully consistent in the headset. Native-curve quantization remains;
this is not a measured physical turn-speed calibration. A neutral stereo ON/OFF
comparison passed. The source-held comparison was uncontrolled at the host-time
post-load guard and differed; the matched native held movement/turn control
bypasses that guard and matches all 96 fingerprint columns/cycles. This isolates
stereo replay with moving input without asserting identical XR action timing. Real focus-loss/reconnection and other input schemes
remain untested. Wrist HUD remains deferred. The scale-4 trial was inconclusive
for the user, so the accepted default remains 3.

Step 5 delivered: framework 5a1264b7 pushed and pinned; both opt-in builds use
the submodule again. Compact evidence: VR_MOVEMENT_RECEIPT.json.
