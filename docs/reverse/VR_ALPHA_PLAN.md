Current priority, 2026-10-05: user shelved jitter and requested weapon tracking.
Work on `feature/vr-all-weapon-tracking` from master `b3033cc` (v0.1.1), with
framework pin `3618bc00`. Preserve `fix/vr-jitter-tolerance` at `04624df`.
This explicit priority supersedes the ordering below. Hardware checks remain
batched; future gameplay runs default to 30 seconds with movement enabled.
See [VR_WEAPON_TRACKING.md](VR_WEAPON_TRACKING.md) for current evidence and gates.

# VR alpha follow-up execution plan

2026-10-05 workflow: user requests several candidates saved separately on
fix/feature branches, then one batch of headset checks. Continue TODO order;
brightness was explicitly brought forward during the world investigation.
Keep verified fixes separate from diagnostic controls and hypotheses. Save
local branch commits and isolated candidate binaries/configuration receipts;
provide baseline/candidate launches together. Do not request headset testing
for each intermediate experiment. Unaccepted candidates stay off develop.
See [VR_HEADSET_BATCH.md](VR_HEADSET_BATCH.md) for the current batch.

Planned: 2026-10-04. Read [VR_HANDOFF.md](VR_HANDOFF.md) and
[VR_ALPHA_TODO.md](VR_ALPHA_TODO.md) first. This plan follows the TODO's section
and item order. It records work to do, not completed fixes or new headset proof.
The released framework pin is `9976567e`; the TODO's `5bafeebf` identifies the
earlier gameplay alpha checkpoint.

Preserve accepted normal boot, coherent genuine eye pairs, menus, movement,
rifle appearance/calibration and pacing. Keep WorldScale 3, rifle units/meter
850, pivot (80,150,100), projection precision 16 and menu distance/width 2m.
Framework changes go in the separate framework checkout; game-specific
visibility, weapons and HUD belong here. No release or upstream submission is
part of this plan.

## 1. World rendering and level correctness

Work through these investigations in order. A shared root cause may close
several items, but each report still needs its own reproduction and verification.

| Order | Investigation | Concrete work and completion gate |
| --- | --- | --- |
| 1.1 | Stationary corruption/shake | Start with an existing rifle scene after verifying the local slot. Freeze one guest render checkpoint. Compare ordinary flat/native, neutral stereo, fixed synthetic head pose, then live tracking. Keep source/projection/resolution settings explicit. Compare pre-projection vertices, producer-bound GTE state, packet fields/order and left/right restore results. Separate geometry changes from pose noise and delivery cadence. Gate: repeated fixed-pose redraws retain stable geometry and introduce no folding relative to flat. |
| 1.2 | Missing nearby floor / black lower polygon | Use the same scene, looking down and leaning, then a dedicated failing position if necessary. Locate the first affected primitive or missing submission in both eyes and flat. Check near clipping, saturation, winding, packet order and visibility separately. Identify whether the black polygon shares the floor producer. Gate: expected floor is visible and no covering artifact; document separate causes if present. |
| 1.3 | Pop-in | Record a slow route and head-turn sequence with native and VR views. Trace candidate/accepted geometry before GPU submission, portal/sector selection, distance tests and streaming. Expand measured game visibility for the headset view only where evidence requires it; check fixed queue capacities and frame costs. Gate: reduced added VR pop-in with geometry/submission and cost evidence. |
| 1.4 | Mission 1 ruins | Obtain a save immediately before the first broken tile plus the route from cold entry. Compare cold versus restored entry, flat versus VR and both eyes across the transition. Trace mesh/texture uploads and replay restoration. Gate: repeated ruins traversal has no new missing/transparent tiles. |
| 1.5 | FOV / large head turns | Exercise side/behind, up/down, leaning and smooth body turns in several sectors. Separate near clipping from bounds/portal/distance rejection. Gate: a recorded coverage matrix, with remaining limitations explicit. |
| 1.6 | Levels / render variants | Extend verified paths to terrain, enemies, animated objects, effects and later areas. Gate: true parallax and clean per-eye restore across representative variants. |

First implementation decision: fix the earliest measured divergence at its
owner. A faithful GTE/GPU/restore defect requires a generic framework fix;
game visibility and VR transform enhancements stay in the game mod. Do not
adjust world scale or rifle calibration to hide corruption. Add missing TCP
inspection before choosing a fix; do not use screenshot hashes as geometry proof.
Useful existing seams are gameplay replay `FUN_80090B80`, level RTPS
`FUN_8008B3E8` (producer writes H=400), and dynamic-object `FUN_800824D0`.
These are starting references, not proof of a new fault's producer.

