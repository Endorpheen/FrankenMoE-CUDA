// The native rule re-expressed as a reference, applied to the same sequences.
#include <cstdio>
#include <cstdint>
extern "C" int64_t g_ngram_call(int64_t,int64_t,int64_t,int64_t*);
static void ref(int64_t n_acc, int64_t n_draft, int64_t & n_low, int & reset) {
    reset = 0;
    if (n_draft <= 0) return;
    double f = (double) n_acc / (double) n_draft;
    if (f < 0.25) { n_low++; if (n_low >= 5) { reset = 1; n_low = 0; } } else n_low = 0;
}
int main() {
    // a sequence of accepts per step, applied to both implementations
    struct Step { int64_t nacc, ndraft; };
    const Step seq[] = { {1,8},{1,8},{1,8},{1,8},{1,8},   // five low in a row -> reset on the fifth
                         {1,8},{8,8},{1,8},{1,8},{1,8},{1,8},  // one high clears the streak
                         {0,0},{2,8},{2,8} };          // zero draft is a no-op, 2/8=0.25 is not low
    int64_t glo=0, glo_ref=0; int fails=0, n=(int)(sizeof(seq)/sizeof(seq[0]));
    for (int i=0;i<n;++i) {
        int64_t out[2]={-1,-1};
        int reset=0;
        int64_t r = g_ngram_call(seq[i].nacc, seq[i].ndraft, glo, out);
        ref(seq[i].nacc, seq[i].ndraft, glo_ref, reset);
        if (r != 0 || out[0] != glo_ref || out[1] != reset) {
            fails++; printf("MISMATCH step %d: goose low=%lld reset=%lld, ref low=%lld reset=%d\n",
                i, (long long)out[0], (long long)out[1], (long long)glo_ref, reset);
        }
        glo = out[0];
    }
    printf("ngram accept: steps=%d fails=%d final_low=%lld\n", n, fails, (long long)glo);
    return fails?1:0;
}
