// Reference: the native map semantics, compared as a set of (key,value) -> count.
#include <cstdio>
#include <cstdint>
#include <map>
#include <vector>
#include <algorithm>
#include <random>
extern "C" int64_t g_cu_call(const int64_t*,int64_t,int64_t,int64_t,int64_t,int64_t*,int64_t*,int64_t*,int64_t*,int64_t);
using Key = std::pair<std::vector<int64_t>, int64_t>;
static std::map<Key,int64_t> ref(const std::vector<int64_t>& inp, int nnew, int mn, int mx) {
    std::map<Key,int64_t> m;
    const int64_t n = (int64_t) inp.size();
    for (int64_t size = mn; size <= mx; ++size) {
        const int64_t start = std::max(n - nnew, size);
        for (int64_t i = start; i < n; ++i) {
            std::vector<int64_t> k(inp.begin() + (i - size), inp.begin() + i);
            m[Key{k, inp[i]}]++;
        }
    }
    return m;
}
int main(){
    std::mt19937 rng(5);
    int fails=0, trials=200;
    const int MAXP=256, MAXNG=4;
    for (int t=0;t<trials;++t){
        int n = 2 + (int)(rng()%12);
        std::vector<int64_t> inp(n);
        for (auto& v: inp) v = (int64_t)(rng()%3);
        int nnew = 1 + (int)(rng()%n);
        int mn = 1 + (int)(rng()%2), mx = mn + (int)(rng()%3);
        if (mx > MAXNG) mx = MAXNG;
        auto want = ref(inp, nnew, mn, mx);
        static int64_t keys[MAXP*MAXNG], vals[MAXP], cnt[MAXP], szs[MAXP];
        std::fill(keys, keys+MAXP*MAXNG, 0); std::fill(vals, vals+MAXP, 0); std::fill(cnt, cnt+MAXP, 0); std::fill(szs, szs+MAXP, 0);
        int64_t got = g_cu_call(inp.data(), n, nnew, mn, mx, keys, vals, cnt, szs, MAXP);
        std::map<Key,int64_t> mine;
        for (int64_t i = 0; i < got; ++i) {
            if (cnt[i] == 0) continue;
            // the stride is ng_max; the valid part of each key is szs[i] entries
            mine[Key{std::vector<int64_t>(keys + i*mx, keys + i*mx + szs[i]), vals[i]}] += cnt[i];
        }
        if (mine.size() != want.size()) { fails++; if (fails<=3) printf("SIZE MISMATCH t=%d got=%zu want=%zu\n", t, mine.size(), want.size()); continue; }
        for (auto& kv : want) {
            auto it = mine.find(kv.first);
            if (it == mine.end() || it->second != kv.second) { fails++; if (fails<=3) printf("VALUE MISMATCH t=%d\n", t); break; }
        }
    }
    printf("cache update: trials=%d fails=%d\n", trials, fails);
    return fails?1:0;
}
