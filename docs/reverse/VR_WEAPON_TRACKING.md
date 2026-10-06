# Weapon tracking implementation

Current status: the original ten weapon profiles shipped in v0.1.3. Passport
and silenced-pistol tracking, including the restored suppressor and 3x passport
scale, were approved for integration into master on 2026-10-07. Normal and
weapon-control launchers use `build-release`; the published ZIP remains v0.1.3.
Measurements below retain their historical binary/branch provenance.

2026-10-05: the user shelved jitter and moved weapon tracking forward. Work is
on `feature/vr-all-weapon-tracking`, refreshed onto v0.1.2 master `1b4f8ae` on
2026-10-06, with released framework pin `3618bc00`. Jitter remains unaccepted
on the separate
`fix/vr-jitter-tolerance` branch at `04624df`; its movement test regressed distant
enemy shapes compared with the integer-view control. Do not include that
experiment in weapon builds.

The user requested all weapon controls ready for a later headset batch. Launch
[RunVRWeaponBatch.bat](../../RunVRWeaponBatch.bat) for ten movement-enabled,
30-second tracked controls; `-Mode compare` adds a native control for each weapon.
See [VR_WEAPON_BATCH.md](VR_WEAPON_BATCH.md) for recipes and scope. All ten IDs
have passing desktop mesh/shot controls. Nine batch cases use multiplayer; the
Thompson uses original single-player slot 6 because native firing was not observed
from multiplayer slot 5. On 2026-10-06 the user completed all ten tracked headset
controls and reported “all trackings look good”, authorizing integration into
master and a current build. Visual tracking is accepted. Jump now uses right-stick
click; native Triangle press/release remains the game action.

## Barrel laser

The tracked-weapon barrel laser raycasts the level's own triangles so the beam
ends on the first surface ahead and marks the impact with a dot; with nothing in
range it keeps the previous fixed-distance beam and shows no dot. It is aimed by
the rendered weapon's own affine (mesh axis/sign) and is firearms-only — the
passport and other non-firing items show none. The muzzle alignment is
provisional. See [VR_LASER_RAYCAST_PLAN.md](VR_LASER_RAYCAST_PLAN.md).

## Passport and silenced-pistol integration (2026-10-07)

The user replaced source slot 6 with a paused single-player checkpoint: passport
ID 7 is equipped; one native Circle switch selects silenced pistol ID 9. Historical
slot-6 inventory below describes the earlier Thompson loadout. The batch preserves
that older fixture separately and never replaces the current source save.

The passport profile retains both articulated covers (nodes 13/14, 16/20 vertices,
68 faces of 317, 15 total nodes). Its native show/use animation remains driven by
the right trigger; it receives no projectile profile or diagnostic ammo edits.
The user reported the passport too small and the silenced pistol missing its
suppressor. Passport now applies a 3x model scale around the existing grip pivot;
other profiles keep their scale. The original silenced-pistol model contains the
suppressor in a disconnected component within arm node 11: vertices 7-18 and
115-120, 23 faces. Retaining that island plus node 13 (57 vertices, 71 faces)
restores all 94 weapon faces while excluding the arm. It shares the existing
pistol calibration, but cannot share its mesh guard.
Ghidra confirms ID 9 shares the native 5110 firearm constructor; ID 7 follows the
passport-specific action branch in `8007B350`, without creating a damage actor.

Both candidates pass native/straight/translated/rotated/unfocused desktop mesh
controls with fresh eye PNGs and nonzero rollback verification, zero mismatches.
Silenced-pistol native/straight/45-degree/unfocused shot controls observe native
construction and first motion; tracking writes six pose fields only while focused.
All ten previous mesh profiles still match the 60 retained native captures.
Revised receipt: [VR_PASSPORT_SILENCED_REVISION_RECEIPT.json](VR_PASSPORT_SILENCED_REVISION_RECEIPT.json).
Current isolated controls:
`analysis/weapon-capture/silencer-passport-revision-20261007/final-build-controls/`.
The initial receipt retains the earlier binary and measurements.
The user approved the revised changes for commit and push on 2026-10-07.
The rebuilt `build-release` includes both profiles. Recorded verification is
desktop; live interaction/damage checks remain open. See the batch guide for
two-item or individual 30-second movement-enabled checks.

## Native inventory

The user supplied multiplayer saves with players facing each other and authorized
weapon switching, firing and temporary replenishment of zero ammo. Inventory
loads copied saves in an isolated flat process on TCP 4372. Source save hashes
are checked after every run; no source save or controller configuration is written.
Slots 1–5 actually load multiplayer. Slots 0, 6 and 9 load single-player scenes;
there are no local slots 7 or 8. Historical single-player descriptions of slots
3 and 5 are superseded by these captures.

