#include "moh_mkb.h"
#include "cpu_state.h"
#include <stdio.h>
#include <string.h>

static unsigned char ram[0x200000];
static int started = 1, xr_active, writes;
int g_psx_render_pass_active;
static uint32_t offset(uint32_t address) { return address & 0x1fffffu; }
uint8_t psx_mod_read_byte(uint32_t address) { return ram[offset(address)]; }
uint16_t psx_mod_read_half(uint32_t address) {
    uint32_t i = offset(address);
    return (uint16_t)(ram[i] | (ram[i + 1] << 8));
}
uint32_t psx_mod_read_word(uint32_t address) {
    uint32_t i = offset(address);
    return (uint32_t)ram[i] | ((uint32_t)ram[i+1] << 8) |
           ((uint32_t)ram[i+2] << 16) | ((uint32_t)ram[i+3] << 24);
}
void psx_mod_write_word(uint32_t address, uint32_t value) {
    uint32_t i = offset(address); ++writes;
    for (int n = 0; n < 4; ++n) ram[i+n] = (uint8_t)(value >> (8*n));
}
void psx_mod_write_half(uint32_t address, uint16_t value) {
    uint32_t i = offset(address); ++writes;
    ram[i] = (uint8_t)value; ram[i + 1] = (uint8_t)(value >> 8);
}
int psx_mod_game_started(void) { return started; }
int psx_openxr_session_active(void) { return xr_active; }
static int guest_calls;
uint32_t psx_mod_call_guest(CPUState *cpu, uint32_t function, uint32_t ret,
                            uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)cpu; (void)a0; (void)a1; (void)a3;
    if (function != 0x8007badcu || ret != 0x80080170u) return 0;
    ++guest_calls;
    psx_mod_write_word(a2 + 0x28u, psx_mod_read_word(a2 + 0x28u) + 0x10000u);
    return 0x55u;
}

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main(void) {
    const uint32_t player = 0x800ee860u, input = player + 0x3ccu;
    /* Synthetic zero-filled code with FNV collisions exercises the matching
     * branch without distributing any original game executable bytes. */
    const unsigned char update_tail[] = {0x03,0x0f,0xf9,0x4d,0x80};
    const unsigned char caller_tail[] = {0x00,0xe1,0x30,0x81,0xb6};
    memcpy(ram + 0x7f300 + 0xe9c - 5, update_tail, 5);
    memcpy(ram + 0x7a6b0 + 0xca0 - 5, caller_tail, 5);
    psx_mod_write_word(0x8009d654u, player);
    psx_mod_write_word(player + 8, 1);
    psx_mod_write_word(player + 0x388, input);
    psx_mod_write_word(input + 0x1c8, 7);
    ram[offset(input + 0x55)] = 12;
    psx_mod_write_word(player + 0x228, 0x00010000);
    psx_mod_write_word(player + 0x22c, 0x04000000);
    psx_mod_write_word(input + 0x178, 0x00020000);
    psx_mod_write_word(input + 0x184, 0x04100000);
    CPUState cpu = {0};
    cpu.gpr[4] = player; cpu.gpr[5] = 0; cpu.gpr[31] = 0x8007af58u;
    CHECK(moh_mkb_guest_eligible());
    psx_mod_write_word(0x8009d654u, 0);
    CHECK(moh_mkb_guest_context() == 2 && !moh_mkb_guest_eligible());
    started = 0;
    CHECK(moh_mkb_guest_context() == 0);
    started = 1;
    psx_mod_write_word(0x8008ce50u, 0x27bdffe0u);
    psx_mod_write_word(0x80045908u, 0x0c019647u);
    CHECK(moh_mkb_guest_context() == 0);
    psx_mod_write_word(0x8008ce50u, 0);
    psx_mod_write_word(0x80045908u, 0);
    psx_mod_write_word(0x8009d654u, player);
    psx_mod_write_word(0x8009a61cu, 1);
    CHECK(moh_mkb_guest_context() == 2 && !moh_mkb_guest_eligible());
    ram[offset(input + 0x55)] = 5;
    CHECK(moh_mkb_guest_context() == 2 && !moh_mkb_guest_eligible());
    ram[offset(input + 0x55)] = 12;
    psx_mod_write_word(0x8009a61cu, 0);
    psx_mod_write_word(0x8009e874u, 3);
    ram[offset(input + 0x56)] = 10;
    ram[offset(input + 0x58)] = 0xff; ram[offset(input + 0x59)] = 0xff;
    ram[offset(input + 0x5a)] = 2;
    CHECK(moh_mkb_guest_weapon_slot(1) == 2);
    CHECK(moh_mkb_guest_weapon_slot(-1) == 2);
    cpu.gpr[17] = input; cpu.gpr[18] = player;
    CHECK(moh_mkb_guest_select_weapon(&cpu, 2) && cpu.gpr[2] == 2);
    CHECK(!moh_mkb_guest_select_weapon(&cpu, 1));
    psx_mod_write_word(input + 0x13c, 1);
    CHECK(moh_mkb_guest_weapon_slot(1) == -2);
    psx_mod_write_word(input + 0x13c, 0);
    MohMkbMouse mouse = {0};
    moh_mkb_mouse_motion(&mouse, 10, 2);
    writes = 0;
    g_psx_render_pass_active = 1;
    CHECK(!moh_mkb_guest_update(&cpu, &mouse, 1, 5));
    CHECK(mouse.pending_x == 10 && writes == 0);
    g_psx_render_pass_active = 0;
    CHECK(moh_mkb_guest_update(&cpu, &mouse, 1, 5));
    CHECK(writes == 4 && mouse.pending_x == 0 && mouse.pending_y == 0);
    CHECK(psx_mod_read_word(player + 0x22c) == 0x04050000);
    CHECK(psx_mod_read_word(input + 0x184) == 0x04150000);
    CHECK(psx_mod_read_word(player + 0x228) == 0);
    CHECK(psx_mod_read_word(input + 0x178) == 0x00010000);

    /* Captured pistol and Thompson states use the same native aim branch. */
    ram[offset(input + 0x55)] = 9;
    moh_mkb_mouse_motion(&mouse, 1, 0);
    CHECK(moh_mkb_guest_update(&cpu, &mouse, 1, 5));
    ram[offset(input + 0x55)] = 1;
    psx_mod_write_word(input + 0x1c8, 8);
    moh_mkb_mouse_motion(&mouse, 1, 0);
    CHECK(moh_mkb_guest_update(&cpu, &mouse, 1, 5));
    psx_mod_write_word(input + 0x1c8, 7);
    ram[offset(input + 0x55)] = 12;
    psx_mod_write_word(input + 0x1c8, 8);
    moh_mkb_mouse_motion(&mouse, 1, 0);
    CHECK(moh_mkb_guest_update(&cpu, &mouse, 1, 5) && mouse.pending_x == 0);
    ram[offset(input + 0x55)] = 3;
    psx_mod_write_word(input + 0x1c8, 9);
    moh_mkb_mouse_motion(&mouse, 1, 0);
    CHECK(moh_mkb_guest_update(&cpu, &mouse, 1, 5) && moh_mkb_guest_aim_kind() == 1);
    ram[offset(input + 0x55)] = 13;
    moh_mkb_mouse_motion(&mouse, 1, 0);
    CHECK(!moh_mkb_guest_update(&cpu, &mouse, 1, 5) && mouse.pending_x == 0);
    ram[offset(input + 0x55)] = 12;
    psx_mod_write_word(input + 0x1c8, 7);

    /* Hash-collision fixtures exercise the four SLUS-00974 adapters without
     * including original game code. No site is registered by these tests. */
    const unsigned char ray_tail[] = {199, 203, 0, 195, 221};
    const unsigned char weapon_tail[] = {184, 86, 0, 23, 100};
    const unsigned char reticle_tail[] = {46, 140, 2, 33, 204};
    memcpy(ram + 0x7e034 + 0x1058 - 5, ray_tail, 5);
    memcpy(ram + 0x7b8f4 + 0x160 - 5, weapon_tail, 5);
    memcpy(ram + 0x7caf8 + 0x428 - 5, reticle_tail, 5);
    MohMkbAim aim = {.held = 1, .zoom_percent = 250};
    cpu.gpr[18] = player; cpu.gpr[17] = input; cpu.gpr[2] = 400;
    CHECK(moh_mkb_guest_firearm_hook(&cpu, 0x8007b254u, &aim));
    CHECK(cpu.gpr[2] == 556);
    cpu.gpr[2] = 400;
    CHECK(moh_mkb_guest_firearm_hook(&cpu, 0x8007b254u, &aim));
    cpu.gpr[2] = 400;
    CHECK(moh_mkb_guest_firearm_hook(&cpu, 0x8007b254u, &aim));
    CHECK(cpu.gpr[2] == 1000 && aim.amount == 1);
    ram[offset(input + 0x55)] = 3;
    psx_mod_write_word(input + 0x1c8, 9);
    aim.amount = 0;
    cpu.gpr[2] = 400;
    CHECK(moh_mkb_guest_firearm_hook(&cpu, 0x8007b254u, &aim) && cpu.gpr[2] == 556);
    ram[offset(input + 0x55)] = 10;
    CHECK(moh_mkb_guest_aim_kind() == 1);
    cpu.gpr[2] = 400;
    CHECK(moh_mkb_guest_firearm_hook(&cpu, 0x8007b254u, &aim));
    ram[offset(input + 0x55)] = 12;
    psx_mod_write_word(input + 0x1c8, 7);
    aim.amount = 1;
    cpu.gpr[4] = player; cpu.gpr[19] = input; cpu.gpr[2] = 1000;
    CHECK(moh_mkb_guest_firearm_hook(&cpu, 0x8007e06cu, &aim));
    CHECK(cpu.gpr[2] == 400);
    cpu.gpr[16] = input; cpu.gpr[17] = player; cpu.gpr[3] = 1000;
    CHECK(moh_mkb_guest_firearm_hook(&cpu, 0x8007b91cu, &aim));
    CHECK(cpu.gpr[3] == 400);
    cpu.gpr[16] = player; cpu.gpr[6] = input; cpu.gpr[2] = 0;
    cpu.gpr[29] = 0x801ff000u;
    CHECK(moh_mkb_guest_firearm_hook(&cpu, 0x8007cb8cu, &aim));
    CHECK(cpu.gpr[2] == 0x20);
    const uint32_t packet = 0x800f2000u;
    const int16_t line[8][2] = {{0,-12},{0,-5},{0,12},{0,5},
                                {-18,0},{-8,0},{18,0},{8,0}};
    cpu.gpr[22] = packet + 64; cpu.gpr[19] = 256; cpu.gpr[20] = 120;
    for (unsigned p = 0; p < 8; ++p)
        for (unsigned axis = 0; axis < 2; ++axis)
            psx_mod_write_half(packet + 16u * (p / 2u) + 8u +
                               4u * (p % 2u) + 2u * axis,
                               (uint16_t)((axis ? 120 : 256) + line[p][axis]));
    psx_mod_write_half(packet + 10, 1);
    writes = 0;
    CHECK(!moh_mkb_guest_tighten_reticle(&cpu, &aim) && writes == 0);
    psx_mod_write_half(packet + 10, 108);
    CHECK(moh_mkb_guest_tighten_reticle(&cpu, &aim));
    CHECK(psx_mod_read_half(packet + 10) == 112 &&
          psx_mod_read_half(packet + 8 + 32) == 244);
    CHECK(!moh_mkb_guest_tighten_reticle(&cpu, &aim));
    aim.held = 0;
    CHECK(moh_mkb_guest_firearm_hook(&cpu, 0x8007cb8cu, &aim));
    CHECK(cpu.gpr[2] == 0);
    ram[offset(input + 0x55)] = 5;
    CHECK(!moh_mkb_guest_firearm_hook(&cpu, 0x8007cb8cu, &aim));
    ram[offset(input + 0x55)] = 12;
    ram[0x7caf8] ^= 1;
    CHECK(!moh_mkb_guest_firearm_hook(&cpu, 0x8007cb8cu, &aim));
    ram[0x7caf8] ^= 1;
    const unsigned char selector_tail[] = {230, 202, 0, 136, 163};
    memcpy(ram + 0x7ba20 + 0xbc - 5, selector_tail, 5);
    ram[offset(input + 0x55)] = 5;
    psx_mod_write_word(input, 0x20);
    cpu.gpr[18] = player; cpu.gpr[17] = input; cpu.gpr[2] = 2;
    CHECK(moh_mkb_guest_sniper_hook(&cpu, 0x8007a864u, 0) && cpu.gpr[2] == 7);
    cpu.gpr[20] = player; cpu.gpr[16] = input; cpu.gpr[2] = 0x120;
    CHECK(moh_mkb_guest_sniper_hook(&cpu, 0x8007f34cu, 1) && cpu.gpr[2] == 0x100);
    CHECK(psx_mod_read_word(input) == 0x20);
    cpu.gpr[2] = 0x20;
    CHECK(moh_mkb_guest_sniper_hook(&cpu, 0x8007f360u, 1) && cpu.gpr[2] == 0);
    /* Scope exit must not restore the uninitialised stationary-aim position. */
    psx_mod_write_word(input, 0);
    psx_mod_write_word(input + 4, 0x20);
    cpu.gpr[2] = 0x20;
    CHECK(moh_mkb_guest_sniper_hook(&cpu, 0x8007f360u, 1) && cpu.gpr[2] == 0);
    ram[offset(input + 0x55)] = 3;
    cpu.gpr[2] = 0x20;
    CHECK(!moh_mkb_guest_sniper_hook(&cpu, 0x8007f360u, 0));
    CHECK(moh_mkb_guest_sniper_hook(&cpu, 0x8007f360u, 1) && cpu.gpr[2] == 0);
    ram[offset(input + 0x55)] = 5;
    psx_mod_write_word(input + 4, 0);
    psx_mod_write_word(input, 0x20);
    cpu.gpr[4] = player; cpu.gpr[19] = input; cpu.gpr[2] = 2447;
    CHECK(moh_mkb_guest_sniper_hook(&cpu, 0x8007e06cu, 1) && cpu.gpr[2] == 400);
    cpu.gpr[22] = player; cpu.gpr[4] = 123;
    CHECK(moh_mkb_guest_sniper_hook(&cpu, 0x8007e1f0u, 1) && cpu.gpr[4] == 256);
    cpu.gpr[2] = 120; cpu.gpr[3] = 30;
    CHECK(moh_mkb_guest_sniper_hook(&cpu, 0x8007e1fcu, 1) && cpu.gpr[3] == 120);
    cpu.gpr[2] = 2447;
    CHECK(moh_mkb_guest_sniper_hook(&cpu, 0x8007e218u, 1) && cpu.gpr[2] == 400);
    cpu.gpr[18] = 12; cpu.gpr[23] = 34;
    CHECK(moh_mkb_guest_sniper_hook(&cpu, 0x8007e87cu, 1) && cpu.gpr[18] == 34);
    const unsigned char shot_tail[] = {148, 201, 1, 28, 86};
    memcpy(ram + 0x4532c + 0x39c - 5, shot_tail, 5);
    const uint32_t camera = 0x800f0000u, shot = 0x800f1000u;
    psx_mod_write_word(0x8009d638u, camera);
    psx_mod_write_word(camera + 8, 2);
    psx_mod_write_word(camera + 0x388, camera + 0x3cc);
    psx_mod_write_word(camera + 0x36c, 0x800473dcu);
    psx_mod_write_word(camera + 0x3d8, 0x30002);
    psx_mod_write_word(0x8009d678u + 8, player);
    psx_mod_write_word(camera + 0x20c, 0);
    psx_mod_write_word(camera + 0x210, 0);
    psx_mod_write_word(camera + 0x214, 0);
    psx_mod_write_word(input + 0x1a4, 0);
    psx_mod_write_word(input + 0x1a8, 0);
    psx_mod_write_word(input + 0x1ac, 65536);
    psx_mod_write_word(shot + 8, 0x13f6);
    cpu.gpr[16] = shot; cpu.gpr[18] = player; cpu.gpr[31] = 0x80045524u;
    CHECK(moh_mkb_guest_shot_hook(&cpu, &aim));
    CHECK(psx_mod_read_half(shot + 0x340) == 4096 &&
          psx_mod_read_half(shot + 0x348) == 4096 &&
          psx_mod_read_half(shot + 0x350) == 4096);
    ram[offset(input + 0x55)] = 12;
    psx_mod_write_word(input, 0);
    psx_mod_write_word(input + 0x1a4, 65536);
    psx_mod_write_word(input + 0x1ac, 0);
    CHECK(moh_mkb_guest_shot_hook(&cpu, &aim));
    CHECK(psx_mod_read_half(shot + 0x344) == 4096 &&
          psx_mod_read_word(shot + 0x228) == 0 &&
          psx_mod_read_word(shot + 0x22c) == 0x04000000);
    psx_mod_write_word(camera + 0x20c, 0x00020000);
    psx_mod_write_word(camera + 0x210, 0xfffd0000);
    psx_mod_write_word(input + 0x1a4, 0x00030000);
    psx_mod_write_word(input + 0x1a8, 0xfffd0000);
    CHECK(moh_mkb_guest_shot_hook(&cpu, &aim));
    CHECK(psx_mod_read_word(shot + 0x354) == 2 &&
          psx_mod_read_word(shot + 0x358) == 0xfffffffdu);
    psx_mod_write_word(camera + 0x20c, 0);
    psx_mod_write_word(camera + 0x210, 0);
    ram[offset(input + 0x55)] = 3;
    psx_mod_write_word(input + 0x1c8, 9);
    psx_mod_write_word(shot + 8, 0x13f3);
    writes = 0;
    CHECK(!moh_mkb_guest_shot_hook(&cpu, &aim) && writes == 0);
    ram[offset(input + 0x55)] = 12;
    psx_mod_write_word(input + 0x1c8, 7);
    psx_mod_write_word(shot + 8, 0x13f6);
    ram[offset(input + 0x55)] = 5;
    psx_mod_write_word(input, 0x20);
    cpu.gpr[18] = player + 4;
    CHECK(!moh_mkb_guest_shot_hook(&cpu, &aim));
    cpu.gpr[18] = player;
    psx_mod_write_word(0x8009d678u + 8, player + 4);
    CHECK(!moh_mkb_guest_shot_hook(&cpu, &aim));
    psx_mod_write_word(0x8009d678u + 8, player);
    const unsigned char view_tail[] = {107, 104, 3, 7, 62};
    const unsigned char view_caller_tail[] = {235, 50, 0, 38, 106};
    memcpy(ram + 0x48eec + 0x250 - 5, view_tail, 5);
    memcpy(ram + 0x4913c + 0x264 - 5, view_caller_tail, 5);
    psx_mod_write_word(player + 0x228, 0);
    psx_mod_write_word(player + 0x22c, 0);
    cpu.gpr[4] = camera + 0x4b0; cpu.gpr[5] = camera; cpu.gpr[31] = 0x800491e8u;
    CHECK(!moh_mkb_guest_precise_view(&cpu, 0));
    CHECK(moh_mkb_guest_precise_view(&cpu, player));
    CHECK(psx_mod_read_half(camera + 0x4cc) == 8192 &&
          psx_mod_read_half(camera + 0x4d4) == 5120 &&
          psx_mod_read_half(camera + 0x4dc) == 8192);
    psx_mod_write_word(camera + 0x468, 0x800);
    CHECK(!moh_mkb_guest_precise_view(&cpu, player));
    psx_mod_write_word(camera + 0x468, 0);
    const unsigned char camera_tail[] = {129, 21, 0, 112, 219};
    const unsigned char pitch_tail[] = {20, 148, 3, 24, 248};
    memcpy(ram + 0x473dc + 0x1e8 - 5, camera_tail, 5);
    memcpy(ram + 0x7badc + 0x130 - 5, pitch_tail, 5);
    MohMkbCamera camera_state = {0};
    MohMkbMouse camera_mouse = {0};
    moh_mkb_mouse_motion(&camera_mouse, 0, -10);
    aim.zoom_percent = 125;
    cpu.gpr[4] = camera; cpu.gpr[31] = 0x8006bdf4u;
    CHECK(moh_mkb_guest_camera_update(&cpu, &camera_mouse, &aim, 5, 4, &camera_state));
    CHECK(camera_state.player == player && camera_state.camera == camera && guest_calls == 1);
    CHECK(psx_mod_read_word(player + 0x228) == 4u * 65536u &&
          psx_mod_read_word(input + 0x178) == 4u * 65536u &&
          psx_mod_read_word(camera + 0x3cc + 0x1c) == 0x10000u);
    cpu.gpr[4] = player; cpu.gpr[5] = input; cpu.gpr[6] = camera + 0x3cc;
    cpu.gpr[31] = 0x80080170u;
    CHECK(moh_mkb_guest_prepared_pitch(&cpu, &camera_state) && cpu.gpr[2] == 0x55);
    CHECK(!moh_mkb_guest_prepared_pitch(&cpu, &camera_state));
    cpu.gpr[20] = player; cpu.gpr[16] = input;
    psx_mod_write_word(input + 0x178, 0);
    cpu.gpr[2] = 1;
    CHECK(moh_mkb_guest_direct_adjust(&cpu, 0x80080134u, &camera_state));
    CHECK(cpu.gpr[2] == 1 &&
          psx_mod_read_word(input + 0x178) == psx_mod_read_word(player + 0x228));
    const uint32_t adjust_sites[] = {0x8007e0e0u, 0x8007e10cu, 0x8007e128u,
        0x8007ecb0u, 0x8007ecb8u, 0x8007ed04u, 0x8007ee70u, 0x8007efb8u};
    cpu.gpr[22] = player; cpu.gpr[19] = input;
    for (unsigned i = 0; i < sizeof(adjust_sites) / sizeof(adjust_sites[0]); ++i) {
        cpu.gpr[2] = 123;
        CHECK(moh_mkb_guest_direct_adjust(&cpu, adjust_sites[i], &camera_state));
        CHECK(cpu.gpr[2] == 0);
    }
    cpu.gpr[2] = 123;
    CHECK(!moh_mkb_guest_direct_adjust(&cpu, 0x8007e08cu, &camera_state));
    CHECK(cpu.gpr[2] == 123);
    ram[offset(input + 0x55)] = 3;
    psx_mod_write_word(input + 0x1c8, 9);
    aim.amount = 0;
    moh_mkb_mouse_motion(&camera_mouse, 2, 0);
    cpu.gpr[4] = camera; cpu.gpr[31] = 0x8006bdf4u;
    CHECK(moh_mkb_guest_camera_update(&cpu, &camera_mouse, &aim, 5, 4, &camera_state));
    cpu.gpr[4] = camera + 0x4b0; cpu.gpr[5] = camera; cpu.gpr[31] = 0x800491e8u;
    CHECK(moh_mkb_guest_precise_view(&cpu, camera_state.player));
    ram[offset(input + 0x55)] = 5;
    psx_mod_write_word(input + 0x1c8, 7);
    psx_mod_write_word(input, 0);
    CHECK(!moh_mkb_guest_sniper_hook(&cpu, 0x8007e87cu, 1));
    ram[offset(input + 0x55)] = 12;
    cpu.gpr[4] = player; cpu.gpr[5] = 0; cpu.gpr[31] = 0x8007af58u;

    writes = 0; moh_mkb_mouse_motion(&mouse, 3, 2);
    cpu.gpr[4] = player + 4;
    CHECK(!moh_mkb_guest_update(&cpu, &mouse, 1, 5));
    CHECK(writes == 0 && mouse.pending_x == 3 && mouse.pending_y == 2);
    cpu.gpr[4] = player;
    writes = 0; moh_mkb_mouse_motion(&mouse, 3, 2);
    ram[offset(input + 0x55)] = 5;
    psx_mod_write_word(input + 0x1c8, 2); /* Scoped mode remains inert. */
    writes = 0;
    CHECK(moh_mkb_guest_eligible());
    CHECK(!moh_mkb_guest_update(&cpu, &mouse, 1, 5));
    CHECK(writes == 0 && mouse.pending_x == 0 && mouse.pending_y == 0);
    ram[offset(input + 0x55)] = 12;
    psx_mod_write_word(input + 0x1c8, 7);
    writes = 0;
    ram[0x7f300] ^= 1;
    moh_mkb_mouse_motion(&mouse, 3, 2);
    CHECK(!moh_mkb_guest_update(&cpu, &mouse, 1, 5));
    CHECK(writes == 0 && mouse.pending_x == 0);
    ram[0x7f300] ^= 1;
    xr_active = 1; moh_mkb_mouse_motion(&mouse, 3, 2);
    CHECK(!moh_mkb_guest_update(&cpu, &mouse, 1, 5));
    CHECK(writes == 0 && mouse.pending_x == 0);
    return 0;
}
