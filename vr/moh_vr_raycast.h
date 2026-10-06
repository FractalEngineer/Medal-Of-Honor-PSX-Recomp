#pragma once
#include "mod_plugins.h"
#include <math.h>
#include <stdint.h>
/* Raycast against the level visibility BVH (see VR_LASER_RAYCAST_PLAN.md).
 * Node layout is the measured SLUS-00974 tree used by moh_vr_frustum.h:
 *   +0..+10  int16 bbox: min xyz at +0/+2/+4, max xyz at +6/+8/+10
 *   +12      leaf count (leaf when > 0)
 *   +16/+20  PVS masks
 *   +24/+32/+28  child pointers
 * Bounds are in the engine's camera-relative world space; RT (gte_ctrl[0..4])
 * and TR (gte_ctrl[5..7]) map that space to camera space (TR is zero on the
 * level pass). The caller converts its ray into that space. */

/* Slab test: does o + t*d (t in [tmin,tmax]) meet the int16 AABB? When it does,
 * t_enter is the entry distance (clamped to tmin when the origin is inside). */
static inline int moh_vr_ray_box(const double o[3],const double d[3],
                                 const int16_t bounds[6],double tmin,double tmax,
                                 double *t_enter) {
    double t0=tmin,t1=tmax;
    for(int i=0;i<3;i++) {
        double lo=bounds[i],hi=bounds[i+3];
        if(lo>hi)return 0;
        if(fabs(d[i])<1e-9) {
            if(o[i]<lo||o[i]>hi)return 0;
            continue;
        }
        double inv=1.0/d[i];
        double ta=(lo-o[i])*inv,tb=(hi-o[i])*inv;
        if(ta>tb){double s=ta;ta=tb;tb=s;}
        if(ta>t0)t0=ta;
        if(tb<t1)t1=tb;
        if(t0>t1)return 0;
    }
    *t_enter=t0;
    return 1;
}

/* Ray vs triangle (Moeller-Trumbore, no back-face cull). */
static inline int moh_vr_ray_tri(const double o[3],const double d[3],
                                 const double v0[3],const double v1[3],const double v2[3],
                                 double tmax,double *t_out) {
    const double e1[3]={v1[0]-v0[0],v1[1]-v0[1],v1[2]-v0[2]};
    const double e2[3]={v2[0]-v0[0],v2[1]-v0[1],v2[2]-v0[2]};
    const double p[3]={d[1]*e2[2]-d[2]*e2[1],d[2]*e2[0]-d[0]*e2[2],d[0]*e2[1]-d[1]*e2[0]};
    double det=e1[0]*p[0]+e1[1]*p[1]+e1[2]*p[2];
    if(det>-1e-9&&det<1e-9)return 0;
    double inv=1.0/det;
    const double t[3]={o[0]-v0[0],o[1]-v0[1],o[2]-v0[2]};
    double u=(t[0]*p[0]+t[1]*p[1]+t[2]*p[2])*inv;
    if(u<0||u>1)return 0;
    const double q[3]={t[1]*e1[2]-t[2]*e1[1],t[2]*e1[0]-t[0]*e1[2],t[0]*e1[1]-t[1]*e1[0]};
    double v=(d[0]*q[0]+d[1]*q[1]+d[2]*q[2])*inv;
    if(v<0||u+v>1)return 0;
    double tt=(e2[0]*q[0]+e2[1]*q[1]+e2[2]*q[2])*inv;
    if(tt<=1e-3||tt>tmax)return 0;
    *t_out=tt;return 1;
}

/* Surface-exact: nearest level TRIANGLE along the ray. A leaf node (+0xc >= 1)
 * holds `count` 16-byte triangle records at +0x18; each record is word0 =
 * [idx0|idx1] and word1 low = idx2, indexing the 8-byte vertex table (`table`,
 * 3 x int16 + pad). Verified against the live dump (VR_LASER_RAYCAST_PLAN.md). */
