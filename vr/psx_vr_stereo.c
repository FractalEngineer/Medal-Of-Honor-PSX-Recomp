/*
 * Medal of Honor VR - entity-transform probe / stereo injection point.
 *
 * FUN_80084718 is the per-entity transform builder (M1, docs/reverse/M1_VIEW_MATRIX.md):
 *
 *     local_14 = *(short*)(entity + 0x98) << 0x13;   // world X
 *     local_10 = *(short*)(entity + 0x9a) << 0x13;   // world Y
 *     local_c  = *(short*)(entity + 0x9c) << 0x13;   // world Z
 *     FUN_80013ae4(arg2, &local_28, arg4, entity);   // scene-graph walker
 *
 * param_1 ($a0 at entry) is the entity. This hook runs at the top of the
 * generated function, so $a0 still holds it.
 *
 * Environment (all optional; the plugin is inert without them):
 *   PSX_VR_PROBE=1        log the entity table and struct dumps
 *   PSX_VR_TARGET=0xADDR  entity to offset (0 or unset = every entity)
 *   PSX_VR_AXIS=0|1|2     axis to offset, 0=X (default), 1=Y, 2=Z
 *   PSX_VR_OFFSET=N       signed delta added to that axis (0 = disabled)
 *   PSX_VR_PASS_PROBE=1   no-op capture at gameplay's render-wait entry
 *                        (also set PSX_VR_INTERP=1; inspect render_pass_stats)
 *   PSX_VR_PASS_WATCHDOG=1 one-shot synthetic rollback fault injection
 *                       =2 inject while inside the real level draw dispatch
 *   PSX_VR_PASS_DRAW=1    experimental bounded scene draw, excluding wait/flip
 *                    =2  clear-only control; =3 level-only coverage control
 *   PSX_VR_STEREO=1      paired-eye scene draw and side-by-side presentation
 *   PSX_VR_EYE_OFFSET=N  diagnostic camera-unit half separation override
 *   PSX_VR_IPD_MM=N      desktop IPD (default 67; XR uses located eye poses)
 *   PSX_VR_UNITS_PER_METER=N provisional metric mapping (default 48/.067)
 *   PSX_VR_WORLD_SCALE=N divisor on units/meter (default 1; Quest profile 3)
 *   PSX_VR_OPENXR=1      headset mode; compile PSX_OPENXR and set host PSX_OPENXR=1
 *   PSX_VR_MOVEMENT=0|1  Quest movement source (default on for XR, off otherwise)
 *   PSX_VR_MOVE_DEADZONE=N radial move/scalar turn deadzone (default .2)
 *   PSX_VR_TURN_GAIN=N   decoded turn response gain (default .65; native game rate)
 *   PSX_VR_AUTHORED_FOCAL=0 disable guest H/400 ratio for XR projection control
 *   PSX_VR_TEXT=0 / PSX_VR_HUD_ICON=0 omit measured text/icon draw calls
 *   PSX_VR_HEAD_YAW=degrees / PSX_VR_HEAD_POSITION=x,y,z synthetic desktop pose
 *   PSX_VR_DESKTOP_FOV=1 deterministic Quest-FOV projection control
 *   PSX_VR_STEREO_FAULT=1 decline one right eye after a successful pair
 *                     =2 watchdog inside right-eye level dispatch
 *   PSX_VR_STEREO_FAULT_HOLD=1 stop pair requests after the injected fault
 *
 * The offset is applied as patch-then-restore: the entity's original value is
 * written back at the start of its NEXT call before re-patching, so the stored
 * position never drifts between frames. The body still reads the patched value.
 */
#include "mod_plugins.h"
#include "cpu_state.h"
#include "psx_cycles.h"
#include "vr_pose_math.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "moh_vr_input.h"

#define FUN_80084718 0x80084718u
/* Geometry pass: entity -> screen geometry. FUN_80082948 dispatches here on
 * entity+0x6c == 0x80 (FUN_80080dd4) else FUN_800814c4. */
#define GEO_800814C4 0x800814c4u
#define GEO_80080DD4 0x80080dd4u
/* Per-model render entry: called from the scene drivers FUN_8006d2e4 /
 * FUN_8006d460 and dispatches through FUN_80082948. Hooking here enumerates
 * every entity rendered in a frame, top-down. */
#define RENDER_800824D0 0x800824d0u
/* The LEVEL vertex transformer: prologue ADDIU $sp,$sp,-288 at 0x8008B3E8,
 * ends 0x8008BEF8. Contains ~12 unrolled RTPS sites (0x8008B5B8 .. 0x8008BE04),
 * each LWC2-loading a vertex from $t0 and SWC2-storing SXY to $t1, plus
 * CTC2 H writes. This is the first RTPS path found on the level, as opposed to
 * the dynamic-object branch. */
#define LVTX_8008B3E8 0x8008b3e8u
/* Legacy VSync probe. VSync is the PsyQ mode wrapper at 0x80016E38; a loop
 * calls VSync(0) at 0x8008B254, right before DrawSync(0) and PutDispEnv
 * (0x80017408). That is exactly the point RENDER_PASSES.md wants passes planned
 * from. This is not slot 1's gameplay wait path, which is FUN_80090B80 below.
 * The legacy probe gates on a0 == 0; it does not isolate a single caller.
 * See docs/reverse/VR_HOOK_POINT.md and VR_PHASE9_STATUS.md for corrections. */
