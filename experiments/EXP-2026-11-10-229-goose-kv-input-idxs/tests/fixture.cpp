// Differential gate for llama_kv_cache::set_input_k_idxs (non-transposed V path uses the same formula).
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
extern "C" { int64_t fg229i_fill_idxs(int64_t,int64_t,const int32_t*,const uint32_t*const*,int64_t,int64_t*); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,size_t at){++checks;
  if(!ok){if(failures<15)printf("FAIL %s at %zu goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

static void run_case(const std::vector<int32_t>& strm,
                     const std::vector<std::vector<int32_t>>& idxs,
                     int64_t kv_size, const char* tag) {
    const int64_t ns = (int64_t) strm.size();
    const int64_t size = ns ? (int64_t) idxs[0].size() : 0;
    // reference: stream-major, value = strm[s]*kv_size + idxs[s][i]
    std::vector<int64_t> ref((size_t)(ns*size), -999);
    for (int64_t s = 0; s < ns; ++s)
        for (int64_t i = 0; i < size; ++i)
            ref[(size_t)(s*size+i)] = (int64_t) strm[(size_t)s]*kv_size + idxs[(size_t)s][(size_t)i];

    std::vector<const uint32_t*> ptrs;
    for (auto& v : idxs) ptrs.push_back((const uint32_t*) v.data());
    std::vector<int64_t> got((size_t)(ns*size) + 1, -999);
    const int64_t n = fg229i_fill_idxs(ns, size, ns ? strm.data() : nullptr,
                                       ns ? ptrs.data() : nullptr, kv_size, got.data());
    ck(n == ns*size, "count", n, ns*size, 0);
    for (size_t i = 0; i < ref.size(); ++i) ck(got[i] == ref[i], tag, got[i], ref[i], i);
    ck(got[ref.size()] == -999, "no overrun", got[ref.size()], -999, ref.size());
}

int main(void){
    run_case({}, {}, 64, "empty");
    run_case({0}, {{0}}, 64, "one");
    run_case({0}, {{0, 1, 2, 3}}, 64, "one-stream");
    run_case({0, 1}, {{0,1,2},{3,4,5}}, 128, "two-streams");   // catches i*n_stream+s ordering
    run_case({2, 0}, {{7,8},{9,10}}, 256, "reordered-strm");   // catches using s instead of strm[s]
    run_case({3, 3}, {{1,2},{3,4}}, 32, "same-strm");
    std::mt19937 rng(229011);
    for (int t = 0; t < 4000; ++t) {
        const int ns = (int)(rng() % 4);
        const int size = 1 + (int)(rng() % 5);
        const int64_t kv = 1 + (int64_t)(rng() % 4096);
        std::vector<int32_t> strm(ns);
        std::vector<std::vector<int32_t>> idxs(ns, std::vector<int32_t>(size));
        for (int s = 0; s < ns; ++s) {
            strm[s] = (int32_t)(rng() % 4);
            for (int i = 0; i < size; ++i) idxs[s][i] = (int32_t)(rng() % (unsigned)kv);
        }
        run_case(strm, idxs, kv, "rand");
    }
    printf("FG229I_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP229_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP229_LOCAL_GATES_PASS\n"); return 0;
}
