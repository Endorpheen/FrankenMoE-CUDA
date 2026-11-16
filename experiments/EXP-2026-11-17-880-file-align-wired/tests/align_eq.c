#include <stdio.h>
#include <stdint.h>
int64_t fg377s_plan(int64_t, int64_t, int64_t, int64_t, int64_t *);
int main(void) {
    const int64_t al[] = { 1, 8, 16, 4096 };
    const int64_t offs[] = { 0, 1, 7, 15, 4095, 4096, 100000 };
    const int64_t szs[] = { 0, 1, 7, 4096, 100000, 1000000 };
    int bad = 0, n = 0;
    for (unsigned i = 0; i < sizeof al/sizeof *al; ++i)
    for (unsigned j = 0; j < sizeof offs/sizeof *offs; ++j)
    for (unsigned k = 0; k < sizeof szs/sizeof *szs; ++k) {
        const int64_t a = al[i], o = offs[j], z = szs[k], cap = 1 << 20;
        int64_t p[4] = {0,0,0,0};
        fg377s_plan(o, z, a, cap, p);
        const int64_t nao = o & ~(a - 1);
        const int64_t nskip = o - nao;
        const int64_t nread = ((o + z + a - 1) & ~(a - 1)) - nao;
        ++n;
        if (p[0] != nao || p[2] != nskip || p[1] != nread) {
            if (++bad <= 3) printf("MISMATCH a=%lld o=%lld z=%lld: goose[%lld,%lld,%lld] native[%lld,%lld,%lld]\n",
                (long long)a,(long long)o,(long long)z,
                (long long)p[0],(long long)p[1],(long long)p[2],
                (long long)nao,(long long)nread,(long long)nskip);
        }
    }
    printf("FG377S_EQ cases=%d mismatches=%d %s\n", n, bad, bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
