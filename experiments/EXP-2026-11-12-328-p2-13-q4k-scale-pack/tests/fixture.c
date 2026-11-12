#include <stdint.h>
#include <stdio.h>
#include <string.h>
int fg328q_enter(const uint8_t * lslm, uint8_t * scales);

static uint64_t rng = 0x9e3779b97f4a7c15ull;
static uint32_t next(void){ rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; return (uint32_t)(rng>>32); }

/* the decoder's read rule, written here so the check is independent of the port */
static void unk4(int j, const uint8_t * q, uint8_t * d, uint8_t * m) {
    if (j < 4) { *d = q[j] & 63; *m = q[j+4] & 63; }
    else { *d = (q[j+4] & 0xF) | ((q[j-4] >> 6) << 4); *m = (q[j+4] >> 4) | ((q[j] >> 6) << 4); }
}

int main(void) {
    int checks = 0, failures = 0;
    for (int t = 0; t < 1000; ++t) {
        uint8_t lslm[16], scales[12] = {0};
        for (int i = 0; i < 16; ++i) lslm[i] = (uint8_t)(next() & 63);
        fg328q_enter(lslm, scales);
        for (int j = 0; j < 8; ++j) {
            uint8_t d, m; unk4(j, scales, &d, &m);
            checks += 2;
            if (d != lslm[j])     { failures++; if (failures < 5) printf("FAIL scale j=%d want=%d got=%d (t=%d)\n", j, lslm[j], d, t); }
            if (m != lslm[8 + j]) { failures++; if (failures < 5) printf("FAIL min   j=%d want=%d got=%d (t=%d)\n", j, lslm[8+j], m, t); }
        }
    }
    printf("EXP328_Q4K_SCALES checks=%d failures=%d\n", checks, failures);
    return failures != 0;
}
