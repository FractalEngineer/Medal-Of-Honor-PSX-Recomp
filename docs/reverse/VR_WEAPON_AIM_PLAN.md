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
   projectile update. Validate range/lifetime after bypassing native auto-aim.
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
