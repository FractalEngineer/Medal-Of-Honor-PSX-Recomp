# Accepted headset-facing world visibility

Accepted by the user on Quest 3 / Virtual Desktop VDXR on 2026-10-05, after
stationary head turns and a movement-enabled slot-0 gameplay test. The user
reported "visibility fix looked good, full improvement" and requested a PR
into game master. This fix is separate from color correction and frozen-view
diagnostics. The framework release pin supplies all required APIs.

The native selector at 8008B3E8 chooses box extrema using the body camera. A
later head-view projection cannot make those extrema correct for a turned or
leaning headset. During stereo replay only, inspect all eight AABB corners in
the actual eye view and reject only when the entire box lies outside a frustum
plane. FOV is unchanged: sections behind the body become eligible when the eye
looks toward them. This does not draw the entire world in 360 degrees.

Preserve native PVS masks, leaf traversal order and subsequent triangle,
lighting and texture paths. Check four live instruction words before overriding
the measured SLUS-00974 function. Bound RAM addresses, node visits, traversal
stack and the 1000-leaf output queue; preflight the complete queue before
writing. Invalid trees/overflow use the native function. Saturated coordinates
remain visible for downstream native clipping. Writes occur within the existing
stereo rollback transaction. Retain the existing entry instrumentation through
one function filter; duplicate entry/filter registration would replace it.

Normal VR launch enables PSX_VR_HEAD_FRUSTUM=1. For the prior selector pass
-NoHeadFrustumDiagnostic to the launcher. Desktop/native play remains unchanged.

Evidence: source-owned frustum/tree fixtures cover forward/behind, rotation,
leaning, plane crossing, saturation, PVS/order, invalid RAM, cycles and queue
overflow. Matched desktop controls retain 96 guest fingerprints and have zero
rollback mismatches; repeated frozen-checkpoint eye images are identical.
The fixed 180-degree slot-0 control gains forest/floor geometry behind the body.

Live movement-enabled run: 361 samples over 180.105 seconds of collection;
314 active stereo samples cover 156.591 adjacent active seconds. Guest VBlanks
59.946 Hz, stereo pairs/XR submissions 29.938 Hz; zero reported XR/pair failures
and tracking-loss samples. Sampled last-pair median 8.040 ms, maximum 17.577 ms.
VDXR used GL_SRGB8_ALPHA8; eyes 2560x1200, desktop swap interval 0, turbo off.
At 157.091 seconds the source became native (47 samples). The bounded launcher
then closed its owned process; the collector's longer deadline ended on debug
disconnect. Active-rate reduction excludes native transitions, gaps and counter
resets. Collection windows do not classify the user's actual activity.

These counters do not measure compositor refresh or motion-to-photon latency,
nor do sampled costs form a complete frame-time distribution. Verification was
off for performance; zero verify checks is not new rollback proof. No matched
live baseline was collected, so no relative performance cost is claimed.
Other sectors, Mission 1 ruins, near-plane clipping and the earlier forward
black-floor artifact remain open. The frozen-render-view jitter control is a
separate diagnostic; this change does not establish a jitter fix.


Master extraction validation: fresh Release build against the unchanged released
framework `9976567e` and cached dependencies, OpenBIOS only; strict
`gcc -std=c11 -Wall -Wextra -Werror` frustum/tree fixture passes. A misleading
same-line statement was split without changing behavior. Exact-build native,
behind-selector-off and behind-selector-on slot-0 captures match all 96 guest
fingerprint fields except process-local startup frame numbers. Both stereo runs
perform 104 verification checks with zero mismatches. At the same guest cycle
1288023937, enabling selection changes 2,513,825 left-eye and 2,456,983 right-eye
pixels and restores forest/floor coverage. Source save hash stays unchanged;
owned processes close. No tracing or frozen-render-view feature is included.

The first build directory hit Windows' dependency-file path limit; using the
shorter `build-vis` directory completed the same build. No source workaround or
generated-code edits were needed.
