#include "moh_mkb.h"
#include "mod_plugins.h"
#include "cpu_state.h"
#include "psx_cycle_freeze.h"
#include "psx_openxr.h"
#include <math.h>

/* SLUS-00974 campaign overlay. Exact function hashes were independently
 * checked against the live save and three retained RAM captures. The input
 * dispatcher is required for a native right turn in a controlled A/B test. */
#define MOH_PLAYER_ROOT 0x8009d654u
#define MOH_PAUSE 0x8009a61cu
#define MOH_INPUT_UPDATE 0x8007f300u
#define MOH_INPUT_CALLER 0x8007a6b0u
#define MOH_INPUT_RETURN 0x8007af58u

static uint32_t code_hash(uint32_t address, uint32_t size) {
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < size; ++i)
        hash = (hash ^ psx_mod_read_byte(address + i)) * 16777619u;
    return hash;
}

static uint32_t owned_input(uint32_t player) {
    if (player < 0x800b0000u || player > 0x801fc000u || (player & 3u) ||
        psx_mod_read_word(player + 8u) != 1u) return 0;
    const uint32_t input = psx_mod_read_word(player + 0x388u);
    if (input != player + 0x3ccu || psx_mod_read_byte(input + 0x152u) != 0u ||
        code_hash(MOH_INPUT_UPDATE, 0xe9cu) != 0xecc55d75u ||
        code_hash(MOH_INPUT_CALLER, 0xca0u) != 0x2f6f9cabu) return 0;
    return input;
}

static uint32_t supported_input(uint32_t player) {
    const uint32_t input = owned_input(player);
    if (!input || psx_mod_read_word(MOH_PAUSE) != 0u) return 0;
    const uint32_t mode = psx_mod_read_word(input + 0x1c8u);
    const uint8_t weapon = psx_mod_read_byte(input + 0x55u);
    /* Match the reference's shared 7/8/9 boundary. The weapon ID can change
     * one update before its mode, so rejecting mismatched pairs creates a
     * one-frame camera discontinuity during native weapon switches. */
    if ((mode != 7u && mode != 8u && mode != 9u) || weapon > 12u)
        return 0;
    return input;
}

static int firearm_weapon(uint8_t weapon) {
    return weapon == 0u || weapon == 1u || weapon == 4u || weapon == 6u ||
           weapon == 8u || weapon == 9u || weapon == 11u || weapon == 12u;
}

static int zoom_weapon(uint8_t weapon) {
    /* Grenades use the same opt-in view zoom and reticle, but their native
     * ballistic constructor and throw strength stay unchanged. */
    return firearm_weapon(weapon) || weapon == 3u || weapon == 10u;
}

static uint32_t owned_camera(uint32_t player) {
    const uint32_t camera = psx_mod_read_word(0x8009d638u);
    if (camera < 0x800b0000u || camera > 0x801fc000u || (camera & 3u) ||
        psx_mod_read_word(camera + 8u) != 2u ||
        psx_mod_read_word(camera + 0x388u) != camera + 0x3ccu ||
        psx_mod_read_word(camera + 0x36cu) != 0x800473dcu) return 0;
    const uint32_t handle = psx_mod_read_word(camera + 0x3d8u);
    if (!handle || (handle & 0xffffu) >= 512u ||
        psx_mod_read_word(0x8009d678u + 4u * (handle & 0xffffu)) != player)
        return 0;
    return camera;
}

int moh_mkb_guest_context(void) {
    if (!psx_mod_game_started() || psx_openxr_session_active() ||
        g_psx_render_pass_active) return 0;
    /* These two overlay words identify the multiplayer code used by the
     * destination's existing VR adapter. Do not apply the campaign/menu
     * session profile to split-screen play. */
    if (psx_mod_read_word(0x8008ce50u) == 0x27bdffe0u &&
        psx_mod_read_word(0x80045908u) == 0x0c019647u) return 0;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    const uint32_t pause = psx_mod_read_word(MOH_PAUSE);
    /* Like Underground, use the menu profile before a campaign player exists
     * and while the campaign is paused. The launcher/BIOS and XR stay outside
     * this transform. */
    if (!owned_input(player)) return 2;
    return pause == 0u ? 1 : 2;
}

