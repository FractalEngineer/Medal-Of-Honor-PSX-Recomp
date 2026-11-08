#include "moh_mkb.h"
#include <math.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main(void) {
    double zoom = 0;
    zoom = moh_mkb_zoom_step(zoom, 1);
    CHECK(fabs(moh_mkb_zoom_ease(zoom) - 7.0 / 27.0) < 1e-9);
    zoom = moh_mkb_zoom_step(zoom, 1);
    CHECK(fabs(moh_mkb_zoom_ease(zoom) - 20.0 / 27.0) < 1e-9);
    zoom = moh_mkb_zoom_step(zoom, 1);
    CHECK(zoom == 1);
    for (int i = 0; i < 3; ++i) zoom = moh_mkb_zoom_step(zoom, 0);
    CHECK(zoom == 0);
    CHECK(fabs(moh_mkb_look_gain(5, 4, 125, 0) - 0.5) < 1e-9);
    CHECK(fabs(moh_mkb_look_gain(5, 4, 125, 1) - 0.32) < 1e-9);
    CHECK(fabs(moh_mkb_look_gain(10, 10, 250, 1) - 0.4) < 1e-9);
    CHECK(moh_mkb_look_gain(0, 4, 125, 0) == 0);
    CHECK(moh_mkb_look_gain(5, 4, 125, NAN) == 0);
    const int32_t eye[3] = {65536, 0, 0};
    int16_t view[9];
    int32_t translation[3];
    moh_mkb_view_matrix(0, 0, eye, view, translation);
    CHECK(view[0] == 8192 && view[4] == 5120 && view[8] == 8192);
    CHECK(translation[0] == -2 && translation[1] == 0 && translation[2] == 0);
    const int32_t origin[3] = {0, 0, 0}, forward[3] = {0, 0, 65536};
    const int32_t upward[3] = {0, 65536, 0};
    int16_t basis[9];
    CHECK(moh_mkb_shot_matrix(origin, forward, basis));
    CHECK(basis[0] == 4096 && basis[4] == 4096 && basis[8] == 4096);
    CHECK(!moh_mkb_shot_matrix(origin, origin, basis));
    CHECK(!moh_mkb_shot_matrix(origin, upward, basis));
    int32_t shot_pitch, shot_yaw;
    CHECK(moh_mkb_shot_angles(origin, forward, &shot_pitch, &shot_yaw));
    CHECK(shot_pitch == 0 && shot_yaw == 0);
    const int32_t rightward[3] = {65536, 0, 0};
    CHECK(moh_mkb_shot_angles(origin, rightward, &shot_pitch, &shot_yaw));
    CHECK(shot_pitch == 0 && shot_yaw == 0x04000000);
    CHECK(!moh_mkb_shot_angles(origin, upward, &shot_pitch, &shot_yaw));
    CHECK(moh_mkb_mounted_yaw(0x01000000, 0x01000000, 0x10000) == 0x01010000);
    CHECK(moh_mkb_mounted_yaw(0x01000000, 0x01000000, 0x08000000) == 0x03940000);
    CHECK(moh_mkb_mounted_yaw(0x0aaa0000, 0x0aaa0000, -0x10000) == (int32_t)0xfaa90000u);
    MohMkbMouse mouse = {0};
    int64_t yaw = 0, pitch = 0;
    moh_mkb_mouse_motion(&mouse, 12.25, -3.5);
    moh_mkb_mouse_motion(&mouse, -2.25, 1.5);
    moh_mkb_mouse_take_scaled(&mouse, 0.5, &yaw, &pitch);
    CHECK(yaw == 327680 && pitch == 65536);
    moh_mkb_mouse_take_scaled(&mouse, 0.5, &yaw, &pitch);
    CHECK(yaw == 0 && pitch == 0);

    /* Sub-Q16 motion is retained, not discarded at each update. */
    moh_mkb_mouse_motion(&mouse, 1.0 / 65536.0, 0);
    moh_mkb_mouse_take_scaled(&mouse, 0.5, &yaw, &pitch);
    CHECK(yaw == 0);
    moh_mkb_mouse_motion(&mouse, 1.0 / 65536.0, 0);
    moh_mkb_mouse_take_scaled(&mouse, 0.5, &yaw, &pitch);
    CHECK(yaw == 1);
    moh_mkb_mouse_motion(&mouse, NAN, 1);
    CHECK(mouse.pending_x == 0 && mouse.pending_y == 0);

    uint8_t x, y;
    moh_mkb_stick_pair(1, 0, 1, 0, &x, &y);
    CHECK(x == 0 && y == 0);
    moh_mkb_stick_pair(1, 1, 1, 1, &x, &y);
    CHECK(x == 128 && y == 128);
    moh_mkb_stick_pair(0, 1, 0, 1, &x, &y);
    CHECK(x == 255 && y == 255);

    /* The game's native selector treats -1 as an unavailable inventory slot. */
    const int16_t inventory[] = {24, -1, 0, -1, 8};
    CHECK(moh_mkb_weapon_slot(inventory, 5, 0, 1, -1) == 2);
    CHECK(moh_mkb_weapon_slot(inventory, 5, 0, -1, -1) == 4);
    CHECK(moh_mkb_weapon_slot(inventory, 5, 4, 1, -1) == 0);
    CHECK(moh_mkb_weapon_slot(inventory, 5, 0, 0, 1) == -1);
    CHECK(moh_mkb_weapon_slot(inventory, 5, 0, 0, 2) == 2);
    CHECK(moh_mkb_weapon_slot(inventory, 9, 0, 1, -1) == -1);

    MohMkbAngles angles = {0x10000, 0x0fff0000, 0x20000, 0x0ffe0000};
    CHECK(moh_mkb_apply_look(&angles, 0x30000, 0x40000));
    CHECK(angles.player_yaw == 0x20000 && angles.input_yaw == 0x10000);
    CHECK(angles.player_pitch == 0x50000 && angles.input_pitch == 0x60000);
    CHECK(moh_mkb_apply_look(&angles, -0x40000, INT64_MAX));
    CHECK(angles.player_yaw == 0x0ffe0000 && angles.input_yaw == 0x0ffd0000);
    CHECK(angles.player_pitch == 0x02a90000 && angles.input_pitch == 0x02aa0000);
    CHECK(moh_mkb_apply_look(&angles, INT64_MIN, INT64_MIN));
    CHECK(angles.player_pitch == -0x02aa0000 && angles.input_pitch == -0x02a90000);
    angles.player_pitch = 0x02aa0001;
    CHECK(!moh_mkb_apply_look(&angles, 0, 0));
    return 0;
}
