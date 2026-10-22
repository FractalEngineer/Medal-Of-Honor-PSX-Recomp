# AGENTS_VR.MD

Cut taglines, helper text and secondary labels. If a screen needs them to make sense, fix the layout.
Make the best decision that will elevate all three: the user experience (UX), developer experience (DX), and agent experience (AX) always, without breaking anything.

## Purpose and precedence

This is the VR-specific operating guide for Medal of Honor: PSX Recomp. It extends the repository and task instructions; it does not override the owner, the current task, or a nearer `AGENTS.md`.

- Read the current [VR handoff](docs/reverse/VR_HANDOFF.md) and [alpha backlog](docs/reverse/VR_ALPHA_TODO.md) before changing VR behavior.
- Read the framework's [agent entry point](psxrecomp/AGENTS.md) and [framework rules](psxrecomp/CLAUDE.md) before changing `psxrecomp/`.
- Treat [VR_PLAN.md](docs/VR_PLAN.md) as historical design context. Current scope and priorities live in the handoff and backlog.
- Verify branch, submodule pin, version, and build directory from source; do not trust a copied hash or old build path.

## Current baseline

At the time of writing, `VERSION` is `0.1.6`, the normal target is Release / OpenGL / OpenXR with PGXP enabled, debug tools disabled, LLE batching, projective replay enabled, and stereo shedding disabled by default. The tested headset path is Quest 3 through Virtual Desktop VDXR; SteamVR renders but has not had equivalent broad acceptance, and Meta Quest Link remains unvalidated.

Confirm the current state with:

```powershell
git rev-parse --abbrev-ref HEAD
git rev-parse --short HEAD
git submodule status
Get-Content VERSION
```

Do not turn historical receipts into claims about a newer binary. A build, test, or headset result belongs only to the exact source, configuration, executable, scene, and settings that produced it.

## Architecture in one path

```text
RunVR-*.bat / vr/run_vr.ps1
  -> select OpenXR runtime and opt-in VR environment
  -> activate trusted game plugin moh.vr.stereo
  -> game hooks identify the live frame and level-render producer
  -> framework stereo transaction freezes and checkpoints guest state
  -> each eye renders from the same checkpoint with an exact PSXModRenderView
  -> framework restores guest/GPU/asset state and publishes one atomic eye pair
  -> OpenXR presents the fresh pair, a native menu/video surface, or a HUD quad
  -> off-replay input and hand poses drive controller, weapon, laser, and wrist UI
```

The game owns title-specific addresses, hook decisions, input policy, and content. The framework owns generic capture, projection, restoration, presentation, and device services. Generated game C is output, not an implementation surface.

### Main seams and ownership

| Layer | Owns | Main sources |
| --- | --- | --- |
| Launcher/configuration | Runtime selection, opt-in flags, build directory, disc and isolated memcard paths | [RunVR-VDXR.bat](RunVR-VDXR.bat), [vr/run_vr.ps1](vr/run_vr.ps1), [CMakeLists.txt](CMakeLists.txt) |
| Game VR plugin | Level hooks, frame boundary, camera policy, input mapping, weapon profiles, laser, HUD content | [vr/psx_vr_stereo.c](vr/psx_vr_stereo.c), [vr/moh_vr_input.c](vr/moh_vr_input.c), [vr/moh_vr_aim.c](vr/moh_vr_aim.c), [vr/moh_vr_hud.c](vr/moh_vr_hud.c) |
| Mod selection | Stable plugin id and activation through the packaged catalog | [mods/preloaded/packages/moh.vr.stereo/0.1.0/manifest.toml](mods/preloaded/packages/moh.vr.stereo/0.1.0/manifest.toml) |
| Framework projection | GTE camera-space provenance, per-eye projection, signed near-plane handling | [render-pipeline.md](docs/reverse/render-pipeline.md), [STEREO_RENDERING.md](psxrecomp/docs/STEREO_RENDERING.md) |
| Framework transaction | Render-pass planning, checkpointing, watchdog, sandbox, atomic pair publication | [VR_EXECUTION_PLAN.md](docs/reverse/VR_EXECUTION_PLAN.md), [RENDER_PASSES.md](psxrecomp/docs/RENDER_PASSES.md) |
| Headset backend | OpenXR session, poses, swapchains, layers, input/hands, native surfaces | [OPENXR_RENDERING.md](psxrecomp/docs/OPENXR_RENDERING.md), [VR_OPENXR_SETUP.md](docs/reverse/VR_OPENXR_SETUP.md) |
| Evidence/tooling | Captures, TCP diagnostics, frame traces, rollback checks, receipts | [VR_PHASE9_STATUS.md](docs/reverse/VR_PHASE9_STATUS.md), [FRAME_TRACE.md](psxrecomp/docs/FRAME_TRACE.md) |