int moh_mkb_guest_eligible(void) { return moh_mkb_guest_context() == 1; }

int moh_mkb_guest_aim_kind(void) {
    if (!moh_mkb_guest_eligible()) return 0;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    const uint32_t owned = owned_input(player);
    if (owned && psx_mod_read_byte(owned + 0x55u) == 5u) return 2;
    const uint32_t input = supported_input(player);
    if (!input) return 0;
    return zoom_weapon(psx_mod_read_byte(input + 0x55u)) ? 1 : 0;
}

int moh_mkb_guest_weapon_slot(int direction) {
    if (!moh_mkb_guest_eligible()) return -1;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    const uint32_t input = owned_input(player);
    if (!input) return -1;
    if (psx_mod_read_word(input + 0x13cu) || psx_mod_read_word(input + 0x124u))
        return -2;
    const uint32_t count = psx_mod_read_word(0x8009e874u);
    const unsigned current = psx_mod_read_byte(input + 0x54u);
    if (!count || count > 8u || current >= count) return -1;
    int16_t reserve[8];
    for (unsigned i = 0; i < count; ++i)
        reserve[i] = (int16_t)psx_mod_read_half(input + 0x56u + 2u * i);
    return moh_mkb_weapon_slot(reserve, count, current, direction, -1);
}

int moh_mkb_guest_select_weapon(CPUState *cpu, int slot) {
    if (!cpu || slot < 0 || !moh_mkb_guest_eligible()) return 0;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    const uint32_t input = owned_input(player);
    const uint32_t count = psx_mod_read_word(0x8009e874u);
    if (!input || count > 8u || (unsigned)slot >= count ||
        cpu->gpr[18] != player || cpu->gpr[17] != input ||
        (int16_t)psx_mod_read_half(input + 0x56u + 2u * (unsigned)slot) == -1)
        return 0;
    cpu->gpr[2] = (uint32_t)slot;
    return 1;
}

/* SLUS-00974 equivalents of Underground's focal, ray, model and
 * reticle adapters. Only scratch registers are changed at each instruction;
 * the native input flags and actual focal length remain intact. Gameplay
 * validation of the combined aim path remains outstanding. */
int moh_mkb_guest_firearm_hook(CPUState *cpu, uint32_t site, MohMkbAim *aim) {
    if (!cpu || !aim || aim->zoom_percent < 100 || aim->zoom_percent > 250 ||
        !isfinite(aim->amount) || aim->amount < 0 || aim->amount > 1 ||
        !moh_mkb_guest_eligible()) return 0;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    const uint32_t input = supported_input(player);
    if (!input || !zoom_weapon(psx_mod_read_byte(input + 0x55u))) return 0;

    if (site == 0x8007b254u && cpu->gpr[18] == player &&
        cpu->gpr[17] == input && cpu->gpr[2] == 400u) {
        aim->amount = moh_mkb_zoom_step(aim->amount, aim->held);
        const double ratio = 1.0 + (aim->zoom_percent / 100.0 - 1.0) *
                                      moh_mkb_zoom_ease(aim->amount);
        cpu->gpr[2] = (uint32_t)lround(400.0 * ratio);
        return 1;
    }
    if (site == 0x8007e06cu && cpu->gpr[4] == player &&
        cpu->gpr[19] == input &&
        code_hash(0x8007e034u, 0x1058u) == 0x66bbea17u) {
        cpu->gpr[2] = 400u;
        return 1;
    }
    if (site == 0x8007b91cu && cpu->gpr[17] == player &&
        cpu->gpr[16] == input &&
        code_hash(0x8007b8f4u, 0x160u) == 0x44cb07e8u) {
        cpu->gpr[3] = 400u;
        return 1;
    }
    if (site == 0x8007cb8cu && cpu->gpr[16] == player &&
        cpu->gpr[6] == input &&
        code_hash(0x8007caf8u, 0x428u) == 0xd1b6bcf0u) {
        aim->reticle_stack = aim->held ? cpu->gpr[29] : 0;
        cpu->gpr[2] = aim->held ? 0x20u : 0u;
        return 1;
    }
    return 0;
}

