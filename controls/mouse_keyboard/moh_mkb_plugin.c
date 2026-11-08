#include "moh_mkb.h"
#include "mod_plugins.h"
#include "cpu_state.h"
#include "psx_keybinds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOH_MKB_PLUGIN_ID "moh.input.mouse-keyboard"
#define MOH_MKB_FEATURE_ID "mouse-keyboard"

static MohMkbControls controls;
static MohMkbMouse mouse;
static MohMkbAim aim;
static MohMkbCamera camera_state;
static int active, mouse_captured, profile_context, profile_installed;
static int scope_owned;
static int transform_installed, mouse_policy_installed;
static uint32_t mouse_strafe = 128;
static int look_sensitivity, aim_sensitivity, aimed_zoom;
static int selecting_slot = -1, selection_age, cycle_held[2];

static int option_integer(const char *id, int fallback, int minimum, int maximum) {
    char value[24], *end;
    if (!psx_mod_option_value(MOH_MKB_PLUGIN_ID, MOH_MKB_FEATURE_ID,
                              id, value, sizeof(value))) return fallback;
    const long n = strtol(value, &end, 10);
    return end != value && !*end && n >= minimum && n <= maximum ? (int)n : fallback;
}

static SDL_Scancode control_code(PSXModInputBinding binding) {
    return binding.kind == PSX_MOD_BIND_MOUSE ? PSXKB_MOUSE_SC(binding.code) :
           binding.kind == PSX_MOD_BIND_KEY ? (SDL_Scancode)binding.code : SDL_SCANCODE_UNKNOWN;
}

static int raw_down(PSXModInputBinding binding, const uint8_t *keys, uint32_t buttons) {
    if (binding.kind == PSX_MOD_BIND_KEY) return keys && keys[binding.code];
    if (binding.kind == PSX_MOD_BIND_MOUSE) return (buttons & (1u << (binding.code - 1))) != 0;
    return 0;
}

static void contextual_controls(int suppress_held) {
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    const uint32_t buttons = SDL_GetMouseState(NULL, NULL);
    for (int sc = 1; sc < SDL_NUM_SCANCODES; ++sc)
        if (!keys || !keys[sc]) psx_keybinds_release_control(1, (SDL_Scancode)sc);
    for (int button = 1; button <= 5; ++button)
        if (!(buttons & (1u << (button - 1))))
            psx_keybinds_release_control(1, PSXKB_MOUSE_SC(button));
    if (!suppress_held) return;
    for (int action = 0; action < MOH_MKB_ACTION_COUNT; ++action)
        for (int j = 0; j < controls.bindings[action].count; ++j) {
            PSXModInputBinding binding = controls.bindings[action].input[j];
            SDL_Scancode sc = control_code(binding);
            if (sc && raw_down(binding, keys, buttons)) psx_keybinds_suppress_control(1, sc);
        }
}

static int action_down(MohMkbAction action, const uint8_t *keys, uint32_t buttons) {
    const PSXModInputBindings *bindings = &controls.bindings[action];
    for (int i = 0; i < bindings->count; ++i) {
        PSXModInputBinding binding = bindings->input[i];
        SDL_Scancode sc = control_code(binding);
        if (sc && !psx_keybinds_control_suppressed(1, sc) && raw_down(binding, keys, buttons))
            return 1;
    }
    return 0;
}

static void clear_transient(void) {
    moh_mkb_mouse_reset(&mouse);
    controls.wheel_steps = 0;
    controls.wheel_remainder = 0;
    mouse_strafe = 128;
    selecting_slot = -1;
    selection_age = cycle_held[0] = cycle_held[1] = 0;
    aim.held = 0;
    aim.amount = 0;
    aim.reticle_stack = 0;
    memset(&camera_state, 0, sizeof(camera_state));
}

