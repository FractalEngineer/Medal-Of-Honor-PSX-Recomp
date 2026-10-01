# PSX Native Recomp → VR Support Plan

## Project Status

Current baseline:

- PS1/PSX title has already been statically recompiled.
- Native executable builds and runs successfully.
- The game reaches normal gameplay.
- Recompiler/runtime work is therefore considered **complete enough to begin VR integration**.

The project should now focus on the smallest subset of reverse engineering necessary to expose the game's camera, projection, geometry submission, input, HUD, and cutscene behavior to a modern VR renderer.

The goal is **not** to fully decompile the entire game into clean source code.

The goal is:

> Preserve the working recomp, intercept the rendering/camera paths that matter, introduce a modern render abstraction, then layer OpenXR stereo rendering and 6DoF head tracking on top.

---

# High-Level Architecture

```text
Working Native Recompiled Game
        │
        ▼
Game Logic / Original Camera / Original GTE calls
        │
        ├────────────► Reverse-engineering database
        │              - Ghidra
        │              - symbol map
        │              - structs
        │              - camera/player notes
        │
        ▼
Geometry + Camera Interception Layer
        │
        ▼
Modern Renderer
        │
        ├────► Flat renderer
        ├────► Widescreen / arbitrary FOV
        └────► Stereo left/right eye rendering
                        │
                        ▼
                     OpenXR
                        │
                        ├────► HMD pose
                        ├────► left/right eye projection
                        ├────► VR controller input
                        └────► swapchains
```

---

# Phase 1 — Establish a Stable VR Development Baseline

Before modifying rendering behavior, create a reproducible baseline.

## Tasks

- Tag or branch the current working recomp.
- Record the exact game version/disc hash.
- Confirm deterministic startup into gameplay.
- Add runtime logging if not already available.
- Add a debug configuration with symbols enabled.
- Add an easy way to disable all VR patches and launch the original native render path.

Suggested branches:

```text
main               working native recomp
vr-dev             active VR development
render-refactor     renderer interception experiments
```

## Success Criteria

- Native game still runs unchanged.
- VR work can be toggled off completely.
- Regressions can be bisected cleanly.

---

# Phase 2 — Map the Camera and Render Pipeline

This is the most important reverse-engineering stage.

Do not attempt to name or understand the entire executable.

Identify only the systems required for VR.

## Primary Reverse-Engineering Targets

```text
MainLoop()
UpdatePlayer()
UpdateCamera()
BuildCameraMatrix()
BuildProjection()
SetGeomOffset()
SetGeomScreen()
RotTransPers()
RotTransPers4()
GPU packet submission
Draw ordering / OT handling
ReadController()
UpdateHUD()
CutsceneCamera()
```

Names will differ in the actual game.

## Locate

### Camera state

Find variables or structures representing:

```text
camera.position
camera.rotation
camera.target
camera.up
camera.fov / projection distance
camera near/far behavior
view matrix
projection parameters
```

### Player state

Find:

```text
player.position
player.rotation
player eye/head height
player movement vector
player aim direction
```

### Projection state

PS1 titles commonly use GTE projection configuration rather than an explicit modern FOV.

Identify use of values conceptually equivalent to:

```text
SetGeomScreen(H)
SetGeomOffset(OFX, OFY)
```

and any game-side values that determine projection distance.

## Deliverables

Create and maintain:

```text
reverse/camera.md
reverse/player.md
reverse/render-pipeline.md
reverse/memory-map.md
reverse/functions.csv
```

## Success Criteria

- Camera world position can be identified at runtime.
- Camera orientation can be manipulated.
- Projection scale/FOV can be modified.
- Player transform can be identified independently from the camera.

---

# Phase 3 — Implement a Debug / Free Camera

Before attempting VR, prove that the game's camera can be detached from the original camera behavior.

## Required Features

Add a debug camera with:

```text
WASD / stick movement
mouse / stick look
vertical movement
FOV adjustment
speed adjustment
freeze original camera option
```

Ideally provide hotkeys:

```text
F1  toggle original/debug camera
F2  freeze game camera
F3  show camera position
F4  show projection parameters
```

