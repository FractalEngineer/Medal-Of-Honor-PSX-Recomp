#include "moh_vr_aim.h"
#include "vr_pose_math.h"
#include <math.h>
#include <string.h>
int moh_vr_matrix_inverse(const double m[9],double inverse[9]) {
    if(!m || !inverse)return 0;
    for(int i=0;i<9;i++)if(!isfinite(m[i]))return 0;
    double r[9]={m[4]*m[8]-m[5]*m[7],m[2]*m[7]-m[1]*m[8],m[1]*m[5]-m[2]*m[4],
                 m[5]*m[6]-m[3]*m[8],m[0]*m[8]-m[2]*m[6],m[2]*m[3]-m[0]*m[5],
                 m[3]*m[7]-m[4]*m[6],m[1]*m[6]-m[0]*m[7],m[0]*m[4]-m[1]*m[3]};
    double det=m[0]*r[0]+m[1]*r[3]+m[2]*r[6];
    if(!isfinite(det) || fabs(det)<.01 || fabs(det)>64)return 0;
    for(int i=0;i<9;i++){r[i]/=det;if(!isfinite(r[i]) || fabs(r[i])>8)return 0;}
    memcpy(inverse,r,sizeof r);return 1;
}
int moh_vr_aim_shot(const PSXModOpenXRHands *h,const double inv[9],
                    const double tr[3],double units,int32_t position[3],int32_t angles[2]) {
    if(!h || !inv || !tr || !position || !angles || h->struct_size!=sizeof *h ||
       !h->focused || !h->origin_valid || h->age_ms>150)return 0;
    const PSXModTrackedPose *v=&h->pose[1][PSX_MOD_XR_AIM_POSE];
    if(!v->active || (v->flags&3u)!=3u)return 0;
    double q[4],p[3],r[9],t[3];
    for(int i=0;i<4;i++)q[i]=v->orientation_xyzw[i];
    for(int i=0;i<3;i++)p[i]=v->position_m[i];
    if(!vr_pose_to_transform(q,p,h->origin_orientation_xyzw,h->origin_position_m,
                             units,r,t))return 0;
    int32_t result[3];double d[3]={0};
    for(int i=0;i<3;i++) {
        double x=0;
        for(int j=0;j<3;j++) {
            if(!isfinite(inv[i*3+j]) || !isfinite(tr[j]))return 0;
            x+=inv[i*3+j]*(t[j]-tr[j]);
            d[i]+=inv[i*3+j]*r[j*3+2];
        }
        if(!isfinite(x) || fabs(x)>32767)return 0;
        result[i]=(int32_t)llround(x*65536);
    }
    double length=sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
    if(!isfinite(length) || length<.01)return 0;
    const double turn=4096.0/(2*3.14159265358979323846);
    double pitch=atan2(-d[1],hypot(d[0],d[2]))*turn;
    double yaw=atan2(d[0],d[2])*turn;if(yaw<0)yaw+=4096;
    int32_t a[2]={(int32_t)llround(pitch*65536),(int32_t)llround(yaw*65536)};
    memcpy(position,result,sizeof result);memcpy(angles,a,sizeof a);return 1;
}
