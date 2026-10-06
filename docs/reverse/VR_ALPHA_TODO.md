# VR alpha follow-up checklist

Updated: 2026-10-07. Baseline: published v0.1.3 (`c72ddc95`), framework
`3618bc00`, Quest 3 / Virtual Desktop VDXR. Current build/layout and evidence
are in [VR_HANDOFF.md](VR_HANDOFF.md); controls are in
[ALPHA_README.md](../../ALPHA_README.md). This is the single active backlog.

Follow the numbered order below, with jitter explicitly shelved by the user.
All ten supplied weapon tracking controls are visually accepted and released;
remaining combat/special-mode/asset checks are separate. Batch future headset
checks, defaulting to 30 seconds with movement enabled. Cleanup does not start
or reprioritize any implementation task.

Unchecked items remain open. **Reported** is user-observed; **known gap** is an
implementation/validation limit; **proposal** needs a layout choice before coding.
Preserve the accepted normal boot/menu, stereo, color/visibility, locomotion,
weapon calibration and pacing. World scale remains provisional; rejected movie
bicubic and jitter experiments are excluded.

## 1. World rendering and level correctness - highest priority

- [ ] **Shelved: garbled and shaky world, even while stationary.** Reproduce
  in a fixed scene. Compare flat/native, neutral stereo, fixed synthetic head
  pose, and actual tracked poses. Establish whether vertices/packets change at
  a frozen guest checkpoint, whether small pose noise is amplified, or whether
  the visible effect comes from frame delivery. Investigate precision,
  transforms, packet interpretation and draw order as hypotheses; do not call
  it tracking jitter or a PSX limitation without a control. Success: stable
  geometry with a fixed pose and no new folding/corruption relative to flat.
  Frozen render views stop the shaking, but are diagnostic. Continuous-pose
  experiments were unchanged/possibly worse; tolerance 1 reduced world/weapon
  shaking while worsening sky/compass. Candidates remain outside the release.
  Commit `04624df` in the local retired-branch bundle preserves the experiment;
  its walking test worsened distant-enemy stability. See the handoff for its path.
  Resume only when requested, then investigate draw provenance before accepting a fix.
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
- [x] **Head turns lose world tiles in the tested slot-0 scene.** Per-eye
  AABB tree selection is merged in PR #2 and included in v0.1.1. User accepted
  movement-enabled Quest 3 / VDXR gameplay on 2026-10-05. Eye FOV and native
  sector visibility are unchanged; broader coverage remains open above.
- [ ] **Known gap: stereo coverage across levels and render variants.** Extend
  beyond the measured scenes: terrain, dynamic enemies, animated objects,
  effects and later-area paths. Preserve true parallax for world geometry.

## 2. Headset color and presentation

- [x] **Headset brighter / lower contrast than flat: accepted correction.**
  The user accepted the color repair on Quest 3 / Virtual Desktop VDXR on
  2026-10-05. Framework `3618bc00` prefers an sRGB XR swapchain and explicitly
  decodes display-encoded source RGB for linear-only runtimes, preserving
  desktop presentation gamma. Real-GL fixtures cover gameplay and native
  copies, both formats, gamma and incoming framebuffer-sRGB state. No blanket
  brightness multiplier. Other runtimes and separate native-menu perceptual
  acceptance remain unverified; see the latest VR_PHASE9_STATUS.md entry.
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
- [ ] **Known gap: remaining weapon combat and asset validation.** Inventory each obtainable
  weapon and record model/shot identifiers, supported pose path and results.
  Test equip/switch, mesh and grip, muzzle/shot alignment, ammo, fire/release,
  reload, recoil, animations, sound and damage. Cover grenades/projectiles and
  scoped/special aim behavior where present; do not assume firearm ray logic.
  Native inventory now identifies ten weapon IDs across multiplayer slots 1–5.
  The released implementation adds guarded mesh profiles for all ten, retaining animated
  gun parts, and weapon/actor pairs for the common shot constructor. Rifle,
  Thompson and fragmentation-grenade single-player controls pass. All ten IDs
  now pass desktop mesh and native shot/first-motion controls. The prepared batch
  runs nine multiplayer cases and single-player Thompson, movement enabled for
  30 seconds each. Native firing was not observed in slot 3 MP40 / slot 5 Thompson
  controls; verified alternatives are used. The user accepted visual tracking for
  all ten headset controls on 2026-10-06. Firing/damage, special modes and broader
  single-player assets remain open. See VR_WEAPON_TRACKING.md and the historical
  VR_WEAPON_CONTROLS_RECEIPT.json; keep this item unchecked until those gates pass.
- [ ] **Passport and silenced pistol: headset acceptance.** The user replaced
  slot 6 with a paused passport/silenced-pistol checkpoint. Guarded mesh profiles
  cover IDs 7 and 9; passport keeps native show/use animation without projectiles
  or ammo edits, and silenced pistol uses the native 5110 shot path. Desktop mesh
  and pistol shot controls pass. Test 30 seconds each with movement enabled;
  assess passport activation and grip, then pistol alignment/reload/hits.
  User feedback: initial passport too small; initial pistol omitted suppressor.
  Revised candidate restores the native suppressor and enlarges passport 3x;
  repeat desktop controls pass. User approved integration into master on
  2026-10-07; live retest remains open. See VR_WEAPON_BATCH.md.
  The original Thompson fixture is preserved separately.
- [x] **All ten supplied weapon tracking controls: visual acceptance.** Quest 3 /
  VDXR ten-case batch completed on 2026-10-06; user reported all tracking looks
  good and requested pushing to master. Broader validation remains above.
- [x] **Jump on right-stick click.** User-requested gameplay mapping delivers
  native Triangle. The former left-Y jump binding is removed; menu routing is
  unchanged. Press/release/activity/focus and combined-input regression pass.
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
