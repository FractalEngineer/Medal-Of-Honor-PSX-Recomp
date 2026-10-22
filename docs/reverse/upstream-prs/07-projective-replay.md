# Preserve precise eye poses and clip near-camera geometry

**Current:** Quantized eye transforms cause stepped world motion; near-plane crossings can remove nearby geometry.

**Desired:** Preserve precise poses and use homogeneous facing/clipping during protected eye replay.

**Validation:** Six geometry/rollback tests and the GPU-history guard pass. Quest 3/VDXR acceptance is historical fork evidence; rendering costs more.

[MoH integration](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/tree/1957e28a5dc7f5daa79e2b9b6ed867eb06b41ace) | [Source and credit](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/07-projective-replay/docs/internal/upstream/vr-projective-replay.md)
