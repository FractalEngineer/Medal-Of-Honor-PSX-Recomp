# Mouse and keyboard port status

For a clear flat-play picture during input testing, enable Image Filtering and
set Smoothing to Sharp, Texture filtering to Nearest, and Dithering to Off; keep
CRT Filter disabled. Smooth scaling and native-grid dithering are framework
defaults even when Image Filtering is disabled. The user confirmed this setting
combination fixes the CRT-like appearance. See the flat-play section of the
root README for the player-facing instructions.

The default-off SLUS-00974 package ports the Underground mouse and keyboard
controls with destination-specific hook addresses, instruction words, player
and camera paths, and weapon IDs. It supports displacement-based look,
configurable gameplay and menu bindings, firearm and grenade zoom, and the
native sniper scope with walking and shot corrections. Do not reuse Underground
addresses or offsets without independent evidence.
Wheel selection now routes through the native
switch path; its instruction was checked with an isolated save, while the
integrated mouse-wheel path has not been independently verified by the agent.
The user reported the earlier gameplay checks were good so far. The aim
binding now connects ordinary-firearm zoom without native R2 aim and sniper
R2 with walking, camera and shot corrections. On the first integrated test,
the user reported incorrect firearm aim, a teleport on leaving the sniper
scope, view jerks on weapon changes, and no throwable mouse look. The current
revision rebuilds the aimed shot basis from the eye to the native target,
prevents the stale-position restoration on scope exit, including capture
release or a weapon switch during scope, and keeps the direct
camera active across native weapon-mode transitions and mode 9. On retest the
user reported the scope exit and weapon switching were okay, but throwables
still lacked aim and aimed firearm shots went to the upper left. The next
revision restores input angles at the common dispatcher branch, including the
control-scheme path that bypassed the previous hook. It also updates projectile
angles and matrix translation along with the aimed basis: the native actor loop
rebuilds that matrix from the stored angles on subsequent updates. These two
latest changes were subsequently reported good by the user. The user requested
right-click aiming for throwables too; grenade IDs 3 and 10 now use the same
opt-in zoom and reticle as firearms, while their ballistic constructor remains
native. The user subsequently confirmed that the final addition works.
An earlier R2-only attempt was removed
because native aim restricts movement. Front-end menu bindings now use
the same no-campaign-player context as Underground, with the destination's
existing multiplayer overlay guard; they still require user gameplay validation. The package
describes these limitations in the UI.

The following evidence records the port and its verification history:

- `docs/reverse/VR_INPUT_ONE_STICK.md` records the campaign player root
  `0x8009d654`, input pointer at player + `0x388`, and weapon ID at input +
  `0x55` in sampled states, including silenced pistol ID 9 in mode 7.
- The retained campaign inventory captures show Thompson ID 1 in mode 8 and
  M1 ID 12 in mode 7. The input dispatcher shares its angle and aim-assist
  code for modes 7 and 8. `FUN_8007ba20` independently maps the accepted
  firearm IDs to those two modes when unscoped. This does not independently
  verify the resulting view or shot alignment for each weapon.
- The retained `analysis/ghidra/moh_ram.bin` and two slot-00 weapon captures
  contain player `0x800ee860`, class 1, and input `0x800eec2c`: the input
  pointer resolves to player + `0x3cc` in those samples. This is evidence for
  those captures only; do not infer all states or reuse Underground's `0x3fc`.
- The retained `analysis/ghidra/decomp/FUN_8007e034.c` computes aim correction
  using its first argument's `+0x228`/`+0x22c` fields and writes corrections
  at `+0x234`/`+0x238`. In `analysis/ghidra/moh_ram.bin`, the dispatcher at
  `0x8007f300` loads `s0` from `a0+0x388`, keeps `a0` in `s4`, and calls
  that routine at `0x8008013c` with `a0=s4` and `a1=s0` for modes 7 or 8.
  The observed instruction at `0x8008013c` is `0x0c01f80d` (JAL); this is
  static evidence, not an approved hook. Live ownership, update order, and
  shot/view consequences still need proof.
- The title-specific decompilations of `FUN_8007f300` and `FUN_8007ba20` used
  above are in `build-release/mkb-runtime-probe/`, not
  `analysis/ghidra/decomp/`. The selector maps aimed scoped rifle ID 5 to
  mode 2; ordinary aimed firearms use modes 0 or 1. Their input branches
  differ from modes 7 and 8, so they cannot safely be enabled by extending the
  existing weapon allowlist alone.
