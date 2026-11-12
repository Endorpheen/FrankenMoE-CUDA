#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dequantize_row_q6_K(const void * x, float * y, int64_t k);
void fg311q_decode_ref(const void * src, float * dst, int64_t k);

static uint64_t rng = 0x243f6a8885a308d3ull;
static uint32_t next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 32); }

static int checks = 0, failures = 0;
static void ck(int ok, const char * what, int nb, int i) {
    checks++;
    if (!ok) { failures++; if (failures < 6) printf("FAIL %s nb=%d i=%d\n", what, nb, i); }
}

int main(void) {
    const int sizes[] = { 256, 512, 2560, 4096 };
    for (unsigned s = 0; s < sizeof(sizes)/sizeof(sizes[0]); ++s) {
        const int k = sizes[s], nb = k / 256;
        uint8_t * x = malloc((size_t) nb * 210);
        float * ref = malloc((size_t) k * 4), * ours = malloc((size_t) k * 4);
        for (int i = 0; i < nb; ++i) {
            uint8_t * b = x + i * 210;
            for (int j = 0; j < 1; ++j) { uint16_t h = (uint16_t)(next() & 0x3fff) | (j == 0 ? 0x0c00 : 0); memcpy(b + j*2, &h, 2); }
            for (int j = 4; j < 210; ++j) b[j] = (uint8_t)(next() & 0xff);
            if (i == nb - 1) memset(b, 0, 210);                  // degenerate super-block
        }
        memset(ours, 0x7f, (size_t) k * 4);
        dequantize_row_q6_K(x, ref, k);
        fg311q_decode_ref(x, ours, k);
        for (int i = 0; i < k; ++i) ck(memcmp(&ref[i], &ours[i], 4) == 0, "float bits", nb, i);
        free(x); free(ref); free(ours);
    }
    printf("EXP311_Q6K checks=%d failures=%d\n", checks, failures);
    return failures != 0;
}
