#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dequantize_row_nvfp4(const void * x, float * y, int64_t k);
void fg327q_decode_ref(const void * src, float * dst, int64_t k);

#define BLOCK 36
#define QK 64

static uint64_t rng = 0x243f6a8885a308d3ull;
static uint32_t next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 32); }

static int checks = 0, failures = 0;
static void ck(int ok, int nb, int i) {
    checks++;
    if (!ok) { failures++; if (failures < 6) printf("FAIL nb=%d i=%d\n", nb, i); }
}

int main(void) {
    const int sizes[] = { 256, 512, 2560, 4096 };
    for (unsigned s = 0; s < sizeof(sizes)/sizeof(sizes[0]); ++s) {
        const int k = sizes[s], nb = k / QK;
        uint8_t * x = malloc((size_t) nb * BLOCK);
        float * ref = malloc((size_t) k * 4), * ours = malloc((size_t) k * 4);
        for (int i = 0; i < nb; ++i) {
            uint8_t * b = x + i * BLOCK;
            for (int j = 0; j < BLOCK; ++j) b[j] = (uint8_t)(next() & 0xff);
            if (i == nb - 1) memset(b, 0, BLOCK);               // degenerate block
        }
        memset(ours, 0x7f, (size_t) k * 4);
        dequantize_row_nvfp4(x, ref, k);
        fg327q_decode_ref(x, ours, k);
        for (int i = 0; i < k; ++i) ck(memcmp(&ref[i], &ours[i], 4) == 0, nb, i);
        free(x); free(ref); free(ours);
    }
    printf("EXP327_NVFP4 checks=%d failures=%d\n", checks, failures);
    return failures != 0;
}
