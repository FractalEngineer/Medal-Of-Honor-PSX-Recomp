# Capture both HUD mattes with one checkpoint and readback

**Current:** Two HUD mattes require separate checkpoint captures and GPU readbacks.

**Desired:** Render both from one baseline and return both images through one readback and paired API.

**Validation:** Capture and abort tests pass. Consumers must migrate to the paired API.

[MoH integration](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/tree/1957e28a5dc7f5daa79e2b9b6ed867eb06b41ace) | [Source and credit](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/04-paired-hud-capture/docs/internal/upstream/vr-paired-hud-capture.md)
