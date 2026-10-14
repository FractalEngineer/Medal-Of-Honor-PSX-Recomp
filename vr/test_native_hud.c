/* Native artwork / fallback contract. Build against candidate framework headers. */
#include "moh_vr_hud.h"
#include "mod_plugins.h"
#include "xr_surface_math.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static PSXModOpenXRHands hands;
static PSXModOpenXRSurface submitted[2];
static int accepted=3, calls;
int psx_mod_openxr_hands(PSXModOpenXRHands *out){*out=hands;return 1;}
int psx_mod_openxr_surface(const PSXModOpenXRSurface *surface){submitted[surface->id]=*surface;calls++;return !!(accepted&(1<<surface->id));}
static uint8_t black[512*240*4],white[512*240*4];
static void content(void) {
    memset(black,0,sizeof black);memset(white,255,sizeof white);
    /* Opaque BLACK is actual art, with transparency surrounding it. */
    for(int y=20;y<30;y++)for(int x=20;x<30;x++)memset(white+(y*512+x)*4,0,3);
    for(int y=10;y<13;y++)for(int x=380;x<385;x++) {
        int i=(y*512+x)*4;black[i]=white[i]=120;black[i+1]=white[i+1]=200;black[i+2]=white[i+2]=80;
    }
}
int main(void) {
    putenv("PSX_VR_WRIST_HUD=1");putenv("PSX_VR_WRIST_REPLACE=1");
    hands.focused=hands.origin_valid=1;
    for(int i=0;i<2;i++){hands.pose[i][0].active=1;hands.pose[i][0].flags=15;hands.pose[i][0].orientation_xyzw[3]=1;}
    content();moh_vr_hud_frame(1.2f);CHECK(moh_vr_hud_set_native(black,white));moh_vr_hud_prepare();
    CHECK(calls==2 && moh_vr_hud_replacement_mask()==3);
    CHECK(submitted[0].flags==PSX_MOD_OPENXR_SURFACE_FACE_HEAD && submitted[1].flags==PSX_MOD_OPENXR_SURFACE_FACE_HEAD);
    CHECK(fabsf(submitted[0].position_m[1]-.06f)<.00001f && fabsf(submitted[1].position_m[1]-.06f)<.00001f);
    /* Quarter-turning the left grip cancels its mirrored attachment roll. */
    hands.pose[0][0].orientation_xyzw[3]=.707106781f;hands.pose[0][0].orientation_xyzw[2]=.707106781f;
    moh_vr_hud_prepare();
    CHECK(fabsf(submitted[0].position_m[1]-.06f)<.00001f);
    CHECK(submitted[0].orientation_xyzw[3]==1); /* controller roll cannot rotate text */
    hands.pose[0][0].orientation_xyzw[3]=1;hands.pose[0][0].orientation_xyzw[2]=0;
    const uint8_t *p=submitted[0].rgba;
    CHECK(p[((96-1-8)*96+8)*4+3]==255); /* art: alpha remains opaque black */
    CHECK(p[((96-1-8)*96+8)*4]==0);
    CHECK(p[3]==0); /* surrounding matte removed */
    CHECK(fabsf(submitted[0].size_m[1]-.144f)<.0001f);
    moh_vr_hud_frame(1);CHECK(moh_vr_hud_replacement_mask()==0);calls=0;moh_vr_hud_prepare();CHECK(calls==0); /* no stale art */
    CHECK(moh_vr_hud_set_native(black,white));hands.pose[0][0].active=0;moh_vr_hud_prepare();CHECK(moh_vr_hud_replacement_mask()==2);
    moh_vr_hud_frame(1);CHECK(moh_vr_hud_set_native(black,white));hands.focused=0;calls=0;moh_vr_hud_prepare();CHECK(calls==0 && !moh_vr_hud_replacement_mask());
    hands.focused=1;hands.pose[0][0].active=1;hands.age_ms=101;
    calls=0;moh_vr_hud_prepare();CHECK(calls==0);
    hands.age_ms=0;accepted=1;moh_vr_hud_prepare();CHECK(moh_vr_hud_replacement_mask()==1); /* upload rejection restores that side */
    white[(20*512+20)*4]=255; /* conflicting matte channel: fail closed */
    moh_vr_hud_frame(1);CHECK(!moh_vr_hud_set_native(black,white));calls=0;moh_vr_hud_prepare();CHECK(calls==0);
    /* Generic LOCAL projection: binocular disparity and common-origin invariance. */
    PSXModOpenXRSurface v={0};v.orientation_xyzw[3]=1;v.position_m[2]=-1;v.size_m[0]=v.size_m[1]=.2f;
    float eye[7]={-.03f,0,0,0,0,0,1},fov[4]={-.785398f,.785398f,.785398f,-.785398f},left[36],right[36],shifted[36];
    CHECK(xr_surface_vertices(&v,eye,fov,left));eye[0]=.03f;CHECK(xr_surface_vertices(&v,eye,fov,right));
    CHECK(left[0]>right[0]);
    v.position_m[0]+=2;v.position_m[1]+=3;eye[0]+=2;eye[1]+=3;
    CHECK(xr_surface_vertices(&v,eye,fov,shifted));for(int i=0;i<36;i++)CHECK(fabsf(right[i]-shifted[i])<.00001f);
    v.position_m[2]=.1f;CHECK(!xr_surface_vertices(&v,eye,fov,shifted));
    v.position_m[2]=-1;v.flags=PSX_MOD_OPENXR_SURFACE_FACE_HEAD;
    CHECK(xr_surface_vertices(&v,eye,fov,left));
    v.orientation_xyzw[3]=0;v.orientation_xyzw[0]=1;
    CHECK(xr_surface_vertices(&v,eye,fov,right));
    for(int i=0;i<36;i++)CHECK(fabsf(left[i]-right[i])<.00001f); /* head-facing ignores hand rotation */
    float moved_head[3]={eye[0]+.5f,eye[1],eye[2]};
    CHECK(xr_surface_vertices_head(&v,eye,fov,moved_head,shifted));
    CHECK(fabsf(left[0]-shifted[0])+fabsf(left[3]-shifted[3])>.001f); /* head translation reorients */
    puts("Native cutout, per-hand fallback, stale-frame and LOCAL projection tests passed.");return 0;
}
