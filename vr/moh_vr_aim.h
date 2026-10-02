#pragma once
#include "mod_plugins.h"
#include <stdint.h>
int moh_vr_matrix_inverse(const double matrix[9],double inverse[9]);
/* Native level transform maps world to camera, including authored scale.
 * Snapshot getter is external; this pure conversion never polls XR actions. */
int moh_vr_aim_shot(const PSXModOpenXRHands *hands,const double inverse[9],
                    const double camera_tr[3],double units_per_meter,
                    int32_t position_q16[3],int32_t pitch_yaw_q16[2]);
