# Wrist HUD: preserve native rendering

Updated 2026-10-15. This replaces the earlier custom-instrument proposal.

Render the game's own health/compass, owned ammo text and weapon icon inside an
isolated, restored render transaction. Draw against black and white mattes to
recover transparency, including opaque black artwork. Extract the two owned
regions from this HUD-only image. Scenery, subtitles and notification flashes
are excluded from this capture. Preserve the native art, fonts and formatting.

Project these pixels onto transparent quads at the predicted LOCAL grip poses.
Draw into each real stereo eye before OpenXR copy so the headset, application eye
captures and desktop mirror share the same rendering. The framework owns generic
capture/surface/projection; the game plugin owns producers and packet ownership.
Health and ammunition continue to come from the native renderer.

Replacement is opt-in per hand. Suppress only owned packets and text entries
after that wrist's capture and grip are valid. Lost or stale tracking, unfocus,
unsupported overlay/dimensions and capture failure restore the ordinary HUD.
Requests expire every frame. Pause keeps its existing native menu surface.

Separate OpenXR surface swapchains were tried but displayed stale pixels on one
surface with the local runtime. The host eye pass removed that failure and makes
the final submitted pixels inspectable by unattended checks.

## Acceptance gates

1. Reconstruct the black/white mattes from extracted RGBA and reject unrelated
   content outside the HUD regions.
2. Inspect both compositor eyes with synthetic grip translation, rotation,
   recenter, loss, recovery and unfocus; assert the corresponding fallback pixels.
3. Load save slot 0 into gameplay. Fire, reload, switch, pause/resume and reload
   the save; compare native formatted values and changed wrist pixels.
4. Require nonzero restored-pass verification without restore mismatches, VRAM
   leaks, nesting repairs or watchdog events.
5. Before default enablement, extend coverage to damage/healing, objective compass
   changes, subtitles/notifications, grenades/passport, other missions, session
   recreation and a performance comparison. Then assess physical wrist placement,
   readability and comfort in the headset.

Current scope is the measured 512x240 single-player overlay. The baseline framework
pin and accepted world/weapon defaults are unchanged. See
[the handoff](VR_WRIST_HUD_HANDOFF.md) for implementation, commands and evidence.
