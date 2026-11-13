// Differential gate for llama_memory_recurrent::seq_add.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits>
#include <random>
#include <vector>
extern "C" { int64_t fg231a_seq_add(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t*); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b){++checks;
  if(!ok){if(failures<15)printf("FAIL %s goose=%lld ref=%lld\n",w,a,b);++failures;}}

// transcription of the native body: returns {apply, newpos}
static std::pair<int,int64_t> ref(int64_t shift,int64_t p0,int64_t p1,int64_t seq_id,int64_t size,
                                  int64_t has_tail,int64_t tail_pos,int64_t tail_has_seq){
    if (shift == 0) return {0, tail_pos};
    if (p0 < 0) p0 = 0;
    if (p1 < 0) p1 = std::numeric_limits<int64_t>::max();
    if (p0 == p1) return {0, tail_pos};
    if (!(0 <= seq_id && seq_id < size)) return {0, tail_pos};
    if (!has_tail) return {0, tail_pos};
    if (!tail_has_seq) return {0, tail_pos};
    if (p0 <= tail_pos && tail_pos < p1) return {1, tail_pos + shift};
    return {0, tail_pos};
}

int main(void){
    const int64_t MAX = std::numeric_limits<int64_t>::max();
    struct C_ { int64_t sh,p0,p1,sid,size,ht,tp,ths; } fixed[] = {
        {0,0,10,0,4,1,5,1},        // shift 0 -> no-op
        {5,0,10,0,4,1,5,1},        // in range -> apply
        {5,0,10,0,4,1,10,1},       // pos == p1 -> NOT in range
        {5,0,10,0,4,1,0,1},        // pos == p0 -> in range
        {5,-1,-1,0,4,1,5,1},       // both negative -> 0..MAX -> apply
        {5,3,3,0,4,1,5,1},         // zero-width -> no-op
        {5,0,10,-1,4,1,5,1},       // seq_id < 0 -> no-op
        {5,0,10,4,4,1,5,1},        // seq_id == size -> no-op
        {5,0,10,0,4,0,5,1},        // no tail -> no-op
        {5,0,10,0,4,1,5,0},        // tail belongs to another seq -> no-op
        {-3,0,10,0,4,1,5,1},       // negative shift
    };
    for (auto& c : fixed) {
        int64_t out[2] = {-777,-777};
        const int64_t got = fg231a_seq_add(c.sh,c.p0,c.p1,c.sid,c.size,c.ht,c.tp,c.ths,out);
        auto e = ref(c.sh,c.p0,c.p1,c.sid,c.size,c.ht,c.tp,c.ths);
        ck(got == e.first, "apply", got, e.first);
        if (e.first) ck(out[0] == e.second, "pos", out[0], e.second);
        else         ck(out[0] == -777, "no write", out[0], -777);
        ck(out[1] == -777, "no overrun", out[1], -777);
    }
    std::mt19937 rng(231011);
    for (int t=0;t<20000;++t) {
        const int64_t sh = (int64_t)(rng()%21) - 10;
        int64_t p0 = (int64_t)(rng()%41) - 20;
        int64_t p1 = (rng()%4==0) ? -1 : (int64_t)(rng()%41) - 20;
        const int64_t sid = (int64_t)(rng()%8) - 2;
        const int64_t size = (int64_t)(rng()%6);
        const int64_t ht = (int64_t)(rng()%2), ths = (int64_t)(rng()%2);
        const int64_t tp = (int64_t)(rng()%41) - 20;
        int64_t out[2] = {-777,-777};
        const int64_t got = fg231a_seq_add(sh,p0,p1,sid,size,ht,tp,ths,out);
        auto e = ref(sh,p0,p1,sid,size,ht,tp,ths);
        ck(got == e.first, "apply", got, e.first);
        if (e.first) ck(out[0]==e.second, "pos", out[0], e.second);
        else ck(out[0]==-777, "no write", out[0], -777);
        (void)MAX;
    }
    printf("FG231A_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP231_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP231_LOCAL_GATES_PASS\n"); return 0;
}
