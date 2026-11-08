#pragma once

#include <stdint.h>
#include "mod_input_binding.h"

/* Host displacement is accumulated until a proven game update consumes it. */
typedef struct MohMkbMouse {
    double pending_x, pending_y;
} MohMkbMouse;

void moh_mkb_mouse_reset(MohMkbMouse *mouse);
void moh_mkb_mouse_motion(MohMkbMouse *mouse, double dx, double dy);
void moh_mkb_mouse_take_scaled(MohMkbMouse *mouse, double gain,
                               int64_t *yaw, int64_t *pitch);
void moh_mkb_stick_pair(int left, int right, int up, int down,
                        uint8_t *x, uint8_t *y);
/* Reference firearm zoom advances over three native updates. */
double moh_mkb_zoom_step(double amount, int held);
double moh_mkb_zoom_ease(double amount);
double moh_mkb_look_gain(int sensitivity, int aim_sensitivity,
                         int zoom_percent, double amount);
void moh_mkb_view_matrix(int32_t pitch, int32_t yaw, const int32_t eye[3],
                         int16_t rotation[9], int32_t translation[3]);
/* Build a Q12 shot basis from a native Q16 origin and target. */
int moh_mkb_shot_matrix(const int32_t origin[3], const int32_t target[3],
                        int16_t rotation[9]);
int moh_mkb_shot_angles(const int32_t origin[3], const int32_t target[3],
                        int32_t *pitch, int32_t *yaw);
/* Select from the native inventory's occupied slots, skipping -1 entries. */
int moh_mkb_weapon_slot(const int16_t *reserve, unsigned count, unsigned current,
                        int direction, int requested);

/* Q16 native angle fields. Apply the same displacement to player and input
 * base angles, retaining their native aim-assist correction. */
typedef struct MohMkbAngles {
    int32_t player_pitch, player_yaw, input_pitch, input_yaw;
} MohMkbAngles;
int moh_mkb_apply_look(MohMkbAngles *angles, int64_t yaw, int64_t pitch);
int32_t moh_mkb_mounted_yaw(int32_t current, int32_t anchor, int64_t delta);

struct CPUState;
/* Look is restricted to campaign weapon/mode pairs in the shared native
 * aim branch; unsupported contexts discard motion. */
int moh_mkb_guest_eligible(void);
int moh_mkb_guest_mounted(void);
/* 0 unsupported, 1 custom zoom (firearm or grenade), 2 scoped rifle. */
int moh_mkb_guest_aim_kind(void);
/* 0 unsupported, 1 owned campaign player, 2 native pause menu. */
int moh_mkb_guest_context(void);
int moh_mkb_guest_update(struct CPUState *cpu, MohMkbMouse *mouse,
                         int captured, int sensitivity);
/* -2 means the native weapon switch is busy; -1 means no valid selection. */
int moh_mkb_guest_weapon_slot(int direction);
int moh_mkb_guest_select_weapon(struct CPUState *cpu, int slot);

/* Firearm aiming state and the destination instruction adapters. */
typedef struct MohMkbAim {
    double amount;
    int held;
    int zoom_percent;
    uint32_t reticle_stack;
} MohMkbAim;
int moh_mkb_guest_firearm_hook(struct CPUState *cpu, uint32_t site,
                               MohMkbAim *aim);
int moh_mkb_guest_tighten_reticle(struct CPUState *cpu, MohMkbAim *aim);
int moh_mkb_guest_sniper_hook(struct CPUState *cpu, uint32_t site, int scope_owned);
int moh_mkb_guest_shot_hook(struct CPUState *cpu, const MohMkbAim *aim);
int moh_mkb_guest_precise_view(struct CPUState *cpu, uint32_t prepared_player);
typedef struct MohMkbCamera {
    uint32_t player, camera, result;
    int preparing;
} MohMkbCamera;
int moh_mkb_guest_camera_update(struct CPUState *cpu, MohMkbMouse *mouse,
                                const MohMkbAim *aim, int sensitivity,
                                int aim_sensitivity, MohMkbCamera *state);
int moh_mkb_guest_prepared_pitch(struct CPUState *cpu, MohMkbCamera *state);
int moh_mkb_guest_direct_adjust(struct CPUState *cpu, uint32_t site,
                                const MohMkbCamera *state);

typedef enum MohMkbAction {
    MOH_MKB_FORWARD, MOH_MKB_BACKWARD, MOH_MKB_LEFT, MOH_MKB_RIGHT,
    MOH_MKB_FIRE, MOH_MKB_AIM, MOH_MKB_RELOAD, MOH_MKB_USE,
    MOH_MKB_JUMP, MOH_MKB_CROUCH, MOH_MKB_NEXT_WEAPON, MOH_MKB_PREV_WEAPON,
    MOH_MKB_PAUSE, MOH_MKB_OBJECTIVES, MOH_MKB_MENU_UP, MOH_MKB_MENU_DOWN,
    MOH_MKB_MENU_LEFT, MOH_MKB_MENU_RIGHT, MOH_MKB_MENU_CONFIRM,
    MOH_MKB_MENU_BACK, MOH_MKB_ACTION_COUNT
} MohMkbAction;

typedef struct MohMkbControls {
    PSXModInputBindings bindings[MOH_MKB_ACTION_COUNT];
    double wheel_remainder;
    int wheel_steps;
} MohMkbControls;

const char *moh_mkb_action_id(MohMkbAction action);
const char *moh_mkb_action_default(MohMkbAction action);
void moh_mkb_controls_reset(MohMkbControls *controls);
int moh_mkb_controls_set(MohMkbControls *controls, MohMkbAction action,
                         const char *value);
int moh_mkb_action_down(const MohMkbControls *controls, MohMkbAction action,
                        const uint8_t *keys, unsigned key_count, uint32_t buttons);
void moh_mkb_controls_wheel(MohMkbControls *controls, double dy);
int moh_mkb_controls_take_wheel(MohMkbControls *controls);
