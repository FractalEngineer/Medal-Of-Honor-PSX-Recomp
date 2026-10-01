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
  unconditionally by `runtime/src/main.cpp:11681` / `:11686`
  (`ae_np_online_count` / `ae_np_online_get`).
- **Cause:** the netplay-off stub block (`#if !defined(PSX_HAS_LOBBY_CLIENT)`,
  lines 12-169) mirrors the lobby API but omitted those two functions.
- **Fix:** two one-line stubs added in the block's existing style.
- **Impact:** every single-player build. High value, low risk.
- **Upstream target:** `RetroPortingToolKit/psxrecomp` (fork base; `mstan/psxrecomp`
  is the same lineage).
- **Status:** pending — upstream after VR is working.

## 2. GTE vertex capture seam — `2c919f08` (VR feature, not a bug)

- **Files:** `runtime/include/gte_capture.h`, `runtime/src/gte_capture.c`,
  `runtime/src/gte.cpp` (one hook in `gte_rtps_internal`), `runtime/runtime.cmake`.
- **Purpose:** capture camera-space vertices at the GTE RTPS/RTPT seam — the only
  place world/camera-space geometry is visible (renderers are 2-D VRAM
  rasterizers). Additive, default-off.
- **Status:** fork-only. Upstreaming is a separate decision once the VR design
  settles.
