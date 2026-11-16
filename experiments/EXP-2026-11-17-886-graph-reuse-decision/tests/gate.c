#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int64_t gr_check(const int64_t *, const int64_t *, const int64_t *, int64_t);
int main(void) {
    srand(12345);
    int bad = 0, n = 0;
    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t nf = 1 + rand() % 8;
        int64_t p[8], w[8], h[8];
        long c = 1;
        for (int64_t i = 0; i < nf; ++i) {
            p[i] = rand() % 2;
            w[i] = rand() % 4;
            h[i] = (rand() % 3) ? w[i] : rand() % 4;
            c &= (!p[i]) || (w[i] == h[i]);
        }
        const int64_t g = gr_check(p, w, h, nf);
        ++n;
        if ((g != 0) != (c != 0)) {
            if (++bad <= 3) printf("MISMATCH nf=%lld goose=%lld native=%ld\n", (long long)nf, (long long)g, c);
        }
    }
    printf("GR_EQ cases=%d mismatches=%d %s\n", n, bad, bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