| Slot | Observed mode | Native weapon IDs in cycle order |
| --- | --- | --- |
| 0 | Single-player | 12, 3 |
| 1 | Multiplayer | 8, 6, 2, 3 |
| 2 | Multiplayer | 8, 5, 6, 10 |
| 3 | Multiplayer | 12, 5, 11, 3 |
| 4 | Multiplayer | 4, 11, 2, 3 |
| 5 | Multiplayer | 12, 1, 4, 10 |
| 6 | Single-player | 1, 3, 12 |
| 9 | Single-player | 12, 3 |

Compact evidence: [VR_WEAPON_SET_RECEIPT.json](VR_WEAPON_SET_RECEIPT.json) and
[VR_SAVE_STATE_INVENTORY.json](VR_SAVE_STATE_INVENTORY.json). Raw inventory:
`analysis/weapon-capture/native-inventory-20261005-final/inventory.json`.
The native reference binary is the exact v0.1.1 release build. Split-screen
references establish models and native state; they do not establish multiplayer
stereo support.

## Candidate implementation

The first-person model is named by `player+256` in both modes. Input is named by
`player+904`; `input+84` is a loadout slot and `input+85` is the weapon ID. In
single-player the first-person object is embedded at `input+784`. Multiplayer's
`input+164` is the third-person body/held model and must not be substituted for
the first-person model. Multiplayer's first-person object is at `input+2452`.
Single-player scene/camera globals are `8009D64C` and
`8009D654`; multiplayer uses `80099418`, `80099428` and current player `8009942C`.

Profiles in `vr/moh_vr_weapons.h` match the captured native first-person meshes:

| ID | Weapon label | Native faces | Retained gun faces | Gun nodes | Native shot ID |
| --- | --- | ---: | ---: | --- | --- |
| 1 | Thompson | 382 | 191 | 26–31 | `13F7` / 5111 |
| 2 | Bazooka | 190 | 190 | 2 | `13F1` / 5105 |
| 3 | Fragmentation grenade | 337 | 86 | 23 | `13F3` / 5107 |
| 4 | Shotgun | 334 | 169 | 26 | `13F7` / 5111 |
| 5 | Scoped rifle | 317 | 211 | 22 | `13F6` / 5110 |
| 6 | BAR | 327 | 190 | 26–29 | `13F7` / 5111 |
| 8 | Pistol | 315 | 115 | 13 | `13F6` / 5110 |
| 10 | Stick grenade | 285 | 112 | 21 | `13F3` / 5107 |
| 11 | MP40 | 309 | 200 | 24–27 | `13F7` / 5111 |
| 12 | Accepted M1 rifle | 280 | 174 | 22 | `13F6` / 5110 |

Zero-vertex joints remain part of the gun assembly. Retain magazine/bolt faces
and mixed-node MP40 faces; selecting just one dominant node loses gun parts.
Validate the native total face count, node count, gun-node vertex counts and
selected face count before entering the tracked path. Unknown/mismatched assets
retain native behavior. Remove original arms only inside each eye transaction;
RAM/asset rollback restores the native model. Preserve the accepted rifle's
scale 850, pivot `(80,150,100)` and projection precision 16. Other weapons currently
use that calibration as a candidate and need individual headset assessment.

Local Ghidra headless analysis of the actual captured RAM verifies the native
firearm dispatcher `8007D5C8`, grenade event `8007CF4C`, grenade wrapper `80045808`,
and shared constructor `8004532C`. Its basis call remains `8004551C -> 8006A76C`.
The candidate validates the equipped weapon/shot-ID pair at that measured seam,
with the existing player ownership, camera freshness, code and tracking guards.
It changes origin/angles; native speed, ammo, fire/release, reload, collision,
damage, spread and grenade flight code continue to run. Grenades use the tracked
pitch during their native launch-velocity calculation, then native flight takes over.

Multiplayer code differs: transform `800836C0`, geometry `8007E414`, firing
`8007A150`, shared constructor `80045718`, and basis `8006591C` called at `80045908`.
The first-person mesh layouts match, including the rifle/grenade single-player
controls. The opt-in `PSX_VR_WEAPON_MP_CONTROL=1` bench adds these measured hooks
and native player-one view reconstruction. It preserves two-player simulation,
draws player one into each eye, and uses `8009943C` as shot/mesh owner, rather
than the transient current-player camera global. See the batch guide for the
512x120 render contract, omitted split-screen HUD and native visibility limits.

## Verification and remaining gates

The current [VR_WEAPON_CONTROLS_RECEIPT.json](VR_WEAPON_CONTROLS_RECEIPT.json)
records the tested binary/source hashes, all ten desktop controls and the user's
headset visual acceptance at `c8280a1`. Its binary hash is historical after the
right-stick jump rebuild; it must not be rewritten to claim that binary was tested
in the earlier headset run. The
older [VR_WEAPON_TRACKING_RECEIPT.json](VR_WEAPON_TRACKING_RECEIPT.json) records
the 2026-10-05 single-player checkpoint and is retained as historical evidence.

