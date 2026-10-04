# VR alpha follow-up checklist

Date: 2026-10-03. Alpha baseline: game `28b0c55`, framework `5bafeebf`.
Target: Quest 3 through Virtual Desktop / VDXR; user IPD 67mm.

This is the current backlog, not a claim that the causes below are established.
**Reported** means user-observed; **known gap** means implementation or validation
is incomplete; **proposal** needs a choice before changing the accepted behavior.
Unchecked items remain open. Recommended order: rendering/correctness, weapon
coverage and controls, wrist UI, then calibration and broader release coverage.

Keep the accepted baseline: genuine per-eye gameplay, normal VR boot without a
save, usable menus/pause surface, consistent locomotion, accepted rifle grip/
appearance, and the sound/framerate fix. WorldScale 3 remains provisional.
The rejected movie bicubic trial stays removed. This checklist does not request
an alpha tag, release upload, or changes to gameplay yet.

## 1. World rendering and level correctness - highest priority

- [ ] **Reported: garbled and shaky world, even while stationary.** Reproduce
  in a fixed scene. Compare flat/native, neutral stereo, fixed synthetic head
  pose, and actual tracked poses. Establish whether vertices/packets change at
  a frozen guest checkpoint, whether small pose noise is amplified, or whether
  the visible effect comes from frame delivery. Investigate precision,
  transforms, packet interpretation and draw order as hypotheses; do not call
  it tracking jitter or a PSX limitation without a control. Success: stable
  geometry with a fixed pose and no new folding/corruption relative to flat.
- [ ] **Reported: floor immediately below the player appears unrendered.**
  Also reproduce the previously captured large black lower-world polygon.
  Determine whether these share a producer: absent geometry, an occluding
  polygon, clipping, winding/culling, or insufficient game visibility coverage.
  Compare both eyes and flat output at the same player position while looking
  down and leaning. Success: expected floor coverage without a covering artifact.
- [ ] **Reported: significant pop-in.** Separate native game draw distance,
  portal/sector visibility, asset streaming, and VR-induced rejection. Inspect
  what geometry is submitted before changing renderer settings. Expand the
  game's visibility coverage for headset FOV/rotation/translation where needed;
  a projection change cannot reveal geometry the game never submitted.
  Success: measured reduction in added VR pop-in, with costs documented.
- [ ] **Reported: later-area breakdown / missing or transparent tiles in
  Mission 1's ruins.** Record a reproducible route or dedicated save and the
  first affected location. Compare flat and VR, cold entry and restored entry,
  and before/after streaming transitions. Trace affected geometry/texture
  producers and per-eye restoration rather than assuming the floor issue is
  the same fault. Success: ruins traversal without new missing/transparent tiles.
- [ ] **Known gap: wider-FOV and large-head-turn coverage.** Verify side/behind
  views, looking down/up, leaning and smooth body turns in several sectors.
  Check near-plane clipping and bounds separately from portal/distance culling.
  The user accepted eye-frustum world selection on Quest 3 / VDXR, including a
  movement-enabled slot-0 gameplay run, on 2026-10-05. Normal VR launch enables
  it; FOV and native PVS masks stay unchanged. See VR_HEAD_VISIBILITY_FIX.md.
  Multi-sector and near-plane coverage remain open.
- [ ] **Known gap: stereo coverage across levels and render variants.** Extend
  beyond the measured scenes: terrain, dynamic enemies, animated objects,
  effects and later-area paths. Preserve true parallax for world geometry.

## 2. Headset color and presentation

- [ ] **Reported: headset brighter / lower contrast than flat.** Use the same
  frame and view with controlled display settings. Inspect source texture,
  presentation gamma, XR swapchain format and linear/sRGB handling, including
  native UI quads versus gameplay projection. Compare captured pixels before
  submission and the headset observation; check VD/headset settings separately.
  Gamma/encoding mismatch is a hypothesis, not an established cause. Success:
  documented color path and matched reference behavior without a blanket
  brightness adjustment hiding the fault.
