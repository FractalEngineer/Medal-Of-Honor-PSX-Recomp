# VR handoff

Updated: 2026-10-06. Start here, then read [VR_ALPHA_TODO.md](VR_ALPHA_TODO.md).
The todo list is the single backlog; historical plans and receipts retain their
original scope and are not current instructions.

## Current baseline

- Published release: [v0.1.3 Alpha](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/releases/tag/v0.1.3),
  source `c72ddc95`, Windows x64 / OpenGL / optional OpenXR / bundled OpenBIOS.
- Framework pin: `3618bc00381588b7e9ea9b5173e8872470ed0379`.
  UI pin: `5de138a8b176ee66ee583ce1748f2059b97e15ca`.
- Accepted on Quest 3 / Virtual Desktop VDXR: normal boot/menu/briefing,
  genuine per-eye gameplay, locomotion, tracked weapon visuals for all ten supplied
  weapon controls, headset color, head-turn visibility and the rebuilt normal launcher.
- Jump is right-stick click. [ALPHA_README.md](../../ALPHA_README.md) owns the
  current installation and control chart; do not copy old Y-button mappings.
- Jitter is shelved at `04624df`, preserved in the local retired-branch bundle
  described below. The tolerance experiment worsened distant-enemy stability;
  it is not in the release.
- Visual acceptance does not close physical damage alignment, scoped/special
  modes, broader single-player assets, wrist HUD or reconnect/mission coverage.

Keep the accepted settings: WorldScale 3, weapon model units/meter 850,
pivot `(80,150,100)`, projection precision 16, menu distance/width 2m.
Use runtime eye poses; the user's IPD preference is 67mm. Physical world scale
is still uncalibrated. The rejected bicubic movie experiment stays removed.

## Working layout

| Path | Purpose |
| --- | --- |
| `build-release/` | Current normal VR and weapon-control build |
| `psxrecomp/`, `recomp-ui/` | Clean pinned source dependencies for that build |
| `build-release-011/` | Retained detached worktree with local changes and verified packaging emitters; not a game-launch target |
| `build-vr-jitter/` | Shelved experiment; do not use for ordinary play |
| `dist/moh-0.1.3-windows-x64.zip` | Exact published Windows artifact |
| `analysis/weapon-capture/` | Current accepted weapon/release evidence and Ghidra project |
| `analysis/vr-proof/` | Retained color/visibility evidence and HUD/compass discovery |
| `analysis/archive/20261006/` | Verified archives of older captures, runtime logs and release files |
| `analysis/cleanup-20261006/` | Cleanup manifest, preserved hashes and validation |

All normal and weapon launchers now default to `build-release`; an explicit
`-BuildDirectory` still selects a candidate. Previously the batch used
`build-vr-weapons` while normal RunVR used a stale v0.1.0 build. Check the actual
executable path and build provenance, not just the Git revision.

Do not remove registered game/framework worktrees or pre-existing framework
changes. The separate framework checkout has unrelated local changes; preserve
those. Framework implementation work belongs there, then gets committed/pushed
to the fork and pinned here. Do not edit dependency copies or generated C.

The retained `build-release-011/build-recompiler` emitters were used for the
published package. A fresh emitter configure at pin `3618bc00` currently fails
its registration guard because `test_openxr_color_gl.c` is driven by
`run_openxr_color_gl.py` but is not listed by the CMake registration checker.
Keep the working emitters until that fixture is properly registered/classified;
do not disable the guard or patch the pinned checkout to bypass it.

Master is the only active local/remote game branch. Retired branch history is
archived locally in `analysis/archive/20261006/retired-game-branches.bundle`;
`retired-game-branches-refs.txt` lists the archived names and commits. The two
linked game worktrees are detached at their original commits, with local changes
preserved. Framework-repository branches were not changed.

## Launch and rebuild

Connect VDXR and run `RunVR.bat`; use `RunFlat.bat` for ordinary flat play.
No save is needed for normal VR boot. Future live gameplay checks default to
30 seconds with movement enabled, after asking whether the headset is ready.

```powershell
.\RunVR.bat
.\RunVR.bat -Build
.\RunVRWeaponBatch.bat -Weapon mp40
.\RunVRWeaponBatch.bat -Mode compare
.\RunVRWeaponBatch.bat -Desktop -Verify -Seconds 1
```