static void refresh_profile(void) {
    static const PsxPlayerBinds empty_profile = {0};
    const int context = moh_mkb_guest_context();
    contextual_controls(profile_context >= 0 && profile_context != context);
    if (profile_context == context) return;
    clear_transient();
    if (context && !profile_installed) {
        profile_installed = psx_keybinds_set_session_player(1, &empty_profile);
        if (!profile_installed) { profile_context = -1; return; }
    } else if (!context && profile_installed) {
        psx_keybinds_clear_session_players();
        profile_installed = 0;
    }
    profile_context = context;
}

static void on_vblank(void) {
    if (!active) return;
    refresh_profile();
    if (!mouse_captured || profile_context != 1) return;
    if (selecting_slot >= 0) {
        if (++selection_age > 180) selecting_slot = -1;
        return;
    }
    const int direction = moh_mkb_controls_take_wheel(&controls);
    if (!direction) return;
    const int slot = moh_mkb_guest_weapon_slot(direction);
    if (slot == -2) controls.wheel_steps += direction;
    else if (slot >= 0) { selecting_slot = slot; selection_age = 1; }
}

static int mouse_eligible(uint32_t native_buttons) {
    return active && profile_installed && (native_buttons & 0x0008u) &&
           moh_mkb_guest_eligible();
}

static void mouse_event(const PSXModMouseEvent *event) {
    if (!event || event->struct_size != sizeof(*event)) return;
    if (event->type == PSX_MOD_MOUSE_RESET || event->type == PSX_MOD_MOUSE_ACQUIRED ||
        event->type == PSX_MOD_MOUSE_HOLD_RELEASE) {
        clear_transient();
        mouse_captured = event->type == PSX_MOD_MOUSE_ACQUIRED;
    } else if (event->type == PSX_MOD_MOUSE_MOTION && mouse_captured) {
        moh_mkb_mouse_motion(&mouse, event->dx, event->dy);
    } else if (event->type == PSX_MOD_MOUSE_WHEEL && mouse_captured) {
        moh_mkb_controls_wheel(&controls, event->dy);
    }
}

static void mouse_sample(uint64_t now, PSXModMouseOutput *output) {
    (void)now;
    if (!output || output->struct_size != sizeof(*output)) return;
    output->override_right = 1;
    output->rx = mouse_strafe;
    output->ry = 128;
}

