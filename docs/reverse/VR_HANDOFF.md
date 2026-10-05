# VR alpha handoff - start here

Updated: 2026-10-05. This file is the entry point for a new chat/agent.
Follow the latest user request; the backlog is not authorization to implement
all items or publish a release. Read current local changes before resetting,
regenerating or committing anything.

Release preparation: the user merged PR #1 (headset color) and PR #2 (head-turn
world visibility), then requested Windows v0.1.1 before continuing jitter work.
This release branch starts from master `7ac0509`, with framework `3618bc00`.
Normal VR selects world geometry using each eye's view; FOV and native sector
visibility are unchanged. Both fixes were accepted on Quest 3 / VDXR, including
movement-enabled visibility gameplay. Experimental continuous-pose/tolerance
branches are excluded. Jitter remains the next TODO; frozen poses stop shaking
but are diagnostic, while the tolerance-1 candidate improves world/weapon and
worsens sky/compass. See the latest release receipt for package verification.

## Current checkpoint and local work

- Game: Medal of Honor PS1, SLUS-00974 (NTSC-U). Release integration is on
  `master`; `vr-dev` retains the same alpha commit for continued development.
  Alpha checkpoint **28b0c558bfc81258aeee1e93451a12ab15c189a1**, pushed to
  `origin` (FractalEngineer/Medal-Of-Honor-PSX-Recomp).
- Framework: branch `fix/vr-headset-color-release`, current pin
  **3618bc00381588b7e9ea9b5173e8872470ed0379**, pushed to `fork`
  (FractalEngineer/psxrecomp). Main framework `origin` is upstream; do not push
  our VR branch there by accident. Game submodule pins this checkpoint cleanly.
- User authorized pushing the repo and publishing the Windows **v0.1.0 alpha**:
  [release](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/releases/tag/v0.1.0).
  See [ALPHA_README.md](../../ALPHA_README.md) and
  [VR_ALPHA_RELEASE_RECEIPT.json](VR_ALPHA_RELEASE_RECEIPT.json). Remaining work
  is substantial; upstream framework PRs follow as a separate task.
- Release preparation adds portable disc selection, a PowerShell startup check,
  RunFlat.bat, experimental-channel VR support and packaging/docs. Framework
  follow-ups refresh the OpenBIOS stamp (generated C unchanged) and fix Windows
  dependency bundling. No guest simulation/rendering implementation changed.
  The game release links OpenBIOS only; the retail BIOS stamp remains stale.
  Use `git log -1 -- docs/reverse/VR_HANDOFF.md` to identify the release docs commit.
- Main framework has a pre-existing untracked `.commandcode/` directory; it
  was excluded from the checkpoint. Do not silently add or remove unrelated work.
- No game is running from the completed tests. Close owned games when done.

Local repositories:

```text
C:/Users/titan/Desktop/Github_Projects/Mine/Medal-Of-Honor-PSX-Recomp
C:/Users/titan/Desktop/Github_Projects/psxrecomp
```

The game's `psxrecomp/` is the pinned build source. Framework changes belong in
the separate framework repo, then get committed/pushed and pinned in the game.
Avoid leaving divergent copies or accidentally building against another root.

## Read first - current state and rules

Read the applicable AGENTS.md/CLAUDE.md instructions in the actual checkout.
Then consult these docs in order; no need to load every historical receipt.

