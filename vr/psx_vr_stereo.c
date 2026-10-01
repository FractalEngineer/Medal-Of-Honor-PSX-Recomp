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
 * Two jobs, in order:
 *   1. Probe — enumerate the entity table and its positions. This is the
 *      observability the M1 camera-relative question was blocked on: the fn
 *      ring does not instrument 0x80084718, so it has to come from here.
 *   2. Inject — a per-eye offset on the camera entity, for stereo. Disabled
 *      until the entity table tells us which entity is the camera.
 *
 * Set PSX_VR_PROBE=1 to log the table. Nothing else runs without it.
 */
#include "mod_plugins.h"
#include "cpu_state.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define FUN_80084718 0x80084718u

#define VR_ENTITY_SLOTS 256
#define VR_PROBE_PERIOD 20u /* calls between position samples */

typedef struct {
    uint32_t entity;
    int16_t x, y, z;
    uint32_t hits;
} VrEntitySlot;

static VrEntitySlot g_slots[VR_ENTITY_SLOTS];
static uint32_t g_calls;
static int g_probe; /* PSX_VR_PROBE */

static void vr_entity_entry(CPUState* cpu, uint32_t address) {
    uint32_t entity = cpu->gpr[4]; /* $a0 = param_1 */
    (void)address;

    if (!g_calls) {
        fprintf(stdout, "vr-probe: hook FIRED at %08X a0=%08X\n", address, entity);
        fflush(stdout);
    }

    if (!entity) return;

    /* Only guest RAM. $a0 is a pointer, so a stale/nonsense value must not be
     * dereferenced blindly. */
    if (entity < 0x80000000u || entity >= 0x80200000u) return;

    int16_t x = (int16_t)psx_mod_read_half(entity + 0x98);
    int16_t y = (int16_t)psx_mod_read_half(entity + 0x9a);
    int16_t z = (int16_t)psx_mod_read_half(entity + 0x9c);

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

static void vr_entity_activate(void) {
    fprintf(stdout, "vr-probe: moh.vr.stereo ACTIVATED (probe=%d)\n", g_probe);
    fflush(stdout);
}

PSX_MOD_CONSTRUCTOR(psx_register_moh_vr_stereo_plugin) {
    const char* env = getenv("PSX_VR_PROBE");
    g_probe = env && env[0] && env[0] != '0';
    fprintf(stdout, "vr-probe: registering moh.vr.stereo for %08X\n",
            FUN_80084718);
    fflush(stdout);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo", FUN_80084718,
                                                 vr_entity_entry);
    (void)psx_mod_register_activation_plugin("moh.vr.stereo",
                                             vr_entity_activate);
}