#define VSYNC_80016E38 0x80016e38u
#define RENDER_WAIT_80090B80 0x80090b80u
#define MOH_FLIP_PERIOD_VBLANKS 2u   /* 30 Hz game: flips every 2 VBlanks */

#define VR_ENTITY_SLOTS 256
#define VR_PROBE_PERIOD 20u   /* calls between entity position samples */
#define VR_GEO_PERIOD 40u     /* calls between GTE register samples */

typedef struct {
    uint32_t entity;
    int16_t x, y, z;
    uint32_t hits;
} VrEntitySlot;

/* Restore record: one per entity we have patched. */
typedef struct {
    uint32_t entity;
    int16_t orig[3];
    int patched;
} VrPatchSlot;

static VrEntitySlot g_slots[VR_ENTITY_SLOTS];
static VrPatchSlot g_patch[VR_ENTITY_SLOTS];
static uint32_t g_calls;
static int g_probe;      /* PSX_VR_PROBE */
static uint32_t g_target;/* PSX_VR_TARGET, 0 = all */
static int g_axis;       /* PSX_VR_AXIS */
static int32_t g_offset; /* PSX_VR_OFFSET */
static void vr_nested_watchdog(CPUState* cpu);

static int16_t vr_rd(uint32_t ent, int axis) {
    return (int16_t)psx_mod_read_half(ent + 0x98 + (uint32_t)axis * 2);
}

static void vr_wr(uint32_t ent, int axis, int16_t v) {
    psx_mod_write_half(ent + 0x98 + (uint32_t)axis * 2, (uint16_t)v);
}

static VrPatchSlot* vr_patch_slot(uint32_t ent) {
    uint32_t s = (ent >> 4) & (VR_ENTITY_SLOTS - 1);
    for (uint32_t i = 0; i < VR_ENTITY_SLOTS; ++i) {
        uint32_t k = (s + i) & (VR_ENTITY_SLOTS - 1);
        if (g_patch[k].entity == ent) return &g_patch[k];
        if (g_patch[k].entity == 0) { g_patch[k].entity = ent; return &g_patch[k]; }
    }
    return NULL;
}

static void vr_dump(const char* tag, uint32_t base, uint32_t off, uint32_t len) {
    fprintf(stdout, "vr-probe:   %s +%03X:", tag, off);
    for (uint32_t i = 0; i < len; i += 4) {
        fprintf(stdout, " %08X", psx_mod_read_word(base + off + i));
    }
    fprintf(stdout, "\n");
    fflush(stdout);
}

static void vr_entity_entry(CPUState* cpu, uint32_t address) {
    uint32_t entity = cpu->gpr[4]; /* $a0 = param_1 */
    (void)address;

    if (!entity) return;

    /* Only guest RAM. $a0 is a pointer, so a stale/nonsense value must not be
     * dereferenced blindly. */
    if (entity < 0x80000000u || entity >= 0x80200000u) return;

    VrPatchSlot* ps = vr_patch_slot(entity);

    /* Undo the previous frame's patch before reading, so the value we read is
     * the game's own and repeated frames do not accumulate. */
    if (ps && ps->patched) {
        for (int a = 0; a < 3; ++a) vr_wr(entity, a, ps->orig[a]);
        ps->patched = 0;
    }

    int16_t x = vr_rd(entity, 0);
    int16_t y = vr_rd(entity, 1);
    int16_t z = vr_rd(entity, 2);

    if (ps) {
        ps->orig[0] = x; ps->orig[1] = y; ps->orig[2] = z;
        if (g_offset != 0 && (g_target == 0 || g_target == entity)) {
            int16_t v = vr_rd(entity, g_axis);
            vr_wr(entity, g_axis, (int16_t)(v + g_offset));
            ps->patched = 1;
            if (g_calls < 4) {
                fprintf(stdout, "vr-probe: PATCH ent=%08X axis=%d %d -> %d\n",
                        entity, g_axis, v, (int)(int16_t)(v + g_offset));
                fflush(stdout);
            }
        }
    }

    uint32_t slot = (entity >> 4) & (VR_ENTITY_SLOTS - 1);
    for (uint32_t i = 0; i < VR_ENTITY_SLOTS; ++i) {
        uint32_t s = (slot + i) & (VR_ENTITY_SLOTS - 1);
        if (g_slots[s].entity == entity) {
            g_slots[s].x = x;
            g_slots[s].y = y;
            g_slots[s].z = z;
            g_slots[s].hits++;
            break;
        }
        if (g_slots[s].entity == 0) {
            g_slots[s].entity = entity;
            g_slots[s].x = x;
            g_slots[s].y = y;
            g_slots[s].z = z;
            g_slots[s].hits = 1;
            if (g_probe) {
                fprintf(stdout, "vr-probe: NEW ENTITY %08X\n", entity);
                vr_dump("ent", entity, 0x000, 32);
                vr_dump("ent", entity, 0x080, 64);
                vr_dump("ent", entity, 0x380, 64);
                fflush(stdout);
            }
            break;
        }
    }

    g_calls++;
    if (g_probe && (g_calls % VR_PROBE_PERIOD) == 0) {
        fprintf(stdout, "vr-probe: call=%u ent=%08X pos=(%d,%d,%d)\n", g_calls,
                entity, x, y, z);
        fflush(stdout);
    }
}

