#include "moh_vr_hud.h"
#include "mod_plugins.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(PSX_MOD_OPENXR_SURFACES)
enum { W=256, H=128 };
static unsigned char pixels[2][W*H*4];
static int initialized,enabled,test_mode,native_ready;
static int widths[2]={W,W},heights[2]={H,H};
static float pixel_aspect=1;
static unsigned replacement_mask;
void moh_vr_hud_frame(float aspect) {replacement_mask=0;native_ready=0;pixel_aspect=isfinite(aspect)&&aspect>.1f&&aspect<8?aspect:1;}
unsigned moh_vr_hud_replacement_mask(void) {return replacement_mask;}

int moh_vr_hud_wants_native(void) {
    const char *v=getenv("PSX_VR_WRIST_HUD");return v && atoi(v)!=0;
}
int moh_vr_hud_set_native(const uint8_t *black,const uint8_t *white) {
    native_ready=0;
    if(!black || !white)return 0;
    const int rects[2][4]={{16,12,96,96},{374,8,128,36}};
    int coverage[2]={0,0};
    for(int side=0;side<2;side++) {
        int x=rects[side][0],y=rects[side][1],w=rects[side][2],h=rects[side][3];
        widths[side]=w;heights[side]=h;
        for(int row=0;row<h;row++)for(int col=0;col<w;col++) {
            unsigned source=((row+y)*512+col+x)*4;
            unsigned dest=((h-1-row)*w+col)*4;
            int delta[3],sum=0;
            for(int c=0;c<3;c++){delta[c]=(int)white[source+c]-black[source+c];sum+=delta[c];}
            /* Dual mattes preserve opaque black artwork; never colour-key HUD. */
            if(delta[0]<-2 || delta[1]<-2 || delta[2]<-2 ||
               abs(delta[0]-delta[1])>3 || abs(delta[1]-delta[2])>3)return 0;
            int alpha=255-(sum+1)/3;if(alpha<0)alpha=0;if(alpha>255)alpha=255;
            pixels[side][dest+3]=alpha;
            for(int c=0;c<3;c++) {
                int value=alpha?(black[source+c]*255+alpha/2)/alpha:0;
                pixels[side][dest+c]=value>255?255:value;
            }
            if(alpha>128)coverage[side]++;
        }
    }
    native_ready=(coverage[0]>=50?1:0)|(coverage[1]>=5?2:0);
    if(!native_ready)return 0;
    static int dumped;
    const char *directory=getenv("PSX_VR_WRIST_DUMP_DIRECTORY");
    if(!dumped && directory && *directory) {
        char path[1024];const uint8_t *sources[4]={black,white,pixels[0],pixels[1]};
        const char *names[4]={"hud-black.rgba","hud-white.rgba","hud-left.rgba","hud-right.rgba"};
        size_t sizes[4]={512*240*4,512*240*4,96*96*4,128*36*4};
        for(int i=0;i<4;i++) {
            snprintf(path,sizeof path,"%s/%s",directory,names[i]);FILE *file=fopen(path,"wb");
            if(file){fwrite(sources[i],1,sizes[i],file);fclose(file);}
        }
        dumped=1;
    }
    return 1;
}

