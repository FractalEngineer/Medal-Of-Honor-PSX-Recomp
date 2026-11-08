#include "moh_mkb.h"
#include <math.h>
#include <string.h>

static const struct { const char *id, *defaults; } actions[MOH_MKB_ACTION_COUNT] = {
    {"forward", "key:26"}, {"backward", "key:22"},
    {"left", "key:4"}, {"right", "key:7"},
    {"fire", "mouse:1,key:40"}, {"aim", "mouse:3"},
    {"reload", "key:21"}, {"use", "key:8"},
    {"jump", "key:44,key:42"}, {"crouch", "key:224"},
    {"next_weapon", "key:20,wheel:down"}, {"prev_weapon", "wheel:up"},
    {"pause", "key:19"}, {"objectives", "key:43"},
    {"menu_up", "key:26,key:82"}, {"menu_down", "key:22,key:81"},
    {"menu_left", "key:4,key:80"}, {"menu_right", "key:7,key:79"},
    {"menu_confirm", "key:8,key:40,mouse:1"}, {"menu_back", "key:20,key:42"}
};

static int valid_action(MohMkbAction action) {
    return (unsigned)action < MOH_MKB_ACTION_COUNT;
}

const char *moh_mkb_action_id(MohMkbAction action) {
    return valid_action(action) ? actions[action].id : NULL;
}

const char *moh_mkb_action_default(MohMkbAction action) {
    return valid_action(action) ? actions[action].defaults : NULL;
}

int moh_mkb_controls_set(MohMkbControls *controls, MohMkbAction action,
                         const char *value) {
    PSXModInputBindings parsed;
    if (!controls || !valid_action(action) ||
        !psx_mod_parse_input_bindings(value, &parsed)) return 0;
    for (int i = 0; i < parsed.count; ++i)
        if (parsed.input[i].kind == PSX_MOD_BIND_WHEEL &&
            action != MOH_MKB_NEXT_WEAPON && action != MOH_MKB_PREV_WEAPON)
            return 0;
    controls->bindings[action] = parsed;
    return 1;
}

void moh_mkb_controls_reset(MohMkbControls *controls) {
    if (!controls) return;
    memset(controls, 0, sizeof(*controls));
    for (int i = 0; i < MOH_MKB_ACTION_COUNT; ++i)
        (void)moh_mkb_controls_set(controls, (MohMkbAction)i, actions[i].defaults);
}

int moh_mkb_action_down(const MohMkbControls *controls, MohMkbAction action,
                        const uint8_t *keys, unsigned key_count, uint32_t buttons) {
    if (!controls || !valid_action(action)) return 0;
    const PSXModInputBindings *bindings = &controls->bindings[action];
    for (int i = 0; i < bindings->count; ++i) {
        const PSXModInputBinding binding = bindings->input[i];
        if (binding.kind == PSX_MOD_BIND_KEY && keys &&
            (unsigned)binding.code < key_count && keys[binding.code]) return 1;
        if (binding.kind == PSX_MOD_BIND_MOUSE &&
            (buttons & (1u << (binding.code - 1)))) return 1;
    }
    return 0;
}

void moh_mkb_controls_wheel(MohMkbControls *controls, double dy) {
    if (!controls || !isfinite(dy)) return;
    double whole;
    controls->wheel_remainder = modf(controls->wheel_remainder +
                                     fmax(-16.0, fmin(16.0, dy)), &whole);
    if (!whole) return;
    const int direction = whole > 0 ? 1 : -1;
    for (int action = MOH_MKB_NEXT_WEAPON; action <= MOH_MKB_PREV_WEAPON; ++action) {
        const PSXModInputBindings *bindings = &controls->bindings[action];
        for (int i = 0; i < bindings->count; ++i) {
            if (bindings->input[i].kind == PSX_MOD_BIND_WHEEL &&
                bindings->input[i].code == direction) {
                int steps = controls->wheel_steps + (int)fabs(whole) *
                            (action == MOH_MKB_NEXT_WEAPON ? 1 : -1);
                controls->wheel_steps = steps < -16 ? -16 : steps > 16 ? 16 : steps;
                break;
            }
        }
    }
}

int moh_mkb_controls_take_wheel(MohMkbControls *controls) {
    if (!controls || !controls->wheel_steps) return 0;
    const int step = controls->wheel_steps > 0 ? 1 : -1;
    controls->wheel_steps -= step;
    return step;
}
