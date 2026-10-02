#include "moh_vr_input.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
/* Producer-bound slot-3 calibration, read from the native converter's tables. */
static const uint8_t measured_curve[128] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10,10,10,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,94,98,102,106,110,114,118,122,126,130,134,138,142,146,150,160,170,180,190,200,210,220,230,240,250,255,255};
int main(void) {
    MOHVRAnalogResponse response = {0};
    for (unsigned i = 0; i < 3; ++i)
        response.axis[i] = (MOHVRAnalogAxis){91, 360, 199, i ? 8960 : 15360};
    memcpy(response.curve, measured_curve, sizeof measured_curve);
    assert(moh_vr_analog_response(&response, 0, 45) == -34 * 15360);
    assert(moh_vr_analog_response(&response, 0, 211) == 76 * 15360);
    PSXModOpenXRInput input = {0}; PSXModControllerState pad;
    input.struct_size = sizeof input; input.focused = 1;
    input.active[0] = input.active[1] = 1;
    assert(moh_vr_input_map(&input, .2, .65, &response, &pad));
    assert(pad.buttons == 0xffff && pad.lx == 128 && pad.ly == 128 && pad.rx == 128);
    for (int i = 0; i <= 100; ++i) {
        input.stick[1][0] = i / 100.f;
        assert(moh_vr_input_map(&input, .2, .65, &response, &pad));
        int positive = moh_vr_analog_response(&response, 0, pad.lx);
        input.stick[1][0] = -i / 100.f;
        assert(moh_vr_input_map(&input, .2, .65, &response, &pad));
        int negative = moh_vr_analog_response(&response, 0, pad.lx);
        assert(abs(positive + negative) <= 2 * 15360);
    }
    input.stick[1][0] = 0;
    for (int degrees = 0; degrees < 360; degrees += 15) {
        double angle = degrees * 3.141592653589793 / 180;
        input.stick[0][0] = (float)cos(angle);
        input.stick[0][1] = (float)sin(angle);
        assert(moh_vr_input_map(&input, .2, .65, &response, &pad));
        double x = moh_vr_analog_response(&response, 2, pad.rx) / (255. * 8960);
        double y = -moh_vr_analog_response(&response, 1, pad.ly) / (255. * 8960);
        assert(fabs(hypot(x, y) - 1) < .03);
        assert(fabs(x - cos(angle)) < .025 && fabs(y - sin(angle)) < .025);
    }
    input.stick[0][0] = input.stick[0][1] = .1f;
    input.stick[1][0] = .15f;
    assert(moh_vr_input_map(&input, .2, .65, &response, &pad));
    assert(pad.lx == 128 && pad.ly == 128 && pad.rx == 128);
    input.stick[0][0] = input.stick[0][1] = input.stick[1][0] = 1;
    input.focused = 0;
    assert(moh_vr_input_map(&input, .2, .65, &response, &pad));
    assert(pad.lx == 128 && pad.ly == 128 && pad.rx == 128);
    input.focused = 1; input.active[0] = input.active[1] = 0;
    assert(moh_vr_input_map(&input, .2, .65, &response, &pad));
    assert(pad.lx == 128 && pad.ly == 128 && pad.rx == 128);
    input.active[0] = 1; input.stick[0][0] = NAN; input.stick[0][1] = .19f;
    assert(moh_vr_input_map(&input, .2, .65, &response, &pad) && pad.rx == 128 && pad.ly == 128);
    assert(!moh_vr_input_map(&input, 1, .65, &response, &pad));
    response.axis[0].negative_factor = 0;
    assert(!moh_vr_input_map(&input, .2, .65, &response, &pad));
    return 0;
}
