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

## Held-rifle prototype (2026-10-03)

Step 5 now has a render-only prototype, gated to the measured rifle render
entity at player input+784 and native H=133/TR=0. The geometry filter wraps
80080DD4; its nested 80084718 filter calls the native node walk, then maps
packed XYZ/padding vertices in 800AB148..DAT8009EABC before RTPS at RA80080F2C.
The native weapon projection matrix W is scaled; use its full inverse. For
hand rotation R, grip translation t, model scale s and provisional pivot p:
A=inverse(W)*R*s; b=inverse(W)*t-A*p. Scoped authored focal reference 133
replaces 400 for this object, then restores the eye view. Original animation
vertices remain the input; arms move with the rifle. Writes are inside each
eye transaction and roll back. The world render is untouched by this mapping.

Grip position and aim orientation require focus, activity, both validity flags
and <=150ms age. Rifle model units/meter=850 and pivot=(80,150,100) are guesses
for physical calibration, not measured headset grip locations. Shots still
start at aim-space origin, not the model barrel tip. Mesh size, grip/barrel
alignment, arms separation, recoil/reload acceptance, actual Quest action-space
validity, body turn/recenter and long-range/other-weapon behavior remain open.

Desktop: five fresh paired slot-0 captures prove native/straight/translated/
rotated/focus-loss response; every manifest is newer than the capture marker
and both PNGs finish decoding while the pose is held. Producer V0 changes
[72,82,389] -> [9,4,88] -> [17,4,88] -> [-1,4,76] -> [72,82,389]. A selected
90,000-pixel left-eye world region is exactly equal throughout. The right-eye
rotated rifle overlaps that region; its differing pixels are retained without
a world-isolation claim. Focus loss restores native lower-frame weapon pixels.

With mesh+shot override enabled in slot 5, a synthetic aim toward a provisional
torso point produces a native enemy damage write at 8004ACC0: 6 -> 3.5. Complete
producer-filtered slices retain player pose stores and first native movement.
Eight aiming regressions pass; final verified run has 192 checks, zero mismatch,
zero dropped stores/leaks and one deliberately injected right-eye watchdog.
The fault retains the previous pair, later pairs recover, and slot 5 reloads
afterward. Framework 35b209d4 fixes omitted mod callback host context across
longjmp; UPSTREAM_PENDING.md inventories it. Both pinned SDK builds pass.
See VR_WEAPON_POSE_RECEIPT.json. Bulk captures remain ignored; no frame-rate
or actual headset aiming acceptance is claimed. Owned test games are closed.

Reproduce mesh controls with Pillow installed:

```powershell
./vr/run_vr.ps1 -Desktop -DesktopFov -MovementDiagnostic -WeaponPoseDiagnostic -Verify -StereoFaultDiagnostic 3 -Slot 5 -Seconds 180
# Separate terminal while the owned test is running:
python vr/check_weapon_pose.py analysis/vr-proof/<fresh-dir> --slot 0
```

Next: short Quest check using `./vr/run_vr.ps1 -WeaponPoseDiagnostic -Slot 5
-Seconds 180`. No synthetic pose overrides and no injected faults. Inspect
right-hand grip/aim activity/validity, move/rotate the rifle with body still,
then turn/recenter and fire at the enemy. Tune pivot/scale/muzzle offset from
those observations. Ordinary launches retain native weapon handling until
tracked aiming is accepted; temporary right-grip native aim remains.
