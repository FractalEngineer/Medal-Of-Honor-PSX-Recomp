#pragma once
#include "mod_plugins.h"
#include <stdint.h>
int moh_vr_matrix_inverse(const double matrix[9],double inverse[9]);
int moh_vr_weapon_transform(const double inverse[9],const double rotation[9],
                            const double translation[3],double scale,const double pivot[3],
                            double out_rotation[9],double out_translation[3]);
/* Native level transform maps world to camera, including authored scale.
 * Snapshot getter is external; this pure conversion never polls XR actions. */
int moh_vr_aim_shot(const PSXModOpenXRHands *hands,const double inverse[9],
                    const double camera_tr[3],double units_per_meter,
                    int32_t position_q16[3],int32_t pitch_yaw_q16[2]);
