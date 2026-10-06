# VR input: one-stick aiming on a mounted gun

## Why

A mounted machine gun aims with the game's **native left stick**
(`LX` = traverse, `LY` = elevation). Our mapping deliberately splits that stick
across the hands — correct for turn + move, awkward for two-axis aiming.

## How the axes map

`moh_vr_input_map` (vr/moh_vr_input.c) writes the emulated pad from the OpenXR
sticks:

| Pad axis | Native role | Source |
| --- | --- | --- |
| `lx` | turn / traverse | right hand stick X × `turn_gain` |
| `ly` | move / elevation | left hand stick Y |
| `rx` | strafe | left hand stick X |

The right stick's Y is otherwise **unused** (only its X turns), so merging it
into `ly` gives one hand both axes and changes nothing else in ordinary play.

## Trigger

`vr_controller_source` passes `merged_y` when the **left grip** is held
(`input.squeeze[0] >= .55`). It is a deliberate action, so the right stick's Y
stays inert in normal play. `PSX_VR_ONE_STICK` adds automatic modes: `0` off
(default), `2` always, `1` only while a carried Thompson/BAR/MP40 is equipped.
The launcher exposes it as `-OneStick N`.

## Probe — looking for a mounted-gun flag (live guest)

An automatic trigger wants a mounted-gun state to read. What the live RAM shows:

- The player is `*(0x8009d654)`, and `*(player+0x388)` is the player's **input**
  struct — the same one holding the weapon id at byte `input+0x55` (that is
  `input+85` in decimal, the offset the weapon code reads).
- Analog read path: `FUN_80075e7c` ← accessors `FUN_80075fb4` / `FUN_80076008`
  ← the input-apply `FUN_8007f300`, which dispatches on the control mode
  `ctrl[0x72]`.
- Sampled in **save state 1** and **save state 0** alike: weapon id **9**
  (silenced pistol) and `ctrl[0x72] == 7`. That branch is `FUN_8007e034`, the
  **aim-assist** that walks nearby entities and snaps the aim — ordinary
  auto-aim, not a gun.
- A mount/dismount A/B (pressing Use/Square to leave the gun) was contaminated:
  the same diff moved the level globals in `0x800AB8xx` (the visibility-queue
  area), i.e. it triggered a scene transition rather than a clean dismount.

**Conclusion:** the sampled states were not manning the gun — the weapon id and
control mode are identical to ordinary play — so no flag could be read off them,
and the grip modifier is the trigger of record. To gate automatically, compare a
savestate taken *while on the gun* against a normal one, or find the mount/use
handler and read what it sets.
