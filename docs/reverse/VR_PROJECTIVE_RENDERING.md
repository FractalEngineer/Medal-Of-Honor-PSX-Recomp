# VR near-floor and head-motion rendering fix

Current slot-2 follow-up: [VR_SLOT2_PERFORMANCE.md](VR_SLOT2_PERFORMANCE.md) records
production defaults, the empty-projective-setting correction, overload-flicker
fix, headless receipts and the measured throughput improvement. Current framework
pin: `5932092c`; normal `build-release` uses debug tools OFF and PGXP ON.


Accepted by the owner on 2026-10-23 after a 30-second Quest 3 / Virtual Desktop
VDXR session: both the missing nearby floor and world shakiness were fixed.
This result supersedes the earlier shelved jitter experiment for the current
source build. It does not close unrelated pop-in, ruins/streaming, combat or
broader mission coverage work.

## Source and normal launch

- Parent branch: `vr-perf-pr1-pr2`, based on `a664ba4`.
- Framework branch: `vr-head-pitch`, pinned at `efa10eee` (previously `96981469`).
- UI pin remains `41c88ffea852b970af8d50312620b17404e455ad`.
- Release / OpenGL / OpenXR build, `MOH_PGXP=ON` (now the CMake default).
  Existing build directories configured OFF must explicitly enable it again.
- Normal VR enables projective replay by default. `-ProjectiveDiagnostic`
  explicitly enables it; `-NoProjectiveDiagnostic` or `PSX_VR_PROJECTIVE=0`
  selects the comparison path. A build without PGXP cannot enable the fix.
- `build-release` was rebuilt after promoting these accepted settings to defaults.
  No release package was published as part of this change.

## Mechanism

The original replay lost fractional eye translation/rotation and projected
geometry through integer GTE screen/depth limits. Near-camera and behind-camera
corners could produce wrong facing and a large black floor wedge; millimeter
head motion produced periodic quantized jumps.

The framework carries exact host rotation and translation alongside the ordinary
Q12/integer view. During eye replay it preserves full camera-space homogeneous
projection through PGXP provenance, determines facing before screen saturation,
and clips tagged triangles against a signed near plane. The GPU uses that
projection at the normal VR aspect ratio too. Integer and precise eye offsets
are not added twice. Frustum checks and tracked laser geometry use the same view.

PGXP provenance/CPU mode is scoped to rollback-protected eye transactions and
restored on success, checkpoint failure and abort. The ordinary guest GTE path,
untagged packet fallback and HUD capture retain their existing behavior. No
generated game C was edited and no savestate compatibility checks were bypassed.

## Validation and screenshots

Raw local receipts are under the ignored `analysis/vr-proof/` directory; they
contain rendered game assets and are not committed. The corresponding tools and
source tests are committed.

| Check | Result | Local evidence directory |
| --- | --- | --- |
| Nearby floor, slot 1 | Lowest 6% black-pixel band: left 100% -> 0%; right 91.72% -> 0.74% (residual weapon pixels). Floor continuous in both eyes. | `20261023-baseline`, `20261023-scoped-floor` |
| Translation, 21 poses 1 mm apart | Meaningful movement on 20/20 intervals in each eye; quantized control only 5/20 and 4/20. Delta CV 3.4%/2.6%, versus 173%/200%. | `20261023-quantized-sweep`, `20261023-scoped-sweep` |
| Pitch, 21 poses 0.01 degree apart | Movement on 20/20 intervals; delta CV 3.7%/4.9%. | `20261023-scoped-rotation` |
| Stationary control | No meaningful ROI movement; tiny isolated flicker remains, so whole-image hashes are not all identical. | `20261023-accepted-still` |
| Rollback | 757 checks, zero mismatches in final scoped capture. | `20261023-scoped-sweep` |
| Broader views | Forest/bunker, down/up pitch, 60/120 mm offsets visually checked. Render math unchanged by subsequent scope-only performance change. | `20261023-candidate-slot0`, `20261023-candidate-slot2`, `20261023-candidate-up` |
| Actual headset | Owner explicitly accepted both floor and shakiness. Zero XR, tracking-loss or surface failures; wrist surfaces present. 9 sheds/10 empty submissions occurred during startup and did not grow afterward. | `20261023-headset-session`, `20261023-headset-candidate` |

Visual comparison: `analysis/vr-proof/20261023-world-comparison.png`.
Final floor pair: `20261023-scoped-floor/step-000/p000378_left.png` and right.
Headset pair: `20261023-headset-candidate/p000011_left.png` and right.
Black-pixel band measurements include weapon pixels; they are not a general
geometry-coverage metric.

The new `vr/check_world_stability.py` sweeps an existing owned session using
atomic stereo dumps, explicit synthetic poses and a world ROI excluding weapon
and HUD. Its receipt records per-step hashes/deltas, views and restore statistics.
Use `--axis x --step-mm 1 --steps 21`, or `--pitch-step-mdeg 10` for rotation.

Passed: Release target build; GTE architectural/projective/facing and precise-eye
translation tests; GPU projective/provenance/fallback/scope tests; render sandbox
and abort tests; strict pose/frustum tests; GPU GP0 history guard; Python compile.
The tests cover the double-translation regression and restoration of PGXP modes.

## Performance remains open

Matched headless measurements used the same PGXP binary, slot 1, Quest 3
projection replay, wrist HUD/weapon settings, shedding disabled, verification off,
35 seconds warmup, then a quiet 30-second trace without screenshot readbacks.

| Metric | Projective off | Final projective replay |
| --- | ---: | ---: |
| Pair time p50 | 11.552 ms | 16.822 ms |
| Pair time p95 | 12.736 ms | 18.393 ms |
| Guest simulation interval p50 | 33 ms | 40 ms |
| HUD capture p50 | 2.610 ms | 3.157 ms |

Receipts: `20261023-perf-control`, `20261023-perf-scoped`. The visual fix adds
about 5.27 ms to median pair rendering in this scene; it is not an FPS gain.
Scoping precision to eyes reduces non-eye work compared with the initial
always-on candidate. Further performance work must preserve this accepted image.
The moving headset session is not a matched performance A/B.

## Savestate compatibility and isolated testing

Changing GTE source changes execution identity, and enabling PGXP changes the
savestate ABI flavor. Old states can therefore be rejected by the current build;
ordinary memory cards are unaffected. Keep compatibility checks enabled. Create
new states with this build for ordinary use.

For render-only diagnostics we copied the owner's three states into
`analysis/vr-proof/20261023-fixture/openbios/`, rebased only ABI/header identity
(ABI `0x2001e`, execution cookie `0x601abb88`), and left guest payloads unchanged.
`20261023-fixture/receipt.json` records source/fixture hashes and header changes.
Original personal saves were verified unchanged. This controlled fixture is not
a general savestate migration tool or a claim that arbitrary old states are safe.
The accepted headset run used:

```powershell
.\RunVR-VDXR.bat -ProjectiveDiagnostic -Slot 1 `
  -MemcardDirectory analysis/vr-proof/20261023-fixture -Seconds 30
```

`-MemcardDirectory` keeps diagnostic states and writes in the selected isolated
directory. Normal launch needs no projective switch once built with PGXP.