2026-10-06: all ten IDs pass native/straight/translated/rotated/unfocused mesh
controls with complete fresh eye PNGs, the measured H=133 weapon producer and
zero rollback mismatches. Native/straight/45-degree/unfocused shot controls
observe the native constructor and first movement; tracked shots write six pose
fields, native and unfocused controls write none. The shotgun emits at least six
native pellet constructors in every control. Native automatic-fire spread and
shotgun angle perturbations occur after the hooked basis call and remain native.
Streaming trace collection rejects ring overwrite/truncation and distinguishes
constructor instances when native actors reuse addresses.

Single-player rifle, Thompson and fragmentation-grenade controls also pass on
the current binary. A right-eye abort inside tracked shotgun vertices triggers one
real watchdog, restores with zero mismatches, and recovers to fresh stereo pairs.
The compact receipt names accepted per-weapon files; a failed multi-slot inventory
is not accepted wholesale. Slot 3 MP40 and slot 5 Thompson produced no native shot
in their firing controls, so the batch uses verified alternatives. Causes remain
unestablished; no native fire logic was bypassed.

- Release/OpenXR/OpenGL build in `build-vr-weapons` passes with the strict
  OpenBIOS stamp, framework `3618bc00`, and release UI pin `5de138a8`.
- Compiled profiles match all 60 captured native meshes across all eight saves.
  All ten IDs are covered; malformed gun vertex indices are rejected. Receipt:
  `analysis/weapon-capture/profile-fixtures-20261005/receipt.json`.
- Synthetic stereo native/straight/translated/rotated/unfocused mesh controls
  pass for the rifle, Thompson and fragmentation grenade. Render verification
  reports zero restoration mismatches. Receipt:
  `analysis/weapon-capture/tracked-mesh-controls-20261005/inventory.json`.
- Native constructor/position/direction and first-motion controls pass for rifle
  and Thompson. Delayed fragmentation-grenade controls also pass after refreshing
  synthetic hand samples throughout release; unfocused poses retain native shots.
  Receipts: `tracked-aim-controls-20261005/slot-00/weapon-00/aim_controls.json`,
  `tracked-aim-controls-20261005b/slot-06/weapon-00/aim_controls.json`, and
  `tracked-grenade-aim-controls-20261005c/slot-06/weapon-01/aim_controls.json`
  under `analysis/weapon-capture/`. Earlier incomplete runs are not acceptance.
- Source save hashes remain unchanged and owned diagnostic processes close.

Still open: live firing/damage and pose controls for the other seven IDs in
single-player; physical grip/barrel alignment for each weapon; recoil/reload,
switching, close-wall behavior, shotgun spread, rocket collision, both grenade
types and scoped mode; controller/head/body independence; hardware tracking-loss
recovery; broader gameplay acceptance. The captured ten IDs are the supplied
loadouts, not a proof that every special/NPC weapon ID is player-obtainable.
Keep the broader weapon-validation TODO unchecked. The user authorized merging
and pushing the accepted tracking into master on 2026-10-06. Legacy
grip/aim bindings remain until tracked coverage and special modes are validated.

The next hardware batch should use movement-enabled 30-second runs, as requested,
with the accepted color/visibility baseline and individual weapon references.
Do not launch against or terminate an unrelated user's game on TCP 4370.

Reproduce inventory and asset fixtures from the game root:

```powershell
python vr/inspect_weapon_sets.py analysis/weapon-capture/new-inventory --executable build-release-011/b/Medal_of_Honor__Recompiled.exe --slots 0 1 2 3 4 5 6 9
python vr/check_weapon_profiles.py analysis/weapon-capture/new-inventory analysis/weapon-capture/new-fixtures
python vr/inspect_weapon_sets.py analysis/weapon-capture/new-pose-controls --executable build-vr-weapons/Medal_of_Honor__Recompiled.exe --slots 0 6 --pose-check
```

Use a fresh output directory each time. `--aim-check` adds native synthetic-shot
controls; `--only-weapon-ids` limits those controls. Zero clip replenishment during
aim tests touches only the measured `input+102+2*slot` field in the isolated process.


Prepared hardware launch (when the headset and the game's TCP port are free):

```powershell
.\RunVRWeaponCheck.bat -Slot 6
.\RunVRWeaponCheck.bat -Slot 0
```

Each run defaults to 30 seconds with movement enabled. Slot 6 cycles Thompson,
fragmentation grenade and rifle; slot 0 cycles rifle and fragmentation grenade.
The launcher requires the local `build-vr-weapons` candidate. It does not enable
experimental jitter code, and these commands are not a completed headset test.