static inline int moh_vr_ray_tree_tris(uint32_t root,uint32_t table,
                                       const double o[3],const double d[3],
                                       double tmax,double *t_hit) {
    if(!root||(root&3)||table<0x80000000u||table>=0x801ff000u||tmax<=0)return 0;
    uint32_t stack[256];unsigned top=1,visited=0;
    double best=tmax;int found=0;
    stack[0]=root;
    while(top) {
        uint32_t node=stack[--top],off=node&0x1fffffffu;
        if(!node||(node&3)||off>0x200000u-36||++visited>8192)return 0;
        int16_t bounds[6];
        for(int i=0;i<6;i++)bounds[i]=(int16_t)psx_mod_read_half(node+(uint32_t)i*2);
        for(int i=0;i<3;i++)if(bounds[i]>bounds[i+3])return 0;
        double t_enter;
        if(!moh_vr_ray_box(o,d,bounds,0.0,best,&t_enter))continue;
        int32_t count=(int32_t)psx_mod_read_word(node+12);
        if(count>=1) {
            /* The ray starts inside this box: no surface of our own container. */
            if(t_enter<=1e-3)continue;
            if(count>4096)return 0;
            uint32_t list=psx_mod_read_word(node+24);
            if(list<0x80000000u||list>=0x801ff000u)return 0;
            for(int32_t i=0;i<count;i++) {
                uint32_t rec=list+(uint32_t)i*16u;
                if(rec<0x80000000u||rec+8u>0x801ff000u)return 0;
                uint32_t w0=psx_mod_read_word(rec),w1=psx_mod_read_word(rec+4u);
                uint32_t idx[3]={w0&0xffffu,w0>>16,w1&0xffffu};
                double v[9];int bad=0;
                for(int k=0;k<3;k++) {
                    uint32_t a=table+idx[k]*8u;
                    if(a<0x80000000u||a+6u>0x801ff000u){bad=1;break;}
                    for(int c=0;c<3;c++)
                        v[k*3+c]=(double)(int16_t)psx_mod_read_half(a+(uint32_t)c*2u);
                }
                if(bad)return 0;
                double tth;
                if(moh_vr_ray_tri(o,d,v,v+3,v+6,best,&tth)){best=tth;found=1;}
            }
            continue;
        }
        const unsigned offsets[3]={24,32,28};
        for(int i=0;i<3;i++) {
            uint32_t child=psx_mod_read_word(node+offsets[i]);
            if(child) {if(top==256)return 0;stack[top++]=child;}
        }
    }
    if(!found)return 0;
    *t_hit=best;return 1;
}

static inline int moh_vr_ray_tree(uint32_t root,const double o[3],const double d[3],
                                  const uint32_t masks[2],int bypass_masks,
                                  double tmax,double *t_hit,uint32_t *leaf) {
    if(!root||(root&3)||tmax<=0)return 0;
    uint32_t stack[256];unsigned top=1,visited=0;
    double best=tmax;uint32_t best_leaf=0;int found=0;
    stack[0]=root;
    while(top) {
        uint32_t node=stack[--top],off=node&0x1fffffffu;
        if(!node||(node&3)||off>0x200000u-36||++visited>8192)return 0;
        if(!bypass_masks&&masks&&(!(psx_mod_read_word(node+16)&masks[0])||
                                  !(psx_mod_read_word(node+20)&masks[1])))continue;
        int16_t bounds[6];
        for(int i=0;i<6;i++)bounds[i]=(int16_t)psx_mod_read_half(node+(uint32_t)i*2);
        for(int i=0;i<3;i++)if(bounds[i]>bounds[i+3])return 0;
        double t_enter;
        if(!moh_vr_ray_box(o,d,bounds,0.0,best,&t_enter))continue;
        if((int32_t)psx_mod_read_word(node+12)>0) {
            if(t_enter>1e-3) {best=t_enter;best_leaf=node;found=1;}
            continue;
        }
        const unsigned offsets[3]={24,32,28};
        for(int i=0;i<3;i++) {
            uint32_t child=psx_mod_read_word(node+offsets[i]);
            if(child) {if(top==256)return 0;stack[top++]=child;}
        }
    }
    if(!found)return 0;
    if(t_hit)*t_hit=best;
    if(leaf)*leaf=best_leaf;
    return 1;
}
