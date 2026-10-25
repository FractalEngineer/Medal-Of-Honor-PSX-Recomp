#pragma once
#include "mod_plugins.h"
#include "vr_geometry_math.h"
#include <math.h>
/* Test-only: force the collector to consider the whole tree, ignoring both the PVS masks and the
 * screen planes. Geometry near the player is dropped by the guest before the eye replay ever sees it,
 * and this separates "the game never submits it" from "the eye replay loses it". Not for play: it
 * submits far more than a frame needs. */
static int moh_vr_keep_all;
/* Conservative AABB test in the actual eye view. A box straddling a plane
 * remains visible. Saturated GTE coordinates defer to native triangle clipping. */
static inline int moh_vr_box_visible(const int16_t bounds[6],const double matrix[9],
                                     const double tr[3],const PSXModRenderView *view,
                                     double cx,double cy,int width,int height) {
    if(!view||!bounds||!matrix||!tr)return 1;
    double fx=view->projection?view->fx_q16/65536.0:400;
    double fy=view->projection?view->fy_q16/65536.0:400;
    cx+=view->projection?view->cx_delta_q16/65536.0:0;
    cy+=view->projection?view->cy_delta_q16/65536.0:0;
    double eye_rotation[9];
    for(int i=0;i<9;i++)
        eye_rotation[i]=view->precise?view->rotation[i]:view->rotation_q12[i]/4096.0;
    double eye_translation[3];
    for(int i=0;i<3;i++)
        eye_translation[i]=view->precise?view->translation_precise[i]:view->translation[i];
    const int visible=psx_vr_box_visible(bounds,matrix,tr,eye_rotation,eye_translation,
                                         fx,fy,cx,cy,width,height);
    if(visible==PSX_VR_GEOMETRY_UNKNOWN)return 1;
    return visible;
}
/* Collect a complete queue before the caller writes anything. On malformed
 * or oversized trees, the original guest routine handles the request. */
static inline int moh_vr_collect_tree(uint32_t root,uint32_t queue[1000],unsigned *count,
                                      const double matrix[9],const double tr[3],
                                      const PSXModRenderView *view,double cx,double cy,
                                      const uint32_t masks[2],int bypass_masks) {
    uint32_t stack[256];unsigned top=1,visited=0;*count=0;stack[0]=root;
    while(top) {
        uint32_t node=stack[--top],off=node&0x1fffffffu;
        if(!node||(node&3)||off>0x200000u-36||++visited>8192)return 0;
        if(!bypass_masks&&!moh_vr_keep_all&&(!(psx_mod_read_word(node+16)&masks[0])||
                          !(psx_mod_read_word(node+20)&masks[1])))continue;
        int16_t bounds[6];
        for(int i=0;i<6;i++)bounds[i]=(int16_t)psx_mod_read_half(node+i*2);
        for(int i=0;i<3;i++)if(bounds[i]>bounds[i+3])return 0;
        if(!moh_vr_keep_all&&!moh_vr_box_visible(bounds,matrix,tr,view,cx,cy,512,240))continue;
        if((int32_t)psx_mod_read_word(node+12)>0) {
            if(*count==1000)return 0;
            queue[(*count)++]=node;
        } else {
            const unsigned offsets[3]={24,32,28};
            for(int i=0;i<3;i++) {
                uint32_t child=psx_mod_read_word(node+offsets[i]);
                if(child) {if(top==256)return 0;stack[top++]=child;}
            }
        }
    }
    return 1;
}
