#include "psx_keybinds.h"
static uint8_t test_keys[SDL_NUM_SCANCODES];
static uint32_t test_buttons;
static const uint8_t *test_keyboard(int *count) {
    if (count) *count = SDL_NUM_SCANCODES;
    return test_keys;
}
#if defined(PSX_SDL3)
static uint32_t test_mouse(float *x, float *y) { (void)x; (void)y; return test_buttons; }
#else
static uint32_t test_mouse(int *x, int *y) { (void)x; (void)y; return test_buttons; }
#endif
#undef SDL_GetKeyboardState
#define SDL_GetKeyboardState test_keyboard
#undef SDL_GetMouseState
#define SDL_GetMouseState test_mouse
#include "moh_mkb_plugin.c"

static PSXModActivationCallback registered_activate, registered_reset;
static PSXModOptionChangedCallback registered_option;
static PSXModFunctionEntryCallback registered_update;
static PSXModFunctionEntryCallback registered_weapon;
static PSXModFunctionEntryCallback registered_camera, registered_aim;
static int registered_sites, registered_filters, test_aim_kind = 1, test_mounted;
static PSXModActivationCallback registered_restore, registered_vblank;
static PSXModPadTransform installed_transform;
static PSXModMousePolicy installed_mouse;
static uint8_t suppressed[PSXKB_MOUSE_SC_BASE + 6];
static int test_context = 1, session_profile, persistent_capture;
static int update_calls;
static const char *override_id, *override_value;
static int bad_id;