/* Geometry-pass probe: log what these functions actually receive before we
 * try to patch anything. Registers only, bounded, no writes. */
static uint32_t g_geo_calls;

static void vr_geo_entry(CPUState* cpu, uint32_t address) {
    g_geo_calls++;
    if (!g_probe) return;
    /* The accumulated world->camera transform lives in the GTE on entry:
     * control regs 5/6/7 are TRX/TRY/TRZ, 24/25/26 are OFX/OFY/H. Logging
     * these across a strafe is what identifies the translation. */
    if (g_geo_calls % VR_GEO_PERIOD == 0) {
        fprintf(stdout,
                "vr-gte: n=%u T=(%ld,%ld,%ld) OF=(%ld,%ld) H=%ld\n",
                g_geo_calls,
                (long)(int32_t)cpu->gte_ctrl[5], (long)(int32_t)cpu->gte_ctrl[6],
                (long)(int32_t)cpu->gte_ctrl[7],
                (long)(int32_t)cpu->gte_ctrl[24], (long)(int32_t)cpu->gte_ctrl[25],
                (long)(int32_t)cpu->gte_ctrl[26]);
        fflush(stdout);
    }
    if (g_geo_calls > 40) return;
    uint32_t a0 = cpu->gpr[4], a1 = cpu->gpr[5], a2 = cpu->gpr[6], a3 = cpu->gpr[7];
    uint32_t ra = cpu->gpr[31];
    fprintf(stdout,
            "vr-geo: call=%u at=%08X a0=%08X a1=%08X a2=%08X a3=%08X ra=%08X\n",
            g_geo_calls, address, a0, a1, a2, a3, ra);
    fflush(stdout);
}

/* Top-of-pipeline probe: enumerate every entity that reaches the per-model
 * render entry, so we can see whether the world is in the same list as the
 * weapon. Bounded distinct-pointer table, no writes. */
#define VR_RENDER_SLOTS 512
static uint32_t g_render_ent[VR_RENDER_SLOTS];
static uint32_t g_render_hits[VR_RENDER_SLOTS];
static uint32_t g_render_calls;
static uint32_t g_render_distinct;

static void vr_render_entry(CPUState* cpu, uint32_t address) {
    if (!g_probe) return;
    uint32_t a0 = cpu->gpr[4];
    g_render_calls++;

    int known = 0;
    for (uint32_t i = 0; i < VR_RENDER_SLOTS; ++i) {
        if (g_render_ent[i] == a0) { g_render_hits[i]++; known = 1; break; }
    }
    if (!known) {
        for (uint32_t i = 0; i < VR_RENDER_SLOTS; ++i) {
            if (g_render_ent[i] == 0) {
                g_render_ent[i] = a0;
                g_render_hits[i] = 1;
                g_render_distinct++;
                fprintf(stdout, "vr-render: NEW a0=%08X (distinct=%u) at call %u\n",
                        a0, g_render_distinct, g_render_calls);
                fflush(stdout);
                break;
            }
        }
    }
    if ((g_render_calls % 200) == 0) {
        fprintf(stdout, "vr-render: calls=%u distinct=%u\n", g_render_calls,
                g_render_distinct);
        fflush(stdout);
    }
    (void)address;
}

/* Level vertex transformer. RTPS computes RT*V + TR and only THEN perspective-
 * divides, so adding a delta to TRX (GTE control reg 5) shifts every vertex in
 * camera space before projection. That is a true per-eye offset, not a screen
 * shift. Logged once so we can see what TR actually holds here. */
static uint32_t g_lvtx_calls;
static int g_lvtx_logged;

static void vr_lvtx_entry(CPUState* cpu, uint32_t address) {
    (void)address;
    vr_nested_watchdog(cpu);
    g_lvtx_calls++;
    if (g_probe && !g_lvtx_logged) {
        g_lvtx_logged = 1;
        fprintf(stdout,
                "vr-lvtx: TR=(%ld,%ld,%ld) H=%ld (offset=%d)\n",
                (long)(int32_t)cpu->gte_ctrl[5], (long)(int32_t)cpu->gte_ctrl[6],
                (long)(int32_t)cpu->gte_ctrl[7],
                (long)(int32_t)cpu->gte_ctrl[26], g_offset);
        fflush(stdout);
    }
    if (g_offset != 0 && g_axis == 0) {
        int32_t tr = (int32_t)cpu->gte_ctrl[5];
        cpu->gte_ctrl[5] = (uint32_t)(tr + g_offset);
    }
}

/* Frame-loop VSync(0). Planning here (rather than running a pass) is provable
 * with no presenter: psx_mod_render_pass_plan reports 0 and the status says why,
 * and the two counters show the gate accepting only the frame loop's call. */
