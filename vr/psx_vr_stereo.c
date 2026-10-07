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
 *   PSX_VR_WEAPON_AIM=1 experimental player weapon shot override
 *   PSX_VR_WEAPON_POSE=1 tracked weapon meshes, render-only
 *   PSX_VR_WEAPON_MP_CONTROL=1 opt-in player-one multiplayer weapon test view
 *   PSX_VR_WEAPON_MODEL_UNITS_PER_METER=N / PSX_VR_WEAPON_PIVOT=x,y,z
 *                        provisional rifle scale/pivot (defaults 850 / 80,150,100)
 *   PSX_VR_LASER=0|1     barrel laser, held on the right grip (default: on with
 *                        PSX_VR_WEAPON_POSE; firearms only, not the passport)
 *   PSX_VR_LASER_RANGE_M=N / PSX_VR_LASER_ORIGIN_M=N beam length / start offset
 *   PSX_VR_LASER_AXIS=0|1|2 / PSX_VR_LASER_SIGN=+1|-1 model barrel direction
 *   PSX_VR_LASER_MUZZLE=x,y,z model-space muzzle offset from the pivot
 *   PSX_VR_LASER_RAY=0|1 raycast the level tree so the beam ends on the first
 *                        surface (default 1, surface-exact triangles; =0 uses
 *                        the sector boxes); PSX_VR_LASER_DOT=N dot half-size px
 *   PSX_VR_LASER_BRIGHT=N beam/dot red 1..255 (default 160; lower = fainter)
 *   PSX_VR_MOVEMENT=0|1  Quest movement source (default on for XR, off otherwise)
 *   PSX_VR_MOVE_DEADZONE=N radial move/scalar turn deadzone (default .2)
 *   PSX_VR_TURN_GAIN=N   decoded turn response gain (default .78; native game rate +20%)
 *   PSX_VR_ONE_STICK=0|1|2 both axes of the native left stick from one hand:
 *                        0 off, 2 always, 1 only while a carried machine gun is
 *                        equipped. Holding the LEFT GRIP always merges them too
 *                        (aim the mounted gun with one hand); the right stick's
 *                        Y is otherwise inert.
 *   PSX_VR_AUTHORED_FOCAL=0 disable guest H/400 ratio for XR projection control
 *   PSX_VR_TEXT=0 / PSX_VR_HUD_ICON=0 omit measured text/icon draw calls
 *   PSX_VR_HEAD_YAW=degrees / PSX_VR_HEAD_POSITION=x,y,z synthetic desktop pose
 *   PSX_VR_DESKTOP_FOV=1 deterministic Quest-FOV projection control
 *   PSX_VR_STEREO_FAULT=1 decline one right eye after a successful pair
 *                     =2 watchdog inside right-eye level dispatch
 *                     =3 watchdog inside wrapped tracked weapon draw
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
#include "moh_vr_aim.h"
#include "moh_vr_weapons.h"
#include "moh_vr_frustum.h"
#include "moh_vr_raycast.h"

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
static void vr_laser_draw(void);
static void vr_m3_mul(const double m[9],const double v[3],double out[3]);
static int g_one_stick; /* PSX_VR_ONE_STICK: 0 off, 1 auto on machine guns, 2 always */
static int vr_machine_gun(void);

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
static int g_in_pass; /* Native camera snapshots never update during replay. */
static int g_mp_control, g_scene_mp;
static int vr_mp_active(void) {
    return g_mp_control && psx_mod_read_word(0x8008ce50u)==0x27bdffe0u &&
           psx_mod_read_word(0x80045908u)==0x0c019647u;
}
static uint32_t vr_weapon_player(void) {
    return psx_mod_read_word(vr_mp_active()?0x8009943cu:0x8009d654u);
}
static int g_weapon_aim; /* Experimental shot override, explicitly opt-in. */
static int g_weapon_pose, g_weapon_outer, g_weapon_inner;
static double g_weapon_model_units_per_meter=850;
static double g_weapon_projection_scale=16; /* Precision units, never physical size. */
static double g_weapon_pivot[3]={80,150,100}; /* Provisional native grip pivot. */
static double g_weapon_draw_rotation[9],g_weapon_draw_translation[3];
static uint32_t g_weapon_render_entity;
/* Barrel laser: fixed-distance Gouraud beam drawn from the tracked weapon,
 * held on the right grip, for firearms only (passport and other shot_id 0
 * weapons are excluded). Placement reuses the weapon's own camera affine so
 * the beam matches the rendered mesh, then projects with the same per-eye
 * focal/centre the GTE used. Tunable so a headset pass can align the muzzle. */
static int g_laser;
static double g_laser_range_m=150, g_laser_origin_m=.35;
static int g_laser_bright=160;           /* PSX_VR_LASER_BRIGHT: beam red 1..255 */
static double g_laser_muzzle[3]={0,0,0}; /* model-space offset from the pivot */
static int g_laser_axis=2;               /* model axis the barrel points along */
static double g_laser_sign=1;            /* +1/-1 flips that axis */
static int g_laser_valid;                /* placement captured for this eye */
static int g_laser_grip;                 /* right grip held, sampled off-replay */
static double g_laser_M[9],g_laser_C[3],g_laser_scale;
static int32_t g_laser_ofx,g_laser_ofy,g_laser_fx,g_laser_fy,g_laser_cx,g_laser_cy;
static uint32_t g_laser_h,g_laser_href;
static int g_laser_ray=1;                /* PSX_VR_LASER_RAY: cast the level tree */
static int g_laser_tris=1;               /* PSX_VR_LASER_TRI: surface-exact triangles */
static double g_laser_dot=2;             /* PSX_VR_LASER_DOT: dot half-size (px) */
static uint32_t g_level_root;            /* level visibility tree root (selector $a0) */
static int g_level_root_frame;           /* root captured for this frame */
static double g_camera_inverse[9], g_camera_tr[3], g_camera_rt[9];
static uint64_t g_camera_cycle;
static uint32_t g_camera_owner;
static int g_camera_valid;
static uint32_t g_lvtx_calls;
static int g_lvtx_logged;
static void vr_cache_camera(CPUState *cpu,uint32_t owner) {
    double matrix[9];
    for(int i=0;i<9;i++)matrix[i]=(int16_t)(cpu->gte_ctrl[i/2]>>((i%2)*16))/4096.0;
    memcpy(g_camera_rt,matrix,sizeof matrix);
    g_camera_valid=moh_vr_matrix_inverse(matrix,g_camera_inverse);
    for(int i=0;i<3;i++)g_camera_tr[i]=(int32_t)cpu->gte_ctrl[5+i];
    g_camera_cycle=psx_cycle_count;g_camera_owner=owner;
}
static void vr_mp_level_entry(CPUState *cpu,uint32_t address) {
    (void)address;
    vr_nested_watchdog(cpu);
    if(g_weapon_aim && !g_in_pass && vr_mp_active() &&
       psx_mod_read_word(0x8009942cu)==vr_weapon_player())
        vr_cache_camera(cpu,vr_weapon_player());
}