## Feature paths

### 1. Build, launch, and runtime selection

Path: launcher arguments -> environment contract -> CMake/runtime features -> package activation -> readiness result.

- `RunVR-VDXR.bat`, `RunVR-SteamVR.bat`, and `RunVR-Oculus.bat` select a runtime without changing the system's active runtime.
- `RunFlat.bat` must clear VR overrides and remain a genuine flat path with no headset requirement.
- The launcher owns opt-in values such as projective replay, head tracking, movement, weapon pose, menu surface, and wrist HUD. New settings need a clear default, a diagnostic escape hatch, and process-scope restoration.
- Release builds require OpenXR, PGXP for the accepted projective path, and the preloaded VR package. Debug-heavy instrumentation stays out of production.
- Headset startup is ready only after the runtime is running and frames have been submitted. A failed runtime must fall back to a visible, actionable error rather than a blank or silently degraded session.

Relevant documentation: [current installation and controls](ALPHA_README.md), [OpenXR setup](docs/reverse/VR_OPENXR_SETUP.md), [mod packages](psxrecomp/docs/MOD_PACKAGES.md), [build guide](psxrecomp/docs/BUILDING.md).

Minimum checks: fresh configure/build for the intended target; flat launch without a headset; selected runtime startup; explicit disc path; isolated memcard path; clean exit; environment and runtime settings restored.

### 2. Level geometry, stereo, and atomic presentation

Path: guest level transform -> GTE projection -> render-pass checkpoint -> per-eye replay -> atomic pair -> presentation.

- The live VR geometry path is the level projector and the framework GTE projection seam. Do not substitute a screen-space offset for camera-space stereo.
- Both eyes must begin from the same guest CPU, GTE, RAM, VRAM, and asset state. Only the eye view and its entitled render effects may differ.
- Projective replay preserves host translation/rotation while retaining the guest's normal fixed-point path. It must keep signed facing and near-plane clipping, avoid double-applying offsets, and leave untagged or non-eye work on its ordinary path.
- Publish complete pairs atomically. A failed eye retains the previous complete pair; never publish one eye from one scene state and the other from another.
- Prefer complete frames under overload unless a measured A/B explicitly opts into shedding. Black-frame bursts are worse than a held complete image.
- Menu, boot, briefing, and FMV presentation are distinct from gameplay stereo. Use the native surface path for content that must remain flat.

Relevant documentation: [grounded render pipeline](docs/reverse/render-pipeline.md), [projective rendering fix](docs/reverse/VR_PROJECTIVE_RENDERING.md), [stereo execution plan](docs/reverse/VR_EXECUTION_PLAN.md), [stereo backend contract](psxrecomp/docs/STEREO_RENDERING.md), [render passes](psxrecomp/docs/RENDER_PASSES.md), [status and corrections](docs/reverse/VR_PHASE9_STATUS.md).

Minimum checks: source-side tests for projection and pose math; exact pixel checks for both eyes; same-cycle metadata; zero-offset identity; nonzero parallax; CPU/GTE/RAM/asset/GPU restoration; abort and watchdog recovery; flat-path identity.

### 3. Head pose, origin, FOV, and world visibility

Path: OpenXR session pose -> validated origin/recenter -> rigid eye view -> projection and visibility selection -> world render.

