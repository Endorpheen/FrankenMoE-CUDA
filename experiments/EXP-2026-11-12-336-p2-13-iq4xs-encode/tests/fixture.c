#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void quantize_row_iq4_xs_ref(const float * x, void * y, int64_t k);
void fg336q_enter(const float * x, uint8_t * y, int64_t k);
void fg336q_scales_enter(const float * x, float * out);
static int nearest_ref(float f){float v=f+12582912.f;int i;memcpy(&i,&v,4);return (i&0x007fffff)-0x00400000;}
#define BLOCK 136
static uint64_t rng = 0x243f6a8885a308d3ull;
static uint32_t next(void){ rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; return (uint32_t)(rng>>32); }
static float frand(void){ return (float)((int32_t)(next() & 0xffffff) - 0x800000) / 1048576.0f; }
int main(void) {
    int checks = 0, failures = 0;
    const int sizes[] = { 256, 512, 2560, 4096 };
    for (unsigned s = 0; s < 4; ++s) {
        const int k = sizes[s], nb = k/256;
        float * x = malloc((size_t)k*4);
        uint8_t * r = malloc((size_t)nb*BLOCK), * g = malloc((size_t)nb*BLOCK);
        for (int i = 0; i < k; ++i) x[i] = frand() * 4.0f;
        if (s == 3) { for (int q = 0; q < 32; ++q) x[k-32+q] = 0.01f * (float)(q - 16); }
        if (s == 2) { x[0] = 120.0f; x[1] = -110.0f; }
        memset(r, 0xAA, (size_t)nb*BLOCK); memset(g, 0x55, (size_t)nb*BLOCK);
        quantize_row_iq4_xs_ref(x, r, k);
        fg336q_enter(x, g, k);
        checks += nb*BLOCK;
        for (int i = 0; i < nb*BLOCK; ++i) if (r[i] != g[i]) {
            failures++;
            if (failures < 3) {
                const int blk = i/BLOCK;
                float o[32]; for (int q = 0; q < 32; ++q) o[q] = -999.f;
                fg336q_scales_enter(x + blk*256, o);
                printf("FAIL byte %d block %d ref=%02x ours=%02x\n", i%BLOCK, blk, r[i], g[i]);
                float amax = 0, ms = 0;
                for (int ib = 0; ib < 8; ++ib) { float ad = fabsf(o[ib]); if (ad > amax) { amax = ad; ms = o[ib]; } }
                float dsup = -ms/32.0f, idsup = dsup ? 1.0f/dsup : 0.f;
                float sd = dsup, od = o[16];
                printf("   dsup ref=%.9g ours=%.9g %s\n", sd, od, memcmp(&sd,&od,4)?"DIFF":"");
                for (int ib = 0; ib < 8; ++ib) {
                    int rl = nearest_ref(idsup*o[ib]); if (rl > 31) rl = 31; if (rl < -32) rl = -32;
                    int ol = (int)o[8+ib];
                    if (rl != ol) printf("   l0 ib=%d ref=%d ours=%d DIFF\n", ib, rl, ol);
                }
                printf("   block bytes ref: "); for (int q = 0; q < 8; ++q) printf("%02x ", r[blk*BLOCK+q]); printf("\n");
                printf("   block bytes our: "); for (int q = 0; q < 8; ++q) printf("%02x ", g[blk*BLOCK+q]); printf("\n");
            }
        }
        free(x); free(r); free(g);
    }
    printf("EXP336_IQ4XS_ENC checks=%d failures=%d\n", checks, failures);
    return failures != 0;
}