static uint32_t g_vsync_hits;
static uint32_t g_vsync_other;
static uint32_t g_vsync_last_status = 0xFFFFFFFFu;
static int g_interp;          /* PSX_VR_INTERP: enable interpolation + FLIP src */
static int g_pass_probe;      /* PSX_VR_PASS_PROBE: no-op before render wait */
static int g_pass_watchdog;   /* opt-in synthetic abort, not a guest draw */
static int g_pass_watchdog_done;
static int g_pass_draw;
static int g_in_pass;         /* cleared after API return, including watchdog abort */
static int g_stereo;
static int32_t g_eye_offset = 24;
/* Provisional scale preserves legacy 48-unit separation at user IPD 67mm.
 * It is a tuning starting point, not a measured physical scale. */
static double g_ipd_mm = 67.0, g_units_per_meter = 48.0 / .067;
static double g_world_scale = 1.0;
static unsigned g_draw_mask = 15u; /* diagnostic call exclusion, not HUD labels */
static int g_icon_visible = 1, g_text_visible = 1;
static int g_openxr;
static int g_movement = -1;
static double g_move_deadzone = .2, g_turn_gain = .65;
static int g_authored_focal = 1, g_desktop_fov;
static double g_head_yaw, g_head_position[3];
static PSXModRenderView g_eye_view[2];
static uint32_t g_stereo_eye;
static uint32_t g_stereo_success;
static int g_stereo_fault, g_stereo_fault_done, g_stereo_fault_hold;
static uint32_t g_interp_hz;  /* PSX_VR_INTERP_HZ: 0 = follow display refresh */
/* The pass rect. RENDER_PASSES.md wants "the display rect the next flip shows
 * (its DISPENV)". MoH double buffers - the GP1 0x05 origin alternates y=0 and
 * y=240 - so a constant y is suspect. Default 512x240 at (0,0);
 * PSX_VR_RECT="x,y,w,h" overrides, PSX_VR_RECT_ALT=1 adds 240 to y on
 * alternate frames to follow the flip. */
static int g_rect_x, g_rect_y = 0;
static int g_rect_w = 512, g_rect_h = 240;
static int g_rect_alt;

/* Default body is a no-op. Opt-in draw/coverage controls exercise the measured
 * slot-1 scene slice; paired-eye capture and presentation remain separate work. */
static uint32_t g_pass_runs;

static void vr_watchdog_inject(CPUState* cpu) {
    g_pass_watchdog_done = 1;
    cpu->gpr[4] ^= 0x13579bdfu;
    cpu->gte_ctrl[5] ^= 0x2468ace0u;
    psx_mod_write_word(0x800a0100u, 0x12345678u);
    psx_mod_write_word(0x1f8003f0u, 0x89abcdefu);
    for (uint32_t i = 0; i < 256u; ++i) psx_advance_cycles(65536u);
}

static void vr_nested_watchdog(CPUState* cpu) {
    if (g_in_pass && g_stereo && g_stereo_success && g_stereo_eye == PSX_MOD_EYE_RIGHT &&
        g_stereo_fault == 2 && !g_stereo_fault_done) {
        g_stereo_fault_done = 1;
        vr_watchdog_inject(cpu);
    }
    if (g_in_pass && g_pass_draw && g_pass_watchdog == 2 && !g_pass_watchdog_done)
        vr_watchdog_inject(cpu);
}

/* Reconstruct the calls before 0x8005047C from FUN_800503D4, then submit the
 * freshly built OT directly. Calling the enclosing function would enter a
 * frozen-time wait/flip. Each real function returns through the existing hook
 * caller's return address, which dispatch uses as its stop contract. */
static void vr_guest_call(CPUState* cpu, uint32_t entry, uint32_t a0,
                          uint32_t a1, uint32_t a2, uint32_t ra) {
    cpu->gpr[4] = a0;
    cpu->gpr[5] = a1;
    cpu->gpr[6] = a2;
    cpu->gpr[31] = ra;
    psx_dispatch_call(cpu, entry, ra);
}