static int transform_pad(const PSXModPadFrame *frame, PSXModPadOutput *out) {
    if (!active || !profile_installed || !frame || !out ||
        frame->struct_size != sizeof(*frame) || out->struct_size != sizeof(*out)) return 0;
    const int context = moh_mkb_guest_context();
    if (!context || context != profile_context) return 0;
    aim.held = 0;
    uint8_t keys[SDL_NUM_SCANCODES];
    psx_keybinds_host_keys(SDL_GetKeyboardState(NULL), keys);
    const uint32_t buttons = SDL_GetMouseState(NULL, NULL);
    int down[MOH_MKB_ACTION_COUNT];
    for (int i = 0; i < MOH_MKB_ACTION_COUNT; ++i)
        down[i] = action_down((MohMkbAction)i, keys, buttons);
    /* The transform supplies a DualShock only in the verified context. The
     * framework leaves the saved controller mode and physical pad untouched. */
    if (out->type == PSX_MOD_PAD_DIGITAL) {
        out->buttons = frame->buttons;
        out->lx = frame->lx; out->ly = frame->ly; out->rx = frame->rx; out->ry = frame->ry;
    }
    if (context == 2) {
        if (down[MOH_MKB_MENU_UP] && !down[MOH_MKB_MENU_DOWN]) out->buttons &= ~0x0010u;
        if (down[MOH_MKB_MENU_RIGHT] && !down[MOH_MKB_MENU_LEFT]) out->buttons &= ~0x0020u;
        if (down[MOH_MKB_MENU_DOWN] && !down[MOH_MKB_MENU_UP]) out->buttons &= ~0x0040u;
        if (down[MOH_MKB_MENU_LEFT] && !down[MOH_MKB_MENU_RIGHT]) out->buttons &= ~0x0080u;
        if (down[MOH_MKB_MENU_CONFIRM]) out->buttons &= ~0x4000u;
        if (down[MOH_MKB_MENU_BACK]) out->buttons &= ~0x1000u;
        if (down[MOH_MKB_PAUSE]) out->buttons &= ~0x0008u;
        if (down[MOH_MKB_MENU_UP] || down[MOH_MKB_MENU_DOWN] ||
            down[MOH_MKB_MENU_LEFT] || down[MOH_MKB_MENU_RIGHT])
            out->lx = out->ly = out->rx = out->ry = 128;
        mouse_strafe = 128;
        return 1;
    }
    out->type = PSX_MOD_PAD_DUALSHOCK;
    if (down[MOH_MKB_FIRE]) out->buttons &= ~0x4000u;
    if (mouse_captured) {
        const int kind = moh_mkb_guest_aim_kind();
        if (kind) {
            aim.held = down[MOH_MKB_AIM] || !(out->buttons & 0x0200u);
            if (kind == 1) out->buttons |= 0x0200u;
            else if (aim.held) out->buttons &= ~0x0200u;
        }
        if (kind != 1) aim.amount = 0;
    }
    if (down[MOH_MKB_RELOAD] || down[MOH_MKB_USE]) out->buttons &= ~0x8000u;
    if (down[MOH_MKB_JUMP]) out->buttons &= ~0x1000u;
    if (down[MOH_MKB_CROUCH]) out->buttons &= ~0x0100u;
    if (down[MOH_MKB_PAUSE]) out->buttons &= ~0x0008u;
    if (down[MOH_MKB_OBJECTIVES]) out->buttons &= ~0x0001u;
    for (int i = 0; i < 2; ++i) {
        const int held = down[MOH_MKB_NEXT_WEAPON + i];
        if (mouse_captured && held && !cycle_held[i]) {
            const int next = controls.wheel_steps + (i ? -1 : 1);
            controls.wheel_steps = next < -16 ? -16 : next > 16 ? 16 : next;
        }
        cycle_held[i] = held;
    }
    if (mouse_captured && selecting_slot >= 0 && selection_age <= 6)
        out->buttons &= ~0x2000u;
    const int moving = down[MOH_MKB_LEFT] || down[MOH_MKB_RIGHT] ||
                       down[MOH_MKB_FORWARD] || down[MOH_MKB_BACKWARD];
    if (moving) {
        uint8_t x, y;
        moh_mkb_stick_pair(down[MOH_MKB_LEFT], down[MOH_MKB_RIGHT],
                           down[MOH_MKB_FORWARD], down[MOH_MKB_BACKWARD], &x, &y);
        out->lx = 128; out->ly = y; out->rx = x;
    }
    mouse_strafe = out->rx;
    if (mouse_captured) {
        out->lx = 128;
        if (moving) out->rx = 128;
    }
    return 1;
}

static void refresh_options(void) {
    moh_mkb_controls_reset(&controls);
    for (int i = 0; i < MOH_MKB_ACTION_COUNT; ++i) {
        char id[64], value[96];
        snprintf(id, sizeof(id), "bind_%s", moh_mkb_action_id((MohMkbAction)i));
        if (psx_mod_option_value(MOH_MKB_PLUGIN_ID, MOH_MKB_FEATURE_ID,
                                 id, value, sizeof(value)))
            (void)moh_mkb_controls_set(&controls, (MohMkbAction)i, value);
    }
    look_sensitivity = option_integer("look_sensitivity", 5, 1, 10);
    aim_sensitivity = option_integer("aim_sensitivity", 4, 1, 10);
    aimed_zoom = option_integer("aimed_zoom", 125, 100, 250);
    aim.zoom_percent = aimed_zoom;
}