int moh_mkb_guest_tighten_reticle(CPUState *cpu, MohMkbAim *aim) {
    if (!cpu || !aim || !aim->held || !aim->reticle_stack ||
        cpu->gpr[29] != aim->reticle_stack || !moh_mkb_guest_eligible() ||
        code_hash(0x8007caf8u, 0x428u) != 0xd1b6bcf0u) return 0;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    const uint32_t input = supported_input(player);
    if (!input || !zoom_weapon(psx_mod_read_byte(input + 0x55u))) return 0;
    const uint32_t base = cpu->gpr[22] - 64u, physical = base & 0x1fffffffu;
    if ((base & 3u) || physical < 0xb0000u || physical > 0x1fffc0u) return 0;
    const int16_t centre[2] = {(int16_t)cpu->gpr[19], (int16_t)cpu->gpr[20]};
    const int16_t original[8][2] = {{0,-12},{0,-5},{0,12},{0,5},
                                    {-18,0},{-8,0},{18,0},{8,0}};
    const int16_t tight[8][2] = {{0,-8},{0,-3},{0,8},{0,3},
                                 {-12,0},{-5,0},{12,0},{5,0}};
    for (unsigned p = 0; p < 8; ++p)
        for (unsigned axis = 0; axis < 2; ++axis)
            if ((int16_t)psx_mod_read_half(base + 16u * (p / 2u) + 8u +
                                           4u * (p % 2u) + 2u * axis) !=
                centre[axis] + original[p][axis]) return 0;
    for (unsigned p = 0; p < 8; ++p)
        for (unsigned axis = 0; axis < 2; ++axis)
            psx_mod_write_half(base + 16u * (p / 2u) + 8u +
                               4u * (p % 2u) + 2u * axis,
                (uint16_t)(centre[axis] + original[p][axis] +
                    lround((tight[p][axis] - original[p][axis]) *
                           moh_mkb_zoom_ease(aim->amount))));
    aim->reticle_stack = 0;
    return 1;
}

/* Scope remains native; these are the destination equivalents of the
 * reference's walking and centered-ray adaptations. Scoped gameplay still
 * requires validation. */
int moh_mkb_guest_sniper_hook(CPUState *cpu, uint32_t site, int scope_owned) {
    if (!cpu || !moh_mkb_guest_eligible()) return 0;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    const uint32_t input = owned_input(player);
    if (!input || (psx_mod_read_byte(input + 0x55u) != 5u &&
                   !(site == 0x8007f360u && scope_owned))) return 0;
    const uint32_t flags = psx_mod_read_word(input);
    /* On scope release the native dispatcher tests the prior frame's flag
     * and otherwise restores a position that the walking path never saved. */
    if (!(flags & 0x20u) &&
        !(site == 0x8007f360u && (psx_mod_read_word(input + 4u) & 0x20u)))
        return 0;

    if (site == 0x8007a864u && cpu->gpr[18] == player &&
        cpu->gpr[17] == input && cpu->gpr[2] == 2u &&
        code_hash(0x8007ba20u, 0xbcu) == 0x7b947081u) {
        cpu->gpr[2] = 7u;
        return 1;
    }
    if ((site == 0x8007f34cu || site == 0x8007f360u) &&
        cpu->gpr[20] == player && cpu->gpr[16] == input) {
        cpu->gpr[2] &= ~0x20u;
        return 1;
    }
    if (code_hash(0x8007e034u, 0x1058u) != 0x66bbea17u) return 0;
    if (site == 0x8007e06cu && cpu->gpr[4] == player && cpu->gpr[19] == input) {
        cpu->gpr[2] = 400u;
        return 1;
    }
    if (cpu->gpr[22] != player || cpu->gpr[19] != input) return 0;
    if (site == 0x8007e1f0u) cpu->gpr[4] = 256u;
    else if (site == 0x8007e1fcu) cpu->gpr[3] = cpu->gpr[2];
    else if (site == 0x8007e218u) cpu->gpr[2] = 400u;
    else if (site == 0x8007e87cu) cpu->gpr[18] = cpu->gpr[23];
    else return 0;
    return 1;
}