static int vr_draw_scene(CPUState* cpu, const PSXModRenderPass* pass) {
    const uint32_t ctx = 0x8009a620u;
    const uint32_t ra = cpu->gpr[31];
    uint32_t index = psx_mod_read_word(ctx + 8708u);
    uint32_t world = psx_mod_read_word(0x8009cc64u);
    uint32_t camera = psx_mod_read_word(0x8009d654u);
    if (index > 1u || world < 0x80000000u || world >= 0x80200000u ||
        camera < 0x80000000u || camera >= 0x80200000u) return 0;
    /* Set the game's current draw environment without PutDispEnv or a wait.
     * Then clear the entire pass rect: untouched baseline pixels cannot hide
     * missing coverage in the first replay image. */
    vr_guest_call(cpu, 0x80017348u, ctx + 16u + 184u * index, 0, 0, ra);
    psx_mod_write_word(0x1f801810u, 0x02000000u);
    psx_mod_write_word(0x1f801810u, ((uint32_t)pass->y << 16) | pass->x);
    psx_mod_write_word(0x1f801810u, ((uint32_t)pass->h << 16) | pass->w);
    if (g_pass_draw == 2) return 1;
    vr_guest_call(cpu, 0x80050548u, psx_mod_read_word(0x8009d638u), 0, 0, ra);
    if (g_pass_draw == 3) {
        /* Resolve the level callback's entity from the current scene list.
         * Do not pin the observed slot-1 entity address. */
        uint32_t entity = psx_mod_read_word(0x8009d64cu);
        uint32_t remaining = 512u;
        while (entity >= 0x80000000u && entity < 0x801ff000u && remaining--) {
            if (psx_mod_read_word(entity + 880u) == 0x80053c20u) break;
            entity = psx_mod_read_word(entity + 4u);
        }
        if (entity < 0x80000000u || entity >= 0x801ff000u ||
            psx_mod_read_word(entity + 880u) != 0x80053c20u) return 0;
        vr_guest_call(cpu, 0x80053c20u, entity, ctx, 0, ra);
        vr_guest_call(cpu, 0x800172d8u,
                      psx_mod_read_word(ctx + 8728u) + 511u * 4u, 0, 0, ra);
        return 1;
    }
    if ((g_draw_mask & 1u) && psx_mod_read_byte(world + 1423u))
        vr_guest_call(cpu, 0x80088368u, ctx, 0, 0, ra);
    if ((g_draw_mask & 2u) && (psx_mod_read_word(world + 1420u) & 0x00ffffffu))
        vr_guest_call(cpu, 0x80089a0cu, ctx, 0, 0, ra);
    vr_guest_call(cpu, 0x80064550u, 0, 0, 0, ra);
    if (g_text_visible && (g_draw_mask & 4u)) vr_guest_call(cpu, 0x8005f86cu, psx_mod_read_word(ctx + 8728u),
                  psx_mod_read_word(ctx + 8732u), 0, ra);
    vr_guest_call(cpu, 0x8006c714u, ctx, psx_mod_read_word(0x8009d64cu), 0, ra);
    if (g_icon_visible && (g_draw_mask & 8u))
        vr_guest_call(cpu, 0x8008019cu, camera, ctx, 0, ra);
    /* ClearOTagR initializes 512 entries; DrawOTag starts from the final one.
     * 0x800172D8 is DrawOTag, confirmed by its own debug-string producer. */
    vr_guest_call(cpu, 0x800172d8u, psx_mod_read_word(ctx + 8728u) + 511u * 4u,
                  0, 0, ra);
    return 1;
}

static int vr_pass_fn(CPUState* cpu, void* user, uint32_t alpha_q16) {
    g_pass_runs++;
    if (g_pass_probe && g_pass_watchdog == 1 && !g_pass_watchdog_done) {
        /* Host selector survives rollback. Set it before the watchdog longjmp,
         * so later passes prove the entry guard recovered and can run again.
         * Deliberately change checkpointed CPU, GTE, RAM and scratchpad state;
         * PSX_RENDER_PASS_VERIFY checks restoration after the abort. */
        /* The default watchdog is 8M cycles; it can only be lowered by env.
         * Keep the injection bounded even if that contract changes. */
        vr_watchdog_inject(cpu);
        return 0; /* Unexpected survival: discard rather than publish. */
    }
    if (g_pass_draw) return user ? vr_draw_scene(cpu, user) : 0;
    if (g_probe && g_pass_runs <= 4u) {
        fprintf(stdout, "vr-pass-run: run=%u alpha=%u\n", g_pass_runs, alpha_q16);
        fflush(stdout);
    }
    return 1;   /* keep the image */
}

static int vr_stereo_fn(CPUState* cpu, void* user, uint32_t eye) {
    g_stereo_eye = eye;
    if (g_stereo_success && eye == PSX_MOD_EYE_RIGHT &&
        g_stereo_fault == 1 && !g_stereo_fault_done) {
        g_stereo_fault_done = 1;
        return 0;
    }
    /* A camera to the left sees scene coordinates translated right. The host
     * seam replaces its value per eye, affects RTPS/RTPT before division, and
     * restores automatically after the eye (including longjmp rollback). */
    if (!psx_mod_render_view(&g_eye_view[eye])) return 0;
    return user ? vr_draw_scene(cpu, user) : 0;
}