static int option_changed(const char *id, const char *value) {
    (void)value;
    if (!id) return 0;
    if (!strncmp(id, "bind_", 5)) {
        int known = 0;
        for (int i = 0; i < MOH_MKB_ACTION_COUNT; ++i)
            if (!strcmp(id + 5, moh_mkb_action_id((MohMkbAction)i))) known = 1;
        if (!known) return 0;
    } else if (strcmp(id, "look_sensitivity") && strcmp(id, "aim_sensitivity") &&
               strcmp(id, "aimed_zoom")) return 0;
    if (active) contextual_controls(1);
    refresh_options();
    if (active) contextual_controls(1);
    clear_transient();
    return 1;
}

static void deactivate(void) {
    active = 0;
    if (transform_installed) (void)psx_mod_set_pad_transform(0, NULL);
    transform_installed = 0;
    if (profile_installed) psx_keybinds_clear_session_players();
    profile_installed = 0;
    if (mouse_policy_installed) {
        (void)psx_mod_set_local_mouse_persistent_capture(0);
        (void)psx_mod_set_local_mouse_capture_across_transitions(0);
    }
    mouse_policy_installed = 0;
    memset(&controls, 0, sizeof(controls));
    clear_transient();
    mouse_captured = 0;
    scope_owned = 0;
    profile_context = -1;
    look_sensitivity = aim_sensitivity = aimed_zoom = 0;
}

static void restored(void) {
    clear_transient();
    mouse_captured = 0;
    scope_owned = 0;
}

static void activate(void) {
    if (active) return;
    refresh_options();
    clear_transient();
    profile_context = 0;
    const PSXModPadTransform transform = {
        .struct_size = sizeof(transform),
        .allowed_types = PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_DIGITAL) |
                         PSX_MOD_PAD_TYPE_BIT(PSX_MOD_PAD_DUALSHOCK),
        .initial_type = PSX_MOD_PAD_DIGITAL,
        .transform = transform_pad,
    };
    const PSXModMousePolicy policy = {
        .struct_size = sizeof(policy), .hold_control = PSX_MOD_MOUSE_HOLD_NONE,
        .eligible = mouse_eligible, .event = mouse_event, .sample = mouse_sample,
    };
    if (!psx_mod_set_pad_transform(0, &transform)) { deactivate(); return; }
    transform_installed = 1;
    if (!psx_mod_set_local_mouse_policy(&policy)) { deactivate(); return; }
    mouse_policy_installed = 1;
    if (!psx_mod_set_local_mouse_persistent_capture(1)) { deactivate(); return; }
    active = 1;
    refresh_profile();
}

static void input_update(CPUState *cpu, uint32_t address) {
    if (address == 0x8007f300u && !camera_state.player)
        (void)moh_mkb_guest_update(cpu, &mouse, mouse_captured, look_sensitivity);
}

static void camera_update(CPUState *cpu, uint32_t address) {
    if (active && mouse_captured && address == 0x800473dcu &&
        !moh_mkb_guest_camera_update(cpu, &mouse, &aim, look_sensitivity,
                                     aim_sensitivity, &camera_state))
        memset(&camera_state, 0, sizeof(camera_state));
}

static int precise_view(CPUState *cpu, uint32_t address) {
    return active && mouse_captured && address == 0x80048eecu &&
           moh_mkb_guest_precise_view(cpu, camera_state.player);
}

static int prepared_pitch(CPUState *cpu, uint32_t address) {
    return active && mouse_captured && address == 0x8007badcu &&
           moh_mkb_guest_prepared_pitch(cpu, &camera_state);
}