static void rect(unsigned char *p,int x,int y,int w,int h,int r,int g,int b) {
    for(int j=y;j<y+h && j<H;j++)for(int i=x;i<x+w && i<W;i++) {
        unsigned char *at=p+((H-1-j)*W+i)*4; /* OpenGL bottom-up */
        at[0]=r;at[1]=g;at[2]=b;at[3]=255;
    }
}
/* Small authored bitmap alphabet for an unmistakable diagnostic card. */
static const struct {char c;unsigned char rows[7];} font[]={
    {'H',{17,17,17,31,17,17,17}}, {'U',{17,17,17,17,17,17,14}},
    {'D',{30,17,17,17,17,17,30}}, {'T',{31,4,4,4,4,4,4}},
    {'E',{31,16,16,30,16,16,31}}, {'S',{15,16,16,14,1,1,30}},
    {'L',{16,16,16,16,16,16,31}}, {'R',{30,17,17,30,20,18,17}},
    {'P',{30,17,17,30,16,16,16}}, {'A',{14,17,17,31,17,17,17}},
    {'M',{17,27,21,21,17,17,17}}, {'O',{14,17,17,17,17,17,14}},
    {'I',{31,4,4,4,4,4,31}}, {'V',{17,17,17,17,17,10,4}},
    {'G',{14,17,16,23,17,17,14}}, {'N',{17,25,25,21,19,19,17}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
    {'-',{0,0,0,31,0,0,0}}, {'/',{1,2,2,4,8,8,16}}
};
static void text(unsigned char *p,int x,int y,const char *s,int scale) {
    for(;*s;s++,x+=6*scale)for(unsigned k=0;k<sizeof font/sizeof font[0];k++)
        if(font[k].c==*s)for(int row=0;row<7;row++)for(int col=0;col<5;col++)
            if(font[k].rows[row]&(16>>col))rect(p,x+col*scale,y+row*scale,scale,scale,240,245,255);
}
static void rotate(const float q[4],const float p[3],float out[3]) {
    float cross[3]={q[1]*p[2]-q[2]*p[1],q[2]*p[0]-q[0]*p[2],q[0]*p[1]-q[1]*p[0]};
    float again[3]={q[1]*cross[2]-q[2]*cross[1],q[2]*cross[0]-q[0]*cross[2],q[0]*cross[1]-q[1]*cross[0]};
    for(int i=0;i<3;i++)out[i]=p[i]+2*(q[3]*cross[i]+again[i]);
}
void moh_vr_hud_prepare(void) {
    if(!initialized) {
        const char *v=getenv("PSX_VR_WRIST_HUD_TEST");test_mode=v && atoi(v)!=0;
        enabled=test_mode || moh_vr_hud_wants_native();
        initialized=1;
        if(test_mode)for(int side=0;side<2;side++) {
            unsigned char *p=pixels[side];
            rect(p,0,0,W,H,side?230:0,side?20:205,side?205:220);
            rect(p,5,5,W-10,H-10,12,20,28);
            text(p,18,18,side?"R HUD":"L HUD",4);
            text(p,18,66,"TEST",5);
        }
    }
    if(!enabled || (!test_mode && !native_ready))return;
    PSXModOpenXRHands hands={0};hands.struct_size=sizeof hands;
    if(!psx_mod_openxr_hands(&hands) || !hands.focused || !hands.origin_valid || hands.age_ms>100)return;
    for(int side=0;side<2;side++) {
        if(!test_mode && !(native_ready&(1u<<side)))continue;
        const PSXModTrackedPose *grip=&hands.pose[side][PSX_MOD_XR_GRIP_POSE];
        if(!grip->active || (grip->flags&15)!=15)continue;
        PSXModOpenXRSurface surface={0};surface.struct_size=sizeof surface;
        surface.id=side;surface.width_px=widths[side];surface.height_px=heights[side];surface.rgba=pixels[side];
        surface.size_m[0]=.12f;surface.size_m[1]=.12f*heights[side]/widths[side]*(test_mode?1:pixel_aspect);
        /* Inward side of each wrist: left +X, right -X, as when clapping.
         * Mirror the quarter-turn attachment rather than using the underside.
         * Keep diagnostic cards in their established harness frame. */
        const float *grip_q=grip->orientation_xyzw;
        float s=side?.707106781f:-.707106781f,c=.707106781f;
        float palm_q[4]={grip_q[0]*c+grip_q[1]*s,
                         grip_q[1]*c-grip_q[0]*s,
                         grip_q[2]*c+grip_q[3]*s,
                         grip_q[3]*c-grip_q[2]*s};
        const float *q=test_mode?grip_q:palm_q;
        const float offset[3]={0,.045f,.06f};float rotated[3];
        rotate(q,offset,rotated);
        for(int i=0;i<3;i++)surface.position_m[i]=grip->position_m[i]+rotated[i];
        /* Panel tilt -35 degrees around grip-local X. */
        float a=sinf(-35.0f*3.14159265f/360),b=cosf(-35.0f*3.14159265f/360);
        surface.orientation_xyzw[0]=q[3]*a+q[0]*b;
        surface.orientation_xyzw[1]=q[1]*b+q[2]*a;
        surface.orientation_xyzw[2]=q[2]*b-q[1]*a;
        surface.orientation_xyzw[3]=q[3]*b-q[0]*a;
        if(!test_mode) {
            /* Follow the hand's position only; the renderer faces both surfaces
             * at the same predicted head center, upright in LOCAL space. */
            for(int i=0;i<3;i++)surface.position_m[i]=grip->position_m[i];
            surface.position_m[1]+=.06f;
            memset(surface.orientation_xyzw,0,sizeof surface.orientation_xyzw);
            surface.orientation_xyzw[3]=1;
            surface.flags=PSX_MOD_OPENXR_SURFACE_FACE_HEAD;
        }
        if(psx_mod_openxr_surface(&surface) && !test_mode) {
            const char *replace=getenv("PSX_VR_WRIST_REPLACE");
            if(replace && atoi(replace)!=0)replacement_mask|=1u<<side;
        }
    }
}
#else
void moh_vr_hud_prepare(void) {}
int moh_vr_hud_wants_native(void) {return 0;}
int moh_vr_hud_set_native(const uint8_t *black,const uint8_t *white) {(void)black;(void)white;return 0;}
void moh_vr_hud_frame(float aspect) {(void)aspect;}
unsigned moh_vr_hud_replacement_mask(void) {return 0;}
#endif