static void vr_vsync_entry(CPUState* cpu, uint32_t address) {
    (void)address;
    if (g_in_pass || g_pass_probe || g_stereo) return;
    /* Diagnostic: $ra is NOT reliable here. Static recompilation turns a guest
     * JAL into a native C call, so the callee's $ra is only meaningful when the
     * caller was interpreted (overlay code). Print what actually arrives so the
     * gate can be built on evidence instead of assumption. */
    if (g_probe && g_vsync_other < 24u) {
        g_vsync_other++;
        fprintf(stdout, "vr-vsync: a0=%08X ra=%08X sp=%08X\n",
                cpu->gpr[4], cpu->gpr[31], cpu->gpr[29]);
        fflush(stdout);
    }
    if (cpu->gpr[4] != 0u) {
        return;
    }
    g_vsync_hits++;
    if (g_probe && (g_vsync_hits <= 3u || (g_vsync_hits % 60u) == 0u)) {
        uint32_t alpha[8];
        uint32_t n = psx_mod_render_pass_plan(MOH_FLIP_PERIOD_VBLANKS, 1u, alpha, 8u);
        uint32_t st = psx_mod_render_pass_status();
        g_vsync_last_status = st;
        fprintf(stdout,
                "vr-pass: mode0 VSync hits=%u status=%u plan=%u\n",
                g_vsync_hits, st, n);
        fflush(stdout);
        for (uint32_t i = 0; i < n; i++) {
            PSXModRenderPass pass;
            memset(&pass, 0, sizeof pass);
            pass.struct_size = (uint32_t)sizeof(PSXModRenderPass);
            pass.alpha_q16 = alpha[i];
            /* MoH is double buffered: the DISPENV origin alternates y=0 / y=240,
             * so the rect the next flip shows is one of these two 512x240 bands. */
            pass.x = (uint16_t)g_rect_x;
            pass.y = (uint16_t)(g_rect_y + (g_rect_alt && (g_vsync_hits & 1u) ? 240 : 0));
            pass.w = (uint16_t)g_rect_w;
            pass.h = (uint16_t)g_rect_h;
            {
                int r = psx_mod_render_pass(cpu, &pass, vr_pass_fn, NULL);
                if (g_probe) {
                    fprintf(stdout,
                            "vr-pass-call: alpha=%u rect=%ux%u+%u+%u ret=%d status_after=%u runs=%u\n",
                            alpha[i], (unsigned)pass.w, (unsigned)pass.h,
                            (unsigned)pass.x, (unsigned)pass.y, r,
                            psx_mod_render_pass_status(), g_pass_runs);
                    fflush(stdout);
                }
            }
        }
    }
}

/* Slot 1 gameplay uses FUN_80090B80's DrawSync(-1) loop. Its first stores
 * permit the IRQ-driven flip (PutDispEnv itself runs in the exception and
 * cannot start a pass). Before those stores, read the upcoming DISPENV using
 * the exact !buffer_index calculation at 0x80090CCC..0x80090CE8. */
static void vr_wait_entry(CPUState* cpu, uint32_t address) {
    (void)address;
    if ((!g_pass_probe && !g_stereo) || g_in_pass) return;
    if (g_stereo && g_stereo_fault_done && g_stereo_fault_hold) return;
    uint32_t index = psx_mod_read_word(0x8009c824u);
    if (index > 1u) return;
    uint32_t env = 0x8009a7a0u + (index == 0u ? 20u : 0u);
    PSXModRenderPass pass;
    memset(&pass, 0, sizeof pass);
    pass.struct_size = sizeof pass;
    pass.x = psx_mod_read_half(env);
    pass.y = psx_mod_read_half(env + 2u);
    pass.w = psx_mod_read_half(env + 4u);
    pass.h = psx_mod_read_half(env + 6u);
    /* Scope the first proof to the measured gameplay mode; startup is 256 wide. */
    if (pass.w != 512u || pass.h != 240u || pass.x + pass.w > 1024u ||
        pass.y + pass.h > 512u) return;
    if (g_stereo) {
        PSXModStereoFrame frame;
        memset(&frame, 0, sizeof frame);
        frame.struct_size = sizeof frame;
        frame.period_vblanks = MOH_FLIP_PERIOD_VBLANKS;
        frame.x = pass.x; frame.y = pass.y; frame.w = pass.w; frame.h = pass.h;
        if (g_openxr) {
            if (!psx_mod_openxr_begin(pass.w, pass.h, g_units_per_meter)) return;
            if (!psx_mod_openxr_view(0, &g_eye_view[0]) ||
                !psx_mod_openxr_view(1, &g_eye_view[1])) {
                (void)psx_mod_openxr_end(0); return;
            }
            if (g_authored_focal) g_eye_view[0].projection_h_ref = g_eye_view[1].projection_h_ref = 400;
        } else {
            double angle = g_head_yaw * 3.141592653589793 / 180;
            double q[4] = {0, sin(angle*.5), 0, cos(angle*.5)};
            const double oq[4] = {0,0,0,1}, op[3] = {0,0,0};
            const double fov[2][4] = {{-.890127,.645328,.716254,-.910357},
                                        {-.645328,.890127,.716254,-.910357}};
            for (int eye=0;eye<2;eye++) {
                double x = (eye ? 1 : -1) * g_eye_offset / g_units_per_meter;
                double p[3] = {g_head_position[0]+cos(angle)*x,
                               g_head_position[1],g_head_position[2]-sin(angle)*x};
                if (!vr_pose_to_view(q,p,oq,op,fov[eye],g_units_per_meter,pass.w,pass.h,
                                     &g_eye_view[eye])) return;
                g_eye_view[eye].projection = g_desktop_fov;
                if (g_authored_focal) g_eye_view[eye].projection_h_ref = 400;
            }
        }
        g_in_pass = 1;
        int rendered = psx_mod_render_stereo(cpu, &frame, vr_stereo_fn, &pass);
        if (rendered) g_stereo_success++;
        if (g_openxr) (void)psx_mod_openxr_end(rendered);
        g_in_pass = 0;
        g_stereo_eye = 0;
        return;
    }
    uint32_t alpha[1];
    uint32_t n = psx_mod_render_pass_plan(MOH_FLIP_PERIOD_VBLANKS, 1u, alpha, 1u);
    if (!n) return;
    pass.alpha_q16 = alpha[0];
    g_in_pass = 1;
    (void)psx_mod_render_pass(cpu, &pass, vr_pass_fn, &pass);
    g_in_pass = 0;
}