int psx_mod_register_activation_plugin(const char *id, PSXModActivationCallback cb) {
    if (strcmp(id, MOH_MKB_PLUGIN_ID)) bad_id = 1;
    registered_activate = cb; return 1;
}
int psx_mod_register_session_reset_plugin(const char *id, PSXModActivationCallback cb) {
    if (strcmp(id, MOH_MKB_PLUGIN_ID)) bad_id = 1;
    registered_reset = cb; return 1;
}
int psx_mod_register_savestate_plugin(const char *id, PSXModActivationCallback cb) {
    if (strcmp(id, MOH_MKB_PLUGIN_ID)) bad_id = 1;
    registered_restore = cb; return 1;
}
int psx_mod_register_vblank_plugin(const char *id, PSXModVBlankCallback cb) {
    if (strcmp(id, MOH_MKB_PLUGIN_ID)) bad_id = 1;
    registered_vblank = cb; return 1;
}
int psx_mod_register_option_changed_plugin(const char *id, PSXModOptionChangedCallback cb) {
    if (strcmp(id, MOH_MKB_PLUGIN_ID)) bad_id = 1;
    registered_option = cb; return 1;
}
int psx_mod_register_function_entry_plugin(const char *id, uint32_t address,
                                            PSXModFunctionEntryCallback cb) {
    if (strcmp(id, MOH_MKB_PLUGIN_ID)) bad_id = 1;
    if (address == 0x8007f300u) registered_update = cb;
    else if (address == 0x800473dcu) registered_camera = cb;
    else bad_id = 1;
    return 1;
}
int psx_mod_register_function_filter_plugin(const char *id, uint32_t address,
                                             PSXModFunctionFilterCallback cb) {
    if (strcmp(id, MOH_MKB_PLUGIN_ID) || !cb ||
        (address != 0x80048eecu && address != 0x8007badcu)) bad_id = 1;
    ++registered_filters;
    return 1;
}
int psx_mod_register_instruction_plugin(const char *id, uint32_t address, uint32_t word,
                                         PSXModFunctionEntryCallback cb) {
    if (strcmp(id, MOH_MKB_PLUGIN_ID)) bad_id = 1;
    if (address == 0x8007a948u && word == 0x00408021u) registered_weapon = cb;
    else { registered_aim = cb; ++registered_sites; }
    return 1;
}
int moh_mkb_guest_aim_kind(void) { return test_aim_kind; }
int moh_mkb_guest_mounted(void) { return test_mounted; }
int moh_mkb_guest_camera_update(CPUState *cpu, MohMkbMouse *m, const MohMkbAim *a,
                                int look, int scoped, MohMkbCamera *s) {
    (void)cpu; (void)m; (void)a; (void)look; (void)scoped; (void)s; return 1;
}
int moh_mkb_guest_precise_view(CPUState *cpu, uint32_t player) { (void)cpu; (void)player; return 1; }
int moh_mkb_guest_prepared_pitch(CPUState *cpu, MohMkbCamera *s) { (void)cpu; (void)s; return 1; }
int moh_mkb_guest_firearm_hook(CPUState *cpu, uint32_t site, MohMkbAim *a) {
    (void)cpu; (void)site; (void)a; return 1;
}
int moh_mkb_guest_sniper_hook(CPUState *cpu, uint32_t site, int owned) {
    (void)cpu; (void)site; (void)owned; return 1;
}
int moh_mkb_guest_direct_adjust(CPUState *cpu, uint32_t site, const MohMkbCamera *s) {
    (void)cpu; (void)site; (void)s; return 1;
}
int moh_mkb_guest_tighten_reticle(CPUState *cpu, MohMkbAim *a) { (void)cpu; (void)a; return 1; }
int moh_mkb_guest_shot_hook(CPUState *cpu, const MohMkbAim *a) { (void)cpu; (void)a; return 1; }
int moh_mkb_guest_weapon_slot(int direction) { return direction > 0 ? 2 : 1; }
int moh_mkb_guest_select_weapon(CPUState *cpu, int slot) {
    if (!cpu) return 0;
    cpu->gpr[2] = (uint32_t)slot; return 1;
}
int moh_mkb_guest_update(CPUState *cpu, MohMkbMouse *state, int captured, int gain) {
    (void)cpu; (void)state; (void)gain;
    ++update_calls;
    return captured;
}
int moh_mkb_guest_context(void) { return test_context; }
int moh_mkb_guest_eligible(void) { return test_context == 1; }
int psx_keybinds_set_session_player(int player, const PsxPlayerBinds *profile) {
    if (player != 1 || !profile) return 0;
    session_profile = 1; return 1;
}
void psx_keybinds_clear_session_players(void) { session_profile = 0; }
void psx_keybinds_suppress_control(int player, SDL_Scancode sc) {
    (void)player; suppressed[sc] = 1;
}
void psx_keybinds_release_control(int player, SDL_Scancode sc) {
    (void)player; suppressed[sc] = 0;
}
int psx_keybinds_control_suppressed(int player, SDL_Scancode sc) {
    (void)player; return suppressed[sc];
}
void psx_keybinds_host_keys(const uint8_t *input, uint8_t out[SDL_NUM_SCANCODES]) {
    for (unsigned i = 0; i < SDL_NUM_SCANCODES; ++i)
        out[i] = input && !suppressed[i] ? input[i] : 0;
}
int psx_mod_set_pad_transform(uint32_t player, const PSXModPadTransform *transform) {
    if (player) return 0;
    if (transform) installed_transform = *transform;
    else memset(&installed_transform, 0, sizeof(installed_transform));
    return 1;
}
int psx_mod_set_local_mouse_policy(const PSXModMousePolicy *policy) {
    if (!policy) return 0;
    installed_mouse = *policy; return 1;
}
int psx_mod_set_local_mouse_persistent_capture(int enabled) { persistent_capture = enabled; return 1; }
int psx_mod_set_local_mouse_capture_across_transitions(int enabled) { (void)enabled; return 1; }
int psx_mod_option_value(const char *package, const char *feature, const char *id,
                         char *out, uint32_t size) {
    if (strcmp(package, MOH_MKB_PLUGIN_ID) || strcmp(feature, MOH_MKB_FEATURE_ID))
        bad_id = 1;
    if (!override_id || strcmp(override_id, id)) return 0;
    snprintf(out, size, "%s", override_value);
    return 1;
}

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main(void) {
    CHECK(registered_activate && registered_reset && registered_option && registered_update && registered_weapon &&
          registered_camera && registered_aim && registered_sites == 22 && registered_filters == 2 &&
          registered_restore && registered_vblank && !bad_id);
    registered_activate();
    CHECK(active && session_profile && persistent_capture && installed_mouse.eligible &&
          installed_transform.transform);
    CHECK(look_sensitivity == 5 && aim_sensitivity == 4 && aimed_zoom == 125);
    CHECK(controls.bindings[MOH_MKB_FIRE].count == 2);
    override_id = "bind_fire"; override_value = "key:5,mouse:2,key:7";
    CHECK(registered_option("bind_fire", override_value));
    CHECK(controls.bindings[MOH_MKB_FIRE].count == 3);
    CHECK(controls.bindings[MOH_MKB_FIRE].input[1].code == 2);
    override_value = "wheel:up";
    CHECK(registered_option("bind_fire", override_value));
    CHECK(controls.bindings[MOH_MKB_FIRE].count == 2);
    override_id = "aimed_zoom"; override_value = "250";
    CHECK(registered_option("aimed_zoom", override_value) && aimed_zoom == 250);
    override_value = "251";
    CHECK(registered_option("aimed_zoom", override_value) && aimed_zoom == 125);
    CHECK(!registered_option("unknown", "1"));
    CHECK(!registered_option("bind_unknown", "key:1"));
    CHECK(!registered_option(NULL, "1"));
    CPUState cpu = {0};
    registered_update(&cpu, 0x8007f304u);
    CHECK(update_calls == 0);
    registered_update(&cpu, 0x8007f300u);
    CHECK(update_calls == 1 && !mouse_captured);

    override_id = NULL;
    CHECK(registered_option("bind_fire", "mouse:1,key:40"));
    CHECK(installed_mouse.eligible(0xffffu));
    CHECK(!installed_mouse.eligible(0xfff7u));
    PSXModMouseEvent event = {.struct_size = sizeof(event), .type = PSX_MOD_MOUSE_ACQUIRED};
    installed_mouse.event(&event);
    CHECK(mouse_captured);
    event.type = PSX_MOD_MOUSE_MOTION; event.dx = 8; event.dy = -3;
    installed_mouse.event(&event);
    CHECK(mouse.pending_x == 8 && mouse.pending_y == -3);
    event.type = PSX_MOD_MOUSE_WHEEL; event.dy = -1;
    installed_mouse.event(&event);
    registered_vblank();
    CHECK(selecting_slot == 2 && selection_age == 1);
    PSXModPadFrame frame = {.struct_size = sizeof(frame), .buttons = 0xffff,
        .lx = 128, .ly = 128, .rx = 128, .ry = 128, .type = PSX_MOD_PAD_DIGITAL};
    PSXModPadOutput out = {.struct_size = sizeof(out), .buttons = 0xffff,
        .lx = 128, .ly = 128, .rx = 128, .ry = 128, .type = PSX_MOD_PAD_DIGITAL};
    test_buttons = 1; suppressed[PSXKB_MOUSE_SC(1)] = 1;
    CHECK(installed_transform.transform(&frame, &out));
    CHECK(out.type == PSX_MOD_PAD_DUALSHOCK && (out.buttons & 0x4000) && !(out.buttons & 0x2000));
    registered_weapon(&cpu, 0x8007a948u);
    CHECK(cpu.gpr[2] == 2 && selecting_slot == -1);
    event.type = PSX_MOD_MOUSE_WHEEL; event.dy = 1;
    installed_mouse.event(&event);
    registered_vblank();
    CHECK(selecting_slot == 1);
    registered_weapon(&cpu, 0x8007a948u);
    CHECK(cpu.gpr[2] == 1 && selecting_slot == -1);
    suppressed[PSXKB_MOUSE_SC(1)] = 0;
    CHECK(installed_transform.transform(&frame, &out) && !(out.buttons & 0x4000));
    test_buttons = 1u << 2; out.buttons = 0xffff;
    CHECK(installed_transform.transform(&frame, &out) && (out.buttons & 0x0200));
    CHECK(aim.held);
    test_aim_kind = 2; out.buttons = 0xffff;
    CHECK(installed_transform.transform(&frame, &out) && !(out.buttons & 0x0200));
    registered_aim(&cpu, 0x8007a864u);
    CHECK(scope_owned);
    event.type = PSX_MOD_MOUSE_HOLD_RELEASE;
    installed_mouse.event(&event);
    CHECK(!mouse_captured && scope_owned);
    registered_aim(&cpu, 0x8007f360u);
    CHECK(!scope_owned);
    event.type = PSX_MOD_MOUSE_ACQUIRED;
    installed_mouse.event(&event);
    test_aim_kind = 1;
    test_buttons = 0; out.buttons = 0xffff;
    test_keys[SDL_SCANCODE_W] = test_keys[SDL_SCANCODE_D] = 1;
    CHECK(installed_transform.transform(&frame, &out));
    CHECK(out.ly == 0 && out.rx == 128 && mouse_strafe == 255);
    test_mounted = 1; out.lx = out.ly = out.rx = out.ry = 128;
    CHECK(installed_transform.transform(&frame, &out));
    CHECK(out.lx == 255 && out.ly == 0 && out.rx == 128 && mouse_strafe == 128);
    test_mounted = 0;
    CHECK(installed_transform.transform(&frame, &out));
    PSXModMouseOutput output = {.struct_size = sizeof(output)};
    installed_mouse.sample(0, &output);
    CHECK(output.override_right && output.rx == 255 && output.ry == 128);
    registered_restore();
    CHECK(!mouse_captured && mouse.pending_x == 0);
    test_context = 0; registered_vblank();
    CHECK(!session_profile && !installed_mouse.eligible(0xffff));
    CHECK(!installed_transform.transform(&frame, &out));
    memset(test_keys, 0, sizeof(test_keys));
    registered_vblank();
    test_context = 2; registered_vblank();
    test_keys[SDL_SCANCODE_E] = 1; out.buttons = 0xffff;
    out.type = PSX_MOD_PAD_DIGITAL;
    CHECK(installed_transform.transform(&frame, &out));
    CHECK(!(out.buttons & 0x4000) && out.type == PSX_MOD_PAD_DIGITAL);
    test_keys[SDL_SCANCODE_UP] = 1;
    out.lx = 0; out.ly = 255; out.rx = 0; out.ry = 255;
    CHECK(installed_transform.transform(&frame, &out));
    CHECK(!(out.buttons & 0x0010) && out.lx == 128 && out.ly == 128 &&
          out.rx == 128 && out.ry == 128);
    test_keys[SDL_SCANCODE_UP] = 0;
    test_context = 1; registered_vblank();
    CHECK(suppressed[SDL_SCANCODE_E]);
    out.buttons = 0xffff;
    CHECK(installed_transform.transform(&frame, &out) && (out.buttons & 0x8000));
    test_keys[SDL_SCANCODE_E] = 0; registered_vblank();
    CHECK(!suppressed[SDL_SCANCODE_E]);
    registered_reset();
    CHECK(look_sensitivity == 0 && controls.bindings[MOH_MKB_FIRE].count == 0 &&
          !session_profile && !persistent_capture && !installed_transform.transform);
    CHECK(!bad_id);
    return 0;
}
