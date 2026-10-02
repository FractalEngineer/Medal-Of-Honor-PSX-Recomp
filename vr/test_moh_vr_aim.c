#include "moh_vr_aim.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    /* A scaled 90-degree camera, independent known world coordinates.
     * C maps +world X to +camera Z; player camera center=(10,20,30). */
    const double c[9]={0,0,-2,0,1.25,0,2,0,0},tr[3]={60,-25,-20};
    double inv[9];assert(moh_vr_matrix_inverse(c,inv));
    PSXModOpenXRHands h={0};h.struct_size=sizeof h;h.focused=h.origin_valid=1;
    h.origin_orientation_xyzw[3]=1;
    PSXModTrackedPose *v=&h.pose[1][PSX_MOD_XR_AIM_POSE];v->active=1;v->flags=15;
    v->orientation_xyzw[3]=1;
    int32_t p[3],a[2];assert(moh_vr_aim_shot(&h,inv,tr,100,p,a));
    assert(p[0]==10*65536 && p[1]==20*65536 && p[2]==30*65536);
    assert(a[0]==0 && a[1]==1024*65536);
    v->position_m[0]=.2f;v->position_m[1]=.1f;v->position_m[2]=-.4f;
    assert(moh_vr_aim_shot(&h,inv,tr,100,p,a));
    assert(abs(p[0]-30*65536)<2 && abs(p[1]-12*65536)<2 && abs(p[2]-20*65536)<2);
    /* +XR yaw points left relative to native camera, changing world +X to +Z. */
    v->orientation_xyzw[1]=(float)sqrt(.5);v->orientation_xyzw[3]=(float)sqrt(.5);
    assert(moh_vr_aim_shot(&h,inv,tr,100,p,a));assert(abs(a[1])<2);
    int32_t old[3];memcpy(old,p,sizeof p);
    h.age_ms=151;assert(!moh_vr_aim_shot(&h,inv,tr,100,p,a));assert(!memcmp(old,p,sizeof p));
    h.age_ms=0;h.focused=0;assert(!moh_vr_aim_shot(&h,inv,tr,100,p,a));
    h.focused=1;v->flags=1;assert(!moh_vr_aim_shot(&h,inv,tr,100,p,a));
    v->flags=15;v->active=0;assert(!moh_vr_aim_shot(&h,inv,tr,100,p,a));
    v->active=1;v->position_m[0]=NAN;assert(!moh_vr_aim_shot(&h,inv,tr,100,p,a));
    const double singular[9]={0};assert(!moh_vr_matrix_inverse(singular,inv));
    puts("PASS: scaled camera inverse, world aim axes/origin and invalid snapshot rejection");
}