static void aim_instruction(CPUState *cpu, uint32_t site) {
    if (!active || (!mouse_captured && !(site == 0x8007f360u && scope_owned))) return;
    switch (site) {
    case 0x8007b254u: case 0x8007b91cu: case 0x8007cb8cu:
        (void)moh_mkb_guest_firearm_hook(cpu, site, &aim); break;
    case 0x8007e06cu:
        (void)moh_mkb_guest_firearm_hook(cpu, site, &aim);
        (void)moh_mkb_guest_sniper_hook(cpu, site, scope_owned); break;
    case 0x8007a864u:
        if (moh_mkb_guest_sniper_hook(cpu, site, scope_owned)) scope_owned = 1;
        break;
    case 0x8007f360u:
        (void)moh_mkb_guest_sniper_hook(cpu, site, scope_owned);
        if (!mouse_captured || !aim.held) scope_owned = 0;
        break;
    case 0x8007f34cu:
    case 0x8007e1f0u: case 0x8007e1fcu: case 0x8007e218u: case 0x8007e87cu:
        (void)moh_mkb_guest_sniper_hook(cpu, site, scope_owned); break;
    case 0x8007cf14u: (void)moh_mkb_guest_tighten_reticle(cpu, &aim); break;
    case 0x80045524u: (void)moh_mkb_guest_shot_hook(cpu, &aim); break;
    default: (void)moh_mkb_guest_direct_adjust(cpu, site, &camera_state); break;
    }
}

static void weapon_selected(CPUState *cpu, uint32_t address) {
    if (active && mouse_captured && selecting_slot >= 0 && address == 0x8007a948u &&
        moh_mkb_guest_select_weapon(cpu, selecting_slot)) {
        selecting_slot = -1;
        selection_age = 0;
    }
}

PSX_MOD_CONSTRUCTOR(register_moh_mkb) {
    (void)psx_mod_register_activation_plugin(MOH_MKB_PLUGIN_ID, activate);
    (void)psx_mod_register_session_reset_plugin(MOH_MKB_PLUGIN_ID, deactivate);
    (void)psx_mod_register_savestate_plugin(MOH_MKB_PLUGIN_ID, restored);
    (void)psx_mod_register_vblank_plugin(MOH_MKB_PLUGIN_ID, on_vblank);
    (void)psx_mod_register_option_changed_plugin(MOH_MKB_PLUGIN_ID, option_changed);
    (void)psx_mod_register_function_entry_plugin(MOH_MKB_PLUGIN_ID, 0x8007f300u, input_update);
    (void)psx_mod_register_function_entry_plugin(MOH_MKB_PLUGIN_ID, 0x800473dcu, camera_update);
    (void)psx_mod_register_function_filter_plugin(MOH_MKB_PLUGIN_ID, 0x80048eecu, precise_view);
    (void)psx_mod_register_function_filter_plugin(MOH_MKB_PLUGIN_ID, 0x8007badcu, prepared_pitch);
    (void)psx_mod_register_instruction_plugin(MOH_MKB_PLUGIN_ID, 0x8007a948u,
                                               0x00408021u, weapon_selected);
    static const struct { uint32_t site, word; } sites[] = {
        {0x8007b254u, 0xae2200a4u}, {0x8007e06cu, 0x2c420191u},
        {0x8007b91cu, 0x1462001fu}, {0x8007cb8cu, 0x104000e3u},
        {0x8007cf14u, 0x0c014efdu}, {0x8007a864u, 0xae2201c8u},
        {0x8007f34cu, 0x30420020u}, {0x8007f360u, 0x30420020u},
        {0x8007e1f0u, 0x2484ff00u}, {0x8007e1fcu, 0x00621823u},
        {0x8007e218u, 0x00021180u}, {0x8007e87cu, 0x12570059u},
        {0x80045524u, 0x8e08020cu}, {0x80080134u, 0x10400005u},
        {0x8007e0e0u, 0xaec20234u}, {0x8007e10cu, 0xaec20234u},
        {0x8007e128u, 0xaec20238u}, {0x8007ecb0u, 0xaec20238u},
        {0x8007ecb8u, 0xaec20238u}, {0x8007ed04u, 0xaec20234u},
        {0x8007ee70u, 0xaec20238u}, {0x8007efb8u, 0xaec20234u},
    };
    for (unsigned i = 0; i < sizeof(sites) / sizeof(sites[0]); ++i)
        (void)psx_mod_register_instruction_plugin(MOH_MKB_PLUGIN_ID,
                                                   sites[i].site, sites[i].word,
                                                   aim_instruction);
}