[VR_WEAPON_BATCH.md](VR_WEAPON_BATCH.md) owns the diagnostic recipes. These
source-only controls require Python. Nine cases use original multiplayer saves;
Thompson uses single-player slot 6. MP40 uses slot 4 because its native firing
control passed there. The multiplayer bench is not general multiplayer VR support.

Original saves: 0 is single-player, 1-5 multiplayer, 6 and 9 single-player.
Slots 7/8 are absent. Slot contents can change when the user saves again; verify
mode and equipped weapon after loading instead of treating old hashes as current.
Never overwrite source saves. Diagnostics use copied saves, their own TCP port
4372, and an explicit isolated `--memcard-dir` for save/load tests.

For a manual configure, use the root pinned submodules and a quoted version:

```powershell
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DPSX_OPENXR=ON "-DPSX_GAME_VERSION:STRING=0.1.3" "-DPSXRECOMP_ROOT:PATH=$PWD/psxrecomp" "-DRECOMP_UI_ROOT:PATH=$PWD/recomp-ui"
cmake --build build-release --target psx-runtime
```

Packaging uses `scripts/package_release.sh build-release windows-x64
build-release-011/build-recompiler`. The release ZIP already contains all runtime
assets and the overlay toolchain; rebuilding the game does not require repackaging.

## Read only what the task needs

| Document | Use |
| --- | --- |
| [VR_ALPHA_TODO.md](VR_ALPHA_TODO.md) | Priority, remaining gates and acceptance criteria |
| [VR_WEAPON_BATCH.md](VR_WEAPON_BATCH.md) | Headset/desktop weapon controls and save recipes |
| [VR_WEAPON_TRACKING.md](VR_WEAPON_TRACKING.md) | Native models, mesh/shot hooks and ownership guards |
| [VR_HEAD_VISIBILITY_FIX.md](VR_HEAD_VISIBILITY_FIX.md) | Accepted per-eye world selection |
| [M1_VIEW_MATRIX.md](M1_VIEW_MATRIX.md), [render-pipeline.md](render-pipeline.md) | Native transform/render producers |
| [VR_PHASE9_STATUS.md](VR_PHASE9_STATUS.md) | Chronological measurements and corrections; consult targeted entries |
| [VR_PROOF_CLEANUP_PLAN.md](VR_PROOF_CLEANUP_PLAN.md) | Retention/archive access |
| [VR_CLEANUP_RECEIPT.json](VR_CLEANUP_RECEIPT.json) | Housekeeping manifest and regression checks |
| [VR_RELEASE_V0.1.3_RECEIPT.json](VR_RELEASE_V0.1.3_RECEIPT.json) | Published artifact and verification |
| [Framework CLAUDE.md](../../psxrecomp/CLAUDE.md) | Foundation/enhancement and inspection rules |
| [Framework timing plan](../../psxrecomp/docs/internal/FAITHFUL_TIMING_PLAN.md) | Required framework status |
| [Framework OpenXR docs](../../psxrecomp/docs/OPENXR_RENDERING.md) | XR lifecycle/presentation contract |
| [Upstream inventory](../UPSTREAM_PENDING.md) | Generic changes for future upstream PRs |

## Investigation rules

Inspect guest state through TCP; do not replace missing diagnostics with printf.
Normal launch uses 4370; owned controls use 4372 and refuse an occupied port.
Record exact executable/configuration, native producer, fresh per-eye output,
nonzero restoration checks and user acceptance separately. Close owned games.

Static main-EXE hooks require configured generation; overlay hooks fire at runtime.
Measured gameplay replay waits at `80090B80`; level transforms at `8008B3E8`;
`800824D0` is dynamic-object-only. Native UI/video and gameplay have distinct
presentation sources. Four VBlanks of scene inactivity currently trigger the
native surface; long gameplay stalls and transition coverage remain open.

Append new measured results to the status log and keep bulk evidence ignored.
Do not rewrite historical receipt hashes to describe a later binary. Build and
check the concrete candidate before asking for a headset batch. Publishing or
pushing follows the latest user authorization, not this handoff.
