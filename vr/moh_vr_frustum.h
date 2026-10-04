#pragma once
#include "mod_plugins.h"
#include <math.h>
/* Conservative AABB test in the actual eye view. A box straddling a plane
 * remains visible. Saturated GTE coordinates defer to native triangle clipping. */
static inline int moh_vr_box_visible(const int16_t bounds[6],const double matrix[9],
                                     const double tr[3],const PSXModRenderView *view,
                                     double cx,double cy,int width,int height) {
    if(width<=0||height<=0) return 1;
    double fx=view->projection?view->fx_q16/65536.0:400;
    double fy=view->projection?view->fy_q16/65536.0:400;
    cx+=view->projection?view->cx_delta_q16/65536.0:0;
    cy+=view->projection?view->cy_delta_q16/65536.0:0;
    if(!isfinite(fx)||!isfinite(fy)||fx<=0||fy<=0) return 1;
    unsigned inside=0;
    for(int corner=0;corner<8;corner++) {
        double p[3],eye[3];
        for(int row=0;row<3;row++) {
            p[row]=tr[row];
            for(int col=0;col<3;col++)
                p[row]+=matrix[row*3+col]*bounds[col+((corner>>col)&1)*3];
        }
        for(int row=0;row<3;row++) {
            eye[row]=view->translation[row];
            for(int col=0;col<3;col++)eye[row]+=view->rotation_q12[row*3+col]/4096.0*p[col];
            if(!isfinite(eye[row]))return 1;
        }
        if(fabs(eye[0])>=32760||fabs(eye[1])>=32760||fabs(eye[2])>=65530)return 1;
        /* Four screen planes and the camera plane, with a small guard band
         * for integer GTE projection/rounding. No native-corner sign selection. */
        if(eye[2]>=-1)inside|=1;
        if(fx*eye[0]+(cx+2)*eye[2]>=-2)inside|=2;
        if((width-cx+2)*eye[2]-fx*eye[0]>=-2)inside|=4;
        if(fy*eye[1]+(cy+2)*eye[2]>=-2)inside|=8;
        if((height-cy+2)*eye[2]-fy*eye[1]>=-2)inside|=16;
    }
    return inside==31;
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
        if(!bypass_masks&&(!(psx_mod_read_word(node+16)&masks[0])||
                          !(psx_mod_read_word(node+20)&masks[1])))continue;
        int16_t bounds[6];
        for(int i=0;i<6;i++)bounds[i]=(int16_t)psx_mod_read_half(node+i*2);
        for(int i=0;i<3;i++)if(bounds[i]>bounds[i+3])return 0;
        if(!moh_vr_box_visible(bounds,matrix,tr,view,cx,cy,512,240))continue;
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
