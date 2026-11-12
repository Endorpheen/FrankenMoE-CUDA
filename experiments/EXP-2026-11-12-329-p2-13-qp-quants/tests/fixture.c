/* Differential gate for the Goose make_qp_quants.
 *
 * ORACLE CAVEAT: make_qp_quants is static in libggml-base, so it cannot be called from outside. The oracle
 * below is a verbatim copy of the checked-in source. That makes this a transcription check, not an
 * artifact check; the artifact-based acceptance for this helper is the end-to-end Q4_K encoder gate, which
 * will compare against the exported quantize_row_q4_K_ref.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define GROUP_MAX_EPS 1e-15f

static int nearest_int(float fval) { float val = fval + 12582912.f; int i; memcpy(&i, &val, 4); return (i & 0x007fffff) - 0x00400000; }

static float make_qp_quants(int n, int nmax, const float * x, uint8_t * L, const float * quant_weights) {
    float max = 0;
    for (int i = 0; i < n; ++i) max = MAX(max, x[i]);
    if (max < GROUP_MAX_EPS) { for (int i = 0; i < n; ++i) L[i] = 0; return 0.f; }
    float iscale = nmax / max;
    for (int i = 0; i < n; ++i) L[i] = nearest_int(iscale * x[i]);
    float scale = 1/iscale;
    float best_mse = 0;
    for (int i = 0; i < n; ++i) { float diff = x[i] - scale*L[i]; float w = quant_weights[i]; best_mse += w*diff*diff; }
    for (int is = -4; is <= 4; ++is) {
        if (is == 0) continue;
        float iscale_is = (0.1f*is + nmax)/max;
        float scale_is = 1/iscale_is;
        float mse = 0;
        for (int i = 0; i < n; ++i) { int l = nearest_int(iscale_is*x[i]); l = MIN(nmax, l); float diff = x[i] - scale_is*l; float w = quant_weights[i]; mse += w*diff*diff; }
        if (mse < best_mse) { best_mse = mse; iscale = iscale_is; }
    }
    float sumlx = 0, suml2 = 0;
    for (int i = 0; i < n; ++i) { int l = nearest_int(iscale * x[i]); l = MIN(nmax, l); L[i] = l; float w = quant_weights[i]; sumlx += w*x[i]*l; suml2 += w*l*l; }
    for (int itry = 0; itry < 5; ++itry) {
        int n_changed = 0;
        for (int i = 0; i < n; ++i) {
            float w = quant_weights[i];
            float slx = sumlx - w*x[i]*L[i];
            float sl2 = suml2 - w*L[i]*L[i];
            if (slx > 0 && sl2 > 0) {
                int new_l = nearest_int(x[i]*sl2/slx);
                new_l = MIN(nmax, new_l);
                if (new_l != L[i]) {
                    slx += w*x[i]*new_l; sl2 += w*new_l*new_l;
                    if (slx*slx*suml2 > sumlx*sumlx*sl2) { L[i] = new_l; sumlx = slx; suml2 = sl2; ++n_changed; }
                }
            }
        }
        if (!n_changed) break;
    }
    return suml2 > 0.0f ? sumlx/suml2 : 0.0f;
}

float fg329q_enter(int64_t n, int64_t nmax, const float * x, uint8_t * L, const float * w);

static uint64_t rng = 0x243f6a8885a308d3ull;
static uint32_t next(void){ rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; return (uint32_t)(rng>>32); }
static float frand(void){ return (float)((int32_t)(next() & 0xffffff) - 0x800000) / 1048576.0f; }

int main(void) {
    int checks = 0, failures = 0;
    const int ns[] = { 8, 16, 32, 64 };
    for (int t = 0; t < 400; ++t) {
        const int n = ns[t % 4], nmax = (t % 2) ? 63 : 15;
        float x[64], w[64];
        uint8_t lr[64], lg[64];
        for (int i = 0; i < n; ++i) { x[i] = fabsf(frand()) * 3.0f; w[i] = 0.1f + fabsf(frand()); }
        if (t % 17 == 0) for (int i = 0; i < n; ++i) x[i] = 0.0f;
        if (t % 3 == 0) { x[0] *= 40.0f; w[0] *= 25.0f; }
        if (t % 5 == 0) { for (int i = 0; i < n; ++i) w[i] = (i % 2) ? 0.01f : 3.0f; }
        if (t % 7 == 0) { for (int i = 0; i < n; ++i) x[i] = x[i] * x[i]; }
        memset(lr, 0xAA, sizeof lr); memset(lg, 0xAA, sizeof lg);
        const float r = make_qp_quants(n, nmax, x, lr, w);
        const float g = fg329q_enter(n, nmax, x, lg, w);
        checks += 1 + n;
        if (memcmp(&r, &g, 4)) { failures++; if (failures < 5) printf("FAIL scale n=%d t=%d ref=%g ours=%g\n", n, t, r, g); }
        for (int i = 0; i < n; ++i) if (lr[i] != lg[i]) { failures++; if (failures < 5) printf("FAIL L[%d] n=%d t=%d ref=%d ours=%d\n", i, n, t, lr[i], lg[i]); }
    }
    printf("EXP329_QP checks=%d failures=%d\n", checks, failures);
    return failures != 0;
}