- Use runtime eye poses and runtime IPD. Do not model IPD as a symmetric camera offset or expose arbitrary stereo separation as an IPD setting.
- Define the reference origin from a valid live pose. Validate stage/local orientation and reject a downward or otherwise invalid origin after recenter or reconnect.
- Keep body/scripted camera state separate from the headset pose. Head translation and rotation layer on the game camera; they must not overwrite authoritative gameplay state.
- Apply per-eye asymmetric FOV and principal-point data correctly. Do not assume symmetric FOV or a fixed display rate across headsets.
- Head visibility selection may remove geometry outside an eye's view, but it must not narrow the game's native sector/PVS content enough to create holes when the player looks or leans.
- Preserve the accepted projective image while investigating pop-in. If the game never submitted geometry, renderer tuning cannot fix it.

Relevant documentation: [view-transform findings](docs/reverse/M1_VIEW_MATRIX.md), [head visibility fix](docs/reverse/VR_HEAD_VISIBILITY_FIX.md), [projective rendering](docs/reverse/VR_PROJECTIVE_RENDERING.md), [headset calibration plan](docs/reverse/VR_HEADSET_PLAN.md), [OpenXR lifecycle](psxrecomp/docs/OPENXR_RENDERING.md).

Minimum checks: yaw/pitch/lean and millimetre translation; near floor and near plane; side/behind views; origin flip and recenter; stage/local modes; asymmetric FOV; eye-height and IPD; save/load and mission transition.

### 4. Input, locomotion, and controller presentation

Path: OpenXR actions -> off-replay input snapshot -> game policy -> resolved controller source -> native guest pad semantics.

- Sample gameplay input outside eye replay. Replay must be deterministic and must not advance input, timers, audio, or gameplay.
- Consume VR input through the game policy layer, then translate to the native controls the game already understands. Preserve native menus, use/reload context, ammo, and gameplay state.
- Release every held action on focus loss, disconnect, tracking loss, menu entry, save/load, pause, and shutdown. Do not leave stale firing, grip, movement, or weapon-cycle state.
- Keep mounted-weapon one-stick behavior distinct from normal locomotion. Movement deadzone and turn gain are accepted settings; change them only with evidence.
- Current controls are release facts, not permanent design mandates. When a requested control change conflicts with native context, resolve the interaction instead of duplicating game logic.

Relevant documentation: [current controls](ALPHA_README.md), [one-stick aiming](docs/reverse/VR_INPUT_ONE_STICK.md), [movement plan](docs/reverse/VR_MOVEMENT_PLAN.md), [input policy and TCP commands](psxrecomp/docs/TCP_COMMANDS.md).

Minimum checks: press/release, focus change, disconnect/reconnect, simultaneous move/fire/turn, menu routing, pause/resume, load, and no stale guest buttons.

### 5. Weapon tracking, aiming, and laser

Path: tracked hand pose -> guarded weapon profile -> render-only mesh transform and/or native shot basis -> laser raycast.

- Weapon mesh tracking is render-only by default. Shot-origin/direction overrides are explicit experimental behavior and need separate validation.
- Guard every title-specific match with the expected weapon, player, model, instruction, or object state. A changed scene or weapon must take the stock path.
- Preserve native animations, ammo, fire, release, reload, recoil, and weapon switching. The passport is not a firearm and must not fabricate a projectile or laser.
- Firearms, grenades/projectiles, scoped/special modes, and multiplayer controls are different paths. Do not generalize from one weapon profile to every asset.
- The laser must use the same camera/weapon basis as the visible weapon, terminate on the intended surface, and remain absent for unsupported items.
- Visual acceptance proves alignment, not damage-ray alignment. Physical barrel-to-shot hits require their own test.

Relevant documentation: [weapon tracking implementation](docs/reverse/VR_WEAPON_TRACKING.md), [aiming plan](docs/reverse/VR_WEAPON_AIM_PLAN.md), [weapon test batch](docs/reverse/VR_WEAPON_BATCH.md), [laser raycast plan](docs/reverse/VR_LASER_RAYCAST_PLAN.md), [combat plan](docs/reverse/VR_COMBAT_PLAN.md).

Minimum checks: `vr/check_weapon_profiles.py`, `vr/check_weapon_pose.py`, `vr/check_weapon_aim.py`, mesh preservation, supported/unsupported guards, native control behavior, and a copied-save batch on port `4372`.