## Why This Matters

If a free camera works while:

- gameplay continues,
- geometry renders correctly,
- the camera can rotate beyond the original view,
- the camera can translate independently,

then the core separation necessary for VR is already proven.

## Success Criteria

- Full 360° camera rotation.
- Arbitrary camera translation.
- No major world geometry corruption.
- Original gameplay remains functional.

---

# Phase 4 — Create a Render Abstraction Layer

Do not tie VR logic directly into individual PS1 GPU calls.

Introduce an intermediate view/render abstraction.

Suggested structure:

```cpp
struct RenderView {
    Mat4 view;
    Mat4 projection;

    Vec3 position;
    Quat rotation;

    float nearPlane;
    float farPlane;

    int width;
    int height;
};
```

The flat game initially uses:

```cpp
RenderView mainView;
```

VR later uses:

```cpp
RenderView leftEye;
RenderView rightEye;
```

## Success Criteria

- Existing non-VR rendering runs through the abstraction.
- Flat rendering remains visually equivalent to the current executable.

---

# Phase 5 — Decouple Render Resolution from PS1 Screen Coordinates

The original game may assume values such as:

```text
320 x 240
256 x 240
512 x 240
```

Do not change game logic to operate directly in headset resolution.

Keep original screen-space assumptions intact where possible and translate them in the renderer.

The game should continue believing it is drawing into PS1-style coordinates while the modern renderer outputs at arbitrary resolution.

## Add Support For

```text
1280x720
1920x1080
2560x1440
arbitrary viewport sizes
```

## Success Criteria

- Render resolution changes without breaking world transforms.
- Menus/HUD still align correctly or can be handled separately.

---

# Phase 6 — Implement Arbitrary Aspect Ratio and FOV

Before stereo rendering, make the game robust to arbitrary projections.

Progression:

```text
Original 4:3
    ↓
16:9
    ↓
ultrawide / arbitrary aspect
    ↓
adjustable horizontal/vertical FOV
```

A PS1 game may expose projection distance rather than FOV.

Conceptually:

```cpp
fov = 2.0f * atan(screen_width / (2.0f * projection_distance));
```

Do not assume this exact formula matches the title until the projection path has been confirmed.

## Add Debug Controls For

```text
FOV
aspect ratio
projection distance
camera near clipping approximation
```

## Success Criteria

- Game can render at multiple aspect ratios.
- FOV can be widened significantly without world geometry collapsing.
- Projection no longer depends on one fixed PS1 framebuffer size.

---

# Phase 7 — Intercept Geometry Before Final Screen Projection

This is likely the most technically significant renderer change.

For correct stereo VR, avoid rendering only already-projected PS1 screen coordinates.

A weak approach is:

```text
world vertex
    ↓
PS1 projection
    ↓
2D screen XY
    ↓
try to fake stereo
```

The preferred approach is:

```text
world/model vertex
    ↓
original model/world transform
    ↓
camera-space vertex
    ↓
modern per-eye projection
```

## Target Interception Points

Investigate calls equivalent to:

```text
RTPS
RTPT
RotTransPers
RotTransPers4
ApplyMatrix
CompMatrix
```

or custom wrappers around PsyQ GTE operations.

Capture sufficient information to reconstruct:

```cpp
Vec3 cameraSpaceVertex;
```

or preferably:

```cpp
Vec3 worldSpaceVertex;
```

Then feed those to the modern renderer.

## Success Criteria

- Geometry can be projected using a modern projection matrix.
- Stereo eye separation produces actual depth rather than screen-space offset.

---

# Phase 8 — Modernize GPU Primitive Submission

Translate original PS1 GPU primitives into modern draw calls.

Support the primitive types actually used by the game, for example:

```text
POLY_F3
POLY_F4
POLY_G3
POLY_G4
POLY_FT3
POLY_FT4
POLY_GT3
POLY_GT4
SPRT
TILE
LINE
```

Preserve:

```text
UVs
CLUT selection
texture page
semi-transparency mode
vertex color
primitive ordering
masking behavior where required
```

## Important

