// Table gate: each case isolates one condition of the native decision order.
#include <cstdio>
#include <cstdint>
extern "C" int64_t g_compat_call(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
                                 int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
struct Case { const char * name; int64_t vt, abt, bt, aet, et, nt; int64_t vd, abd, bd, aed, ed, nd, maxd; int want; };
int main() {
    const Case cases[] = {
      {"identical",             1,1,1,1,2,100, 1,1,1,1,2,100, 128, 1},
      {"vocab type differs",    1,1,1,1,2,100, 2,1,1,1,2,100, 128, 0},
      {"add_bos differs",       1,1,1,1,2,100, 1,0,1,1,2,100, 128, 0},
      {"bos id differs",        1,1,1,1,2,100, 1,1,7,1,2,100, 128, 0},
      {"add_eos differs",       1,1,1,1,2,100, 1,1,1,0,2,100, 128, 0},
      {"eos id differs",        1,1,1,1,2,100, 1,1,1,1,9,100, 128, 0},
      {"vocab size ok",         1,1,1,1,2,100, 1,1,1,1,2,200, 128, 1},
      {"vocab size too far",    1,1,1,1,2,100, 1,1,1,1,2,300, 128, 0},
      {"no add_bos, ids differ",1,0,1,1,2,100, 1,0,5,1,2,100, 128, 1},
      {"no add_eos, ids differ",1,1,1,0,2,100, 1,1,1,0,9,100, 128, 1},
    };
    int fails = 0, n = (int) (sizeof(cases)/sizeof(cases[0]));
    for (int i = 0; i < n; ++i) {
        const Case & c = cases[i];
        int64_t r = g_compat_call(c.vt,c.abt,c.bt,c.aet,c.et,c.nt, c.vd,c.abd,c.bd,c.aed,c.ed,c.nd, c.maxd);
        if ((int) r != c.want) { fails++; printf("MISMATCH %s got=%lld want=%d\n", c.name, (long long) r, c.want); }
    }
    printf("spec compat: cases=%d fails=%d\n", n, fails);
    return fails ? 1 : 0;
}