- [ ] **Known gap: independent headset cadence.** Measure guest VBlanks,
  actual redraws, XR submissions and compositor behavior separately during
  normal play and heavy scenes. Investigate headset-rate pose/render scheduling
  while keeping guest simulation at its intended speed and both eyes coherent.
  Existing ~59 guest Hz movie samples do not prove headset display cadence.
- [ ] **Known gap: robust native-screen/gameplay handoff.** Replace or harden
  the four-VBlank scene-inactivity heuristic if long gameplay stalls can trigger
  an unintended flat surface. Exercise loading, briefing, pause/resume, death,
  mission completion and video transitions without stale or blank XR images.
- [ ] **Deferred picture quality: movies still look grainy/blocky.** Compare
  native decoded source with an independent reference before changing decoding
  or considering reconstruction/replacement assets. Preserve the accepted
  pacing fix and earlier picture; bicubic was tried and rejected by the user.

## 3. Weapons, aiming and controller layout

- [ ] **Known gap: physical barrel-to-shot validation.** With a visible enemy
  and tracing armed before firing, compare rendered barrel, real shot origin/
  direction and actual hits at near/far distances. Vary controller pose with
  head/body fixed, then head/body with controller direction held. Include body
  turning, recentering, recoil/reload and close-wall positions. Accepted visual
  grip/alignment does not establish damage-ray alignment.
- [ ] **Reported requirement: test every weapon.** Inventory each obtainable
  weapon and record model/shot identifiers, supported pose path and results.
  Test equip/switch, mesh and grip, muzzle/shot alignment, ammo, fire/release,
  reload, recoil, animations, sound and damage. Cover grenades/projectiles and
  scoped/special aim behavior where present; do not assume firearm ray logic.
  Current measured shot override is guarded to player-owned rifle id 5110;
  rifle-only face handling uses the measured node-22 layout. Generalize by
  measured weapon layouts/producers, with explicit fallback for unsupported ones.
- [ ] **Reported requirement: disable legacy aiming controls.** Remove the
  temporary right-grip native aim binding and any unwanted legacy camera/zoom
  behavior once tracked aiming coverage is validated. Review special/scoped
  weapons so removing a button does not silently remove required functionality.
  Keep native ammo/fire/reload logic and menu navigation intact.
- [ ] **Proposal: reload and weapon switching on right-controller buttons.**
  Current gameplay layout is X=reload/use, B=weapon cycle, A=use; menu A/trigger
  confirm and B back. Decide the intended A/B assignments and where Use moves
  before implementing. Native reload/use share the Square action and depend on
  game context, so verify doors/switches as well as reload. Retest hold/release
  and simultaneous movement/fire; publish the final control chart.
- [ ] **Known gap: tracking/focus loss and reconnect behavior under combat.**
  Exercise controller disappearance, headset sleep, unfocus/refocus and VDXR
  disconnect/reconnect. Verify neutral releases, no stale weapon pose/shot,
  coherent recenter origin and recoverable rendering. Existing release samples
  and desktop controls do not constitute a controlled hardware reconnect test.

## 4. Wrist UI

- [ ] **Reported requirement: left wrist health bar and compass.** Bind a
  readable surface to the tracked left wrist, with comfortable offset and scale.
  Preserve live health and coherent compass heading as head/body rotate.
- [ ] **Reported requirement: right wrist ammo.** Show current weapon ammo
  and relevant reserve information; update during firing, reload and switching.
- [ ] Extract the measured native HUD content/state without duplicating it on
  the old screen plane. Separate HUD text/icons from other menu/subtitle text.
  Hide the legacy HUD only after equivalent wrist information works.
- [ ] Verify both-eye readability, pose stability, occlusion policy and access
  while aiming/reloading. Define behavior when a hand is untracked, during
  pause/loading, and when the wrist is out of view. Keep accepted pause/menu
  placement independent of wrist HUD scaling.

## 5. VR options menu

