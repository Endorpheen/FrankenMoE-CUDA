// Differential gate for vec_dot_q6_K_q8_1_impl_mmq against a transcription of the native body.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <cmath>
extern "C" { float fg236k_vd_q6k(const int32_t*,const int32_t*,const uint8_t*,float,const float*,int64_t); }
static inline int dp4a_ref(int a,int b,int c){
    const int8_t*as=(const int8_t*)&a; const int8_t*bs=(const int8_t*)&b;
    return c+as[0]*bs[0]+as[1]*bs[1]+as[2]*bs[2]+as[3]*bs[3];
}
static float ref_vd(const int32_t*v,const int32_t*u,const uint8_t*sc,float d6,const float*d8,int64_t n_steps){
    float sumf_d=0.0f;
    int32_t sc_packed; memcpy(&sc_packed, sc, 4);
    const int8_t* sc_reg=(const int8_t*)&sc_packed;
    for (int i0=0; i0<n_steps*4; i0+=4) {
        int sx=0, sy=0;
        for (int i=i0; i<i0+2; ++i) {
            sx=dp4a_ref(v[2*i+0],u[2*i+0],sx);
            sx=dp4a_ref(v[2*i+1],u[2*i+1],sx);
            sy=dp4a_ref(v[2*i+4],u[2*i+4],sy);
            sy=dp4a_ref(v[2*i+5],u[2*i+5],sy);
        }
        sumf_d += d8[i0/4] * ((float)sc_reg[i0/2+0]*sx + (float)sc_reg[i0/2+1]*sy);
    }
    return d6*sumf_d;
}
static int checks, failures;
static void ck(bool ok,double a,double b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL at %d goose=%.9g ref=%.9g\n",at,a,b);++failures;}}
int main(void){
    std::mt19937 rng(236011);
    for (int t=0;t<30000;++t) {
        const int n_steps=(int)(rng()%3);
        std::vector<int32_t> v(16), u(16);
        std::vector<uint8_t> sc(8);
        std::vector<float> d8(4);
        for (auto& x:v) x=(int32_t)rng();
        for (auto& x:u) x=(int32_t)rng();
        for (auto& x:sc) x=(uint8_t)(rng()%256);
        for (auto& x:d8) x=(float)((int)(rng()%2001)-1000)/64.0f;
        const float d6=(float)((int)(rng()%2001)-1000)/512.0f;
        const double got=fg236k_vd_q6k(v.data(),u.data(),sc.data(),d6,d8.data(),n_steps);
        const double ref=ref_vd(v.data(),u.data(),sc.data(),d6,d8.data(),n_steps);
        ck(std::fabs(got-ref) <= 1e-3*(1.0+std::fabs(ref)), got, ref, t);
    }
    printf("FG236K_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP236_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP236_LOCAL_GATES_PASS\n"); return 0;
}
