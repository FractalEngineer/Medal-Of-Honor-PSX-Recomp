# Upstream-pending changes

Fixes / features on our `vr-dev` fork to offer upstream **once VR reaches a
working point**. Do not open upstream PRs yet.

Fork: `github.com/FractalEngineer/psxrecomp` (from
`RetroPortingToolKit/psxrecomp` @ `3505f2a0`).
Submodule pin here: `psxrecomp` @ `vr-dev`.

## 1. Lobby stub link fix — `ce63101f`

`runtime/src/psx_lobby_client.c`: the `PSX_NETPLAY=OFF` stub block omits
`psx_lobby_online_count` / `psx_lobby_online_get` (called unconditionally by
`main.cpp`), so every single-player build fails to link. Two-line fix.
**High value, low risk.** Pending.

## 2. `disasm` TCP debug command — `d58db909` (+ `c2ed6e55`)

`disasm addr=0x… count=N` — guest MIPS disassembly from live RAM, via the
recompiler's `mips_decoder.cpp` (`runtime/include/psx_disasm.h`,
`runtime/src/disasm_shim.cpp`, `debug_server.c`, `runtime.cmake`). Stock server
had none. Generally useful, not VR-specific. Strong upstream candidate.

## 3. GTE FOV scale — `82695b75` / `5633e868` / `bbd01ccf` (VR feature)

`runtime/src/gte.cpp`: `gte_set_fov_scale(num,den)` applied to `H` at the
perspective divide; `PSX_GTE_FOV_SCALE` env; the GTE ring records effective H.
VR-facing; upstream only as part of a settled VR design.