Keep ordering-table behavior compatible initially.

VR correctness matters more than replacing all PS1 rendering behavior with aggressive batching.

## Success Criteria

- Flat modern rendering matches the current native recomp closely.
- 3D primitives render from geometry rather than final PS1 XY coordinates.

---

# Phase 9 — Add Stereo Rendering Before OpenXR

Prove stereo geometry on a normal monitor first.

Render:

```text
left-eye view | right-eye view
```

with a configurable eye separation.

Example parameters:

```text
IPD: 64 mm
world scale: 1.0
stereo convergence: determined by camera model
```

## Success Criteria

- Left and right views contain correct parallax.
- Near objects shift more than distant objects.
- No duplicated game-state updates occur between eyes.

The game should update once per frame and render twice.

```text
UpdateGame()
Render(leftEye)
Render(rightEye)
```

NOT:

```text
UpdateGame()
Render(leftEye)
UpdateGame()
Render(rightEye)
```

---

# Phase 10 — Integrate OpenXR

Introduce OpenXR only after stereo rendering works independently.

## Core OpenXR Flow

Implement:

```text
xrCreateInstance
xrGetSystem
xrCreateSession
xrCreateReferenceSpace
xrEnumerateViewConfigurationViews
xrCreateSwapchain
xrWaitFrame
xrBeginFrame
xrLocateViews
xrAcquireSwapchainImage
render eyes
xrReleaseSwapchainImage
xrEndFrame
```

Create a dedicated module:

```text
vr/OpenXRContext.cpp
vr/OpenXRContext.h
```

## Runtime Targets

Aim for generic OpenXR compatibility rather than a headset-specific API.

Potential runtimes include:

```text
SteamVR OpenXR
Meta OpenXR
Virtual Desktop OpenXR
Monado
Windows-supported OpenXR runtimes
```

## Success Criteria

- Headset initializes.
- Game renders into OpenXR swapchains.
- Left/right eye projections come from OpenXR.
- Headset presents the game world correctly.

---

# Phase 11 — Separate Body Camera from HMD Pose

Do not overwrite the game's camera transform directly with headset orientation.

Use:

```text
Game/body camera
      +
HMD local pose
      =
VR eye camera
```

Conceptually:

```cpp
Quat bodyRotation = gameCamera.rotation;
Quat headRotation = xrHead.orientation;

Quat finalRotation = bodyRotation * headRotation;

Vec3 finalPosition =
    gameCamera.position +
    Rotate(bodyRotation, xrHead.position * worldScale);
```

This allows the original game to continue controlling player/body orientation while the player independently looks around.

## Success Criteria

- Player can look independently from movement direction.
- HMD positional tracking produces room-scale head translation.
- Game scripts can still move/rotate the body camera when necessary.

---

# Phase 12 — World Scale and Player Height

PS1 games rarely operate in real-world meters.

Create a configurable world-to-meter conversion.

Example:

```cpp
float unitsPerMeter;
float worldScale;
float playerEyeHeight;
```

Provide runtime adjustment initially.

## Calibration Targets

Use recognizable objects:

```text
doors
characters
vehicles
furniture
weapon models
player eye level
```

## Success Criteria

- Human-scale objects feel approximately correct.
- IPD does not make the world appear miniature or enormous.

---

# Phase 13 — HUD and 2D Rendering

Do not place the original HUD directly at per-eye screen depth.

Separate 2D primitives from world geometry.

Render HUD/menu content into an offscreen texture.

Then display it as a VR panel.

Suggested default:

```text
HUD distance:      1.5 m
HUD width:         1.2–1.5 m
HUD curvature:     optional
HUD lock mode:     head-locked or world-locked
```

Support at least:

```text
head-locked HUD
world-locked HUD
cinema/menu panel
```

## Success Criteria

- HUD is readable without eye strain.
- Menus remain usable.
- 2D overlays do not produce incorrect stereo depth.

---

# Phase 14 — Cutscene Handling

Scripted PS1 camera movement can be uncomfortable in VR.

Implement multiple modes.

## Native VR Mode

Use the scripted camera position and add independent HMD look.

