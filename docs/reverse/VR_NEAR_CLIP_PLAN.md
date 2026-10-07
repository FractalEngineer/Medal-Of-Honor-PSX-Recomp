# Near-plane clipping for VR world geometry

Status: a vertex pin is implemented in the framework and awaiting a headset check.
Flat play and native projection output are unaffected by construction.

## Implemented: vertex pin in RTPS

`gte_rtps_internal` (`psxrecomp/runtime/src/gte.cpp`) now pins a vertex that falls
behind the eye onto the near plane, before the depth FIFO push and the
perspective divide, so the depth the game tests stays in range and the polygon is
kept:

```c
if (s_render_pose.projection) {
    const int64_t pin = (int64_t)gte_near_pin_sz() << 12;
    if (pin > 0 && mac3 < pin) mac3 = pin;
}
```

Two properties make this safe to try:

- The gate is exact. `s_render_pose` is only written through the render-view call
  the eye passes use (`gte.cpp:568`), and `.projection` is set only for the eye
  projection (`gte.cpp:956`). Native and flat play cannot enter the branch, so
  their projection output is unchanged regardless of the knob.
- It is tunable at runtime. `PSX_VR_NEAR_PIN` sets the near plane in SZ units
  (default 64); `PSX_VR_NEAR_PIN=0` disables the pin for an A/B in the headset.

This is a pin, not a clip: the vertex is placed on the near plane along its own
view ray, not on the polygon edge where the edge crosses it. A surface that meets
the player therefore stretches toward the near plane instead of being cut cleanly
along the edge. That is why it is cheap and why it may smear very close geometry.

If the pin is not good enough visually, the follow-up is a real edge clip at
submit: intersect the polygon's edges with the near plane using the camera-space
vertices the projection ring already holds (`gte.cpp:505-523`) and submit the
remaining polygon. That needs the triangle-to-ring matching described below.

## Symptom

World geometry close to the player is missing: the floor under the player does
not draw, and standing against a wall shows through it. In VR it reads as
popping, because the hole tracks the head instead of staying put.

## What it is not

- Not the head-facing box selection. With `-NoHeadFrustumDiagnostic` (the old
  body-camera selector) the hole is unchanged, so the selector is not deciding
  this either way.
- Not the box test itself. `vr/moh_vr_frustum.h` ORs an "inside" bit per corner
  across all eight corners and rejects only when `inside != 31`, i.e. only when
  every corner is outside the *same* plane. A box the eye is inside, or one
  straddling a plane, cannot be rejected by construction.

## Mechanism

RTPS/RTPT projects one vertex at a time. A vertex behind the camera plane has
its Z clamped and projected anyway, with the saturation flags set; the polygon
test downstream then discards the whole triangle. A floor tile or wall panel is
one large polygon, so a single behind-camera corner removes the entire surface.

Flat play hides this because the camera's forward axis points away from the
ground at the player's feet — the straddling case is off-screen. Looking down in
VR, or standing close to a wall, puts exactly those vertices behind the plane.

## Why the fix is not in the GTE

`gte_rtps_internal` (`psxrecomp/runtime/src/gte.cpp:858`) processes one vertex
and has no access to the polygon's other corners, so it cannot clip. By the time
the triangle is assembled the behind-camera vertex is already projected.

## Where the fix goes, and what it needs

At triangle submit, clip against the near plane in camera space.

The framework already holds every input needed, always on:

- `gte.cpp:505-523` — the projection ring records each RTPS/RTPT's model-space
  vertices (`V0`/`V1`/`V2`), `RT` and `TR`, alongside the screen XY, SZ and FLAG
  outputs. Camera-space Z is `RT·V + TR`.
- `gte.cpp:956-968` — the render-pose override, which is where the eye view
  enters the projection in VR.

Shape: at submit, look up the ring entries for the three vertices; if any
camera-space Z is at or behind the near plane, clip the triangle against that
plane using the camera-space vertices and submit the remainder rather than the
original. Enable it behind its own switch so native and flat play stay
byte-identical by default.

## Placement: framework, not mod

The mod API exposes render passes (`psx_mod_render_pass`), stereo frames, render
views and OpenXR views, but nothing per triangle. A mod can wrap the pass, not
the primitives, so this cannot be a plugin-side fix.

The clip belongs in the framework's GPU command path
(`psxrecomp/runtime/src/gpu.c`, with the renderers behind it), which does see
every incoming primitive.

## Matching a submitted triangle to its projection

The GPU only receives screen-space vertices; the game has already run the GTE by
then, so the behind-camera information is gone by the time the primitive
arrives. The ring closes that gap: it stores each projection's `SXY0`/`SXY1`/
`SXY2` outputs (`gte.cpp:520`) alongside the model-space inputs. Matching an
incoming triangle's three screen coordinates against a ring entry recovers that
entry's camera-space vertices, which is what the clip needs.

This correlation is the risky part of the design: it has to be unambiguous (a
repeated vertex triple must not bind to the wrong entry) and cheap enough to sit
in the submit path. Validate it on captured geometry before anything ships.

## Verification required before it is trusted

Same standard the accepted visibility fix used
([VR_HEAD_VISIBILITY_FIX.md](VR_HEAD_VISIBILITY_FIX.md)):

- Frozen-checkpoint eye images at one guest cycle, selector on and off, to show
  the near geometry returning and nothing else moving.
- The 96-field native fingerprint control, to prove native projection output is
  untouched while the switch is off.
- The stereo rollback transaction, so a bad clip cannot publish a mixed pair.

Do not fold this into the head-frustum change; keep it a separate pinned change
so either can be dropped independently.