- The analogous mode-selection call is at `0x8007a85c`; its delay slot sets
  `a1` to the input pointer, and `0x8007a864` stores the result to input +
  `0x1c8` (instruction `0xae2201c8`). In the retained dispatcher, modes other
  than 7/8 copy input + `0x178`/`0x184` to player + `0x228`/`0x22c` instead
  of calling the ordinary aim-correction routine. These are static clues,
  not proof that overriding the mode alone preserves scoped locomotion,
  centered crosshair, view, or firing. The reference adjusts all of those;
  do not enable a partial scope just because this selector matches.
- The reference's three-update zoom easing math has been ported, unit-tested,
  and connected to guest state. In the destination capture,
  `FUN_8007a6b0` normally resets input + `0xa4` to 400 at `0x8007b254`,
  `FUN_8007e034` skips ray calculation when that field exceeds 400 at
  `0x8007e06c`, and `FUN_8007b8f4` skips weapon rendering unless it is 400
  at `0x8007b91c`. `FUN_8007caf8` draws the native reticle and checks the aim
  flag at `0x8007cb88`; `FUN_8007e034` writes target coordinates at input +
  `0x1a4` through `0x1ac`. These are static candidate counterparts to the
  reference's focal, ray, weapon and reticle paths. Their guarded SLUS-00974
  register adapters and aiming-sensitivity/zoom calculation are now ported and
  unit-tested and registered; the user subsequently reported the aiming and
  shot corrections working in the supplied gameplay saves.
- The reference's generic shot-basis math is ported and connected.
  In this title the runtime-measured firearm constructor is `FUN_8004532c`
  and builds its native basis at `0x8004551c`; `FUN_8006a76c` writes the basis
  at shot + `0x340`. `FUN_800451b0` selects the camera-origin path when the
  player's input flag `0x100` is set. This static mapping does not prove the
  correct scoped target or justify a new shot hook on its own. A guarded
  candidate adapter now uses that post-basis return, the captured camera eye,
  and input + `0x1a4` target; synthetic tests cover its guards and matrix.
  The adapter is connected behind the opt-in feature; gameplay proof of the
  target and camera relationship is still required.
- For the next adaptation pass, the destination's scope-ray equivalents in the
  retained RAM image are `0x8007e1f0` (subtract the 256-pixel horizontal
  center), `0x8007e1fc` (subtract the vertical center), `0x8007e218`
  (scale focal length), and `0x8007e87c` (target-score comparison). The
  aim-flag locomotion tests are `0x8007f34c` and `0x8007f360`;
  its delay-slot store at `0x8007f354` retains the flags, so the comparison
  scratch value must be adjusted without changing the stored flags. The target's
  camera root is `0x8009d638`/`0x8009d64c` in the retained sample, class 2,
  with its eye position at camera + `0x20c`. These observations are static
  mapping and instruction words are confirmed in the supplied save; they are
  not gameplay proof of the candidate hooks.
  `FUN_800473dc` resolves the camera input's handle at + `0x0c` through the
  actor table at `0x8009d678`; in the retained RAM it resolves to the campaign
  player. The candidate shot adapter checks this ownership relationship.
  The guarded selector, locomotion, and ray register adapters have now been
  ported, unit-tested and connected; the complete scoped camera and shot path
  still requires gameplay verification.
- Read-only inspection of the user's current slot 01 save confirms the scoped
  rifle is weapon 5, unscoped mode 7; player input is player + `0x3cc` and the
  class-2 camera's handle resolves to that player. The candidate camera, view,
  pitch, selector, ray, reticle, weapon, and shot-constructor hashes all match
  this save. It was saved unscoped, so this does not establish scoped movement
  or shot alignment. The save was not modified.
- The destination's camera actor update is `0x800473dc`, called from the
  actor loop with return address `0x8006bdf4`. Its input target-height update
  is `0x8007badc`, called from the player dispatcher with return address
  `0x80080170`. `FUN_8004913c` calls the view builder `0x80048eec` with
  camera input + `0xe4`; the builder writes its matrix at input + `0x100` and
  has the same row scaling as the Underground builder. The register-guarded
  camera, prepared-pitch, precise-view and native correction adapters have been
  ported, tested with synthetic memory and connected. Camera timing and scoped
  view/shot agreement still need runtime evidence.
  The shared shot-basis call returns to `0x80045524`, where the destination
  VR adapter already proves player/shot ownership for its own separate path.
