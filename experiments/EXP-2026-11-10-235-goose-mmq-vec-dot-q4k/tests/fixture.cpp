// Differential gate for vec_dot_q4_K_q8_1_impl_mmq against a direct transcription of the native body.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <vector>
#include <cmath>
extern "C" { float fg235v_vd_q4k(const int32_t*,const int32_t*,const uint8_t*,const uint8_t*,
                                 const float*,const float*,float,float,int64_t); }

static inline int dp4a_ref(int a, int b, int c) {
    const int8_t * as = (const int8_t *) &a;
    const int8_t * bs = (const int8_t *) &b;
    return c + as[0]*bs[0] + as[1]*bs[1] + as[2]*bs[2] + as[3]*bs[3];
}
static float ref_vd(const int32_t* v, const int32_t* u, const uint8_t* sc, const uint8_t* m,
                    const float* dsx, const float* dsy, float dm4x, float dm4y, int64_t n_i) {
    float sumf_d = 0.0f, sumf_m = 0.0f;
    for (int i = 0; i < n_i; ++i) {
        int sumi_d = 0;
        for (int j = 0; j < 8; ++j)
            sumi_d = dp4a_ref((v[j] >> (4*i)) & 0x0F0F0F0F, u[i*8 + j], sumi_d);
        sumf_d += dsx[i] * (sc[i] * (float) sumi_d);
        sumf_m += dsy[i] * (float) m[i];
    }
    return dm4x*sumf_d - dm4y*sumf_m;
}

static int checks, failures;
static void ck(bool ok,double a,double b,const char*w,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%.9g ref=%.9g\n",w,at,a,b);++failures;}}

int main(void) {
    std::mt19937 rng(235011);
    for (int t = 0; t < 30000; ++t) {
        const int n_i = (int)(rng()%3);                 // 0, 1 or 2 -- the native loop bound is 2
        std::vector<int32_t> v(8), u(16);
        std::vector<uint8_t> sc(4), m(4);
        std::vector<float> dsx(4), dsy(4);
        for (auto& x : v) x = (int32_t) rng();
        for (int i = 0; i < n_i; ++i) {
            // u holds signed bytes in [-127,127]; build them as packed int32 words
            int32_t w = 0;
            for (int k = 0; k < 4; ++k) {
                const int8_t b = (int8_t)((int)(rng()%255) - 127);
                w |= ((int32_t)(uint8_t)b) << (8*k);
            }
            u[i*8+0] = w;
            u[i*8+1] = (int32_t) rng();
        }
        for (int i = 0; i < 4; ++i) { sc[i] = (uint8_t)(rng()%256); m[i] = (uint8_t)(rng()%256);
                                      dsx[i] = (float)((int)(rng()%2001)-1000)/64.0f;
                                      dsy[i] = (float)((int)(rng()%2001)-1000)/64.0f; }
        const float dm4x = (float)((int)(rng()%2001)-1000)/512.0f;
        const float dm4y = (float)((int)(rng()%2001)-1000)/512.0f;
        const double got = fg235v_vd_q4k(v.data(), u.data(), sc.data(), m.data(),
                                         dsx.data(), dsy.data(), dm4x, dm4y, n_i);
        const double ref = ref_vd(v.data(), u.data(), sc.data(), m.data(), dsx.data(), dsy.data(),
                                  dm4x, dm4y, n_i);
        // host reference is float arithmetic in the same order; compare with an absolute threshold
        const double tol = 1e-3 * (1.0 + fabs(ref));
        ck(fabs(got-ref) <= tol, got, ref, "value", t);
    }
    printf("FG235V_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP235_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP235_LOCAL_GATES_PASS\n"); return 0;
}