## Comfort Mode

Follow cutscene translation/body heading while suppressing or reducing scripted pitch/roll.

## Cinema Mode

Render the original cutscene to a floating virtual screen.

Recommended options:

```text
Cutscene Mode:
- Native VR
- Comfort VR
- Cinema Screen
```

## Success Criteria

- Every cutscene remains viewable.
- No cutscene is allowed to completely break camera tracking.

---

# Phase 15 — Input Layer

Initially preserve the original game control model.

Suggested mapping:

```text
VR left stick        original movement
VR right stick       body/snap turning
A/B/X/Y              original face buttons
triggers/grips       L/R buttons as appropriate
HMD                   look direction only
```

Later optional enhancements can include:

```text
motion-controller aiming
independent weapon direction
hand tracking
room-scale interactions
```

These should not block the initial VR release.

---

# Phase 16 — VR Comfort Features

Implement these early enough that the game can actually be tested for meaningful periods.

Required options:

```text
Snap Turn
- Off
- 15°
- 30°
- 45°

Smooth Turn
- speed control

World Scale
Camera Height
Camera Offset

Head Bob
- 0–100%

Camera Shake
- 0–100%

Scripted Camera Roll
- 0–100%

Vignette
- optional

Cutscene Mode
- Native
- Comfort
- Cinema
```

---

# Phase 17 — Frame Rate and Simulation Decoupling

VR headsets typically expect higher presentation rates than PS1 game logic.

Determine whether the game is tied to:

```text
30 Hz
25 Hz
20 Hz
15 Hz
field rate
frame count
```

Do not blindly force 90 Hz game simulation.

Preferred architecture:

```text
Original simulation tick
        ↓
state snapshots
        ↓
render interpolation
        ↓
72/80/90/120 Hz VR presentation
```

Potentially interpolate:

```text
camera
player transform
actors
moving objects
```

while keeping gameplay logic unchanged.

## Success Criteria

- Game simulation remains correct.
- HMD movement renders smoothly at headset refresh rate.

---

# Phase 18 — Framebuffer Effects and Compatibility

Audit any game-specific effects involving:

```text
framebuffer copies
screen distortion
blur
fog overlays
feedback effects
motion trails
render-to-texture tricks
full-screen fades
FMV playback
```

Classify rendering into:

```text
3D world
2D HUD
2D menus
framebuffer effects
video
```

Fallback incompatible effects to an offscreen flat texture when necessary.

Correct VR presentation is more important than perfectly reconstructing every effect immediately.

---

# Phase 19 — Audio

Initial release:

```text
preserve original stereo audio
```

Optional later work:

```text
identify SPU voices
map emitters to game entities
convert selected sounds to 3D positional audio
```

This is not required for the first VR implementation.

---

# Phase 20 — Optional Enhancements After VR Works

Only begin these after core VR gameplay is stable.

Possible enhancements:

```text
HD texture replacement
higher internal resolution
perspective-correct texture mapping
model replacement
improved filtering
higher draw distance
LOD changes
60 FPS game patches
spatial audio
motion-controller aiming
weapon model replacement
room-scale interaction
```

Avoid combining these with initial VR bring-up.

---

# Recommended Repository Layout

```text
game-vr/
│
├── recomp/
│   ├── generated/
│   ├── patches/
│   └── symbols/
│
├── reverse/
│   ├── camera.md
│   ├── player.md
│   ├── render-pipeline.md
│   ├── memory-map.md
│   └── functions.csv
│
├── runtime/
│   ├── psx/
│   ├── renderer/
│   ├── input/
│   └── audio/
│
├── renderer/
│   ├── RenderView.h
│   ├── GeometryInterceptor.cpp
│   ├── PSXPrimitiveRenderer.cpp
│   ├── TextureCache.cpp
│   └── HUDRenderer.cpp
│
├── vr/
│   ├── OpenXRContext.cpp
│   ├── OpenXRContext.h
│   ├── VRCamera.cpp
│   ├── VRCamera.h
│   ├── VRRenderer.cpp
│   ├── VRInput.cpp
│   ├── VRHUD.cpp
│   ├── VRComfort.cpp
│   └── VRConfig.cpp
│
├── patches/
│   ├── camera.cpp
│   ├── projection.cpp
│   ├── widescreen.cpp
│   ├── framerate.cpp
│   └── vr.cpp
│
├── tools/
│
└── CMakeLists.txt
```