| Document | Why consult it |
| --- | --- |
| [Framework CLAUDE.md](../../psxrecomp/CLAUDE.md) | Framework rules: faithful foundation versus opt-in enhancements; TCP inspection convention. |
| [Framework timing plan](../../psxrecomp/docs/internal/FAITHFUL_TIMING_PLAN.md) | Required framework north star/status log; consult latest entries and update during framework work. |
| [VR_ALPHA_TODO.md](VR_ALPHA_TODO.md) | Current prioritized backlog, latest user reports, open validation and optional design choices. |
| [VR_PHASE9_STATUS.md](VR_PHASE9_STATUS.md) | Measurements, corrections and acceptance history. Start at the latest 2026-10-03 entries; look up older sections as needed. |
| [Game README](../../README.md) | Current normal-boot RunVR.bat usage and build/regeneration instructions. |
| [VR_FULL_BOOT_PLAN.md](VR_FULL_BOOT_PLAN.md) | Accepted native boot/menu/video surface and genuine-stereo gameplay handoff, pacing fix and remaining inactivity-policy limitation. |
| [Framework OPENXR_RENDERING.md](../../psxrecomp/docs/OPENXR_RENDERING.md) | Current XR lifecycle, fresh-pair/native-source contracts, pose/input diagnostics and measured limits. |
| [Framework UPSTREAM_PENDING.md](../../psxrecomp/docs/UPSTREAM_PENDING.md) | Generic framework change inventory for later PRs. Edit the main framework copy when changing framework code. |
| [Game UPSTREAM_PENDING.md](../UPSTREAM_PENDING.md) | Game-side record of framework fixes/features and the current pin. |

Pinned framework links give the alpha version. For new framework work consult
and update the same paths in the separate framework repo as well.

## Consult for the task at hand

| Document | Use |
| --- | --- |
| [VR_EXECUTION_PLAN.md](VR_EXECUTION_PLAN.md) | Completed redraw/restore and stereo milestones; original refusal diagnosis and proof boundaries. |
| [VR_HOOK_POINT.md](VR_HOOK_POINT.md) | Render-wait/flip investigation; read its correction before its older VSync claims. |
| [VR_DOUBLE_RENDER_SCOPE.md](VR_DOUBLE_RENDER_SCOPE.md) | Reuse of sandbox internals and subsequent explicit stereo API implementation. |
| [Framework RENDER_PASSES.md](../../psxrecomp/docs/RENDER_PASSES.md) | Temporal pass machinery/guards. Gameplay stereo now has its own pair contract; do not restart by treating temporal phases as eyes. |
| [M1_VIEW_MATRIX.md](M1_VIEW_MATRIX.md) | Producer-bound transform/GTE findings and level-versus-object paths; read appended corrections. |
| [render-pipeline.md](render-pipeline.md) | Supporting game render-pipeline map. |
| [VR_HEADSET_PLAN.md](VR_HEADSET_PLAN.md) | Scale/HUD/head-pose stages, accepted provisional profile and deferred calibration/culling/cadence work. |
| [VR_OPENXR_SETUP.md](VR_OPENXR_SETUP.md) | Parameters, movement mapping and physical-reference calibration procedure. Some examples/pending statements predate the current batch and combat/weapon work. |
| [VR_MOVEMENT_PLAN.md](VR_MOVEMENT_PLAN.md) | Native analog-curve inversion, symmetric turning/diagonal correction and input-control evidence. |
| [VR_COMBAT_PLAN.md](VR_COMBAT_PLAN.md) | Native action bindings, contextual reload/use and Quest button acceptance; older aiming/pause next steps have later updates. |
| [VR_WEAPON_AIM_PLAN.md](VR_WEAPON_AIM_PLAN.md) | Shot producers, guarded rifle aiming, tracked mesh and physical calibration. Read final acceptance notes as well as early pending items. |
| [VR_PROOF_CLEANUP_PLAN.md](VR_PROOF_CLEANUP_PLAN.md) | Retained proof versus archived assets and why bulk evidence should not be loaded for ordinary context. |
| [LIVE_TESTING.md](LIVE_TESTING.md), [tooling.md](tooling.md), [GHIDRA.md](GHIDRA.md) | Historical testing/disassembly procedures. Old slot descriptions/headless instructions are not the current VR launch contract. |

Older docs contain chronological hypotheses and superseded instructions. Later
recorded corrections and accepted checkpoints take precedence; do not quietly
rewrite historical measurements or resurrect rejected experiments.

