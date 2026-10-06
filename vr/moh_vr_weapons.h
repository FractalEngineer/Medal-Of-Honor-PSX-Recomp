#pragma once
#include <stdint.h>

/* Captured native first-person meshes, indexed by input+85 (weapon ID),
 * never by the loadout-dependent input+84 slot. All nodes from gun_node to
 * node_count are candidates for the weapon assembly; earlier nodes skin arms.
 * Zero-vertex nodes are animation joints. Preserve magazine/bolt subnodes.
 * ID 9 uses a partial-node filter because its suppressor shares arm node 11.
 * See docs/reverse/VR_WEAPON_TRACKING.md for capture provenance. */
typedef struct {
    uint8_t id, gun_node, node_count;
    uint16_t faces, gun_faces;
    uint16_t vertices[6];
} MOHVRWeaponProfile;

static const MOHVRWeaponProfile moh_vr_weapon_profiles[] = {
    { 1, 26, 32, 382, 191, {101,0,0,6,0,16}}, /* Thompson */
    { 2,  2,  3, 190, 190, {139}},            /* Bazooka */
    { 3, 23, 24, 337,  86, {45}},             /* Fragmentation grenade */
    { 4, 26, 27, 334, 169, {114}},            /* Shotgun */
    { 5, 22, 23, 317, 211, {159}},            /* Scoped rifle */
    { 6, 26, 30, 327, 190, {112,0,0,20}},     /* BAR */
    { 7, 13, 15, 317,  68, {16,20}},          /* Passport (both covers) */
    { 8, 13, 14, 315, 115, {68}},             /* Pistol */
    { 9, 11, 14, 296,  94, {121,0,57}},       /* Silenced pistol + suppressor */
    {10, 21, 22, 285, 112, {58}},             /* Stick grenade */
    {11, 24, 28, 309, 200, {146,0,7,7}},      /* MP40 */
    {12, 22, 23, 280, 174, {136}},            /* Accepted M1 rifle */
};

static inline const MOHVRWeaponProfile *moh_vr_weapon_profile(unsigned id) {
    for(unsigned i=0;i<sizeof moh_vr_weapon_profiles/sizeof moh_vr_weapon_profiles[0];i++)
        if(moh_vr_weapon_profiles[i].id==id)return &moh_vr_weapon_profiles[i];
    return 0;
}

/* Native dispatch 8007D5C8 and grenade event 8007CF4C both use constructor
 * 8004532C. These are damage/physics actors, distinct from muzzle effects. */
static inline unsigned moh_vr_weapon_shot_id(unsigned id) {
    switch(id) {
    case 1: case 4: case 6: case 11: return 5111;
    case 2: return 5105;
    case 3: case 10: return 5107;
    case 5: case 8: case 9: case 12: return 5110;
    default: return 0;
    }
}

static inline int moh_vr_weapon_face(const MOHVRWeaponProfile *p,
                                    const uint8_t nodes[3],const uint8_t vertices[3]) {
    for(unsigned i=0;i<3;i++) {
        if(nodes[i]<p->gun_node || nodes[i]>=p->node_count)return 0;
        if(vertices[i]>=p->vertices[nodes[i]-p->gun_node])return -1;
    }
    /* ID 9's suppressor is a disconnected 23-face island within node 11.
     * The remaining node-11 vertices and the mixed 9/11 faces skin the arm.
     * Validate indices above before applying this partial-node selection. */
    if(p->id==9)for(unsigned i=0;i<3;i++)
        if(nodes[i]==11 && !((vertices[i]>=7 && vertices[i]<=18) ||
                            (vertices[i]>=115 && vertices[i]<=120)))return 0;
    return 1;
}

static inline double moh_vr_weapon_model_scale(const MOHVRWeaponProfile *p) {
    return p->id==7 ? 3.0 : 1.0;
}