## 2. Headset color and presentation

1. **Color/contrast:** follow the same frame from native GPU output through
   source texture, XR swapchain format and submitted layer. Compare native
   quads and gameplay projection; record VD/headset display settings separately.
   Fix a demonstrated encoding/format error at its owner. Gate: documented color
   path and matched reference, with user headset assessment.
2. **Cadence:** measure guest VBlanks, redraws, coherent pairs, submissions and
   compositor timing in ordinary/heavy play. Design any headset-rate redraw
   scheduling around frozen guest checkpoints and fresh poses, retaining guest
   simulation speed. Gate: measured delivery improvement and user comfort;
   submission counts alone are insufficient.
3. **Surface handoff:** stress the four-VBlank inactivity policy during stalls,
   loading, briefing, pause, death, completion and video. Replace it with measured
   scene/state signals or a justified hardened policy if it misclassifies play.
   Gate: repeatable transitions without unintended flat/stale/blank frames.
4. **Movies:** compare decoded native source to an independent reference before
   considering decoder changes or replacement assets. Keep rejected bicubic
   removed and accepted sound/video pacing intact.

## 3. All weapons, aiming and controller layout

The requirement is **implement tracked mesh and aiming support for every
obtainable weapon**, then validate it. Testing alone is not completion.
Proceed with weapon implementation now, following the latest user priority. Preserve the measured rifle profile.

1. **Physical shot baseline:** arm shot tracing before firing the rifle at a
   visible target. Compare barrel, native/overridden shot origin/direction and
   damage at near/far ranges. Vary hand, head and body independently; include
   turning, recentering, reload/recoil and a close wall. Gate: rendered aim and
   actual hits agree under these controls.
2. **Inventory and implement every weapon:** identify each weapon from actual
   game captures; record equip/model IDs, owning player, mesh/node/face layouts,
   transform/animation producer, grip pivot/scale, muzzle and firing producer.
   Add measured per-weapon profiles for tracked geometry, arm isolation and
   shot handling. Trace firearms, projectiles/grenades and scoped/special modes
   independently. Do not extend rifle id 5110 or node 22 by assuming shared
   layouts. Decline unknown layouts safely and expose the reason through TCP;
   an unsupported fallback remains an open coverage item. Validate ownership
   so NPC/P2 shots and models are never redirected by P1 tracking. Gate: each
   inventory row passes switch/equip, grip, shot/damage, ammo, press/release,
   reload, recoil/animation, sound and restore tests in its actual game modes.
3. **Remove legacy aim:** after weapon coverage is verified, remove the temporary
   right-grip native aim mapping and unwanted native camera/zoom. Preserve any
   required scoped/special behavior through a deliberate replacement.
4. **Choose right-button layout:** decide A/B reload/weapon assignments and Use
   placement at this step. Square's native reload/use context needs door/switch
   tests. Publish the final bindings and retest simultaneous movement/fire.
5. **Combat recovery:** controlled controller loss, sleep, focus and VDXR
   reconnect tests. Gate: released inputs, no stale hand/shot, coherent recenter
   and recoverable rendering.

### Two-controller reference launcher

From the game root, double-click **RunWeaponCapture.bat**. It starts normal flat
play with P1 and P2 as independent digital keyboard controllers, disables
multitap and VR, skips the host launcher, and verifies both ports through TCP
`pad_status`. Select multiplayer in the game's own menu. No headset is needed.

| PS1 input | P1 key | P2 key |
| --- | --- | --- |
| Up / Down / Left / Right | Arrow keys | I / K / J / L |
| Cross / Circle / Square / Triangle | X / S / Z / A | P / O / H / U |
| L1 / R1 / L2 / R2 | Q / W / E / R | N / M / B / V |
| L3 / R3 | T / Y | F / G |
| Start / Select | Enter / Right Shift | Tab / Left Shift |

Optional physical-controller assignment:

```powershell
.\RunWeaponCapture.bat -Player2Device gamepad
.\RunWeaponCapture.bat -Player1Device gamepad -Player2Device gamepad
.\RunWeaponCapture.bat -DiscPath "C:\path to disc\medal-of-honor.cue"
.\RunWeaponCapture.bat -CheckOnly
```

`gamepad` uses available SDL controllers in player order; connect distinct
physical pads for the two-gamepad option. TCP connection status proves a PS1
port is connected, not that an assigned physical controller delivers input.
The launcher changes only the adjacent `settings.toml` and `keybinds.ini` for
the session, including temporary P1 bindings. Closing the game restores their
original bytes. Original files also remain under ignored
`analysis/weapon-capture/config-<id>/` for recovery if the PowerShell process is
forcibly terminated. Use the printed backup for manual restoration in that case.
Save states/memory cards persist; controller settings restoration does not
remove them. The development executable needs TCP debug tools enabled.

