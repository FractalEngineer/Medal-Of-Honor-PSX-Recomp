# Upstream-pending changes

Fixes / features on our `vr-dev` fork for later upstream review. A working native
VR alpha checkpoint exists, but broader validation is still pending. Do not open
upstream PRs yet; this inventory is not publishing or PR authorization.

Fork: `github.com/FractalEngineer/psxrecomp` (from
`RetroPortingToolKit/psxrecomp` @ `3505f2a0`).
Submodule pin here: `psxrecomp` @ `3618bc00` (color correction over `9976567e`). The exact gitlink,
not the branch name, determines the framework used by the game.

Accepted color follow-up (2026-10-05): user approved Quest 3 / VDXR brightness
and requested a game-master PR. Framework `fix/vr-headset-color-release` contains
one correction commit from the release pin: sRGB swapchain preference, explicit
linear fallback and shared desktop gamma, with real-GL gameplay/native controls.
World experiments remain separate. This approval does not publish the broader
upstream VR stack or a new release.

## Alpha integration and proposed upstream order

The authoritative full framework inventory is
[psxrecomp/docs/UPSTREAM_PENDING.md](../psxrecomp/docs/UPSTREAM_PENDING.md).
The short entries below are an early summary; the linked inventory also covers
paired stereo, rigid views, OpenXR input/poses, rollback repair and UI surfaces.

The native alpha can ship against the pinned fork while upstream review proceeds.
Suggested review groups, with dependencies and fresh validation checked before
each PR:

1. Netplay-disabled link fix: validate both enabled and disabled builds.
2. General diagnostics: separate disassembly, producer-PC trace filtering and
   render-pass refusal inspection into focused changes with their tests.
3. Render transaction correctness: nested mod callback rollback repair and its
   recovery tests, carrying any required transaction dependencies.
4. Optional render enhancements: projection scaling, rigid views and paired
   stereo, with identity/off-path regressions and documented rendering limits.
5. OpenXR backend and shared input/pose/UI APIs, built on the reviewed rendering
   contracts. Keep MoH-specific camera, weapon, movement and HUD policy here.

For the game, retain flat play as the default and enable VR explicitly. Before
integrating `vr-dev` into the fork's `master`, validate the intended packaged
executable in ordinary flat mode and VR mode, including flat launch without an
active headset/runtime. A branch merge alone does not prove compatibility.
The framework fork can remain pinned at the alpha checkpoint during this work;
  game integration and upstream framework acceptance are separate decisions.

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


## Native XR startup and pacing inspection (2026-10-03 checkpoint)

Framework checkpoint 5bafeebf is committed and pushed; this submodule now pins it.
Inventory: runtime/include/{mod_plugins,psx_openxr,gpu_gl_renderer}.h;
runtime/src/{gpu_gl_renderer,psx_openxr,debug_server}.c. Generic persistent
psx_mod_openxr_native_surface API copies fresh native presentation to VIEW quads
before host OSD; source/native-frame stats and actual GL swap interval extend
existing TCP commands. Off-XR stats test and GL runner cleanup linking updated
in main framework. No guest timing/decode/default-render modifications.

Normal boot/menu and natural genuine-stereo gameplay handoff accepted on Quest
3/VDXR, with save generation zero. Game launcher desktop VSync 0 resolved user
sound/framerate complaint; native MDEC control guest rates 49.547 -> 59.195 Hz,
actual swap intervals measured, turbo off. Bicubic trial removed at user request.
Main framework docs/UPSTREAM_PENDING.md carries API/test details for later PR.
User authorized this checkpoint. Framework committed and pushed first; game pin
updated and generation rerun (all 25 C shards and dispatch unchanged).

## Native alpha release preparation (2026-10-03)

User authorized pushing the game and publishing v0.1.0 as an alpha. Framework
9976567e adds a verified OpenBIOS stamp refresh (generated C unchanged) and fixes
Git Bash executable alias selection so Windows dependency bundling/signing runs.
Both fixes were pushed to fork/vr-dev before updating this game's pin. The retail
BIOS stamp remains stale; alpha builds explicitly select only OpenBIOS.

Game changes make the launcher portable, replace its normal Python startup check
with PowerShell TCP inspection, add RunFlat.bat, and ship the VR support feature as
experimental instead of developer-only. No guest simulation/rendering code changed.
Game generation stayed unchanged. Release verification and remaining hardware
scope are recorded in reverse/VR_ALPHA_RELEASE_RECEIPT.json and ALPHA_README.md.
Upstream implementation PRs follow separately; none were opened during publishing.
