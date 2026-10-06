# Weapon and passport headset controls

Prepared on 2026-10-06 on `feature/vr-all-weapon-tracking`, rebased onto master
`1b4f8ae` (v0.1.2). Framework `3618bc00` and the accepted color correction remain
the baseline. Experimental jitter code is excluded. On 2026-10-06 the user
completed the ten tracked Quest 3 / VDXR controls and accepted all weapon tracking.
This establishes visual tracking acceptance; damage alignment and special-mode
behavior remain separate checks. The changes are integrated into master.

Connect Quest 3 through Virtual Desktop / VDXR, then run from the game root:

```powershell
.\RunVRWeaponBatch.bat
```

The current batch adds passport and silenced-pistol candidates to the ten accepted
weapons, with **30 seconds of gameplay per item** and
movement enabled. Each case loads a fresh copy of its original save,
selects the weapon through native Circle presses, validates the equipped ID/model,
and waits for the native equip cooldown before starting the clock. Controller
input and hand tracking are temporarily neutral during preparation; both
diagnostic overrides are cleared before a headset case uses live tracking.
Startup and
weapon preparation add time between cases. Each owned game closes after its case.
Nine controls use multiplayer, with player two facing player one. Current slot 6
is a paused single-player checkpoint with passport equipped, then silenced pistol
after one native switch. Preparation resumes it with native Start. Thompson uses
the preserved original slot-6 fixture, copied only into its isolated test folder.

Right trigger fires; hold/release it for grenades. Left stick moves, right stick
turns, right-stick click jumps, and left X reloads/uses. B cycles weapons, but
leave the selected weapon
equipped during an individual control. Right grip's existing native aim/zoom
binding remains; assess scoped behavior separately. The launcher uses the new
master's 1080p internal-resolution default. It requires Python and the locally
built `build-release` executable.

For a comparison, `native` retains the game's weapon pose/aim while the view
still follows the head. `tracked` uses the right hand. Compare runs each weapon
twice in that order, with 30 seconds for each mode:

```powershell
.\RunVRWeaponBatch.bat -Mode compare
.\RunVRWeaponBatch.bat -Weapon shotgun
.\RunVRWeaponBatch.bat -Weapon bazooka -Mode compare
.\RunVRWeaponBatch.bat -Weapon passport
.\RunVRWeaponBatch.bat -Weapon silenced-pistol
.\RunVRWeaponBatch.bat -List
```

Assess hand attachment, apparent size, gun parts in both eyes, recoil/reload,
barrel-to-shot direction and actual hits. Hold the controller steady while moving
the head, then hold head/body steady while aiming the hand. Walk and fire during
each case. For the shotgun check multiple impacts; for rockets and grenades check
native collision/flight and delayed release. Report each weapon's result by name.
The common rifle calibration is provisional for the other weapons.

| Weapon | Key for `-Weapon` | Mode | Original slot | Native switches |
| --- | --- | --- | ---: | ---: |
| M1 rifle | `rifle` | Multiplayer | 3 | 0 |
| Pistol | `pistol` | Multiplayer | 1 | 0 |
| BAR | `bar` | Multiplayer | 1 | 1 |
| Thompson | `thompson` | Single-player, preserved fixture | 6 | 0 |
| MP40 | `mp40` | Multiplayer | 4 | 1 |
| Shotgun | `shotgun` | Multiplayer | 4 | 0 |
| Scoped rifle | `scoped-rifle` | Multiplayer | 2 | 1 |
| Bazooka | `bazooka` | Multiplayer | 1 | 2 |
| Fragmentation grenade | `frag-grenade` | Multiplayer | 1 | 3 |
| Stick grenade | `stick-grenade` | Multiplayer | 2 | 3 |
| Passport | `passport` | Single-player | 6 | 0 |
| Silenced pistol | `silenced-pistol` | Single-player | 6 | 1 |

Recipes are source-owned in [weapon_controls.json](../../vr/weapon_controls.json).
For a two-item headset batch:

```powershell
python vr/weapon_batch.py --weapons passport silenced-pistol
```

Hold/release the right trigger to show the passport using its native animation
and interaction. It is not a damage weapon, and the harness never edits its ammo.
For the silenced pistol, assess hand attachment, barrel direction, recoil/reload
and actual hits. The revised model restores the original suppressor; passport
is now 3x its initial tracked size. These two profiles need headset retesting;
the previous ten remain accepted.

The Thompson recipe pins the earlier slot-6 save and its SHA-256 in the manifest:
`analysis/weapon-capture/native-inventory-20261005-final/session/saves/openbios/state_8001DFD4_slot06.pst`.
Keep this local fixture; do not restore it over the user's new source slot 6.
MP40 uses slot 4 because its native firing control passed there. The slot 3
MP40 control did not observe a native constructor; its cause remains unestablished.
The slot 5 multiplayer Thompson control also observed no native shot; slot 6
supplies its firing comparison. Both multiplayer mesh controls passed. Those
fixture limitations are recorded rather than bypassing native fire logic.
The harness checks source-save hashes after running and uses its own TCP port
4372, refusing an occupied port. It passes an explicit `--memcard-dir` for its
copied saves and never loads or saves through another game.
For damage weapons, zero clip/reserve ammo gets one shot / 20 reserve rounds
in the isolated process; native reload and depletion continue. It does not replenish nonzero ammo
or change source saves, controller settings or the original loadouts.
Ammo changes are checked after guest frames to ensure they survive eye rollback.

All normal and weapon launchers share the current `build-release` target.
Use `-BuildDirectory` only for an explicitly separate candidate; check the
executable path and stamp before comparing results.

## Scope and evidence

The opt-in `PSX_VR_WEAPON_MP_CONTROL=1` path reconstructs **player one's native
view only** inside each eye transaction. It calls native camera selection,
scene callbacks and ordering-table submission, using the measured 512x120 source.
It excludes the enclosing wait/flip and second-player view from eye rendering.
Native simulation still includes both players. Mesh/shot hooks use the measured
multiplayer addresses and retain owner, freshness, code and asset guards.

This test view omits the native split-screen HUD, retains multiplayer world
visibility selection, and does not add general multiplayer headset/menu support.
The accepted single-player head-frustum fix remains intact; its address-specific
hook is not a claim of multiplayer head-turn coverage. These controls establish
native multiplayer weapon paths; all-weapon single-player asset availability and
special/scoped modes remain separate acceptance gates.

Local receipts go under `analysis/weapon-capture/headset-batch-*`. They record the
binary hash, selected model, ammo edits, timed samples, fresh pair/restore counts
and XR status. They set headset acceptance to false until the user reports it.
Expensive rollback verification is off during ordinary headset checks.

The launcher can be checked without a headset; these are synthetic desktop runs:

```powershell
.\RunVRWeaponBatch.bat -Desktop -Verify -Seconds 1
```

See [VR_WEAPON_TRACKING.md](VR_WEAPON_TRACKING.md) and
[VR_WEAPON_CONTROLS_RECEIPT.json](VR_WEAPON_CONTROLS_RECEIPT.json) for the accepted
desktop evidence and remaining hardware gates.
