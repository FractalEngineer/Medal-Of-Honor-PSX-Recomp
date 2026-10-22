# Upstream VR PR drafts

Eight focused PRs, based on upstream master `9caf4c22`. Each linked draft uses
the previous PR format: current behavior, desired behavior and essential validation.

This is a stacked series: **one feature commit per PR relative to its listed
base**, not eight independent branches off master. Submit in order; after a
prerequisite merges, rebase the next branch onto upstream master.

| # | Draft | Feature branch | Base | Commit |
| --- | --- | --- | --- | --- |
| 1 | [Add opt-in stereo and XR frame attribution](upstream-prs/01-frame-tracing.md) | [`vr-pr/01-frame-tracing`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/01-frame-tracing) | `upstream/master` | `8400b55d` |
| 2 | [Bound stereo shedding and recover at the display deadline](upstream-prs/02-bounded-shedding.md) | [`vr-pr/02-bounded-shedding`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/02-bounded-shedding) | `vr-pr/01-frame-tracing` | `0cdf9595` |
| 3 | [Reuse one guest checkpoint per stereo pair](upstream-prs/03-pair-checkpoint.md) | [`vr-pr/03-pair-checkpoint`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/03-pair-checkpoint) | `vr-pr/02-bounded-shedding` | `4419d234` |
| 4 | [Capture both HUD mattes with one checkpoint and readback](upstream-prs/04-paired-hud-capture.md) | [`vr-pr/04-paired-hud-capture`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/04-paired-hud-capture) | `vr-pr/03-pair-checkpoint` | `e4f1f985` |
| 5 | [Add debug synthetic head translation and pitch](upstream-prs/05-synthetic-head-pose.md) | [`vr-pr/05-synthetic-head-pose`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/05-synthetic-head-pose) | `vr-pr/04-paired-hud-capture` | `18d52c8b` |
| 6 | [Recover an inverted XR reference origin](upstream-prs/06-origin-recovery.md) | [`vr-pr/06-origin-recovery`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/06-origin-recovery) | `vr-pr/05-synthetic-head-pose` | `36f1ff3e` |
| 7 | [Preserve precise eye poses and clip near-camera geometry](upstream-prs/07-projective-replay.md) | [`vr-pr/07-projective-replay`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/07-projective-replay) | `vr-pr/06-origin-recovery` | `fef7a1b6` |
| 8 | [Report XR readiness without the debug server](upstream-prs/08-production-readiness.md) | [`vr-pr/08-production-readiness`](https://github.com/FractalEngineer/psxrecomp/tree/vr-pr/08-production-readiness) | `vr-pr/07-projective-replay` | `534f9ce2` |

All framework changes are game-agnostic. MoH consumers need a pin bump and
rebuild; no game/BIOS regeneration. Each feature commit preserves Yves's author
identity, co-author credit and an exact source/validation record under
`docs/internal/upstream/`. The geometry change affects savestate execution identity.

Fresh validation: focused Windows/GCC 13.2 Release tests and a production
OpenGL/OpenXR/PGXP MoH integration build. Headless readiness uses isolated saves;
physical headset acceptance belongs to the original fork, not these new branches.
The old slot-2 fixture did not load on current upstream; fresh-boot checks passed
and no save compatibility checks were changed. The v0.1.6 release pin is unchanged.
No upstream PRs have been opened.