## What already works / what remains open

Accepted on Quest 3 / Virtual Desktop VDXR:

- Normal VR boot, menus and briefing without loading a save state.
- Genuine per-eye camera-space viewpoint changes, asymmetric XR projection,
  head tracking and coherent eye pairs from one frozen guest checkpoint.
- Left-stick move/strafe, right-stick smooth turn; symmetry/diagonal correction
  accepted. Current combat buttons deliver input.
- Rifle grip/size/alignment and corrected rifle appearance; original arms hidden
  for the measured tracked-rifle mesh. Comfortable pause/menu surface.
- Sound/framerate fix: XR launcher sets desktop PSX_VSYNC=0 while retaining the
  guest real-time deadline cap. It does not enable turbo.

Preserve accepted settings: WorldScale 3, user IPD preference 67mm (headset uses
runtime eye poses), rifle model units/meter 850, pivot (80,150,100), projection
precision scale 16, menu distance/width 2m. Physical world scale is uncalibrated.

Current user reports: stationary garbled/shaky world, absent nearby floor,
pop-in, Mission 1 ruins missing/transparent tiles, and brighter/lower-contrast
headset color. Causes are unproven. A lower black-world polygon was previously
captured; its relationship to the reported missing floor is not established.

Still needed: all-weapon support/testing (current measured shot override is
player rifle id 5110; face isolation is rifle node 22), physical shot-to-barrel
validation, legacy-aim removal, wrist HUD (left health/compass, right ammo),
headset cadence/culling improvements, calibrated proportions, controlled
tracking-loss/reconnect and broad level coverage. Other-headset/runtime tests
and a VR options menu are requested. Right-button reload/weapon layout and
native-menu versus long-grip options overlay remain design choices.

Bicubic movie smoothing was tested, rejected by user preference, and removed.
Keep the earlier picture and accepted pacing; movie grain/quality is unresolved.
See the backlog for reproductions and acceptance criteria.

## Launch, build and debug