static int vr_controller_source(PSXModControllerState *pad) {
    PSXModOpenXRInput input;
    memset(&input,0,sizeof input);input.struct_size=sizeof input;
    if (!psx_mod_openxr_input(&input)) return 0;
    const uint32_t action_masks[9] = {0x40,0x10,0x20,1,0x80,0x80,0x800,0x200,2};
    for (unsigned i = 0; i < 9; ++i)
        if (psx_mod_read_word(0x800b7790u + i*4) != action_masks[i] ||
            psx_mod_read_word(0x800b77e0u + i*4)) return 0;
    MOHVRAnalogResponse response;
    const unsigned entry[3] = {3, 0, 1};
    const unsigned native_axis[3] = {2, 3, 0};
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t base = 0x800b74b0u + entry[i] * 40;
        /* Other input schemes/inverted axes need their own measured mapping.
         * Keep this source neutral until the default scheme is initialized. */
        if (psx_mod_read_word(base) != native_axis[i] ||
            psx_mod_read_word(base + 36)) return 0;
        response.axis[i].negative_limit = psx_mod_read_byte(base + 14);
        response.axis[i].negative_factor = psx_mod_read_word(base + 20);
        response.axis[i].positive_factor = psx_mod_read_word(base + 24);
        response.axis[i].scale = psx_mod_read_word(base + 32) >> 8;
    }
    for (unsigned i = 0; i < sizeof response.curve; ++i)
        response.curve[i] = psx_mod_read_byte(0x8009e3acu + i);
    return moh_vr_input_map(&input, g_move_deadzone, g_turn_gain, &response, pad);
}
static uint32_t vr_controller_mode(const PSXModControllerInput *input) {
    (void)input;return PSX_MOD_CONTROLLER_ANALOG;
}

static void vr_entity_activate(void) {
    if (g_stereo) (void)psx_mod_set_stereo_presentation(1);
    if (g_openxr) (void)psx_mod_openxr_enable(1);
    if (g_movement) {
        (void)psx_mod_set_controller_mode_override(0,PSX_MOD_CONTROLLER_ANALOG);
        (void)psx_mod_set_controller_presentation_policy(0,vr_controller_mode,
                                                        PSX_MOD_CONTROLLER_ANALOG,1);
        (void)psx_mod_set_controller_source(0,vr_controller_source);
    }
    fprintf(stdout,
            "vr-probe: moh.vr.stereo ACTIVATED (probe=%d target=%08X axis=%d offset=%d)\n",
            g_probe, g_target, g_axis, g_offset);
    /* Render passes need the OpenGL presenter, frame interpolation ON, and the
     * FLIP source (RENDER_PASSES.md "Gates"). HOLD means "repeat the newest game
     * frame" instead of crossfading, which is what a plugin supplying its own
     * pass images wants. Opt-in so the faithful path is untouched. */
    if (g_interp && !g_stereo) {
        int a = psx_mod_set_frame_interpolation_source(PSX_MOD_FRAME_SOURCE_FLIP);
        int b = psx_mod_set_frame_interpolation_blend(PSX_MOD_FRAME_INTERPOLATION_HOLD);
        int c = psx_mod_set_frame_interpolation(g_interp_hz);
        fprintf(stdout,
                "vr-interp: source(FLIP)=%d blend(HOLD)=%d rate(%u)=%d\n",
                a, b, g_interp_hz, c);
        fflush(stdout);
    }
}