- A Release runtime probe using an isolated copy of slot 0 and the debug server
  did not establish gameplay state: the load request returned `last_ok=0` while
  guest frames kept advancing. A second request after boot had the same result.
  A new gameplay save subsequently supplied in the isolated run loads with
  `last_ok=1`. Older snapshots can be rejected after an execution identity
  change, but the exact reason for this particular failed load was not logged.
- On that new save, reloading the same snapshot before each case and applying
  digital left/right for about 50 frames changed player `+0x22c` from
  `0x04cc0000` to `0x009f0000` / `0x08fb0000`, while the idle control held
  `0x04cc0000`. Player `+0x228` also changed during both turns, so treating
  `+0x22c` alone as a complete direct-look implementation would be unsafe.
  The P1 controller was digital in these samples. With `PSX_DEBUG_TOOLS=OFF`,
  the runtime does not apply the debug server's analog axis override; the
  attempted analog samples therefore did not test analog motion.
- In the same isolated slot, a single debug write of +`0x04000000` to player
  `+0x22c` alone returned to its prior value within 0.5 seconds and produced an
  identical captured frame. Adding the same amount to input `+0x184` as well
  retained the change, changed the captured view, and changed native target
  fields. A native Cross fire produced a new class-`0x13f6` actor in both the
  baseline and shifted cases, with different recorded shot angles. This is a
  single rifle/save test, not proof that shot direction matches the crosshair
  or that pitch, scoped weapons, replay, or update timing are correct.
- The analogous +`0x01000000` debug write to player `+0x228`, with or without
  the same write to input `+0x180`, did not persist in that test; both values
  returned close to their starting values and the target did not change.
  A later static decompilation of `FUN_8007f300` identifies input `+0x178`
  as the native base pitch, while `+0x180` is an aim correction. In the same
  mode-7 save, adding `0x01000000` to player `+0x228` and input `+0x178`
  retained the base pitch after 0.5 seconds and changed the native shot target.
  Setting input flag `0x10` as well made no difference in this case. This
  shifted case produced a new class-`0x13f6` rifle shot with pitch `15073280`,
  compared with `-851968` for the baseline shot. This establishes a candidate,
  not the frame boundary, pitch limits, behavior across control modes, or
  agreement of the projectile and visible crosshair.
- In the retained RAM image a direct JAL to `0x8007f300` is at
  `0x8007af50`, with `a0=s2` and `a1=s3`. The dispatcher reads input through
  `a0+0x388`. Temporarily replacing its first two instructions in the isolated
  runtime with a return prevented a native right turn while the unchanged
  control turned; loading the saved state restored the original code. The
  live function and caller hashes were `0xecc55d75` and `0x2f6f9cab` (FNV-1a),
  matching three retained RAM captures. Caller-register ownership and exact
  application timing still need to be checked at the hook itself. After the
  hook was declared and built with the feature off, the isolated save loaded
  and native digital right-turn still changed yaw from `0x04cc0000` to
  `0x079d0000`; this is one non-regression sample, not full controller testing.
- With the isolated gameplay save, a native Circle press switched input slot/ID
  from 0/12 to 1/3. Replacing the instruction at `0x8007a948` with a temporary
  slot-0 assignment kept slot/ID at 0/12 for the same input. The original
  instruction is `0x00408021`; the patch was confined to the test process.
- `docs/reverse/VR_WEAPON_TRACKING.md` records observed weapon IDs, loadout
  slot, scene globals, and a guarded native shot seam. Its weapon IDs differ
  from the Underground plugin's firearm table.
- `docs/reverse/VR_PHASE9_STATUS.md` records the pause flag and notes that
  native auto-aim uses a target as well as player angles. Changing angles
  alone is not proof of correct shot direction.
- `vr/moh_vr_input.c` documents controller and menu masks used by the
  destination's VR input, but this does not validate mouse capture or the
  complete keyboard scheme.

Remaining verification: front-end menu bindings, every weapon and scene,
persistence and ordinary-controller non-regression have not been exhaustively
checked in gameplay. No physical-headset acceptance is claimed. The user
confirmed the reported gameplay issues and final grenade aiming addition were
working; the automated Release suite passed all eight configured tests.
