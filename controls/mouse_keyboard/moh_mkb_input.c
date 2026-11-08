#include "moh_mkb.h"
#include <math.h>

double moh_mkb_zoom_step(double amount, int held) {
    const double next = fmax(0, fmin(1, amount + (held ? 1.0 : -1.0) / 3.0));
    return next < 1e-9 ? 0 : next > 1 - 1e-9 ? 1 : next;
}

double moh_mkb_zoom_ease(double amount) {
    amount = fmax(0, fmin(1, amount));
    return amount * amount * (3 - 2 * amount);
}

double moh_mkb_look_gain(int sensitivity, int aim_sensitivity,
                         int zoom_percent, double amount) {
    if (sensitivity < 1 || sensitivity > 10 || aim_sensitivity < 1 ||
        aim_sensitivity > 10 || zoom_percent < 100 || zoom_percent > 250 ||
        !isfinite(amount)) return 0;
    const double blend = moh_mkb_zoom_ease(amount);
    const double ratio = 1.0 + (zoom_percent / 100.0 - 1.0) * blend;
    return ((sensitivity * (1.0 - blend) + aim_sensitivity * blend) / 10.0) / ratio;
}

void moh_mkb_view_matrix(int32_t pitch, int32_t yaw, const int32_t eye[3],
                         int16_t rotation[9], int32_t translation[3]) {
    const double radians = 6.2831853071795864769 / (4096.0 * 65536.0);
    const double sp = sin(pitch * radians), cp = cos(pitch * radians);
    const double sy = sin(yaw * radians), cy = cos(yaw * radians);
    const double rows[9] = {cy, 0, -sy, sy * sp, cp, cy * sp,
                            sy * cp, -sp, cy * cp};
    for (unsigned row = 0; row < 3; ++row) {
        const double scale = row == 1 ? 5120.0 : 8192.0;
        double t = 0;
        for (unsigned col = 0; col < 3; ++col) {
            const unsigned at = 3 * row + col;
            rotation[at] = (int16_t)lround(rows[at] * scale);
            t -= (double)rotation[at] * eye[col] / (4096.0 * 65536.0);
        }
        translation[row] = (int32_t)lround(t);
    }
}

int moh_mkb_shot_matrix(const int32_t origin[3], const int32_t target[3],
                        int16_t rotation[9]) {
    if (!origin || !target || !rotation) return 0;
    double forward[3];
    for (unsigned i = 0; i < 3; ++i) forward[i] = (double)target[i] - origin[i];
    const double length = hypot(hypot(forward[0], forward[1]), forward[2]);
    if (length < 65536.0) return 0;
    for (unsigned i = 0; i < 3; ++i) forward[i] /= length;
    const double horizontal = hypot(forward[0], forward[2]);
    if (horizontal < 0.001) return 0;
    const double right[3] = {forward[2] / horizontal, 0, -forward[0] / horizontal};
    const double up[3] = {-forward[1] * forward[0] / horizontal, horizontal,
                          -forward[1] * forward[2] / horizontal};
    for (unsigned row = 0; row < 3; ++row) {
        rotation[3 * row] = (int16_t)lround(right[row] * 4096);
        rotation[3 * row + 1] = (int16_t)lround(up[row] * 4096);
        rotation[3 * row + 2] = (int16_t)lround(forward[row] * 4096);
    }
    return 1;
}

int moh_mkb_shot_angles(const int32_t origin[3], const int32_t target[3],
                        int32_t *pitch, int32_t *yaw) {
    if (!origin || !target || !pitch || !yaw) return 0;
    const double dx = (double)target[0] - origin[0];
    const double dy = (double)target[1] - origin[1];
    const double dz = (double)target[2] - origin[2];
    const double horizontal = hypot(dx, dz);
    const double length = hypot(horizontal, dy);
    if (length < 65536.0 || horizontal < length * 0.001) return 0;
    const double scale = (4096.0 * 65536.0) / 6.2831853071795864769;
    *pitch = (int32_t)lround(-atan2(dy, horizontal) * scale);
    int64_t y = (int64_t)llround(atan2(dx, dz) * scale) % 0x10000000LL;
    if (y < 0) y += 0x10000000LL;
    *yaw = (int32_t)y;
    return 1;
}

