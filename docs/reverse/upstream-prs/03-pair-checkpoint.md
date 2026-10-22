# Reuse one guest checkpoint per stereo pair

**Current:** Both eyes copy the same guest baseline separately.

**Desired:** Capture once per pair, restore after each eye and retain per-eye GPU/precision protection.

**Validation:** Trace, sandbox and abort tests pass, including shared-state restoration.

[MoH integration](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/tree/1957e28a5dc7f5daa79e2b9b6ed867eb06b41ace) | [Source and credit](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/03-pair-checkpoint/docs/internal/upstream/vr-pair-checkpoint.md)
