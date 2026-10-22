# Recover an inverted XR reference origin

**Current:** A bad initial XR reference can leave the world upside down.

**Desired:** Recapture after ten consecutive inverted samples; ignore transient glitches and ordinary yaw.

**Validation:** Tests cover the threshold, upright samples, yaw and reference-reset streak clearing.

[MoH integration](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/tree/1957e28a5dc7f5daa79e2b9b6ed867eb06b41ace) | [Source and credit](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/06-origin-recovery/docs/internal/upstream/vr-origin-recovery.md)
