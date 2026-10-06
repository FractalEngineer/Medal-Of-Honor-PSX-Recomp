#include "moh_vr_raycast.h"
#include <assert.h>
#include <string.h>
static unsigned char ram[0x200000];
uint32_t psx_mod_read_word(uint32_t a){uint32_t v;memcpy(&v,ram+(a&0x1fffffffu),4);return v;}
uint16_t psx_mod_read_half(uint32_t a){uint16_t v;memcpy(&v,ram+(a&0x1fffffffu),2);return v;}
static void word(uint32_t a,uint32_t v){memcpy(ram+a,&v,4);}
static void set_bounds(uint32_t a,const int16_t b[6]){memcpy(ram+a,b,12);}
static void leaf(uint32_t a,int16_t z0,int16_t z1){
    const int16_t b[6]={-10,-10,z0,10,10,z1};
    set_bounds(a,b);word(a+12,1);
}
static void spine(uint32_t a){
    const int16_t b[6]={-100,-100,0,100,100,1000};
    set_bounds(a,b);word(a+12,0);
}
int main(void){
    const int16_t box[6]={-10,-10,100,10,10,120};
    const double z[3]={0,0,1},x[3]={1,0,0},o[3]={0,0,0};
    double t;

    /* ray vs box */
    assert(moh_vr_ray_box(o,z,box,0.0,1000.0,&t)&&t>99.0&&t<101.0);   /* forward */
    const double behind[3]={0,0,300};
    assert(!moh_vr_ray_box(behind,z,box,0.0,1000.0,&t));              /* starts past it */
    const double inside[3]={0,0,110};
    assert(moh_vr_ray_box(inside,z,box,0.0,1000.0,&t)&&t==0.0);       /* origin inside */
    assert(moh_vr_ray_box(inside,x,box,0.0,1000.0,&t));               /* parallel, inside slab */
    const double outside[3]={0,20,110};
    assert(!moh_vr_ray_box(outside,x,box,0.0,1000.0,&t));             /* parallel, outside */
    const double side[3]={100,0,0};
    assert(!moh_vr_ray_box(side,z,box,0.0,1000.0,&t));                /* misses */
    assert(!moh_vr_ray_box(o,z,box,0.0,50.0,&t));                     /* range too short */

    /* bvh: root -> leaves at z=100/200/300 on the axis; nearest must win */
    memset(ram,0,sizeof ram);
    spine(0x100);leaf(0x140,100,120);leaf(0x180,200,220);leaf(0x1c0,300,320);
    word(0x100+24,0x140);word(0x100+32,0x180);word(0x100+28,0x1c0);
    uint32_t hit=0;
    assert(moh_vr_ray_tree(0x100,o,z,0,1,1000.0,&t,&hit)&&hit==0x140&&t>99.0&&t<101.0);
    assert(!moh_vr_ray_tree(0x100,o,z,0,1,50.0,&t,&hit));             /* range limit */
    assert(moh_vr_ray_tree(0x140,o,z,0,1,1000.0,&t,&hit)&&hit==0x140); /* leaf as root */
    /* masks gate on +16/+20 at every visited node */
    word(0x100+16,15);word(0x100+20,15);
    word(0x140+16,1);word(0x140+20,2);
    word(0x180+16,4);word(0x180+20,2);
    word(0x1c0+16,15);word(0x1c0+20,15);
    const uint32_t hit_masks[2]={1,2},miss_masks[2]={4,2};
    assert(moh_vr_ray_tree(0x100,o,z,hit_masks,0,1000.0,&t,&hit)&&hit==0x140);
    assert(moh_vr_ray_tree(0x100,o,z,miss_masks,0,1000.0,&t,&hit)&&hit==0x180);
    /* origin inside the nearest leaf is skipped, not reported as t=0 */
    const double in_leaf[3]={0,0,110};
    assert(moh_vr_ray_tree(0x100,in_leaf,z,0,1,1000.0,&t,&hit)&&hit==0x180);
    /* malformed box aborts the search */
    word(0x140+12,0);
    const int16_t bad[6]={10,0,0,-10,0,0};
    set_bounds(0x140,bad);
    assert(!moh_vr_ray_tree(0x100,o,z,0,1,1000.0,&t,&hit));
    return 0;
}