### 6. HUD, wrist UI, menus, and native surfaces

Path: classify screen-space content -> capture or preserve native HUD -> choose screen-plane or tracked-wrist surface -> submit readable per-eye content.

- Keep world geometry, HUD, subtitles, menus, and video in their proper presentation paths. Do not fake world depth with a flat screen overlay.
- Prefer a single source of live game state. If HUD content is captured or re-bound to a wrist surface, avoid drawing the same information twice.
- Wrist surfaces must remain readable in both eyes, stable under pose changes, comfortable in scale/offset, and defined when tracking is lost.
- Native menus and video need a comfortable, stable surface with clear apply/cancel and no gameplay input leaking through.
- New VR options must expose accepted settings without turning the screen into documentation. Short labels are correct when the control itself communicates the action.

Relevant documentation: [wrist HUD handoff](docs/reverse/VR_WRIST_HUD_HANDOFF.md), [native surfaces and OpenXR layers](psxrecomp/docs/OPENXR_RENDERING.md), [screen-space ownership masks](psxrecomp/docs/ADAPTIVE_SCREEN_MASKS.md), [current user controls](ALPHA_README.md).

Minimum checks: `vr/test_native_hud.c`, `vr/verify_native_hud.py`, `vr/check_native_hud_gameplay.py`, headless mirror capture, both-eye readability, tracking loss, occlusion, pause/load, and no legacy HUD duplication.

### 7. Performance, cadence, interpolation, and overload policy

Path: guest cadence -> render checkpoint -> paired-eye cost -> frame budget -> complete-pair submission -> compositor reprojection.

- Report guest updates, completed stereo pairs, XR submissions, and headset display cadence separately. They are not interchangeable FPS numbers.
- Measure before optimizing. Use frame traces to attribute hitches to guest, replay, GPU, readback, HUD, or submission stages.
- Presentation-side interpolation is suspended while an XR session is active; the compositor owns reprojection. Do not reintroduce stacked waits or multiple submissions per guest frame.
- Preserve the accepted projective image while reducing cost. Do not trade world stability, near-floor coverage, color, or atomic pairs for an unreviewed speed gain.
- Keep verification and heavyweight diagnostics out of the production hot path. Use opt-in builds and isolated measurement captures.

Relevant documentation: [optimization plan](docs/reverse/VR_OPTIMIZATION_PLAN.md), [optimization status](docs/reverse/VR_OPTIMIZATION_STATUS.md), [slot-2 performance](docs/reverse/VR_SLOT2_PERFORMANCE.md), [frame tracing](psxrecomp/docs/FRAME_TRACE.md), [frame-rate model](psxrecomp/docs/FRAME_RATE.md), [OpenXR interpolation gate](psxrecomp/docs/OPENXR_RENDERING.md).

Minimum checks: matched A/B configuration, warmup, bounded trace, p50/p95 pair and stage times, guest cadence, submission counts, image comparison, and sustained scene behavior.

### 8. Diagnostics and proof

Path: reproduce -> expose state through TCP -> isolate the stage -> capture a control and candidate -> compare exact state/pixels -> retain a compact receipt.

- Prefer existing TCP commands and counters over ad hoc console logging. Normal games use port `4370`; owned automation uses `4372` and must refuse an occupied port.
- Capture the exact executable, source revision, runtime, headset, scene/slot, settings, and configuration with every result.
- Keep original discs, BIOS images, saves, and personal sessions out of diagnostics. Copy test saves and use an explicit isolated `--memcard-dir`.
- A screenshot proves appearance; it does not prove restoration, cadence, tracking, or gameplay logic. Pair visual evidence with machine-readable state.
- Store bulk screenshots and captures as ignored evidence. Commit only compact receipts, source-owned tests, and bounded proof that the repository intends to retain.

Relevant documentation: [live testing](docs/reverse/LIVE_TESTING.md), [live capture](docs/reverse/live-capture.md), [debug tooling](docs/reverse/tooling.md), [headless mirror verification](docs/reverse/VR_HEADLESS_MIRROR_CHECK.md), [TCP command reference](psxrecomp/docs/TCP_COMMANDS.md), [GPU frame tools](psxrecomp/docs/GPU_FRAME_TOOLS.md), [GL readback regions](psxrecomp/docs/GPU_GL_READBACK_REGIONS.md).