---

# Development Milestones

Since native recompilation is already working, milestones begin from the current state.

| Milestone | Result |
|---|---|
| **M0 — Baseline** | Freeze/tag known-working native recomp |
| **M1 — Camera Map** | Camera/player/projection paths identified |
| **M2 — Free Camera** | Camera can move/rotate independently |
| **M3 — Render Abstraction** | Existing flat renderer uses `RenderView` |
| **M4 — Arbitrary Resolution** | Native renderer no longer tied to PS1 output resolution |
| **M5 — Widescreen/FOV** | Arbitrary aspect ratio and FOV work |
| **M6 — Geometry Interception** | 3D vertices available before final screen projection |
| **M7 — Modern Projection** | World geometry rendered using modern matrices |
| **M8 — Stereo Monitor Test** | Correct left/right parallax on normal display |
| **M9 — OpenXR Init** | HMD session + swapchains operational |
| **M10 — VR Rendering** | Correct per-eye headset rendering |
| **M11 — 6DoF Camera** | HMD rotation + translation layered on game camera |
| **M12 — World Scale** | Comfortable real-world scale calibrated |
| **M13 — HUD/Menu VR** | 2D content presented on VR panels |
| **M14 — Cutscenes** | Native/comfort/cinema cutscene modes |
| **M15 — VR Input** | Original controls mapped cleanly to VR controllers |
| **M16 — Comfort** | snap turn, shake reduction, bob controls, etc. |
| **M17 — High Refresh** | render interpolation / high-rate HMD rendering |
| **M18 — Compatibility** | framebuffer effects and edge cases handled |
| **M19 — Polish** | settings, persistence, performance, packaging |

---

# Immediate Next Tasks

Given the current working native executable, the next work should happen in this order:

## 1. Find the camera structure

Identify the runtime memory/state containing:

```text
position
rotation
projection distance / FOV equivalent
```

## 2. Find the projection/GTE configuration

Trace all calls or wrappers related to:

```text
SetGeomScreen
SetGeomOffset
RTPS
RTPT
RotTransPers
RotTransPers4
```

## 3. Add camera logging

Print once per frame or via debug overlay:

```text
camera position
camera rotation
projection value
player position
```

## 4. Implement a free camera

Replace or offset the game's normal camera transform.

## 5. Prove arbitrary FOV

Increase/decrease projection scale without corrupting the world.

## 6. Identify where 3D geometry can be captured before screen projection

This determines whether the title can receive true stereoscopic VR cleanly or requires deeper renderer reconstruction.

---

# Core Technical Principle

Do not modify the original game logic more than necessary.

Aim for:

```text
Original game logic
Original physics
Original animation
Original AI
Original scripting
Original collision
        │
        ▼
Camera + geometry interception
        │
        ▼
Modern renderer
        │
        ▼
VR
```

rather than rewriting the title as a new engine port.

---

# First Major Go / No-Go Checkpoint

The most important technical checkpoint is:

> Can we obtain stable world-space or camera-space geometry before final PS1 screen projection?

If **yes**, proceed with modern projection and normal stereo rendering.

If **partially**, intercept GTE transforms and reconstruct the missing geometry data.

If **no**, investigate deeper GTE emulation/runtime interception before doing significant OpenXR work.

Do not build a large VR layer on top of already-flattened 2D PS1 primitives.

---

# Definition of Initial VR Success

The first successful VR build does **not** need:

```text
motion controls
HD textures
60 FPS game logic
perfect cutscenes
spatial audio
model upgrades
```

It only needs:

```text
working native game
correct stereo geometry
OpenXR output
6DoF head tracking
comfortable world scale
usable original controls
readable HUD
basic cutscene fallback
```

Once those work reliably, the game is fundamentally VR-capable and further improvements become incremental rather than architectural.
