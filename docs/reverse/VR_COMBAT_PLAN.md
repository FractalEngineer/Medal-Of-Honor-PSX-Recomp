# Quest combat controls and aiming

User authorized continuation on 2026-10-02. Movement is accepted; world scale
stays 3. Wrist HUD and independent rendering cadence follow playable combat.

1. Measure the live native action table and its consumers in slot 3. Identify
   fire, reload, use, crouch, weapon change and pause from their producers; do
   not infer gameplay effects from an image hash or pad word alone.
2. Extend opt-in OpenXR input with Touch trigger/grip values and click actions.
   Keep per-action activity separate from thumbstick activity. Fresh unavailable,
   unfocused or failed samples release buttons as well as axes. Add TCP snapshot
   and explicitly synthetic controls, without polling actions during eye replay.
3. Map combat controls through the existing game-owned offline controller source.
   Use the measured native bindings, preserve movement response, and test press,
   hold, release, focus loss and combined input. Keep menu confirm/back usable.
4. Build Debug/Release, perform bounded desktop native/synthetic controls and
   stereo restore checks. Record compact receipts and limitations, inventory
   framework changes in docs/UPSTREAM_PENDING.md, push every commit, then pin.
   Close all owned games. Real Quest buttons remain pending until a headset test.
5. Trace the shot-direction producer and compare body aim versus rendered head
   pose before changing aiming. Add controller pose/aim input only after that
   relationship is established. A rendered head turn is not bullet-aim proof.

Desktop-checked mapping: right trigger fire/confirm, right A use, right B
weapon cycle/back, left X reload/use, left Y jump, left stick click crouch,
right grip native aim, left Menu pause.
Native button words are established below. Quest acceptance is recorded below.

## Desktop results (2026-10-02)

Steps 1-4 are implemented and pass the desktop controls. Real Quest binding
acceptance and ergonomics are pending. Firearm clip/reserve writers, weapon-id
switch and crouch flag writers are retained in VR_COMBAT_RECEIPT.json. The
source requires the measured default action and analog tables, declining other
schemes. Native button edges are owned by the game; the grenade fires on release.

Step 5 started: firearm event func_8007B350 calls func_8007D5C8 at 8007B488;
weapon id12 dispatches to 8007D770, then 80045A78 at 8007D7C0. The latter calls
8004532C, which contains spawn setup and orientation/position logic. These are
live disassembly observations, not a verified damage-ray/velocity producer.
Particle/muzzle effects must be distinguished from actual damage/projectiles
before an aim hook is chosen. No controller or head-to-shot aim change is made.

Delivery: framework ac6f84ba pushed/pinned; final pinned Release capture has two
complete pairs and the default action source. All owned games are closed at
delivery. A Quest button test is the next hardware step.

## Quest results and next work (2026-10-02)

The user confirmed all controls are coming through. The pause menu appears too
close; right-grip native aim is temporary until 6DoF weapon aiming replaces it.
All owned games are closed. World scale remains 3; wrist HUD remains deferred.
The real sample caught grip/R2 and later unfocused neutral input, but missed the
other button presses. Their acceptance is user feedback, not a recorded native
writer measurement. See VR_COMBAT_RECEIPT.json for provenance and capture limits.

Next, complete step 5 on desktop: identify the actual damage/raycast or moving
projectile producer, distinguish the currently found spawn from muzzle effects,
and record its position/direction before any aim override. Then expose tracked
controller grip/aim poses with validity and time through TCP, convert them into
the measured game basis, and drive the verified shot origin/direction together
with the rendered weapon. Keep native ammo/reload logic and neutral release.
Test stationary body/head with changed controller direction and the converse;
only actual shot or hit evidence establishes aiming. Controller tracking loss
must suppress stale pose use. The next headset test follows desktop proof.

Pause placement needs a separate comfortable menu surface; preserve readable
pause/navigation before wrist HUD work. Do not change global world scale to fix
the menu depth. Independent headset cadence remains a later rendering task.

Validation follow-up is pushed/pinned at framework 26281aac (docs only). Tested
Release binary provenance remains ac6f84ba; runtime sources are identical.
