#include "moh_vr_frustum.h"
#include <assert.h>
#include <string.h>
static unsigned char ram[0x200000];
uint32_t psx_mod_read_word(uint32_t a) {uint32_t v;memcpy(&v,ram+(a&0x1fffffffu),4);return v;}
uint16_t psx_mod_read_half(uint32_t a) {uint16_t v;memcpy(&v,ram+(a&0x1fffffffu),2);return v;}
static void word(uint32_t a,uint32_t v) {memcpy(ram+a,&v,4);}
static void node(uint32_t a,int leaf) {
    const int16_t bounds[6]={-10,-10,90,10,10,110};
    memcpy(ram+a,bounds,sizeof bounds);word(a+12,leaf);word(a+16,1);word(a+20,2);
}
int main(void) {
    double matrix[9]={1,0,0,0,1,0,0,0,1},tr[3]={0};
    PSXModRenderView view={0};view.struct_size=sizeof view;
    view.rotation_q12[0]=view.rotation_q12[4]=view.rotation_q12[8]=4096;
    view.projection=1;view.fx_q16=view.fy_q16=256*65536;
    int16_t forward[6]={-10,-10,90,10,10,110};
    int16_t behind[6]={-10,-10,-110,10,10,-90};
    int16_t side[6]={390,-10,90,410,10,110};
    int16_t crossing[6]={-1000,-1000,-10,1000,1000,1000};
    assert(moh_vr_box_visible(forward,matrix,tr,&view,256,120,512,240));
    assert(!moh_vr_box_visible(behind,matrix,tr,&view,256,120,512,240));
    assert(!moh_vr_box_visible(side,matrix,tr,&view,256,120,512,240));
    assert(moh_vr_box_visible(crossing,matrix,tr,&view,256,120,512,240));
    view.rotation_q12[0]=view.rotation_q12[8]=-4096;
    assert(moh_vr_box_visible(behind,matrix,tr,&view,256,120,512,240));
    assert(!moh_vr_box_visible(forward,matrix,tr,&view,256,120,512,240));
    view.rotation_q12[0]=view.rotation_q12[8]=4096;view.translation[0]=-400;
    assert(moh_vr_box_visible(side,matrix,tr,&view,256,120,512,240));
    tr[0]=40000;assert(moh_vr_box_visible(forward,matrix,tr,&view,256,120,512,240));
    tr[0]=view.translation[0]=0;
    uint32_t queue[1000],masks[2]={1,2};unsigned count;
    node(0x100,0);node(0x140,1);node(0x180,1);node(0x1c0,1);
    word(0x118,0x80000140);word(0x120,0x80000180);word(0x11c,0x800001c0);
    assert(moh_vr_collect_tree(0x80000100,queue,&count,matrix,tr,&view,256,120,masks,0));
    assert(count==3&&queue[0]==0x800001c0&&queue[1]==0x80000180&&queue[2]==0x80000140);
    word(0x1d0,0);assert(moh_vr_collect_tree(0x80000100,queue,&count,matrix,tr,&view,256,120,masks,0));
    assert(count==2);assert(moh_vr_collect_tree(0x80000100,queue,&count,matrix,tr,&view,256,120,masks,1)&&count==3);
    word(0x118,0x801ffffc);assert(!moh_vr_collect_tree(0x80000100,queue,&count,matrix,tr,&view,256,120,masks,1));
    word(0x118,0x80000100);word(0x11c,0);word(0x120,0);
    assert(!moh_vr_collect_tree(0x80000100,queue,&count,matrix,tr,&view,256,120,masks,1));
    /* A long tree with 1001 leaves must fall back before publishing a queue. */
    memset(ram,0,sizeof ram);
    for(unsigned i=0;i<1001;i++) {
        uint32_t a=0x100+i*80;node(a,0);node(a+40,1);word(a+28,0x80000000+a+40);
        if(i<1000)word(a+24,0x80000000+a+80);
    }
    assert(!moh_vr_collect_tree(0x80000100,queue,&count,matrix,tr,&view,256,120,masks,1));
    return 0;
}
