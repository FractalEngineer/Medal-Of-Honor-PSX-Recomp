# Unattended XR mirror verification

Updated 2026-10-15. This is the prerequisite harness for wrist-HUD work. It does
not implement the wrist HUD or establish comfort, physical scale, or VDXR parity.

## Runtime choice and findings

VDXR was tried first using the installed runtime without asking for headset use.
It returned `XR_ERROR_FORM_FACTOR_UNAVAILABLE` (-35) at system discovery, with
zero submitted frames. The Streamer service running does not provide a virtual
headset. Evidence: `analysis/vr-proof/vdxr-unworn-20261015/openxr_startup.json`.

SteamVR's earlier logs repeatedly rejected scene startup with
`VRInitError_Init_AnotherAppLaunching`. The old launcher only inserted
`driver_null` if the text was absent, so an existing disabled section was left
disabled; it also allowed competing drivers, Home, standby and the dashboard.

The launcher now parses settings and temporarily forces null, disables multiple
drivers/Home/dashboard, and prevents compositor standby during a bounded test.
It restores the original settings in `finally`. Do not run with SteamVR already
active. A crash/forced termination that prevents `finally` is still a limitation;
prefer an ordinary bounded launch. The system's runtime registry is unchanged.

## Run

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run_vr.ps1 -Headless -HeadlessMirrorCheck -Verify -Slot 0 -CaptureDirectory analysis/vr-proof/<fresh-name> -Seconds 2
```

The automated checks run before the final two-second wait. They take longer than
two seconds. This mode uses the normal windowed renderer with an XR null device;
the game's separate `--headless` mode has no presenter and is unsuitable.
Python with Pillow and installed SteamVR are required. No Python OpenVR package
is needed. The compositor client is pinned to `IVRCompositor_029`; unsupported
interfaces fail explicitly. Use a known single-player save. Save source files
are loaded, never overwritten. A paused save is resumed through native Start
after allowing restored input state to settle. Keep the normal VR controller
source enabled; `-NoMovement` failed to resume the measured paused save.

## What must pass

- Two complete eye pairs with renderer-reported pair IDs, guest cycles and image
  dimensions, plus a 96-frame fingerprint receipt. The current renderer reports
  capture metadata through `stereo_stats.captures`; the client writes manifests.
- Actual advancing OpenXR stereo projection submissions, with tracking and
  application focus (state 5). A startup quad, paused menu, or dashboard-obscured
  visible session is insufficient.
- SteamVR compositor backbuffer AND application-layer PNGs freshly produced for
  this owned game process. Renderer PID is checked before and after each dump.
  `CompositorDumpImages` writes into SteamVR's screenshots directory; the client
  copies new images into the run directory and leaves existing captures intact.
- Synthetic both-hand, left-loss, right-loss, unfocused and recovery snapshots,
  with corresponding compositor images. Overrides are cleared in `finally`.
- Actual guest restoration verification, with zero mismatch, VRAM leak,
  nesting repair and watchdog counts.

The compositor backbuffer includes overlays. Direct visual inspection caught a
SteamVR dashboard hiding gameplay in run `e`, despite advancing submissions.
That run is rejected as unobstructed-scene evidence; the validator now requires
focus, and the temporary null settings disable the dashboard. Run `f` shows
unobstructed stereo gameplay. Do not accept a nonblank image alone as HUD proof.

The dump is asynchronous and is not an atomic capture of a particular guest
pair. Metadata explicitly records this. Exact pair images remain separately
available. This harness presently validates pose injection and capture plumbing,
not rendered wrist behavior: `result.json` explicitly says
`wrist_hud_verified: false`.

## Measured result

Run `analysis/vr-proof/null-mirror-20261015-f` passed all five scenarios. The
compositor images were opened and inspected: both eyes show gameplay without the
dashboard. It completed 528 verified render passes with zero restore mismatch,
VRAM leak, nesting repair or watchdog counts. Both captured eye pairs differ,
with measured wall/ground disparities of -2/-10 native pixels in the existing
comparison ROIs. Those ROIs are image correspondences, not a physical scale proof.

Before any wrist-HUD headset request, extend the harness to assert rendered
panel presence/absence, movement, rotation, recentering, data updates and native
HUD replacement. Use the same LOCAL pose snapshot in production and synthetic
rendering. A controller-space layer that bypasses the overridden snapshot would
invalidate these tests. Retain numeric assertions and visually inspect both eyes.

Valve documents the dump as compositor backbuffer plus application stereo images:
[OpenVR compositor interface](https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h).
