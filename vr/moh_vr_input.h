#pragma once
#include "mod_plugins.h"
/* Native default scheme: left X turns, left Y moves, right X strafes.
 * Calibration is read from the live guest, including its nonlinear lookup. */
typedef struct MOHVRAnalogAxis {
    uint32_t negative_limit, negative_factor, positive_factor, scale;
} MOHVRAnalogAxis;
typedef struct MOHVRAnalogResponse {
    MOHVRAnalogAxis axis[3]; /* native LX, LY, RX */
    uint8_t curve[128];
} MOHVRAnalogResponse;
/* Mirrors FUN_80075E7C's calibrated signed response, before input-state gains. */
int32_t moh_vr_analog_response(const MOHVRAnalogResponse *response,
                             unsigned axis, uint32_t byte);
/* merged_y: the right stick's otherwise-unused Y also drives LY (move/aim), so
 * one hand controls both axes of the native left stick - needed when a machine
 * gun aims with that stick. */
int moh_vr_input_map(const PSXModOpenXRInput *input, double deadzone,
                    double turn_gain, const MOHVRAnalogResponse *response,
                    int merged_y, PSXModControllerState *pad);
/* Native menu buttons/D-pad, independent of gameplay calibration tables. */
int moh_vr_menu_input_map(const PSXModOpenXRInput *input,double deadzone,
                         PSXModControllerState *pad);