void moh_mkb_mouse_reset(MohMkbMouse *mouse) {
    if (mouse) mouse->pending_x = mouse->pending_y = 0;
}

void moh_mkb_mouse_motion(MohMkbMouse *mouse, double dx, double dy) {
    if (!mouse) return;
    if (!isfinite(dx) || !isfinite(dy)) {
        moh_mkb_mouse_reset(mouse);
        return;
    }
    dx = fmax(-1000000.0, fmin(1000000.0, dx));
    dy = fmax(-1000000.0, fmin(1000000.0, dy));
    mouse->pending_x = fmax(-1000000.0, fmin(1000000.0, mouse->pending_x + dx));
    mouse->pending_y = fmax(-1000000.0, fmin(1000000.0, mouse->pending_y + dy));
}

void moh_mkb_mouse_take_scaled(MohMkbMouse *mouse, double gain,
                               int64_t *yaw, int64_t *pitch) {
    if (!mouse || !yaw || !pitch || !isfinite(gain) || gain <= 0 || gain > 8) return;
    const double scale = gain * 65536.0;
    *yaw = (int64_t)(mouse->pending_x * scale);
    *pitch = -(int64_t)(mouse->pending_y * scale);
    mouse->pending_x -= (double)*yaw / scale;
    mouse->pending_y += (double)*pitch / scale;
}

void moh_mkb_stick_pair(int left, int right, int up, int down,
                        uint8_t *x, uint8_t *y) {
    if (!x || !y) return;
    const int horizontal = !!right - !!left;
    const int vertical = !!down - !!up;
    *x = !horizontal ? 128 : horizontal < 0 ? 0 : 255;
    *y = !vertical ? 128 : vertical < 0 ? 0 : 255;
}

int moh_mkb_weapon_slot(const int16_t *reserve, unsigned count, unsigned current,
                        int direction, int requested) {
    if (!reserve || !count || count > 8 || current >= count) return -1;
    if (requested >= 0)
        return (unsigned)requested < count && reserve[requested] != -1 ? requested : -1;
    if (!direction) return -1;
    for (unsigned step = 1; step < count; ++step) {
        const unsigned slot = direction > 0 ? (current + step) % count :
                                              (current + count - step) % count;
        if (reserve[slot] != -1) return (int)slot;
    }
    return -1;
}

static int32_t wrap_yaw(int64_t angle, int64_t displacement) {
    const int64_t turn = 0x10000000LL;
    /* Reduce before addition so even an invalid extreme caller input cannot
     * overflow the signed accumulator. */
    int64_t result = (angle + displacement % turn) % turn;
    if (result < 0) result += turn;
    return (int32_t)result;
}

int moh_mkb_apply_look(MohMkbAngles *angles, int64_t yaw, int64_t pitch) {
    const int64_t limit = 0x02aa0000LL;
    if (!angles || angles->player_pitch < -limit || angles->player_pitch > limit ||
        angles->input_pitch < -limit || angles->input_pitch > limit) return 0;
    int64_t low = -limit - angles->player_pitch;
    int64_t high = limit - angles->player_pitch;
    const int64_t input_low = -limit - angles->input_pitch;
    const int64_t input_high = limit - angles->input_pitch;
    if (low < input_low) low = input_low;
    if (high > input_high) high = input_high;
    if (pitch < low) pitch = low;
    if (pitch > high) pitch = high;
    angles->player_pitch = (int32_t)(angles->player_pitch + pitch);
    angles->input_pitch = (int32_t)(angles->input_pitch + pitch);
    angles->player_yaw = wrap_yaw(angles->player_yaw, yaw);
    angles->input_yaw = wrap_yaw(angles->input_yaw, yaw);
    return 1;
}
