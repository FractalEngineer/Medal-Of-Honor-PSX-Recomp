# Bound stereo shedding and recover at the display deadline

**Current:** An old pair-time estimate can suppress many frames or keep shedding latched after recovery.

**Desired:** Bound skipped frames, probe regularly and recover when a pair meets the display deadline.

**Validation:** Policy, sandbox and abort tests pass. MoH still disables shedding by default.

[MoH integration](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/tree/1957e28a5dc7f5daa79e2b9b6ed867eb06b41ace) | [Source and credit](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/02-bounded-shedding/docs/internal/upstream/vr-bounded-shedding.md)
