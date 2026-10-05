# All-weapon tracking candidate

2026-10-05: the user shelved jitter and moved weapon tracking forward. Work is
on `feature/vr-all-weapon-tracking`, based on v0.1.1 master `b3033cc`, with the
released framework pin `3618bc00`. Jitter remains unaccepted on the separate
`fix/vr-jitter-tolerance` branch at `04624df`; its movement test regressed distant
enemy shapes compared with the integer-view control. Do not include that
experiment in weapon builds.

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
the first-person model. Single-player scene/camera globals are `8009D64C` and
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
controls. New runtime hooks remain single-player scoped.

## Verification and remaining gates

The compact [VR_WEAPON_TRACKING_RECEIPT.json](VR_WEAPON_TRACKING_RECEIPT.json)
records the candidate binary/source hashes and accepted desktop evidence.

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
types and scoped mode; controller/head/body independence; per-eye watchdog
recovery with new assets; Quest acceptance. The captured ten IDs are the supplied
loadouts, not a proof that every special/NPC weapon ID is player-obtainable.
Keep the TODO unchecked and do not merge/publish this candidate yet. Legacy
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
