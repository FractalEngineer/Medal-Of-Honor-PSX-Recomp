# Upstream VR PR draft

Prepared 2026-10-23 for `RetroPortingToolKit/psxrecomp`. This is review text, not
an opened upstream PR. It follows the focused scope, changes, validation and
limitations pattern of [PR #666](https://github.com/RetroPortingToolKit/psxrecomp/pull/666)
and the source/author requirements in the framework's CONTRIBUTING.md.

Refreshed upstream master is `9caf4c22`; the tested fork is `5932092c`.
They have diverged by 182 upstream commits and 24 fork commits. The fork branch
must not be submitted wholesale as though it were a tested current-master port.
The game release remains pinned to the tested fork.

## First PR: preserve precise eye poses and clip near-camera VR geometry

Suggested title: **VR: preserve precise eye poses and clip near-camera geometry**

### Draft body

Stereo render replay quantizes the eye transform and projects near-camera
triangles through integer GTE screen/depth limits. Small head movements can
produce stepped world motion; triangles crossing an eye's near plane can receive
the wrong facing and leave missing geometry close to the viewer.

Preserve the precise host eye transform during scoped render replay and use
homogeneous projection/facing with signed near-plane clipping for tagged
geometry. The motivating Medal of Honor views keep their nearby floor and move
smoothly through millimeter translations and small pitch changes.

#### Changes

- Carry exact rotation and translation alongside the ordinary integer eye view,
  without adding eye translation twice.
- Preserve camera-space projective provenance, evaluate facing before integer
  screen saturation, and clip tagged geometry at the eye's signed near plane.
- Use projective geometry at the normal VR aspect ratio too.
- Scope PGXP CPU/provenance mode to the rollback-protected eye transaction and
  restore it on success, checkpoint failure and abort.
- Cover precise-eye translation, projective facing, provenance/fallback and
  transaction restoration with the corresponding runtime tests.

The framework change is game-agnostic. Game camera hooks, visibility policy,
tracked weapons, launch defaults and artwork remain in the game repository.

#### Validation

The following results apply to the tested fork implementation, not yet to a
reconstructed branch on current upstream master:

- Windows / GCC 13.2 / Ninja Release / OpenGL / OpenXR build with PGXP enabled.
- GTE projective/facing, GPU provenance/fallback/scope, render sandbox/abort,
  precise-pose and game-frustum tests passed in the implementation session.
- Headless 21-pose translation and pitch sweeps produced meaningful world motion
  on all 20 intervals; the quantized translation control moved on only 4–5.
- Nearby-floor image comparisons removed the large black floor wedge in both
  eyes; the final scoped sweep completed 757 restoration checks, zero mismatches.
- The owner accepted floor coverage and world stability during a 30-second
  Quest 3 / Virtual Desktop VDXR headset check.

The image correction has a cost: a matched fork-side slot-1 headless comparison
measured median stereo-pair time increasing from 11.552 to 16.822 ms. This PR
fixes rendering correctness; it does not promise a performance improvement.
Broader mission/sector coverage, reconnect and other runtimes remain unverified.

Integration: [Medal of Honor, SLUS-00974](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/tree/bfaf3bbc75f1618459f945f268d2bc0de914ea10).
Consumers require a framework pin bump and a PGXP-enabled rebuild. There are no
game-codegen or BIOS-emitter changes in this feature, so regeneration is not
required for the feature itself. The GTE execution identity changes; existing
emulator savestates may be incompatible and must not have their checks bypassed.

Source: [efa10eee](https://github.com/FractalEngineer/psxrecomp/commit/efa10eee).
Original human author: **Yves <calibratedbeats@gmail.com>**. Preserve that author
on a clean cherry-pick; a reconstructed/partial port must cite the exact source
and include `Co-authored-by: Yves <calibratedbeats@gmail.com>`. Add the attribution
record under `docs/internal/upstream/vr-precise-projective-replay.md` on the
actual submission branch, including exact base and fresh validation.

### Before opening this draft upstream

1. Reconstruct only this feature on refreshed upstream master. Resolve its
   render-pass/PGXP dependencies without importing the optimization campaign.
2. Keep separate review commits for exact pose transport and projective
   facing/clipping if their dependencies permit; preserve source attribution.
3. Run the affected registered runtime tests and build with XR/debug ON and OFF.
4. Build Medal of Honor against the reconstructed framework and compatible UI,
   then check flat boot/gameplay and VR images/rollback. Record the exact commits.
5. Replace the fork-only validation qualifier with the fresh results actually
   obtained. Link the committed upstream source/attribution record.

## Remaining focused follow-ups

These retain the campaign report's review boundaries. Do not include them all
in the geometry PR; some need no release-facing behavior change.

| Proposed PR | Source / scope | Validation to carry forward and rerun after porting |
| --- | --- | --- |
| Reuse one checkpoint per stereo pair | `063b082a`; capture once, restore each eye; opt-out and mismatch retirement | Sandbox, abort, reuse invalidation, precision state; historical right-eye checkpoint p50 143 to 36 us |
| Pair native-HUD readbacks and checkpoint capture | `a876bae3`, `d2554481`; paired capture API consumed by the game | Byte/image parity, shared capture restoration, capture performance |
| Add opt-in frame/XR tracing and nested attribution | `022eb1ed`, `0e0d2de8`, `5c67dced`, `63271ce7`, `571a8f10`, `87a3e822` | Ring/export/allocation guards, nested-span containment; detailed diagnostics off in production |
| Bound shedding and deadline recovery | `cae62891`, `b6a671c1` | Policy bounds/latch/failure-reset tests; clearly state game launch disables shedding under sustained overload |
| Add debug synthetic head translation/pitch | `2ed12640`, `a3714fad`, associated millidegree extension | XR/debug guards, pose math and deterministic sweeps |
| Recover an inverted XR origin | `96981469` | Add a dedicated origin-recovery fixture; historical accepted-game evidence is not a fixture |
| Report startup readiness without debug TCP | `5932092c`; one-shot readiness and optional lightweight counters | Debug-OFF launch, valid/missing staged save, no diagnostic TCP dependency; check overlap with current upstream diagnostic-release work |

The final fork-side production control recorded 2,183 pairs, zero sheds/empty
submissions/XR failures; separate verification recorded 968 checks and zero
mismatches. Removing detailed debug instrumentation raised headless slot-2 guest
throughput by 23–27%. These are assembled-build observations, not independent
speedup claims for each proposed PR or tests of current upstream master.

Overlay acceleration research, rejected batching/inlining, reverted image flips,
game-specific scripts, personal saves, screenshots containing game assets and
chronological investigation notes are excluded from the framework feature PRs.

The complete original inventory and evidence limits remain in
[VR_CAMPAIGN_PR_REPORT.md](VR_CAMPAIGN_PR_REPORT.md). The game release's current
result is in [VR_SLOT2_PERFORMANCE.md](VR_SLOT2_PERFORMANCE.md).
