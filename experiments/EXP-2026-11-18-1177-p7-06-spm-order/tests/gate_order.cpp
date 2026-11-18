// Native comparators, used as the reference through an actual std::priority_queue pop order.
#include <cstdio>
#include <queue>
#include <vector>
#include <cstdint>
#include <algorithm>
struct S { int64_t left, right; float score; size_t size;
  struct cmp { bool operator()(S&l,S&r){ return (l.score < r.score) || (l.score == r.score && l.left > r.left); } }; };
struct B { int64_t left, right; int rank;
  struct cmp { bool operator()(const B&l,const B&r) const { return l.rank > r.rank || (l.rank == r.rank && l.left > r.left); } }; };
struct Item { double score; int64_t left; int64_t rank; };
extern "C" int64_t g_spm_call(double,int64_t,double,int64_t);
extern "C" int64_t g_bpe_call(int64_t,int64_t,int64_t,int64_t);
int main(){
    std::vector<Item> items = {
        {0.5, 4, 9}, {2.0, 1, 3}, {2.0, 0, 7}, {1.0, 2, 3},
        {2.0, 6, 5}, {0.5, 3, 1}, {3.5, 9, 2}, {1.0, 5, 8},
    };
    int fails = 0;
    // SPM: build the real queue, then compare its pop order with the Goose comparator
    std::priority_queue<S, std::vector<S>, S::cmp> q;
    for (auto & it : items) q.push(S{it.left, 0, (float) it.score, 0});
    for (size_t i = 0; i + 1 < items.size(); ++i) {
        S a = q.top(); q.pop();
        S b = q.top();
        int64_t want = 1;  // a is popped before b, by construction
        int64_t got = g_spm_call(a.score, a.left, b.score, b.left);
        if (got != want) { fails++; printf("SPM mismatch at %zu: a=(%.1f,%lld) b=(%.1f,%lld)\n", i, a.score, (long long)a.left, b.score, (long long)b.left); }
    }
    // BPE: same procedure with the rank queue
    std::priority_queue<B, std::vector<B>, B::cmp> qb;
    for (auto & it : items) qb.push(B{it.left, 0, (int) it.rank});
    for (size_t i = 0; i + 1 < items.size(); ++i) {
        B a = qb.top(); qb.pop();
        B b = qb.top();
        int64_t got = g_bpe_call(a.rank, a.left, b.rank, b.left);
        if (got != 1) { fails++; printf("BPE mismatch at %zu: a=(%d,%lld) b=(%d,%lld)\n", i, a.rank, (long long)a.left, b.rank, (long long)b.left); }
    }
    // antisymmetry: never both true, never both false for distinct items
    for (auto & x : items) for (auto & y : items) {
        int64_t f = g_spm_call(x.score, x.left, y.score, y.left);
        int64_t r = g_spm_call(y.score, y.left, x.score, x.left);
        bool same = (x.score == y.score && x.left == y.left);
        if (!same && f == r) { fails++; printf("SPM not antisymmetric: (%.1f,%lld) vs (%.1f,%lld)\n", x.score, (long long)x.left, y.score, (long long)y.left); }
    }
    printf("spm/bpe order: fails=%d\n", fails);
    return fails ? 1 : 0;
}
