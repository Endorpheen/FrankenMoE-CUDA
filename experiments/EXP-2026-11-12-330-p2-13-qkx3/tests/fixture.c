/* Differential gate for the Goose make_qkx3_quants.
 * ORACLE CAVEAT: the helper is static in libggml-base; the oracle is the function text extracted mechanically
 * from the checked-in source (never retyped), so this is a transcription check. The artifact-based acceptance is
 * the end-to-end Q4_K encoder gate against the exported quantize_row_q4_K_ref.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "oracle.c"

float fg330q_enter(int64_t n, int64_t nmax, const float * x, const float * w, int64_t hasw,
                   uint8_t * L, float * the_min, uint8_t * Laux,
                   float rmin, float rdelta, int64_t nstep, int64_t use_mad);

static uint64_t rng = 0x243f6a8885a308d3ull;
static uint32_t next(void){ rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; return (uint32_t)(rng>>32); }
static float frand(void){ return (float)((int32_t)(next() & 0xffffff) - 0x800000) / 1048576.0f; }

int main(void) {
    int checks = 0, failures = 0;
    for (int t = 0; t < 600; ++t) {
        const int n = 32, nmax = (t & 1) ? 15 : 63;
        const int hasw = (t % 3) == 0;
        float x[32], w[32];
        uint8_t l1[32], l2[32], a1[32], a2[32];
        for (int i = 0; i < n; ++i) { x[i] = frand() * 2.0f; w[i] = 0.05f + fabsf(frand()); }
        if (t % 11 == 0) for (int i = 0; i < n; ++i) x[i] = 0.5f;
        if (t % 13 == 0) { x[0] = 4.0f; }
        const float rmin = -0.9f, rdelta = 0.05f;
        const int nstep = 36, use_mad = 0;
        float m1 = 0, m2 = 0;
        memset(l1, 0xAA, sizeof l1); memset(l2, 0x55, sizeof l2);
        const float s1 = make_qkx3_quants(n, nmax, x, hasw ? w : NULL, l1, &m1, a1, rmin, rdelta, nstep, use_mad);
        const float s2 = fg330q_enter(n, nmax, x, w, hasw, l2, &m2, a2, rmin, rdelta, nstep, use_mad);
        checks += 2 + 2*n;
        if (memcmp(&s1, &s2, 4)) { failures++; if (failures < 4) printf("FAIL scale t=%d ref=%.9g ours=%.9g\n", t, s1, s2); }
        if (memcmp(&m1, &m2, 4)) { failures++; if (failures < 4) printf("FAIL min t=%d ref=%.9g ours=%.9g\n", t, m1, m2); }
        for (int i = 0; i < n; ++i) if (l1[i] != l2[i]) { failures++; if (failures < 4) printf("FAIL L[%d] t=%d ref=%d ours=%d\n", i, t, l1[i], l2[i]); }
    }
    printf("EXP330_QKX3 checks=%d failures=%d\n", checks, failures);
    return failures != 0;
}
