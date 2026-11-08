# Mouse and keyboard port status

The Underground mouse and keyboard plugin has not been ported to SLUS-00974.
Its hook addresses, instruction words, structure offsets, and weapon table
must not be assumed to apply to this game. No mouse and keyboard package or
hooks have been enabled here.

Existing destination evidence provides starting points, not a complete port:

- `docs/reverse/VR_INPUT_ONE_STICK.md` records the campaign player root
  `0x8009d654`, input pointer at player + `0x388`, and weapon ID at input +
  `0x55` in sampled states.
- `docs/reverse/VR_WEAPON_TRACKING.md` records observed weapon IDs, loadout
  slot, scene globals, and a guarded native shot seam. Its weapon IDs differ
  from the Underground plugin's firearm table.
- `docs/reverse/VR_PHASE9_STATUS.md` records the pause flag and notes that
  native auto-aim uses a target as well as player angles. Changing angles
  alone is not proof of correct shot direction.
- `vr/moh_vr_input.c` documents controller and menu masks used by the
  destination's VR input, but this does not validate mouse capture or the
  complete keyboard scheme.

Still needed: derive and validate the direct camera update, view and shot
paths, exact instruction words and overlay identities, weapon selection,
scoped firing, crosshair and zoom paths. Then implement the opt-in package,
bindings and sliders, and verify capture, release, rebinding, persistence,
controller non-regression, menus and gameplay at runtime. The reference's
unit tests alone do not establish any of those game-specific properties.