- [ ] **Reported requirement: expose VR options while playing.** Choose between
  integrating options into the native in-game menu and a dedicated VR overlay
  opened by holding a grip. Both are proposals; menu location and activation
  gesture are not decided yet. If using a long grip, distinguish short presses,
  avoid accidental activation during aiming, and consume gameplay input while
  the menu is open. Keep the menu comfortable and usable with tracked controls.
- [ ] Decide which options to expose: world scale, recenter, turn speed,
  movement deadzone, wrist HUD placement/size, menu distance/size, weapon
  calibration and controller bindings. Indicate changes that need a restart;
  provide reset-to-default and persistent settings. Respect runtime eye poses/
  IPD rather than treating an arbitrary stereo offset as an IPD setting.
- [ ] Verify navigation, input release, apply/cancel, save/reload and return to
  gameplay. Preserve accepted defaults; opening options must not leave firing,
  movement or a grip action held, or unexpectedly change guest simulation speed.

## 6. Calibration, regression coverage and release follow-up

- [ ] **Known gap: physical world scale and relative actor size.** Use a
  documented physical reference and measured game units. Check enemy/world/
  weapon proportions independently; earlier user feedback noted possible small
  enemies. Keep accepted rifle scale/pivot until evidence warrants a change.
  Use runtime eye poses/IPD and never infer units from a detached GTE H sample.
- [ ] Verify head/controller transforms after smooth body turning, recenter,
  save/load and mission changes. Define room-scale leaning/collision limits;
  visual 6DoF alone does not establish player collision behavior.
- [ ] Run representative playthrough coverage beyond Mission 1: checkpoints,
  death/restart, mission transitions, save/load, long sessions and audio/visual
  load. Record reproducible failing locations and the first divergence.
- [ ] Recheck per-eye RAM/asset/GPU rollback and watchdog recovery when adding
  new weapon/level/HUD paths. Existing restore proofs stay valid for their tested
  scope; they do not cover every future asset or level path.
- [ ] **Reported requirement: test other headsets and OpenXR runtimes.** Build
  a compatibility matrix beyond the currently tested Quest 3/VDXR combination.
  Record headset, runtime/version, connection method, controllers, refresh
  rate, eye resolution/FOV and actual runtime IPD. Prioritize additional testers
  and available hardware; device-specific support remains unverified until run.
- [ ] For each headset/runtime combination, test normal VR boot, image
  orientation/color, both-eye projection/parallax, scale, controller bindings,
  weapon grip/aim alignment, wrist/options menus, recenter, focus loss/reconnect,
  exit and sustained performance. Include differing controller layouts and
  action availability; document unsupported features and required remapping.
- [ ] Verify the alpha from a clean checkout/build or intended distribution:
  pinned dependencies, RunVR.bat paths, no required save, normal exit and clear
  unavailable-XR startup behavior. Record executable/config provenance and the
  limitations testers should expect. Alpha publishing itself is a separate task.
- [ ] Prepare upstream PRs for settled generic framework changes using
  docs/UPSTREAM_PENDING.md. Keep game-specific visibility/weapon/HUD logic in
  the game repo and retain focused source-owned tests for framework changes.

## Evidence and working conventions

For each investigation, retain a compact reproduction, a control, actual
measured settings, producer addresses when relevant, and acceptance criteria.
Use TCP inspection and extend it for missing visibility; do not replace it with
printf. Append corrections/results to VR_PHASE9_STATUS.md, inventory framework
changes in UPSTREAM_PENDING.md, and keep bulk captures ignored. Close owned game
processes after tests. Commit/push at the user's authorized checkpoints.

Reference: VR_PHASE9_STATUS.md, VR_LAUNCH_RECEIPT.json, VR_FULL_BOOT_PLAN.md,
VR_HEADSET_PLAN.md, VR_WEAPON_AIM_PLAN.md and VR_WEAPON_VISUAL_RECEIPT.json.
Older pending entries must be read with their later acceptance/correction notes.
