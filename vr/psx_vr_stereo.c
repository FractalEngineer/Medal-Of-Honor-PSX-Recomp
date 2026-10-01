/*
 * Medal of Honor VR — entity-transform probe / stereo injection point.
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
 *
 * The offset is applied as patch-then-restore: the entity's original value is
 * written back at the start of its NEXT call before re-patching, so the stored
 * position never drifts between frames. The body still reads the patched value.
 */
#include "mod_plugins.h"
#include "cpu_state.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FUN_80084718 0x80084718u
/* Geometry pass: entity -> screen geometry. FUN_80082948 dispatches here on
 * entity+0x6c == 0x80 (FUN_80080dd4) else FUN_800814c4. */
#define GEO_800814C4 0x800814c4u
#define GEO_80080DD4 0x80080dd4u
/* Per-model render entry: called from the scene drivers FUN_8006d2e4 /
 * FUN_8006d460 and dispatches through FUN_80082948. Hooking here enumerates
 * every entity rendered in a frame, top-down. */
#define RENDER_800824D0 0x800824d0u

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

static void vr_entity_activate(void) {
    fprintf(stdout,
            "vr-probe: moh.vr.stereo ACTIVATED (probe=%d target=%08X axis=%d offset=%d)\n",
            g_probe, g_target, g_axis, g_offset);
    fflush(stdout);
}

PSX_MOD_CONSTRUCTOR(psx_register_moh_vr_stereo_plugin) {
    const char* e;
    if ((e = getenv("PSX_VR_PROBE"))) g_probe = (e[0] && e[0] != '0');
    if ((e = getenv("PSX_VR_TARGET"))) g_target = (uint32_t)strtoul(e, NULL, 0);
    if ((e = getenv("PSX_VR_AXIS"))) g_axis = atoi(e);
    if ((e = getenv("PSX_VR_OFFSET"))) g_offset = (int32_t)strtol(e, NULL, 0);
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
    (void)psx_mod_register_activation_plugin("moh.vr.stereo",
                                             vr_entity_activate);
}
