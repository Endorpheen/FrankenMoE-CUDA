// Reference: the native loop, faithfully including the priority queue and the lazy skip,
// so the comparison is against a different code path and not against the same scan.
#include <cstdio>
#include <cstdint>
#include <queue>
#include <vector>
#include <random>
extern "C" int64_t g_sm_call(int64_t,const int64_t*,const int64_t*,const double*,const int64_t*,int64_t,int64_t*,int64_t*,int64_t*,int64_t*);
struct B { int64_t left, right; float score; size_t size;
  struct cmp { bool operator()(B&l,B&r){ return (l.score < r.score) || (l.score == r.score && l.left > r.left); } }; };
static std::vector<int64_t> ref_merge(int n, const std::vector<int64_t>&pl, const std::vector<int64_t>&pr,
                                      const std::vector<double>&ps, const std::vector<int64_t>&psize, int np) {
    std::vector<int64_t> next(n), prev(n); std::vector<int64_t> symn(n, 1);
    for (int i = 0; i < n; ++i) { next[i] = i + 1; prev[i] = i - 1; }
    // map text -> pair index; here the pair index is the identity of the merged text
    auto find_pair = [&](int64_t l, int64_t r) -> int {
        for (int k = 0; k < np; ++k) if (pl[k] == l && pr[k] == r) return k;
        return -1;
    };
    std::priority_queue<B, std::vector<B>, B::cmp> q;
    auto try_add = [&](int64_t l, int64_t r) {
        if (l < 0 || r < 0 || l >= n || r >= n) return;
        if (symn[l] <= 0 || symn[r] <= 0 || next[l] != r) return;
        int k = find_pair(l, r);
        if (k < 0) return;
        q.push(B{l, r, (float) ps[k], (size_t) psize[k]});
    };
    for (int i = 0; i + 1 < n; ++i) try_add(i, i + 1);
    std::vector<int64_t> order;
    while (!q.empty()) {
        B b = q.top(); q.pop();
        if (symn[b.left] <= 0 || symn[b.right] <= 0 ||
            (size_t)(symn[b.left] + symn[b.right]) != b.size) continue; // stale
        int k = find_pair(b.left, b.right);
        if (k < 0) continue;
        order.push_back(k);
        const int64_t l = b.left, r = b.right;
        symn[l] += symn[r]; symn[r] = 0; next[l] = next[r];
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
        // every pair with a random score and a size consistent with single symbols
        std::vector<int64_t> pl, pr; std::vector<double> ps; std::vector<int64_t> psize;
        for (int l = 0; l < n; ++l) for (int r = l + 1; r < n; ++r) {
            if (rng() % 2) continue;
            pl.push_back(l); pr.push_back(r); ps.push_back((double)(rng() % 5));
            psize.push_back(2);   // queued for two single symbols
        }
        int np = (int) pl.size();
        if (np == 0) continue;
        auto want = ref_merge(n, pl, pr, ps, psize, np);
        int64_t nx[32], pv[32], sn[32], out[32]; int64_t plA[32], prA[32], pnA[32]; double psA[32];
        if (np > 16) continue;
        for (int i = 0; i < 32; ++i) { nx[i]=pv[i]=sn[i]=out[i]=-9; plA[i]=prA[i]=-9; pnA[i]=0; psA[i]=0; }
        for (int i = 0; i < np; ++i) { plA[i]=pl[i]; prA[i]=pr[i]; psA[i]=ps[i]; pnA[i]=psize[i]; }
        int64_t got = g_sm_call(n, plA, prA, psA, pnA, np, nx, pv, sn, out);
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
    // Discriminating case for the size check: after (1,2) merges, symbol 1 has grown to two
    // units, so the queued (0,1) pair was made for a different text and must be skipped.
    {
        int n = 3;
        std::vector<int64_t> pl{1,0}, pr{2,1}; std::vector<double> ps{9.0,5.0}; std::vector<int64_t> pn{2,2};
        auto want = ref_merge(n, pl, pr, ps, pn, 2);
        int64_t plA[32]={0},prA[32]={0},pnA[32]={0},nx[32]={0},pv[32]={0},sn[32]={0},out[32]={0};
        double psA[32]={0};
        plA[0]=pl[0];prA[0]=pr[0];psA[0]=ps[0];pnA[0]=pn[0];
        plA[1]=pl[1];prA[1]=pr[1];psA[1]=ps[1];pnA[1]=pn[1];
        int64_t got = g_sm_call(n, plA, prA, psA, pnA, 2, nx, pv, sn, out);
        bool ok = got == 1 && out[0] == 0 && want.size() == 1 && want[0] == 0;
        if (!ok) { fails++; printf("SIZE CASE failed: got=%lld want=%zu (out0=%lld)\n", (long long) got, want.size(), (long long) out[0]); }
        else printf("size case: 1 merge, stale pair skipped\n");
    }
    printf("spm merge: trials=%d fails=%d\n", trials, fails);
    return fails ? 1 : 0;
}
