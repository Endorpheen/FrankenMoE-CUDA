// Reference: the native condition, transcribed, with the divide guarded exactly as there.
#include <cstdio>
#include <cstdint>
#include <random>
extern "C" int64_t g_rej_call(double,double,double);
static int ref(double p_x, double q_x, double u) {
    return (q_x > 0.0f && (p_x >= q_x || u < p_x / q_x)) ? 1 : 0;
}
int main(){
    std::mt19937 rng(33);
    std::uniform_real_distribution<double> d(0.0,1.0);
    int fails=0, cases=0;
    for (int i=0;i<5000;++i){
        double q = d(rng), p = d(rng), u = d(rng);
        int64_t got = g_rej_call(p,q,u); int want = ref(p,q,u);
        cases++;
        if (got != want) { fails++; if (fails<=3) printf("MISMATCH p=%.6f q=%.6f u=%.6f got=%lld want=%d\n",p,q,u,(long long)got,want); }
    }
    // boundaries that separate a plausible rewrite from the real rule
    struct C { const char * n; double p,q,u; int want; };
    const C cs[] = {
        {"q zero never accepts",        0.9, 0.0, 0.0, 0},
        {"p above q accepts",           0.6, 0.5, 0.99, 1},
        {"p equals q accepts",          0.5, 0.5, 0.99, 1},
        {"u below ratio accepts",       0.2, 0.8, 0.24, 1},
        {"u above ratio rejects",       0.2, 0.8, 0.26, 0},
        {"u equals ratio rejects",      0.2, 0.8, 0.25, 0},
    };
    for (const auto & c : cs) {
        int64_t got = g_rej_call(c.p,c.q,c.u);
        cases++;
        if (got != c.want) { fails++; printf("BOUNDARY %s got=%lld want=%d\n", c.n, (long long)got, c.want); }
    }
    printf("reject accept: cases=%d fails=%d\n", cases, fails);
    return fails?1:0;
}
