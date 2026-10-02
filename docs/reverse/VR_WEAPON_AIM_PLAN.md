# Tracked weapon and native shot aiming

2026-10-02. Continue accepted movement/combat, WorldScale 3, Quest 3/VDXR.
Rifle slot 0 is the no-enemy direction control; user-created slot 5 has a
nearby visible enemy. Keep owned game tests bounded and close them afterward.

1. Establish actual shot motion and native damage before overriding aim.
   Complete: actor id5110 is initialized by 8004532C, direction at +552/+556,
   speed at +512 and first motion stores 8006CC10/2C/48. Slot-5 forward fire
   reduces enemy 800B7B08 +244 from 6 to 3.5 at 8004ACC0; idle and turned-away
   controls do not. 8004C1B4 calls damage function 8004AC58 at 8004C614.
   Preserve compact producer receipts, distinguishing random speed/spread.
2. Expose independent grip/aim poses at the eye predicted time and recenter
   origin with activity, validity and age. Framework 734977c9 pushed; math,
   lifecycle and live synthetic TCP controls pass. Real poses remain untested.
3. Verify native camera/world coordinate mapping against producer-bound RT/TR
   and live player/shot positions. Select a constructor-to-basis function hook
   after native pose computation, gated to player-owned rifle shots and
   measured instruction/caller structure. Do not redirect enemy shots.
4. Map a fresh valid right aim pose to shot world origin/direction before native
   basis/history initialization. Retain native ammo, collision, damage and
   projectile update. Validate range/transition countdown after bypassing auto-aim.
   Use stationary body with rotated controller, body turn with fixed relative
   controller pose, position controls, native fallback and stale/focus controls.
   Actual trajectory and damage establish aim; screen images alone do not.
5. Identify the held-rifle visual transform and attach it to the grip pose in
   the same basis used by shots. Keep native recoil/reload effects where possible
   and distinguish foreground weapon scaling from world calibration. Verify
   both eye redraws and rollback. Avoid persistent guest render-only patches.
6. Build pinned Debug/Release, record compact receipts and upstream inventory,
   commit and push every commit. Request a bounded Quest pose/alignment/shot
   test only after desktop controls pass; replace temporary right-grip native
   aim when tracked aiming is accepted.

Pause menu placement remains a separate comfortable surface task; wrist HUD
and independent XR rendering cadence remain deferred behind playable aiming.

## Desktop implementation (2026-10-03)

Steps 3-4 now have an experimental player id5110 shot override.
`run_vr.ps1 -WeaponAimDiagnostic` explicitly enables it; ordinary launches keep
native aiming until visual alignment is established. Function 8006A76C is the
native basis builder, called at 8004551C after pose selection. The hook checks
caller RA 80045524, argument/S0 shot identity, S2/player ownership, shot id and
measured caller/entry instruction words. It writes origin/pitch/yaw before native
basis/history initialization. NPC constructors retain their original poses.

Native level RT is a scaled matrix, not an orthonormal rotation. The full inverse
maps a controller point from camera coordinates to native world coordinates:
world = inverse(C) * (handTranslation - TR); direction = inverse(C) * handForward.
The cached producer is taken only at native level entry, before legacy offset,
and expires after four NTSC VBlanks; eye replay never changes it. Aim requires
a focused, active pose with both validity flags and host age <=150ms. Unusable
tracking retains the native shot pose. Unit tests cover scaled basis, axis signs,
translation and stale/invalid rejection.

`vr/check_weapon_aim.py` reproduces eight synthetic controls with complete
recorded actor-arena slices, dynamically identifies the player shot, and checks
pose stores, native movement and NPC isolation. Debug verified all eight;
134 frozen-eye restore checks had zero mismatches, aborts or dropped stores.
Release controls separately measured rotated/translated trajectories and damage.
Some 24-VBlank holds produce two health writes; the receipt records each native
writer rather than calling every hold a single hit. No exact replay/fingerprint
claim is made across active combat controls.

Field input+64 is a countdown whose expiry restores saved actor flags+20; the
measured consumer does not itself destroy the shot. Calling it a proven lifetime
would be premature. Range, native transition policy after auto-aim bypass,
other weapons, actual Quest alignment and the held weapon visual remain open.