Useful entry points: `vr/capture_stereo.py`, `vr/verify_stereo_pairs.py`, `vr/verify_scene_coverage.py`, `vr/check_world_stability.py`, `vr/trace_frames.py`, `vr/hitch_report.py`, `vr/compare_frame_fingerprints.py`, and [vr/proof/README.md](vr/proof/README.md).

### 9. Release and upstream

Path: settle behavior -> validate game-specific and generic layers -> package exact artifact -> verify extracted release -> publish only when authorized.

- Release packaging consumes the intended build and pinned dependencies. Never silently repackage stale emitters or a different build directory.
- The game-specific VR plugin and its package remain in this repository. Generic framework services belong in reviewed framework changes, then reach the game through an intentional pin.
- A branch merge is not runtime validation. Check flat and VR launches, normal boot/menu/gameplay, clean exit, package layout, and dependency notices.
- Do not push, publish, rewrite receipts, or remove retained evidence without the owner's current authorization.

Relevant documentation: [v0.1.6 release notes](docs/reverse/VR_RELEASE_V0.1.6.md), [upstream inventory](docs/UPSTREAM_PENDING.md), [upstream PR draft](docs/reverse/VR_UPSTREAM_PR_DRAFT.md), [proof cleanup policy](docs/reverse/VR_PROOF_CLEANUP_PLAN.md), [package finalizer](scripts/finalize_alpha.py).

Minimum checks: build provenance, `BUILD_INFO.json`, executable and asset staging, clean-checkout or extracted-ZIP smoke test, flat fallback, headset startup/exit, and the release limitation text.

## Change protocol

1. Identify the exact feature and source of truth. Read only the feature documents needed from the tables above, but read the handoff, backlog, and applicable agent rules first.
2. Find the live producer and current behavior. Use TCP-discovered guest state, not a sampled value detached from its producer.
3. Decide ownership before editing. If the change adds a generic runtime service, projection rule, XR behavior, or render transaction, implement it in the framework. If it selects a title address, weapon, input meaning, or HUD item, keep it in the game.
4. Preserve the ordinary path. New behavior must be opt-in or default only with accepted evidence, and `RunFlat.bat` plus non-VR execution must remain valid.
5. Add the smallest source-owned test that prevents the regression. Reuse focused fixtures; do not grow a headless test into a fake substitute for hardware behavior.
6. Validate in layers: compile, focused tests, flat launch, headless/diagnostic capture, then a bounded headset check when hardware behavior matters.
7. Record reproducible evidence and update the owning status or backlog document. Correct historical notes instead of silently rewriting their meaning.
8. Close owned game and runtime processes, restore environment and SteamVR settings, and leave personal saves and unrelated worktree changes untouched.

## Non-negotiable guards

- Never edit reconstructed/generated game C to fix engine or VR behavior.
- Never use a screen-space 2D shift as a substitute for true per-eye world projection.
- Never advance guest logic or consume input during eye replay.
- Never publish a half pair or strand a guest/host transaction after failure.
- Never overwrite a source save, disc image, BIOS image, or another session's build.
- Never infer UX success from an internal counter, or simulation success from a screenshot.
- Never assume one headset's FOV, IPD, origin, swapchain format, or refresh rate applies to another.
- Never leave temporary proof, logs, or diagnostic processes in the workspace after an owned test.
- Never add explanatory in-game text where a better control, state, or layout can carry the meaning.

## Definition of done for a VR change

A VR change is complete only when:

- its owner layer is correct and no generated or pinned dependency was patched as a shortcut;
- the normal flat path and existing accepted VR defaults still behave;
- relevant source tests pass and any hardware-only claim is labeled as such;
- the exact reproduction, configuration, state, and visual result are recorded;
- failure, focus loss, or unavailable-runtime behavior is deliberate;
- the owning docs or backlog reflect the result without overstating coverage;
- the workspace and owned processes are clean.
