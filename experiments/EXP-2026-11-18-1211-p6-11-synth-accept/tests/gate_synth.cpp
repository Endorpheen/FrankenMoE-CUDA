// Reference: the native condition, transcribed, including the strict comparison.
#include <cstdio>
#include <cstdint>
#include <random>
extern "C" int64_t g_sy_call(double,double,int64_t,int64_t);
static int ref(double prob, double u, int is_replay, int is_eog) {
    const bool accept = is_replay || u < prob;
    if (accept && (!is_replay && is_eog)) return 0;
    return accept ? 1 : 0;
}
int main(){
    std::mt19937 rng(21);
    std::uniform_real_distribution<double> d(0.0,1.0);
    int fails=0, cases=0;
    for (int i=0;i<2000;++i){
        double p = d(rng), u = d(rng);
        int rep = (i%7==0), eog = (i%5==0);
        int64_t got = g_sy_call(p,u,rep,eog);
        int want = ref(p,u,rep,eog);
        cases++;
        if (got != want) { fails++; if (fails<=3) printf("MISMATCH p=%f u=%f rep=%d eog=%d got=%lld want=%d\n", p,u,rep,eog,(long long)got,want); }
    }
    // the boundary: u == p must NOT accept (strict <)
    for (double p : {0.0, 0.25, 0.5, 1.0}) {
        int64_t got = g_sy_call(p, p, 0, 0);
        cases++;
        if (got != 0) { fails++; printf("BOUNDARY MISMATCH u==p==%f got=%lld want=0\n", p, (long long)got); }
    }
    // EOG drafted with a certain probability is still refused
    if (g_sy_call(1.0, 0.0, 0, 1) != 0) { fails++; printf("EOG should be refused\n"); }
    // on replay the drafted EOG is accepted
    if (g_sy_call(0.0, 1.0, 1, 1) != 1) { fails++; printf("replay should accept\n"); }
    cases += 2;
    printf("synth accept: cases=%d fails=%d\n", cases, fails);
    return fails?1:0;
}