/* Candidate counterpart to Underground's zoomed camera-origin and precise
 * scoped-shot adapters. The constructor's owner and shot registers match the
 * destination VR adapter's measured basis seam. The camera/target relationship
 * during scoped gameplay still requires validation. */
int moh_mkb_guest_shot_hook(CPUState *cpu, const MohMkbAim *aim) {
    if (!cpu || !aim || !moh_mkb_guest_eligible() ||
        cpu->gpr[31] != 0x80045524u ||
        code_hash(0x8004532cu, 0x39cu) != 0x0e78a951u) return 0;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    const uint32_t input = owned_input(player);
    const uint32_t shot = cpu->gpr[16];
    if (!input || cpu->gpr[18] != player || shot < 0x800b0000u ||
        shot > 0x801fc000u || (shot & 3u)) return 0;
    const uint32_t shot_type = psx_mod_read_word(shot + 8u);
    const uint32_t weapon = psx_mod_read_byte(input + 0x55u);
    const int scoped = weapon == 5u && (psx_mod_read_word(input) & 0x20u);
    if (scoped ? shot_type != 0x13f6u :
        (!(aim->amount > 0 && aim->amount <= 1) || !supported_input(player) ||
         !firearm_weapon((uint8_t)weapon) ||
         (shot_type != 0x13f6u && shot_type != 0x13f7u && shot_type != 0x13f1u)))
        return 0;
    const uint32_t camera = owned_camera(player);
    if (!camera) return 0;
    int32_t origin[3], target[3], pitch, yaw;
    int16_t rotation[9];
    for (unsigned i = 0; i < 3; ++i) {
        origin[i] = (int32_t)psx_mod_read_word(camera + 0x20cu + 4u * i);
        target[i] = (int32_t)psx_mod_read_word(input + 0x1a4u + 4u * i);
    }
    /* The native basis was built from the muzzle before this hook. Once the
     * origin moves to the eye, rebuild it toward the game's cached ray target;
     * otherwise it retains a direction computed for the old origin. Native
     * weapon-specific spread applied after the constructor remains intact. */
    if (!moh_mkb_shot_matrix(origin, target, rotation) ||
        !moh_mkb_shot_angles(origin, target, &pitch, &yaw)) return 0;
    for (unsigned i = 0; i < 3; ++i)
        psx_mod_write_word(shot + 0x20cu + 4u * i, (uint32_t)origin[i]);
    /* The actor update rebuilds the matrix from these angles each frame.
     * Keep that rebuild and the matrix translation consistent with the new
     * eye origin as well as the first-frame basis. */
    psx_mod_write_word(shot + 0x228u, (uint32_t)pitch);
    psx_mod_write_word(shot + 0x22cu, (uint32_t)yaw);
    for (unsigned i = 0; i < 9; ++i)
        psx_mod_write_half(shot + 0x340u + 2u * i, (uint16_t)rotation[i]);
    for (unsigned i = 0; i < 3; ++i)
        psx_mod_write_word(shot + 0x354u + 4u * i,
                           (uint32_t)(int32_t)(int16_t)((uint32_t)origin[i] >> 16));
    return 1;
}

