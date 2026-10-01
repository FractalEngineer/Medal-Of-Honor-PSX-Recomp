# Render pipeline (grounded)

Data flow, top to bottom, with the files that own each step. Line numbers are
for psxrecomp master `3505f2a0`.

```
guest camera/model code (libgte RotTransPers/RTPT + per-title camera updater)
  ├─ CTC2  RT / TR / OFX / OFY / H         runtime/src/gte.cpp:1512  (cases 24/25/26)
  └─ COP2 command word  ── emitted as ──►  recompiler/src/code_generator.cpp:1682-1685
                                            gte_execute(cpu, 0x…)
        ▼
  gte_execute                     runtime/src/gte.cpp:1811
        ▼
  gte_run_command                 runtime/src/gte.cpp:1782
        ▼                         (0x01 → gte_rtps, 0x30 → gte_rtpt, 0x12 → gte_mvmva)
  gte_rtps  runtime/src/gte.cpp:919   /   gte_rtpt  :926
        ▼
  gte_rtps_internal               runtime/src/gte.cpp:804      <-- VECTOR SEAM (camera space)
     • MAC = TR*4096 + RT·V ; IR saturate ; SZ FIFO
     • h_div_sz = H / SZ3
     • xterm = IR1*h/sz  ×(widescreen squash)  gte.cpp:848-888
     • sx16 = OFX + xterm ; sy16 = OFY + IR2*h/sz   gte.cpp:889-890
     • push_sxy (faithful 11-bit SXY FIFO)     runtime/include/gte.h
        ▼                         (SXY0/1/2, SZ exported back to cpu->gte_data  gte.cpp:1721)
  guest reads SXY via MFC2, builds a GP0 packet in RAM
        ▼  DMA walks the linked list → GPU
  gpu_write_gp0  runtime/src/gpu.c:6238 → gpu_write_gp0_body :6040
        ▼
  gp0_execute_command             runtime/src/gpu.c:5793
        ▼  per primitive
  gp0_exec_mono_tri  :4131   POLY_F3      gp0_exec_textured_tri :4351   POLY_FT3
  gp0_exec_shaded_tri :4239  POLY_G3      gp0_exec_textured_quad :4391  POLY_FT4
  gp0_exec_textured_rect :4705 SPRT (2-D)  gp0_exec_mono_rect :4651      TILE
        ▼
  gr_* facade                     runtime/src/gpu_render.c   (runtime/include/gpu_render.h)
        ▼  vtable: GpuRenderBackend
  sw_*   runtime/src/gpu_sw_renderer.c
  glb_*  runtime/src/gpu_gl_renderer.c   (default in MOH: opengl)
  vkb_*  runtime/src/gpu_vk_renderer.c   (stub unless PSX_HAVE_VULKAN)
        ▼  batch + glDrawArrays / vkCmdDraw
  present at simulated VBlank
     gpu_vblank_tick            runtime/src/gpu.c:3473
       → sdl_vblank_present     runtime/src/main.cpp:8036  (callback bound at :16169)
       → sdl_vblank_present_body runtime/src/main.cpp:6866
       → gl_renderer_present_vram / _wide_fbo   (main.cpp:7683-7689)
```

## Where widescreen already intercepts (precedent for VR)

Widescreen is the existing proof that the projection can be rewritten per frame:

- `gte_set_display_aspect` runtime/src/gte.cpp:785 — sets the squash factor
  (identity at 4:3); applied inside `gte_rtps_internal` at :852-890.
- `refresh_widescreen_projection` runtime/src/main.cpp:1872 — resolves mode and
  pushes `gte_set_display_aspect(...)` + `gpu_ws_configure(...)`.
- Guest **cull widening**: config-driven per-instruction rewrites emitted by
  `recompiler/src/code_generator.cpp:873+`, runtime helpers `psx_ws_*` in
  `runtime/src/gpu.c` (e.g. `psx_ws_x_margin` :1202).
- `game.toml [widescreen]` is parsed in `recompiler/src/config_loader.cpp:1622+`.

VR can reuse the same seam discipline: modify projection/culling **at the GTE**,
never inside the 2-D rasterizers.

## Seam index (what to touch for VR)

| Concern | File:line | Note |
|---|---|---|
| GTE vector/projection seam | `runtime/src/gte.cpp:804` | capture camera-space vertices here |
| GTE command funnel | `runtime/src/gte.cpp:1811` / `:1782` | single entry for all GTE ops |
| RTPS / RTPT | `runtime/src/gte.cpp:919` / `:926` | per-vertex / per-triple |
| GTE control regs (H/OFX/OFY) | `runtime/src/gte.cpp:1512` | projection config source |
| COP2 emit | `recompiler/src/code_generator.cpp:1685` | where guest GTE calls land |
| Primitive → draw | `runtime/src/gpu.c:5793` | GP0 dispatch |
| Renderer facade | `runtime/include/gpu_render.h`, `runtime/src/gpu_render.c` | backend vtable |
| Backends | `gpu_sw_renderer.c` / `gpu_gl_renderer.c` / `gpu_vk_renderer.c` | 2-D VRAM rasterizers |
| Frame boundary | `runtime/src/main.cpp:6866` | present hook (per VBlank) |
| Per-frame plugin hook | `mod_plugins.h` VBlank callback | only host per-frame hook available |

## Consequences for VR

1. World-space vertices must be **reconstructed at the GTE**, not read from the
   renderer. Without this, stereo can only be a screen-space offset (the "weak
   approach" the plan warns against).
2. 2-D primitives (SPRT/TILE) never pass the GTE. They must be classified
   (HUD/menu vs world overlay) and routed to a panel, per plan Phase 13.
3. The GTE capture is a framework change. There is no plugin service for it.
