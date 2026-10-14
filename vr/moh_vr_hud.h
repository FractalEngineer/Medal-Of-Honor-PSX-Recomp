#pragma once
#include <stdint.h>
/* Native HUD cutouts. Each hand falls back to the ordinary HUD when its
 * capture or tracked grip is unavailable. Replacement is separately opt-in. */
void moh_vr_hud_prepare(void);
int moh_vr_hud_wants_native(void);
int moh_vr_hud_set_native(const uint8_t *black,const uint8_t *white);
void moh_vr_hud_frame(float native_y_per_x);
unsigned moh_vr_hud_replacement_mask(void);
