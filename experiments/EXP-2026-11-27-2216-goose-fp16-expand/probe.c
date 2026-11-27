#include "bench.h"
#include "ggml.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


static unsigned long long now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static uint16_t input[65536];
static float ref[65536], got[65536];
int main(void) {
    goose_init(0, NULL);
    for (unsigned i = 0; i < 65536; ++i) input[i] = i;
    ggml_fp16_to_fp32_row(input, ref, 65536);
    expand_row((sl_u16){input,65536}, (sl_f32){got,65536});
    for (unsigned i = 0; i < 65536; ++i) {
        if (memcmp(ref+i,got+i,sizeof *ref)) {
            fprintf(stderr,"first mismatch %04x\n",i);
            return 1;
        }
    }
    puts("Exact parity: 65536 patterns");
    for (unsigned i = 0; i < 65536; ++i) input[i] = 0x0400 + ((i * 257u) % 0x7800);
    for (int pass = 0; pass < 4; ++pass) {
        const int candidate = pass == 1 || pass == 2;
        unsigned long long started = now();
        for (int i = 0; i < 1500; ++i) {
            if (candidate) expand_row((sl_u16){input,65536},(sl_f32){got,65536});
            else reference_row((sl_u16){input,65536},(sl_f32){got,65536});
        }
        printf("%s %.3f ns/value\n",candidate?"Goose":"GGML",(now()-started)/(1500.0*65536));
    }
}