Normal headset run: connect Virtual Desktop / VDXR, then use **RunVR.bat** from
the game root. It enables the tracked-rifle prototype and runs until game exit.
No save slot is required. For a bounded equivalent:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run_vr.ps1 -WeaponPoseDiagnostic -Seconds 60
```

Add `-Build` to configure/build the pinned OpenXR Release target. Add `-Desktop`
for desktop-only checks; add `-Slot N` only for an explicit reproduction.
`-CaptureDirectory` has a slot-loading diagnostic workflow; it is not normal boot.
Ask for headset availability when user assessment is actually needed; do not
assume a previous active stream is still available.

The extracted Windows alpha also uses RunVR.bat, with the executable at the
package root. It picks/remembers the user's CUE or accepts -DiscPath; normal boot
uses PowerShell TCP status and needs no installed Python. Source slot/capture
diagnostics still require their Python helpers. Use RunFlat.bat for ordinary flat
play: -Desktop keeps stereo diagnostics enabled and is not the flat player mode.

```powershell
cmake --build build-debug --target psx-runtime
python psxrecomp/psxrecomp_cli.py generate --config game.toml --project-root . --disc Input/medal-of-honor/medal-of-honor.cue
python psxrecomp/tools/debug_client.py ping
python psxrecomp/tools/debug_client.py openxr_stats
python psxrecomp/tools/debug_client.py video_info
```

- Windowed runs require `--no-launcher`; the wrapper supplies it. Otherwise the
  shared ImGui launcher may never boot the game. Headless cannot render eye/pass
  images. Actual XR tool launches needed external/unsandboxed execution here;
  sandboxed launches returned -35 while the same external command succeeded.
  Do not claim the user's headset was unavailable from that signal alone.
- JSON-over-TCP port 4370; the wrapper refuses an already occupied port. Use
  read_ram/disasm, GPU/GP1/trace commands, stereo/restore/XR stats as needed.
  Add missing inspection to runtime/src/debug_server.c, not printf/log files.
- Use rg/git grep from repo roots; absolute Windows grep paths have been flaky.
  Read BOM-bearing debug JSON using utf-8-sig.
- Slot 0: local rifle/no-enemy test; slot 3: earlier moving-enemy stereo scene;
  slot 5: user-created rifle/enemy scene. These are local artifacts, not shipped
  assumptions. Verify file existence and savestate_status generation/last_ok/
  last_slot after loading; historical slot descriptions can be stale.
- Gameplay replay entry is overlay FUN_80090B80 (DrawSync(-1) wait), not the old
  startup VSync gate. The original temporal refusal was measured as 512x240
  request versus 256x240 startup history, not assumed FBO allocation failure.
- Level RTPS path FUN_8008B3E8 carries the level translation and writes H=400.
  FUN_800824D0 is dynamic-object-only. Main-EXE entry hooks require configured
  mod_function_entry_funcs and regeneration; overlay hooks fire at runtime.
- Native UI/video surface copies fresh GL_BACK content before host OSD;
  gameplay returns to explicit pairs. openxr_stats source 1=pair, 2=native;
  layer 1=projection, 2=quad. Native pair/cycle IDs are zero. Stats are latched
  successful submissions, not automatic proof of current headset visibility.
- Four VBlanks without scene activity re-enable native mode. This heuristic
  needs investigation for heavy gameplay stalls; don't assume every flat image
  is an XR startup failure.

## Evidence, verification and checkpoint rules

Read compact receipts only when relevant: [launch/pacing](VR_LAUNCH_RECEIPT.json),
[weapon visuals](VR_WEAPON_VISUAL_RECEIPT.json), [weapon aiming](VR_WEAPON_AIM_RECEIPT.json),
[tracked poses](VR_WEAPON_POSE_RECEIPT.json), [Quest delivery](VR_WEAPON_QUEST_RECEIPT.json),
[movement](VR_MOVEMENT_RECEIPT.json), [combat](VR_COMBAT_RECEIPT.json),
[headset](VR_HEADSET_RECEIPT.json). Later user acceptance can supersede an older
receipt's pending wording; synthetic and actual hardware results stay distinct.
Bulk captures belong under ignored analysis/vr-proof/. Do not bulk-read vr/proof.

Alpha checks already passed: SDK Debug/Release, strict menu/XR input tests,
render guards, TCP command index, source-owned GL controls (157 checks each at
1x/4x) and required regeneration (25 C shards/dispatch unchanged). Existing
rollback controls include real/nested watchdogs, clean guest restore and later
save-load recovery. They do not prove every level/weapon path is covered.

Measure actual active settings. No screenshot hash alone establishes geometry,
no submitted counter alone establishes comfort/cadence, and zero mismatches
with zero verify checks is not restore proof. Historical detached H=133 level
sampling and turbo/cadence claims were retracted in the status log. Enable
expensive verification for diagnostics, not ordinary headset performance tests.

Append new results/corrections to VR_PHASE9_STATUS.md. Document framework changes
in both UPSTREAM_PENDING.md inventories. Run focused required checks; close the
owned game. Commit/push only at the user's authorized checkpoint, without
Co-authored-by/bot trailers. Push framework first, update the game pin, rerun
required generation/builds, then commit/push game. The user authorized the alpha checkpoint and then this documentation checkpoint.
Further implementation and publishing follow the latest user request; do not
interpret this handoff or its backlog as blanket authorization.

Latest authorization: push the game, tag v0.1.0 and publish an alpha release.
Framework dependency fixes were pushed first and pinned. Flat cold-cache Mission 1,
save/load and exit checks passed from the package with developer tools absent.
No new live Quest test was performed during packaging; earlier acceptance applies
to the gameplay baseline. Owned test games are closed. No upstream PR was opened.