PSX_MOD_CONSTRUCTOR(psx_register_moh_vr_stereo_plugin) {
    const char* e;
    if ((e = getenv("PSX_VR_PROBE"))) g_probe = (e[0] && e[0] != '0');
    if ((e = getenv("PSX_VR_TARGET"))) g_target = (uint32_t)strtoul(e, NULL, 0);
    if ((e = getenv("PSX_VR_AXIS"))) g_axis = atoi(e);
    if ((e = getenv("PSX_VR_OFFSET"))) g_offset = (int32_t)strtol(e, NULL, 0);
    if ((e = getenv("PSX_VR_INTERP"))) g_interp = (e[0] && e[0] != '0');
    if ((e = getenv("PSX_VR_PASS_PROBE"))) g_pass_probe = (e[0] && e[0] != '0');
    if ((e = getenv("PSX_VR_PASS_WATCHDOG"))) g_pass_watchdog = atoi(e);
    if ((e = getenv("PSX_VR_PASS_DRAW"))) g_pass_draw = atoi(e);
    if ((e = getenv("PSX_VR_STEREO"))) g_stereo = (e[0] && e[0] != '0');
    if ((e = getenv("PSX_VR_OPENXR"))) g_openxr = atoi(e) != 0;
    if ((e = getenv("PSX_VR_MOVEMENT"))) g_movement = atoi(e) != 0;
    if (g_movement < 0) g_movement = g_openxr;
    if ((e = getenv("PSX_VR_MOVE_DEADZONE"))) g_move_deadzone = strtod(e,NULL);
    if ((e = getenv("PSX_VR_TURN_GAIN"))) g_turn_gain = strtod(e,NULL);
    if (!isfinite(g_move_deadzone) || g_move_deadzone < 0 || g_move_deadzone > .9) g_move_deadzone = .2;
    if (!isfinite(g_turn_gain) || g_turn_gain < 0 || g_turn_gain > 2) g_turn_gain = .65;
    if ((e = getenv("PSX_VR_HEAD_YAW"))) g_head_yaw = strtod(e, NULL);
    if (!isfinite(g_head_yaw) || fabs(g_head_yaw)>180) g_head_yaw=0;
    if ((e = getenv("PSX_VR_HEAD_POSITION"))) {
        if (sscanf(e,"%lf,%lf,%lf", &g_head_position[0], &g_head_position[1],
                   &g_head_position[2]) != 3) memset(g_head_position,0,sizeof g_head_position);
    }
    if ((e = getenv("PSX_VR_AUTHORED_FOCAL"))) g_authored_focal = atoi(e) != 0;
    if ((e = getenv("PSX_VR_DESKTOP_FOV"))) g_desktop_fov = atoi(e) != 0;
    if (g_openxr) g_stereo=1;
    if ((e = getenv("PSX_VR_IPD_MM"))) g_ipd_mm = strtod(e, NULL);
    if ((e = getenv("PSX_VR_UNITS_PER_METER"))) g_units_per_meter = strtod(e, NULL);
    if (!isfinite(g_ipd_mm) || g_ipd_mm < 0 || g_ipd_mm > 100) g_ipd_mm = 67;
    if (!isfinite(g_units_per_meter) || g_units_per_meter < 1 ||
        g_units_per_meter > 65536) g_units_per_meter = 48.0 / .067;
    if ((e = getenv("PSX_VR_WORLD_SCALE"))) g_world_scale = strtod(e, NULL);
    if (!isfinite(g_world_scale) || g_world_scale < .05 || g_world_scale > 20) g_world_scale = 1;
    g_units_per_meter /= g_world_scale; /* smaller apparent world => more units/meter */
    if (g_units_per_meter < 1 || g_units_per_meter > 65536) {
        g_units_per_meter = 48.0 / .067; g_world_scale = 1;
    }
    g_eye_offset = (int32_t)lround(g_ipd_mm * .0005 * g_units_per_meter);
    if ((e = getenv("PSX_VR_EYE_OFFSET"))) g_eye_offset = (int32_t)strtol(e, NULL, 0);
    if ((e = getenv("PSX_VR_DRAW_MASK"))) g_draw_mask = (unsigned)strtoul(e, NULL, 0) & 15u;
    if ((e = getenv("PSX_VR_HUD_ICON"))) g_icon_visible = atoi(e) != 0;
    if ((e = getenv("PSX_VR_TEXT"))) g_text_visible = atoi(e) != 0;
    if ((e = getenv("PSX_VR_STEREO_FAULT"))) g_stereo_fault = atoi(e);
    if ((e = getenv("PSX_VR_STEREO_FAULT_HOLD"))) g_stereo_fault_hold = atoi(e) != 0;
    if (g_stereo_fault < 0 || g_stereo_fault > 2) g_stereo_fault = 0;
    if (g_eye_offset < 0 || g_eye_offset > 4096) g_eye_offset = 24;
    if (g_stereo) {
        g_pass_draw = 1;
        g_offset = 0; /* legacy entity/TRX patches cannot leak into eye pairs */
        g_pass_watchdog = 0;
    }
    if (g_pass_watchdog < 0 || g_pass_watchdog > 2) g_pass_watchdog = 0;
    if (g_pass_draw < 0 || g_pass_draw > 3) g_pass_draw = 0;
    if ((e = getenv("PSX_VR_INTERP_HZ"))) g_interp_hz = (uint32_t)strtoul(e, NULL, 0);
    if ((e = getenv("PSX_VR_RECT"))) {
        int rx, ry, rw, rh;
        if (sscanf(e, "%d,%d,%d,%d", &rx, &ry, &rw, &rh) == 4) {
            g_rect_x = rx; g_rect_y = ry; g_rect_w = rw; g_rect_h = rh;
        }
    }
    if ((e = getenv("PSX_VR_RECT_ALT"))) g_rect_alt = (e[0] && e[0] != '0');
    if (g_axis < 0 || g_axis > 2) g_axis = 0;

    fprintf(stdout, "vr-probe: registering moh.vr.stereo for %08X\n",
            FUN_80084718);
    fflush(stdout);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo", FUN_80084718,
                                                 vr_entity_entry);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo", GEO_800814C4,
                                                 vr_geo_entry);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo", GEO_80080DD4,
                                                 vr_geo_entry);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",
                                                 RENDER_800824D0,
                                                 vr_render_entry);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",
                                                 LVTX_8008B3E8,
                                                 vr_lvtx_entry);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",
                                                 VSYNC_80016E38,
                                                 vr_vsync_entry);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",
                                                 RENDER_WAIT_80090B80,
                                                 vr_wait_entry);
    (void)psx_mod_register_activation_plugin("moh.vr.stereo",
                                             vr_entity_activate);
}