static void vr_lvtx_entry(CPUState* cpu, uint32_t address) {
    (void)address;
    vr_nested_watchdog(cpu);
    g_lvtx_calls++;
    /* The level visibility tree root arrives in $a0. Keep the first valid one per
     * frame (the outermost selector call) for the laser raycast. */
    if (!g_level_root_frame) {
        uint32_t node=cpu->gpr[4];
        if (node && !(node&3)) {
            int16_t b[6];int ok=1;
            for(int i=0;i<6;i++)b[i]=(int16_t)psx_mod_read_half(node+(uint32_t)i*2);
            for(int i=0;i<3;i++)if(b[i]>b[i+3])ok=0;
            if(ok) {g_level_root=node;g_level_root_frame=1;}
        }
    }
    if (g_weapon_aim && !g_in_pass) {
        vr_cache_camera(cpu,psx_mod_read_word(0x8009d654u));
    }
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
static double g_move_deadzone = .2, g_turn_gain = .78;
static int g_authored_focal = 1, g_desktop_fov;
static double g_head_yaw, g_head_position[3];
static PSXModRenderView g_eye_view[2];
static uint32_t g_stereo_eye;
static int g_head_frustum;

/* Measured SLUS-00974 tree: bbox shorts at +0/+6, leaf count at +12,
 * PVS masks at +16/+20, children +24/+32/+28; native output queue has
 * 1000 words before 800AB140. Preflight everything before writing guest RAM.
 * The sandbox owns rollback. This opt-in candidate never changes native play. */
static int vr_head_frustum_filter(CPUState *cpu,uint32_t address) {
    vr_lvtx_entry(cpu,address);
    if(!g_head_frustum||!g_in_pass||address!=LVTX_8008B3E8||g_stereo_eye>1||
       psx_mod_read_word(address)!=0x27bdfee0u||
       psx_mod_read_word(address+4)!=0x3c02800au||
       psx_mod_read_word(address+8)!=0x3c03800au||
       psx_mod_read_word(address+12)!=0x8c42d638u) return 0;
    uint32_t queue[1000];unsigned count=0;
    double matrix[9],tr[3];
    for(int i=0;i<9;i++)matrix[i]=(int16_t)(cpu->gte_ctrl[i/2]>>((i%2)*16))/4096.0;
    for(int i=0;i<3;i++)tr[i]=(int32_t)cpu->gte_ctrl[5+i];
    double cx=(int32_t)cpu->gte_ctrl[24]/65536.0,cy=(int32_t)cpu->gte_ctrl[25]/65536.0;
    uint32_t masks[2]={psx_mod_read_word(0x800aa178u),psx_mod_read_word(0x800aa17cu)};
    int bypass_masks=psx_mod_read_word(0x800aa180u)!=0;
    if(!moh_vr_collect_tree(cpu->gpr[4],queue,&count,matrix,tr,&g_eye_view[g_stereo_eye],
                            cx,cy,masks,bypass_masks))return 0;
    for(unsigned i=0;i<count;i++)psx_mod_write_word(0x800aa1a0u+i*4,queue[i]);
    psx_mod_write_word(0x800aa19cu,count);psx_mod_write_word(0x800ab140u,0);
    cpu->gte_ctrl[26]=(uint16_t)psx_mod_read_word(0x8009a628u);
    return 1;
}
static uint32_t g_stereo_success;
static int g_menu_surface=1,g_menu_flat;
static double g_menu_distance=2,g_menu_width=2;
static int g_native_surface=-1;
static uint32_t g_scene_age=4;
static void vr_native_surface_mode(int enabled) {
    if(!g_stereo || g_native_surface==enabled)return;
    if(g_openxr && !psx_mod_openxr_native_surface(enabled?g_menu_distance:0,
                                     enabled?g_menu_width:0,enabled?g_units_per_meter:0))return;
    g_native_surface=enabled;
    (void)psx_mod_set_stereo_presentation(enabled?0:1);
}
static void vr_surface_vblank(void) {
    if(!g_stereo || g_in_pass)return;
    /* Explicit render-activity policy: allow two missed 30Hz draw intervals
     * before returning to native UI/video presentation. A claimed draw keeps
     * failures on the scene's empty-layer path, never a flat gameplay fallback. */
    if(g_scene_age<4)g_scene_age++;
    if(g_scene_age>=4)vr_native_surface_mode(1);
}
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

/* Native 5134C's first view, excluding its waits/flip and second-player draw.
 * Camera selection, scene callbacks and OT submission remain native. The test
 * view sets the clip and centre within the eye rollback transaction. */
static int vr_draw_mp_control(CPUState *cpu,const PSXModRenderPass *pass) {
    const uint32_t ctx=0x8009696cu,ra=cpu->gpr[31];
    uint32_t index=psx_mod_read_word(0x80098b70u),camera=psx_mod_read_word(0x80099418u);
    uint32_t player=vr_weapon_player(),list=psx_mod_read_word(0x80099434u);
    if(!vr_mp_active() || index>1 || camera<0x80000000u || camera>=0x801ff000u ||
       player<0x80000000u || player>=0x801ff000u || psx_mod_read_word(player+8u)!=1u ||
       list<0x80000000u || list>=0x801ff000u)return 0;
    uint32_t env=ctx+16u+184u*index;
    psx_mod_write_half(env+2u,pass->y);psx_mod_write_half(env+6u,pass->h);
    psx_mod_write_half(env+10u,pass->y);
    psx_mod_write_word(ctx+4u,pass->h);
    vr_guest_call(cpu,0x80017348u,env,0,0,ra);
    psx_mod_write_word(0x1f801810u,0x02000000u);
    psx_mod_write_word(0x1f801810u,((uint32_t)pass->y<<16)|pass->x);
    psx_mod_write_word(0x1f801810u,((uint32_t)pass->h<<16)|pass->w);
    vr_guest_call(cpu,0x800515f0u,camera,0,0,ra);
    if(psx_mod_read_word(0x8009942cu)!=player)return 0;
    cpu->gte_ctrl[24]=(pass->w/2u)<<16;cpu->gte_ctrl[25]=(pass->h/2u)<<16;
    vr_guest_call(cpu,0x800677fcu,ctx,list,0,ra);
    vr_guest_call(cpu,0x800172d8u,psx_mod_read_word(ctx+8728u)+511u*4u,0,0,ra);
    return 1;
}

static int vr_draw_scene(CPUState* cpu, const PSXModRenderPass* pass) {
    if(g_scene_mp)return vr_draw_mp_control(cpu,pass);
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
    g_weapon_outer=g_weapon_inner=0;
    g_laser_valid=0;
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
    if (!user) return 0;
    int rendered = vr_draw_scene(cpu, user);
    if (rendered) vr_laser_draw();
    return rendered;
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
    if ((!g_pass_probe && !g_stereo) || g_in_pass) return;
    int mp=address==0x8008ce50u;
    if(mp && !vr_mp_active())return;
    g_scene_mp=mp;
    g_level_root_frame=0;
    /* Actions are only fresh at the offline input boundary, never inside an eye
     * replay, so cache the right grip here for the laser. */
    if (g_laser) {
        PSXModOpenXRInput li;memset(&li,0,sizeof li);li.struct_size=sizeof li;
        g_laser_grip = psx_mod_openxr_input(&li) && li.squeeze_active[1] &&
                       li.squeeze[1]>=.55f && li.squeeze[1]<=1;
    }
    if (g_stereo && g_stereo_fault_done && g_stereo_fault_hold) return;
    uint32_t index = psx_mod_read_word(mp?0x80098b70u:0x8009c824u);
    if (index > 1u) return;
    uint32_t env = (mp?0x80096aecu:0x8009a7a0u) + (index == 0u ? 20u : 0u);
    PSXModRenderPass pass;
    memset(&pass, 0, sizeof pass);
    pass.struct_size = sizeof pass;
    pass.x = psx_mod_read_half(env);
    pass.y = psx_mod_read_half(env + 2u);
    pass.w = psx_mod_read_half(env + 4u);
    pass.h = psx_mod_read_half(env + 6u);
    /* Scope the first proof to the measured gameplay mode; startup is 256 wide. */
    if (pass.w != 512u || pass.h != (mp?120u:240u) || pass.x + pass.w > 1024u ||
        pass.y + pass.h > 512u) return;
    if (g_stereo) {
        uint32_t world=psx_mod_read_word(mp?0x80099434u:0x8009cc64u),camera=vr_weapon_player();
        if(world<0x80000000u || world>=0x80200000u || camera<0x80000000u || camera>=0x80200000u)return;
        g_scene_age=0;vr_native_surface_mode(0);
        PSXModStereoFrame frame;
        /* Native pause flag: measured 0->1 writer 80062760 after Start. */
        g_menu_flat=!mp && g_menu_surface && psx_mod_read_word(0x8009a61cu)==1u;
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
        if(g_menu_flat && g_openxr &&
           !psx_mod_openxr_quad(g_menu_distance,g_menu_width,g_menu_width*.75))g_menu_flat=0;
        if(g_menu_flat)for(int eye=0;eye<2;eye++) {
            memset(&g_eye_view[eye],0,sizeof g_eye_view[eye]);
            g_eye_view[eye].struct_size=sizeof g_eye_view[eye];
            g_eye_view[eye].rotation_q12[0]=g_eye_view[eye].rotation_q12[4]=g_eye_view[eye].rotation_q12[8]=4096;
        }
        g_in_pass = 1;
        int rendered = psx_mod_render_stereo(cpu, &frame, vr_stereo_fn, &pass);
        if (rendered) g_stereo_success++;
        if (g_openxr) (void)psx_mod_openxr_end(rendered);
        g_in_pass = 0;
        g_weapon_outer=g_weapon_inner=0; /* also after nested watchdog rollback */
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

/* A machine gun (Thompson/BAR/MP40) aims with the native left stick, and our
 * mapping splits that stick's two axes across the two hands. See moh_vr_input_map. */
static int vr_machine_gun(void) {
    uint32_t player=vr_weapon_player();
    if(player<0x80000000u||player>=0x801ff000u)return 0;
    uint32_t input=psx_mod_read_word(player+904u);
    if(input<0x80000000u||input>=0x801ff000u)return 0;
    switch(psx_mod_read_byte(input+85u)) {
    case 1: case 6: case 11: return 1; /* Thompson, BAR, MP40 */
    default: return 0;
    }
}

static int vr_controller_source(PSXModControllerState *pad) {
    PSXModOpenXRInput input;
    memset(&input,0,sizeof input);input.struct_size=sizeof input;
    if (!psx_mod_openxr_input(&input)) return 0;
    if(g_native_surface==1 || g_menu_flat)
        return moh_vr_menu_input_map(&input,g_move_deadzone,pad);
    int mp=vr_mp_active();
    const uint32_t action_masks[9] = {0x40,0x10,0x20,1,0x80,0x80,0x800,0x200,2};
    for (unsigned i = 0; i < 9; ++i)
        if (psx_mod_read_word((mp?0x800b6028u:0x800b7790u) + i*4) != action_masks[i] ||
            psx_mod_read_word((mp?0x800b6074u:0x800b77e0u) + i*4)) return 0;
    MOHVRAnalogResponse response;
    const unsigned entry[3] = {3, 0, 1};
    const unsigned native_axis[3] = {2, 3, 0};
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t base = (mp?0x800b5d48u:0x800b74b0u) + entry[i] * 40;
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
        response.curve[i] = psx_mod_read_byte((mp?0x8009a19cu:0x8009e3acu) + i);
    /* One-stick merge. On a mounted gun the game aims with the native left
     * stick, whose two axes this mapping splits across the hands. Hold the
     * left grip to aim with one hand - a deliberate action, so the right
     * stick's Y stays inert in ordinary play. */
    int merged=g_one_stick==2 || (g_one_stick==1 && vr_machine_gun()) ||
               (input.squeeze_active[0] && isfinite(input.squeeze[0]) &&
                input.squeeze[0] >= .55f);
    return moh_vr_input_map(&input, g_move_deadzone, g_turn_gain, &response, merged, pad);
}
/* Constructor calls the native basis builder after auto-aim/pose selection.
 * Restrict this to measured player-owned weapon/actor pairs, never NPC shots/replay.
 * With invalid or stale tracking the native shot pose is retained. */
static void vr_shot_basis_entry(CPUState *cpu,uint32_t address) {
    int mp=address==0x8006591cu;
    if(mp && !vr_mp_active())return;
    if(!g_weapon_aim || g_in_pass || !g_camera_valid ||
       cpu->gpr[31]!=(mp?0x80045910u:0x80045524u) || psx_cycle_count<g_camera_cycle ||
       psx_cycle_count-g_camera_cycle>2257920u)return; /* four NTSC VBlanks */
    uint32_t shot=cpu->gpr[4],owner=cpu->gpr[18];
    if(shot<0x80000000u || shot>=0x801ff000u || shot!=cpu->gpr[16] ||
       owner!=g_camera_owner || owner!=vr_weapon_player() ||
       owner<0x80000000u || owner>=0x801ff000u ||
       psx_mod_read_word(owner+8u)!=1u ||
       psx_mod_read_word(mp?0x80045908u:0x8004551cu)!=(mp?0x0c019647u:0x0c01a9dbu) ||
       psx_mod_read_word(mp?0x8004590cu:0x80045520u)!=0xae200044u ||
       psx_mod_read_word(address)!=0x27bdffb8u)return;
    uint32_t input=psx_mod_read_word(owner+904u);
    if(input<0x80000000u || input>0x801fffaau)return;
    unsigned shot_id=moh_vr_weapon_shot_id(psx_mod_read_byte(input+85u));
    if(!shot_id || psx_mod_read_word(shot+8u)!=shot_id)return;
    PSXModOpenXRHands hands={0};hands.struct_size=sizeof hands;
    int32_t position[3],angles[2];
    if(!psx_mod_openxr_hands(&hands) ||
       !moh_vr_aim_shot(&hands,g_camera_inverse,g_camera_tr,g_units_per_meter,
                        position,angles))return;
    for(int i=0;i<3;i++)psx_mod_write_word(shot+524u+i*4u,(uint32_t)position[i]);
    psx_mod_write_word(shot+552u,(uint32_t)angles[0]);
    psx_mod_write_word(shot+556u,(uint32_t)angles[1]);
    psx_mod_write_word(shot+560u,0);
    /* Native 8006A76C builds basis, and constructor copies position history.
     * Speed, damage, ammo, owner, collision and the transition countdown stay native. */
}

/* Capture this eye's weapon camera affine (model -> camera), so the laser can
 * be placed exactly like the rendered mesh. The model point E maps to the
 * camera as C + M * scale * (E - pivot), matching MAC = Rpose*(Rguest*V) +
 * Tpose with the plugin's V = b + A*E. Reused from vr_weapon_geometry_filter. */
static void vr_laser_capture(CPUState* cpu,const PSXModRenderView* view,
                             const double r[9],const double t[3],double scale) {
    double R[9],M[9],C[3];
    for(int i=0;i<9;i++)R[i]=view->rotation_q12[i]/4096.0;
    for(int i=0;i<3;i++) {
        C[i]=view->translation[i];
        for(int j=0;j<3;j++) {
            M[i*3+j]=R[i*3]*r[j]+R[i*3+1]*r[3+j]+R[i*3+2]*r[6+j];
            C[i]+=R[i*3+j]*t[j];
        }
    }
    memcpy(g_laser_M,M,sizeof M);
    memcpy(g_laser_C,C,sizeof C);
    g_laser_scale=scale;
    g_laser_ofx=(int32_t)cpu->gte_ctrl[24];g_laser_ofy=(int32_t)cpu->gte_ctrl[25];
    g_laser_fx=view->fx_q16;g_laser_fy=view->fy_q16;
    g_laser_cx=view->cx_delta_q16;g_laser_cy=view->cy_delta_q16;
    g_laser_h=(uint32_t)cpu->gte_ctrl[26]&0xffffu;g_laser_href=view->projection_h_ref;
    g_laser_valid=1;
}

/* True per-eye perspective: sx = (OFX + cx_delta)/65536 + fx * X/Z, with fx the
 * render view's focal length. The GTE's IR/SZ clamp would flatten a distant beam
 * end into a fixed point, so the laser projects without it. Screen is native
 * 512x240; the GP0 line coordinate range is 11-bit signed. */
static int vr_laser_project(const double P[3],double* sx,double* sy) {
    if(!isfinite(P[0])||!isfinite(P[1])||!isfinite(P[2])||P[2]<=1.0)return 0;
    double x=(double)g_laser_ofx+(double)g_laser_cx+(double)g_laser_fx*P[0]/P[2];
    double y=(double)g_laser_ofy+(double)g_laser_cy+(double)g_laser_fy*P[1]/P[2];
    if(g_laser_href) {
        if(!g_laser_h)return 0;
        x*=(double)g_laser_h/(double)g_laser_href;
        y*=(double)g_laser_h/(double)g_laser_href;
    }
    *sx=x/65536.0;*sy=y/65536.0;return 1;
}

/* Liang-Barsky clip of the segment to the GP0 11-bit coordinate box.
 * Returns 1 when a visible piece remains, -1 when fully outside. */
static int vr_laser_clip(double* x0,double* y0,double* x1,double* y1) {
    const double lo=-1023.0,hi=1023.0;
    double dx=*x1-*x0,dy=*y1-*y0,t0=0.0,t1=1.0;
    const double p[4]={-dx,dx,-dy,dy};
    const double q[4]={*x0-lo,hi-*x0,*y0-lo,hi-*y0};
    for(int i=0;i<4;i++) {
        if(p[i]==0) { if(q[i]<0)return -1; continue; }
        double t=q[i]/p[i];
        if(p[i]<0) { if(t>t1)return -1; if(t>t0)t0=t; }
        else { if(t<t0)return -1; if(t<t1)t1=t; }
    }
    if(t1<=t0)return -1;
    double ax=*x0+dx*t0,ay=*y0+dy*t0,bx=*x0+dx*t1,by=*y0+dy*t1;
    *x0=ax;*y0=ay;*x1=bx;*y1=by;return 1;
}

static void vr_m3_mul(const double m[9],const double v[3],double out[3]) {
    for(int i=0;i<3;i++)out[i]=m[i*3]*v[0]+m[i*3+1]*v[1]+m[i*3+2]*v[2];
}

/* One solid line per eye from the muzzle to the first level surface, plus a
 * dot at the impact. Held on the right grip; firearms only. The endpoint comes
 * from moh_vr_ray_tree_tris (surface-exact level triangles) or from the sector
 * boxes when PSX_VR_LASER_TRI=0; with no hit the beam keeps its fixed range. */
static void vr_laser_draw(void) {
    if(!g_laser) return;
    if(!g_laser_valid) return;
    if(!g_laser_grip) return;
    uint32_t player=vr_weapon_player();
    if(player<0x80000000u || player>=0x801ff000u) return;
    uint32_t input=psx_mod_read_word(player+904u);
    if(input<0x80000000u || input>=0x801ff000u) return;
    if(!moh_vr_weapon_shot_id(psx_mod_read_byte(input+85u))) return; /* no passport */
    /* Barrel direction = the mesh's own rotation (M = Rpose * aim) applied to a
     * model axis, so the beam follows the rendered weapon exactly. */
    double d[3]={0,0,0};d[g_laser_axis]=g_laser_sign;
    double dir[3];
    for(int i=0;i<3;i++)
        dir[i]=g_laser_M[i*3]*d[0]+g_laser_M[i*3+1]*d[1]+g_laser_M[i*3+2]*d[2];
    double o[3],p1[3];
    for(int i=0;i<3;i++) {
        o[i]=g_laser_C[i];
        for(int j=0;j<3;j++)o[i]+=g_laser_M[i*3+j]*(g_laser_scale*g_laser_muzzle[j]);
    }
    double unit=g_units_per_meter*g_weapon_projection_scale;
    for(int i=0;i<3;i++)o[i]+=dir[i]*(g_laser_origin_m*unit);
    int have_dot=0;
    for(int i=0;i<3;i++)p1[i]=o[i]+dir[i]*(g_laser_range_m*unit);
    /* Raycast the level tree: cast in the engine's camera-relative world space
     * (render-pose camera -> guest camera -> world via the cached camera) and
     * bring the hit back the other way. RT/TR is the world->camera mapping. */
    if (g_laser_ray && g_level_root && g_camera_valid) {
        const PSXModRenderView* vv=&g_eye_view[g_stereo_eye];
        double R[9],dt[3],gc[3],gd[3],wo[3],wd[3],thit=0;
        for(int i=0;i<9;i++)R[i]=vv->rotation_q12[i]/4096.0;
        /* The muzzle o and its projection live in the weapon's scoped view, whose
         * translation is scaled by g_weapon_projection_scale; undo that before
         * mapping into the level's world space. */
        for(int i=0;i<3;i++)dt[i]=o[i]/g_weapon_projection_scale-vv->translation[i];
        for(int i=0;i<3;i++) {                 /* R^T, R is orthonormal */
            double c=0,g=0;
            for(int j=0;j<3;j++){c+=R[j*3+i]*dt[j];g+=R[j*3+i]*dir[j];}
            gc[i]=c;gd[i]=g;
        }
        double ct[3];for(int i=0;i<3;i++)ct[i]=gc[i]-g_camera_tr[i];
        vr_m3_mul(g_camera_inverse,ct,wo);
        vr_m3_mul(g_camera_inverse,gd,wd);
        double len=sqrt(wd[0]*wd[0]+wd[1]*wd[1]+wd[2]*wd[2]);
        int hit=0;
        if(len>1e-6) {
            for(int i=0;i<3;i++)wd[i]/=len;
            double tmax=g_laser_range_m*g_units_per_meter;
            if(g_laser_tris) {
                /* Surface-exact: cast every level struct's own tree + vertex
                 * table (array at 0x80098d8c, stride 0xb0, count 0x80098984). */
                uint32_t lc=psx_mod_read_word(0x80098984u);
                if(lc>0u && lc<=8u) for(uint32_t s=0u;s<lc;s++) {
                    uint32_t st=0x80098d8cu+s*0xb0u;
                    uint32_t sub=psx_mod_read_word(st+8u);
                    if(sub<0x80000000u||sub>=0x801ff000u)continue;
                    double tth;
                    if(moh_vr_ray_tree_tris(psx_mod_read_word(sub+0x14u),
                                            psx_mod_read_word(st+0x2cu),wo,wd,tmax,&tth) &&
                       (!hit || tth<thit)) {thit=tth;hit=1;}
                }
            } else {
                hit=moh_vr_ray_tree(g_level_root,wo,wd,NULL,1,tmax,&thit,NULL);
            }
            if(hit) {
                double wh[3],gh[3];
                for(int i=0;i<3;i++)wh[i]=wo[i]+wd[i]*thit;
                vr_m3_mul(g_camera_rt,wh,gh);
                for(int i=0;i<3;i++) {
                    double r2=0;
                    for(int j=0;j<3;j++)r2+=R[i*3+j]*(gh[j]+g_camera_tr[j]);
                    p1[i]=(r2+vv->translation[i])*g_weapon_projection_scale;
                }
                have_dot=1;
            }
        }
    }
    double x0,y0,x1,y1;
    if(!vr_laser_project(o,&x0,&y0)||!vr_laser_project(p1,&x1,&y1))return;
    double hx=x1,hy=y1;   /* dot centre, before the segment clip moves the tip */
    int dot=have_dot && g_laser_dot>=1 && fabs(hx)<=1023 && fabs(hy)<=1023;
    if(vr_laser_clip(&x0,&y0,&x1,&y1)<0)return;
    long isx0=lround(x0),isy0=lround(y0),isx1=lround(x1),isy1=lround(y1);
    uint32_t col=(uint32_t)(g_laser_bright&0xff);
    psx_mod_write_word(0x1f801810u,0x50000000u|col); /* shaded line, solid colour */
    psx_mod_write_word(0x1f801810u,((uint32_t)(isy0&0xffff)<<16)|(uint32_t)(isx0&0xffff));
    psx_mod_write_word(0x1f801810u,col);             /* same colour at the tip: no fade */
    psx_mod_write_word(0x1f801810u,((uint32_t)(isy1&0xffff)<<16)|(uint32_t)(isx1&0xffff));
    if(dot) {
        long r=(long)lround(g_laser_dot),cx=lround(hx),cy=lround(hy);
        long xa=cx-r<-1024?-1024:(cx-r>1023?1023:cx-r);
        long xb=cx+r<-1024?-1024:(cx+r>1023?1023:cx+r);
        long ya=cy-r<-1024?-1024:(cy-r>1023?1023:cy-r);
        long yb=cy+r<-1024?-1024:(cy+r>1023?1023:cy+r);
        uint32_t cw=(uint32_t)(xa&0x7ff),cw2=(uint32_t)(xb&0x7ff);
        uint32_t rw=(uint32_t)(ya&0x7ff),rw2=(uint32_t)(yb&0x7ff);
        psx_mod_write_word(0x1f801810u,0x28000000u|col); /* POLY_G4, same colour */
        psx_mod_write_word(0x1f801810u,col);
        psx_mod_write_word(0x1f801810u,col);
        psx_mod_write_word(0x1f801810u,col);
        psx_mod_write_word(0x1f801810u,(rw<<16)|cw);
        psx_mod_write_word(0x1f801810u,(rw<<16)|cw2);
        psx_mod_write_word(0x1f801810u,(rw2<<16)|cw);
        psx_mod_write_word(0x1f801810u,(rw2<<16)|cw2);
    }
}

/* Native player+256 names the held first-person object. Single-player
 * embeds it at input+784; weapon selection uses input+85, not loadout slot.
 * Wrap its geometry draw so authored H compensation can be scoped to this
 * object, restoring the eye view before any subsequent world/HUD draw. */
static int vr_weapon_geometry_filter(CPUState *cpu,uint32_t address) {
    if(!g_weapon_outer)vr_geo_entry(cpu,address);
    if(!g_weapon_pose || !g_in_pass || g_weapon_outer || g_menu_flat)return 0;
    uint32_t player=vr_weapon_player();
    if(player<0x80000000u || player>=0x801ff000u || psx_mod_read_word(player+8)!=1)return 0;
    uint32_t input=psx_mod_read_word(player+904);
    if(input<0x80000000u || input>=0x801ff000u || cpu->gpr[4]!=input+(g_scene_mp?2452u:784u) ||
       cpu->gpr[4]!=psx_mod_read_word(player+256u) ||
       cpu->gte_ctrl[26]!=133u)return 0;
    const MOHVRWeaponProfile *profile=moh_vr_weapon_profile(psx_mod_read_byte(input+85u));
    if(!profile)return 0;
    PSXModOpenXRHands h={0};h.struct_size=sizeof h;
    if(!psx_mod_openxr_hands(&h) || !h.focused || !h.origin_valid || h.age_ms>150)return 0;
    const PSXModTrackedPose *grip=&h.pose[1][PSX_MOD_XR_GRIP_POSE];
    const PSXModTrackedPose *aim=&h.pose[1][PSX_MOD_XR_AIM_POSE];
    if(!grip->active || !aim->active || (grip->flags&3)!=3 || (aim->flags&3)!=3)return 0;
    double w[9],inverse[9],q[4],p[3],r[9],t[3];
    for(int i=0;i<9;i++)w[i]=(int16_t)(cpu->gte_ctrl[i/2] >> ((i%2)*16))/4096.0;
    for(int i=0;i<4;i++)q[i]=aim->orientation_xyzw[i];
    for(int i=0;i<3;i++)p[i]=grip->position_m[i];
    if(!moh_vr_matrix_inverse(w,inverse) ||
       !vr_pose_to_transform(q,p,h.origin_orientation_xyzw,h.origin_position_m,
                             g_units_per_meter*g_weapon_projection_scale,r,t))return 0;
    double weapon_scale=g_units_per_meter*g_weapon_projection_scale/g_weapon_model_units_per_meter *
                        moh_vr_weapon_model_scale(profile);
    if(!moh_vr_weapon_transform(inverse,r,t,weapon_scale,g_weapon_pivot,g_weapon_draw_rotation,g_weapon_draw_translation))return 0;
    /* Current measured weapon TR is zero; decline another transform contract. */
    if(cpu->gte_ctrl[5] || cpu->gte_ctrl[6] || cpu->gte_ctrl[7])return 0;
    uint32_t header=psx_mod_read_word(cpu->gpr[4]+124u);
    if(header<0x80000000u || header>=0x801ff000u)return 0;
    uint32_t faces=psx_mod_read_word(header),count=psx_mod_read_word(header+4u),gun_faces=0;
    if(count!=profile->faces || faces<0x80000000u || faces>0x80200000u-count*28u)return 0;
    uint32_t nodes_header=psx_mod_read_word(cpu->gpr[4]+128u);
    if(nodes_header<0x80000000u || nodes_header>0x801ffff8u ||
       psx_mod_read_word(nodes_header+4u)!=profile->node_count)return 0;
    uint32_t nodes=psx_mod_read_word(nodes_header);
    if(nodes<0x80000000u || nodes>0x80200000u-profile->node_count*8u)return 0;
    for(unsigned n=profile->gun_node;n<profile->node_count;n++)
        if(psx_mod_read_word(nodes+n*8u+4u)!=profile->vertices[n-profile->gun_node])return 0;
    for(uint32_t i=0;i<count;i++) {
        uint32_t a=faces+i*28u;
        uint8_t ns[3],vs[3];
        for(unsigned k=0;k<3;k++) {
            vs[k]=psx_mod_read_byte(a+22u+k*2u);
            ns[k]=psx_mod_read_byte(a+23u+k*2u);
        }
        int selected=moh_vr_weapon_face(profile,ns,vs);
        if(selected<0)return 0;
        if(selected)gun_faces++;
    }
    if(gun_faces!=profile->gun_faces)return 0;
    g_weapon_render_entity=cpu->gpr[4];g_weapon_outer=1;
    PSXModRenderView view=g_eye_view[g_stereo_eye];
    /* Uniformly enlarge camera coordinates AND the eye displacement. Ratios
     * X/Z and physical stereo stay fixed, while packed XYZ and native depth
     * retain precision and avoid the original H/2 divide threshold. */
    for(int i=0;i<3;i++)view.translation[i]=(int32_t)llround(view.translation[i]*g_weapon_projection_scale);
    if(g_authored_focal)view.projection_h_ref=133;
    if(!psx_mod_render_view(&view)){g_weapon_outer=0;return 0;}
    vr_laser_capture(cpu,&view,r,t,weapon_scale);
    /* Keep the measured weapon assembly, including animated gun parts,
     * and remove native arms inside this eye's rollback transaction. */
    {
        uint32_t kept=0;
        for(uint32_t i=0;i<count;i++) {
            uint32_t a=faces+i*28u;
            uint8_t ns[3],vs[3];
            for(unsigned k=0;k<3;k++) {
                vs[k]=psx_mod_read_byte(a+22u+k*2u);
                ns[k]=psx_mod_read_byte(a+23u+k*2u);
            }
            if(moh_vr_weapon_face(profile,ns,vs)!=1)continue;
            if(kept!=i)for(uint32_t k=0;k<28;k+=4)
                psx_mod_write_word(faces+kept*28u+k,psx_mod_read_word(a+k));
            kept++;
        }
        psx_mod_write_word(header+4u,kept);
    }
    uint32_t ra=cpu->gpr[31];psx_dispatch_call(cpu,address,ra);
    (void)psx_mod_render_view(&g_eye_view[g_stereo_eye]);
    g_weapon_outer=0;return 1;
}
static int vr_weapon_vertices_filter(CPUState *cpu,uint32_t address) {
    if(!g_weapon_inner)vr_entity_entry(cpu,address);
    if(!g_weapon_outer || g_weapon_inner || !g_in_pass ||
       cpu->gpr[4]!=g_weapon_render_entity ||
       cpu->gpr[31]!=(g_scene_mp?0x8007e56cu:0x80080f2cu))return 0;
    g_weapon_inner=1;
    uint32_t ra=cpu->gpr[31];psx_dispatch_call(cpu,address,ra);
    if(g_stereo_success && g_stereo_eye==PSX_MOD_EYE_RIGHT &&
       g_stereo_fault==3 && !g_stereo_fault_done) {
        g_stereo_fault_done=1;vr_watchdog_inject(cpu);return 1;
    }
    /* 84718 sets cursor 8009EABC to 800AB148; 13AE4 advances it once per
     * packed XYZ/padding vertex. Intercept before 80F2C restores W and RTPS. */
    const uint32_t base=g_scene_mp?0x800aa490u:0x800ab148u;
    uint32_t end=psx_mod_read_word(g_scene_mp?0x8009b4fcu:0x8009eabcu);
    if(end>=base && end<=base+16384u && !((end-base)&7u)) {
        for(uint32_t a=base;a<end;a+=8) {
            double source[3],point[3];
            for(int i=0;i<3;i++)source[i]=(int16_t)psx_mod_read_half(a+i*2);
            int valid=1;
            for(int i=0;i<3;i++) {
                point[i]=g_weapon_draw_translation[i];
                for(int j=0;j<3;j++)point[i]+=g_weapon_draw_rotation[i*3+j]*source[j];
                if(!isfinite(point[i]) || fabs(point[i])>32767)valid=0;
            }
            if(valid)for(int i=0;i<3;i++)psx_mod_write_half(a+i*2,(uint16_t)(int16_t)lround(point[i]));
        }
    }
    g_weapon_inner=0;return 1;
}

static uint32_t vr_controller_mode(const PSXModControllerInput *input) {
    (void)input;return PSX_MOD_CONTROLLER_ANALOG;
}

static void vr_entity_activate(void) {
    if (g_stereo) (void)psx_mod_set_stereo_presentation(1);
    if (g_openxr) (void)psx_mod_openxr_enable(1);
    g_scene_age=4;g_native_surface=-1;vr_native_surface_mode(1);
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
    if ((e = getenv("PSX_VR_HEAD_FRUSTUM"))) g_head_frustum = !strcmp(e,"1");
    if ((e = getenv("PSX_VR_WEAPON_POSE"))) g_weapon_pose = atoi(e) != 0;
    if ((e = getenv("PSX_VR_LASER"))) g_laser = atoi(e) != 0; else g_laser = g_weapon_pose;
    if ((e = getenv("PSX_VR_LASER_RANGE_M"))) g_laser_range_m = strtod(e,NULL);
    if ((e = getenv("PSX_VR_LASER_ORIGIN_M"))) g_laser_origin_m = strtod(e,NULL);
    if ((e = getenv("PSX_VR_LASER_AXIS"))) g_laser_axis = atoi(e);
    if ((e = getenv("PSX_VR_LASER_SIGN"))) g_laser_sign = strtod(e,NULL);
    if ((e = getenv("PSX_VR_LASER_MUZZLE"))) {
        double x,y,z;
        if(sscanf(e,"%lf,%lf,%lf",&x,&y,&z)==3 && isfinite(x) && isfinite(y) && isfinite(z) &&
           fabs(x)<32767 && fabs(y)<32767 && fabs(z)<32767) {
            g_laser_muzzle[0]=x;g_laser_muzzle[1]=y;g_laser_muzzle[2]=z;
        }
    }
    if ((e = getenv("PSX_VR_LASER_RAY"))) g_laser_ray = atoi(e) != 0;
    if ((e = getenv("PSX_VR_LASER_TRI"))) g_laser_tris = atoi(e) != 0;
    if ((e = getenv("PSX_VR_LASER_DOT"))) g_laser_dot = strtod(e,NULL);
    if ((e = getenv("PSX_VR_LASER_BRIGHT"))) g_laser_bright = atoi(e);
    if(!isfinite(g_laser_dot) || g_laser_dot<0 || g_laser_dot>32) g_laser_dot=2;
    if(g_laser_bright<1 || g_laser_bright>255) g_laser_bright=160;
    if(!isfinite(g_laser_range_m) || g_laser_range_m<1 || g_laser_range_m>500) g_laser_range_m=150;
    if(!isfinite(g_laser_origin_m) || g_laser_origin_m<0 || g_laser_origin_m>10) g_laser_origin_m=.35;
    if(g_laser_axis<0 || g_laser_axis>2) g_laser_axis=2;
    g_laser_sign = (!isfinite(g_laser_sign) || g_laser_sign==0) ? 1 : (g_laser_sign<0 ? -1 : 1);
    if ((e = getenv("PSX_VR_WEAPON_MP_CONTROL"))) g_mp_control = !strcmp(e,"1");
    if ((e = getenv("PSX_VR_MENU_SURFACE"))) g_menu_surface=atoi(e)!=0;
    if ((e = getenv("PSX_VR_MENU_DISTANCE"))) g_menu_distance=strtod(e,NULL);
    if ((e = getenv("PSX_VR_MENU_WIDTH"))) g_menu_width=strtod(e,NULL);
    if(!isfinite(g_menu_distance) || g_menu_distance<.25 || g_menu_distance>20)g_menu_distance=2;
    if(!isfinite(g_menu_width) || g_menu_width<.25 || g_menu_width>10)g_menu_width=2;
    if ((e = getenv("PSX_VR_WEAPON_MODEL_UNITS_PER_METER"))) g_weapon_model_units_per_meter=strtod(e,NULL);
    if ((e = getenv("PSX_VR_WEAPON_PROJECTION_SCALE"))) g_weapon_projection_scale=strtod(e,NULL);
    if(!isfinite(g_weapon_projection_scale) || g_weapon_projection_scale<1 ||
       g_weapon_projection_scale>32)g_weapon_projection_scale=16;
    if(!isfinite(g_weapon_model_units_per_meter) || g_weapon_model_units_per_meter<1 ||
       g_weapon_model_units_per_meter>65536)g_weapon_model_units_per_meter=850;
    if ((e = getenv("PSX_VR_WEAPON_PIVOT"))) {
        double x,y,z;
        if(sscanf(e,"%lf,%lf,%lf",&x,&y,&z)==3 && isfinite(x) && isfinite(y) && isfinite(z) &&
           fabs(x)<32767 && fabs(y)<32767 && fabs(z)<32767) {
            g_weapon_pivot[0]=x;g_weapon_pivot[1]=y;g_weapon_pivot[2]=z;
        }
    }
    if ((e = getenv("PSX_VR_WEAPON_AIM"))) g_weapon_aim = atoi(e) != 0;
    if ((e = getenv("PSX_VR_MOVEMENT"))) g_movement = atoi(e) != 0;
    if (g_movement < 0) g_movement = g_openxr;
    if ((e = getenv("PSX_VR_MOVE_DEADZONE"))) g_move_deadzone = strtod(e,NULL);
    if ((e = getenv("PSX_VR_TURN_GAIN"))) g_turn_gain = strtod(e,NULL);
    g_one_stick = 0;
    if ((e = getenv("PSX_VR_ONE_STICK"))) g_one_stick = atoi(e);
    if(g_one_stick<0 || g_one_stick>2) g_one_stick=0;
    if (!isfinite(g_move_deadzone) || g_move_deadzone < 0 || g_move_deadzone > .9) g_move_deadzone = .2;
    if (!isfinite(g_turn_gain) || g_turn_gain < 0 || g_turn_gain > 2) g_turn_gain = .78;
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
    if (g_stereo_fault < 0 || g_stereo_fault > 3) g_stereo_fault = 0;
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
    (void)psx_mod_register_function_filter_plugin("moh.vr.stereo", FUN_80084718,
                                                  vr_weapon_vertices_filter);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo", GEO_800814C4,
                                                 vr_geo_entry);
    (void)psx_mod_register_function_filter_plugin("moh.vr.stereo", GEO_80080DD4,
                                                  vr_weapon_geometry_filter);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",
                                                 RENDER_800824D0,
                                                 vr_render_entry);
    (void)psx_mod_register_function_filter_plugin("moh.vr.stereo",LVTX_8008B3E8,
                                                 vr_head_frustum_filter);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",
                                                 VSYNC_80016E38,
                                                 vr_vsync_entry);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",
                                                 RENDER_WAIT_80090B80,
                                                 vr_wait_entry);
    (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",0x8006a76cu,
                                                 vr_shot_basis_entry);
    if(g_mp_control) {
        (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",0x8008ce50u,vr_wait_entry);
        (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",0x8008a440u,vr_mp_level_entry);
        (void)psx_mod_register_function_filter_plugin("moh.vr.stereo",0x8007e414u,vr_weapon_geometry_filter);
        (void)psx_mod_register_function_filter_plugin("moh.vr.stereo",0x800836c0u,vr_weapon_vertices_filter);
        (void)psx_mod_register_function_entry_plugin("moh.vr.stereo",0x8006591cu,vr_shot_basis_entry);
    }
    (void)psx_mod_register_activation_plugin("moh.vr.stereo",
                                             vr_entity_activate);
    (void)psx_mod_register_vblank_plugin("moh.vr.stereo",vr_surface_vblank);
}
