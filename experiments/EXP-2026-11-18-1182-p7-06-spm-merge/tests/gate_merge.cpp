// Reference: the native loop, faithfully including the priority queue and the lazy skip,
// so the comparison is against a different code path and not against the same scan.
#include <cstdio>
#include <cstdint>
#include <queue>
#include <vector>
#include <random>
extern "C" int64_t g_sm_call(int64_t,const int64_t*,const int64_t*,const double*,int64_t,int64_t*,int64_t*,int64_t*,int64_t*);
struct B { int64_t left, right; float score; size_t size;
  struct cmp { bool operator()(B&l,B&r){ return (l.score < r.score) || (l.score == r.score && l.left > r.left); } }; };
static std::vector<int64_t> ref_merge(int n, const std::vector<int64_t>&pl, const std::vector<int64_t>&pr,
                                      const std::vector<double>&ps, int np) {
    std::vector<int64_t> next(n), prev(n); std::vector<char> alive(n, 1);
    for (int i = 0; i < n; ++i) { next[i] = i + 1; prev[i] = i - 1; }
    // map text -> pair index; here the pair index is the identity of the merged text
    auto find_pair = [&](int64_t l, int64_t r) -> int {
        for (int k = 0; k < np; ++k) if (pl[k] == l && pr[k] == r) return k;
        return -1;
    };
    std::priority_queue<B, std::vector<B>, B::cmp> q;
    auto try_add = [&](int64_t l, int64_t r) {
        if (l < 0 || r < 0 || l >= n || r >= n) return;
        if (!alive[l] || !alive[r] || next[l] != r) return;
        int k = find_pair(l, r);
        if (k < 0) return;
        q.push(B{l, r, (float) ps[k], 0});
    };
    for (int i = 0; i + 1 < n; ++i) try_add(i, i + 1);
    std::vector<int64_t> order;
    while (!q.empty()) {
        B b = q.top(); q.pop();
        if (!alive[b.left] || !alive[b.right] || next[b.left] != b.right) continue; // stale
        int k = find_pair(b.left, b.right);
        if (k < 0) continue;
        order.push_back(k);
        const int64_t l = b.left, r = b.right;
        alive[r] = 0; next[l] = next[r];
        if (next[r] < n) prev[next[r]] = l;
        try_add(prev[l], l);
        try_add(l, next[l]);
    }
    return order;
}
int main(){
    std::mt19937 rng(12345);
    int fails = 0, trials = 300;
    for (int t = 0; t < trials; ++t) {
        int n = 3 + (int)(rng() % 6);
        // list every adjacent-or-not pair with a random score; duplicates impossible
        std::vector<int64_t> pl, pr; std::vector<double> ps;
        for (int l = 0; l < n; ++l) for (int r = l + 1; r < n; ++r) {
            if (rng() % 2) continue;
            pl.push_back(l); pr.push_back(r); ps.push_back((double)(rng() % 5));
        }
        int np = (int) pl.size();
        if (np == 0) continue;
        auto want = ref_merge(n, pl, pr, ps, np);
        int64_t nx[32], pv[32], al[32], out[32]; int64_t plA[32], prA[32]; double psA[32];
        if (np > 16) continue;
        for (int i = 0; i < 32; ++i) { nx[i]=pv[i]=al[i]=out[i]=-9; plA[i]=prA[i]=-9; psA[i]=0; }
        for (int i = 0; i < np; ++i) { plA[i]=pl[i]; prA[i]=pr[i]; psA[i]=ps[i]; }
        int64_t got = g_sm_call(n, plA, prA, psA, np, nx, pv, al, out);
        bool ok = (got == (int64_t) want.size());
        for (size_t i = 0; ok && i < want.size(); ++i) if (out[i] != want[i]) ok = false;
        if (!ok) {
            fails++;
            printf("MISMATCH trial %d n=%d np=%d got=%lld want=%zu\n  got:", t, n, np, (long long) got, want.size());
            for (int i = 0; i < got && i < 32; ++i) printf(" %lld", (long long) out[i]);
            printf("\n  want:");
            for (auto v : want) printf(" %lld", (long long) v);
            printf("\n");
            if (fails > 4) break;
        }
    }
    printf("spm merge: trials=%d fails=%d\n", trials, fails);
    return fails ? 1 : 0;
}