/* Counterpart to Underground's precise view filter. */
int moh_mkb_guest_precise_view(CPUState *cpu, uint32_t prepared_player) {
    if (!cpu || !moh_mkb_guest_eligible()) return 0;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    const uint32_t input = supported_input(player);
    const uint32_t camera = input ? owned_camera(player) : 0;
    if (!camera || prepared_player != player || cpu->gpr[4] != camera + 0x4b0u ||
        cpu->gpr[5] != camera || cpu->gpr[31] != 0x800491e8u ||
        (psx_mod_read_word(camera + 0x468u) & 0x1800u) ||
        code_hash(0x80048eecu, 0x250u) != 0xf7ba1ad8u ||
        code_hash(0x8004913cu, 0x264u) != 0xa798b89eu) return 0;
    int32_t eye[3], translation[3];
    int16_t rotation[9];
    for (unsigned i = 0; i < 3; ++i)
        eye[i] = (int32_t)psx_mod_read_word(camera + 0x20cu + 4u * i);
    moh_mkb_view_matrix((int32_t)psx_mod_read_word(player + 0x228u),
                        (int32_t)psx_mod_read_word(player + 0x22cu),
                        eye, rotation, translation);
    const uint32_t matrix = camera + 0x4ccu;
    for (unsigned i = 0; i < 9; ++i)
        psx_mod_write_half(matrix + 2u * i, (uint16_t)rotation[i]);
    for (unsigned i = 0; i < 3; ++i)
        psx_mod_write_word(matrix + 0x14u + 4u * i, (uint32_t)translation[i]);
    cpu->gpr[2] = (uint32_t)translation[2];
    return 1;
}

/* Camera-before-player equivalent of Underground's direct look boundary. */
int moh_mkb_guest_camera_update(CPUState *cpu, MohMkbMouse *mouse,
                                const MohMkbAim *aim, int sensitivity,
                                int aim_sensitivity, MohMkbCamera *state) {
    if (!cpu || !mouse || !aim || !state || sensitivity < 1 || sensitivity > 10 ||
        aim_sensitivity < 1 || aim_sensitivity > 10 || !moh_mkb_guest_eligible())
        return 0;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    const uint32_t input = supported_input(player);
    const uint32_t camera = input ? owned_camera(player) : 0;
    if (!camera || cpu->gpr[4] != camera || cpu->gpr[31] != 0x8006bdf4u ||
        code_hash(0x800473dcu, 0x1e8u) != 0x1b117b22u ||
        code_hash(0x8007badcu, 0x130u) != 0x04878fbcu) return 0;
    double gain = moh_mkb_look_gain(sensitivity, aim_sensitivity,
                                    aim->zoom_percent, aim->amount);
    if (psx_mod_read_byte(input + 0x55u) == 5u &&
        (psx_mod_read_word(input) & 0x20u)) {
        const double focal = psx_mod_read_word(input + 0xa4u);
        gain = (aim_sensitivity / 10.0) * 400.0 / fmax(400.0, focal);
    }
    if (!(gain > 0 && gain <= 8)) return 0;
    MohMkbAngles angles = {
        (int32_t)psx_mod_read_word(player + 0x228u),
        (int32_t)psx_mod_read_word(player + 0x22cu),
        (int32_t)psx_mod_read_word(player + 0x228u),
        (int32_t)psx_mod_read_word(player + 0x22cu)
    };
    const int32_t old_pitch = angles.player_pitch;
    int64_t yaw = 0, pitch = 0;
    moh_mkb_mouse_take_scaled(mouse, gain, &yaw, &pitch);
    if (!moh_mkb_apply_look(&angles, yaw, pitch)) return 0;
    state->player = player;
    state->camera = 0;
    psx_mod_write_word(player + 0x228u, (uint32_t)angles.player_pitch);
    psx_mod_write_word(player + 0x22cu, (uint32_t)angles.player_yaw);
    psx_mod_write_word(input + 0x178u, (uint32_t)angles.player_pitch);
    psx_mod_write_word(input + 0x184u, (uint32_t)angles.player_yaw);
    if (angles.player_pitch != old_pitch) {
        const uint32_t camera_input = camera + 0x3ccu;
        const uint32_t old_target = psx_mod_read_word(camera_input + 0x28u);
        state->preparing = 1;
        state->result = psx_mod_call_guest(cpu, 0x8007badcu, 0x80080170u,
                                            player, input, camera_input, 0);
        state->preparing = 0;
        psx_mod_write_word(camera_input + 0x1cu,
            psx_mod_read_word(camera_input + 0x1cu) +
            psx_mod_read_word(camera_input + 0x28u) - old_target);
        state->camera = camera;
    }
    return 1;
}