### Capture protocol

**User capture update, 2026-10-04:** the multiplayer weapon-set saves are now
available, beginning at slot 1; characters already face each other. Slot 0 is
reserved for the direct in-game single-player scene and is the starting point
for section 1.1. For weapons, load the relevant multiplayer set, switch weapons
and fire. The user authorizes replenishing zero ammo where needed; first measure
the current player's weapon/ammo structure, change only that diagnostic state,
and retain native fire/reload/damage behavior. Do not persist edits over the
source saves. [VR_SAVE_STATE_INVENTORY.json](VR_SAVE_STATE_INVENTORY.json) records
available files/hashes; ignored copies preserve the original bytes. Exact
weapon names/slot assignments remain to be observed, including older higher
slots. Existing historical slot-3/5 single-player descriptions are superseded.
Initial follow-up branches were local `develop` in both checkouts; weapon work
is now on `feature/vr-all-weapon-tracking`; the pinned
game submodule stays at the release version until framework changes are ready
for the documented checkpoint/pin workflow.

- For the first rendering task, retain a stationary failing-world scene; for
  the ruins task, retain a state before the first affected location and its route.
- For later weapons, place P1 facing stationary P2 at a clear near/mid range.
  Save before firing, with one weapon equipped and both players' inputs released.
  Record mode/map, weapon name, distance, ammo and available alternate weapons.
  Use P2 as the damage target; include a farther position where practical.
- Inventory existing slots before saving. The runtime has slots 0..11; use the
  latest slot contract above and verify each actual load. Preserve useful existing
  states and archive named copies locally when twelve slots are insufficient.
  Retain each original `.pst` filename inside its named capture directory so
  it can later be copied back to a chosen slot deliberately. Keep binaries ignored.
- Use the runtime's configured save-state-menu hotkey (default **F7**), choose an
  unused slot, then **S** to save. For scripted saves use TCP
  `{"cmd":"savestate","op":"save","slot":N}`, then
  confirm `savestate_status` completed successfully. Record the executable/pin,
  actual save path, slot and completed generation; verify a load before relying
  on a capture. Do not assign reserved slots without checking local files.
- Multiplayer is a source of model/fire/damage references. Check its player
  records, viewport/camera basis, weapon availability and firing paths against
  single-player before reusing a profile. Add single-player enemy states for
  weapons or special modes missing/different in multiplayer. A split-screen
  capture is not evidence that multiplayer stereo itself is supported.

## 4. Wrist UI

Implement left health/compass first, then right ammo/reserve. Measure native HUD
state and distinguish HUD primitives from menus/subtitles before hiding the old
HUD. Keep wrist transforms in the shared tracked/recentered origin. Validate
live updates, compass heading during head/body turns, both-eye readability,
pose stability and aiming/reloading access. Define occlusion, untracked/out-of-
view hands, pause/loading behavior. Gate: equivalent useful information works
on wrists before legacy HUD removal; accepted menu placement remains separate.

## 5. VR options

Choose native-menu integration versus a dedicated overlay at this stage; no
grip gesture is preselected. Then expose measured settings with persistent
defaults/reset, apply/cancel and restart labels. Include recenter, turn/deadzone,
world scale, wrist/menu placement, weapon calibration and bindings as appropriate.
Keep runtime eye poses authoritative for IPD. Gate: navigation and return to
play release held actions, retain guest speed, and survive restart/settings load.

## 6. Calibration, coverage and release follow-up

Follow the TODO order: physical reference/world and actor proportions; transform
regressions after turning/recenter/save-load/mission changes; representative
playthroughs; per-eye restore/watchdog controls for all added paths; additional
headsets/runtimes and their full input/render/reconnect matrix; clean build and
distribution checks; then settled generic upstream PR preparation. Retain the
accepted rifle pivot/scale until a separate measured calibration justifies change.
Publishing and pushing require their own user-authorized checkpoint.

## Evidence and session boundaries

Each item gets a compact reproduction, matched control, actual active settings,
first divergent producer/path, focused verification and user assessment where
hardware perception matters. Append results/corrections to VR_PHASE9_STATUS.md;
update both upstream inventories for framework changes and the framework timing
status during framework work. Bulk evidence stays ignored. Close owned test
processes. Leave TODO boxes open until their completion gates pass.

Next work: **1.1 stationary corruption/shake controls**. First use available
local states and synthetic poses; request headset availability only for the
live perception/tracking comparison. No new rendering diagnosis is asserted
by this planning checkpoint.
