# Upstream-pending changes

Fixes / features that live on our `vr-dev` fork and should be offered upstream
**once VR reaches a working point**. Do not open upstream PRs yet.

Fork: `github.com/FractalEngineer/psxrecomp` (branched from
`RetroPortingToolKit/psxrecomp` @ `3505f2a0`).
Submodule pin in this repo: `psxrecomp` @ `vr-dev`.

## 1. Lobby stub link fix — `ce63101f`

- **File:** `runtime/src/psx_lobby_client.c`
- **Symptom:** any `PSX_NETPLAY=OFF` (the default) build fails to link:
  undefined `psx_lobby_online_count`, `psx_lobby_online_get`, referenced
  unconditionally by `runtime/src/main.cpp:11681` / `:11686`.
- **Cause:** the netplay-off stub block (`#if !defined(PSX_HAS_LOBBY_CLIENT)`,
  lines 12-169) mirrors the lobby API but omitted those two functions.
- **Fix:** two one-line stubs added in the block's existing style.
- **Impact:** every single-player build. High value, low risk.
- **Status:** pending — upstream after VR is working.

## 2. `disasm` TCP debug command — `d58db909` (tooling, generally useful)

- **Files:** `runtime/include/psx_disasm.h`, `runtime/src/disasm_shim.cpp`,
  `runtime/src/debug_server.c` (handler + command table),
  `runtime/runtime.cmake` (links `recompiler/src/mips_decoder.cpp`).
- **What:** `disasm addr=0x… count=N` — disassemble guest instructions from live
  RAM via the recompiler's existing MIPS decoder. The stock server had no
  disassembly.
- **Status:** fork-only; not VR-specific. Strong upstream candidate.

## 3. (none yet)

VR-specific work (renderer/OpenXR) stays on the fork by design — not a
bug-fix candidate.