int moh_mkb_guest_prepared_pitch(CPUState *cpu, MohMkbCamera *state) {
    if (!cpu || !state || state->preparing || !state->player || !state->camera ||
        cpu->gpr[4] != state->player || cpu->gpr[5] != state->player + 0x3ccu ||
        cpu->gpr[6] != state->camera + 0x3ccu || cpu->gpr[31] != 0x80080170u)
        return 0;
    state->camera = 0;
    cpu->gpr[2] = state->result;
    return 1;
}

int moh_mkb_guest_direct_adjust(CPUState *cpu, uint32_t site,
                                const MohMkbCamera *state) {
    if (!cpu || !state || !state->player || !moh_mkb_guest_eligible() ||
        psx_mod_read_word(MOH_PLAYER_ROOT) != state->player) return 0;
    const uint32_t player = state->player, input = supported_input(player);
    if (!input) return 0;
    /* All control schemes join at this branch. Some jump directly here,
     * bypassing the preceding mode calculation. */
    if (site == 0x80080134u) {
        if (cpu->gpr[20] != player || cpu->gpr[16] != input) return 0;
        psx_mod_write_word(input + 0x178u, psx_mod_read_word(player + 0x228u));
        psx_mod_write_word(input + 0x184u, psx_mod_read_word(player + 0x22cu));
        return 1;
    }
    if (cpu->gpr[22] != player || cpu->gpr[19] != input ||
        code_hash(0x8007e034u, 0x1058u) != 0x66bbea17u) return 0;
    switch (site) {
    case 0x8007e0e0u: case 0x8007e10cu: case 0x8007e128u:
    case 0x8007ecb0u: case 0x8007ecb8u: case 0x8007ed04u:
    case 0x8007ee70u: case 0x8007efb8u:
        cpu->gpr[2] = 0;
        return 1;
    default:
        return 0;
    }
}

int moh_mkb_guest_update(CPUState *cpu, MohMkbMouse *mouse,
                         int captured, int sensitivity) {
    if (!mouse) return 0;
    /* Eye replay must never consume input queued for the real update. */
    if (g_psx_render_pass_active) return 0;
    if (!cpu || !captured || sensitivity < 1 || sensitivity > 10 ||
        !psx_mod_game_started() || psx_openxr_session_active()) goto reject;
    const uint32_t player = psx_mod_read_word(MOH_PLAYER_ROOT);
    if (player < 0x800b0000u || player > 0x801fc000u || (player & 3u)) goto reject;
    /* Other callers must not consume or discard this player's queued motion. */
    if (cpu->gpr[4] != player || cpu->gpr[5] != 0u ||
        cpu->gpr[31] != MOH_INPUT_RETURN) return 0;
    const uint32_t input = supported_input(player);
    if (!input || psx_mod_read_word(MOH_PAUSE)) goto reject;

    MohMkbAngles angles = {
        (int32_t)psx_mod_read_word(player + 0x228u),
        (int32_t)psx_mod_read_word(player + 0x22cu),
        (int32_t)psx_mod_read_word(input + 0x178u),
        (int32_t)psx_mod_read_word(input + 0x184u)
    };
    int64_t yaw = 0, pitch = 0;
    moh_mkb_mouse_take_scaled(mouse, sensitivity / 10.0, &yaw, &pitch);
    if (!moh_mkb_apply_look(&angles, yaw, pitch)) goto reject;
    if (yaw || pitch) {
        psx_mod_write_word(input + 0x178u, (uint32_t)angles.input_pitch);
        psx_mod_write_word(input + 0x184u, (uint32_t)angles.input_yaw);
        psx_mod_write_word(player + 0x228u, (uint32_t)angles.player_pitch);
        psx_mod_write_word(player + 0x22cu, (uint32_t)angles.player_yaw);
    }
    return 1;
reject:
    moh_mkb_mouse_reset(mouse);
    return 0;
}
