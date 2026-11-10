// Differential gate for llama_kv_cache::set_input_k_shift.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg230s_fill_shift(int64_t,const int32_t*,const int32_t*const*,
                                       const int32_t*const*,int32_t*); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,size_t at){++checks;
  if(!ok){if(failures<15)printf("FAIL %s at %zu goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

static void run_case(const std::vector<int32_t>& sizes,
                     const std::vector<std::vector<int32_t>>& shift,
                     const std::vector<std::vector<int32_t>>& empty,
                     const char* tag) {
    int64_t total = 0; for (auto v : sizes) total += v;
    std::vector<int32_t> ref((size_t)total, -999);
    int64_t row = 0;
    for (size_t s = 0; s < sizes.size(); ++s) {
        for (int32_t i = 0; i < sizes[s]; ++i)
            ref[(size_t)(row+i)] = empty[s][(size_t)i] ? 0 : shift[s][(size_t)i];
        row += sizes[s];
    }
    std::vector<const int32_t*> sp, ep;
    for (auto& v : shift) sp.push_back(v.data());
    for (auto& v : empty) ep.push_back(v.data());
    std::vector<int32_t> got((size_t)total + 1, -999);
    const int64_t n = fg230s_fill_shift((int64_t)sizes.size(),
                                        sizes.empty()?nullptr:sizes.data(),
                                        sp.empty()?nullptr:sp.data(),
                                        ep.empty()?nullptr:ep.data(), got.data());
    ck(n == total, "count", n, total, 0);
    for (size_t i = 0; i < ref.size(); ++i) ck(got[i]==ref[i], tag, got[i], ref[i], i);
    ck(got[ref.size()]==-999, "no overrun", got[ref.size()], -999, ref.size());
}

int main(void){
    run_case({}, {}, {}, "empty");
    run_case({3}, {{5,-7,9}}, {{0,0,0}}, "one-stream");
    run_case({3}, {{5,-7,9}}, {{1,0,1}}, "empties");         // empty cells must write 0, not the value
    run_case({2,4}, {{1,2},{3,4,5,6}}, {{0,0},{0,0,0,0}}, "two-streams"); // per-stream widths
    run_case({1,5}, {{8},{1,1,1,1,1}}, {{0},{1,1,1,1,1}}, "ragged");     // row offset uses own sizes
    std::mt19937 rng(230011);
    for (int t = 0; t < 3000; ++t) {
        const int ns = (int)(rng()%4);
        std::vector<int32_t> sizes(ns);
        std::vector<std::vector<int32_t>> sh(ns), em(ns);
        for (int s=0;s<ns;++s) {
            sizes[s] = (int32_t)(rng()%6);
            sh[s].resize((size_t)sizes[s]); em[s].resize((size_t)sizes[s]);
            for (int i=0;i<sizes[s];++i){ sh[s][(size_t)i]=(int32_t)(rng()%200)-100; em[s][(size_t)i]=(int32_t)(rng()%2); }
        }
        run_case(sizes, sh, em, "rand");
    }
    printf("FG230S_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP230_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP230_LOCAL_GATES_PASS\n"); return 0;
}
