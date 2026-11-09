// Differential gate for the incremental per-sequence min/max module.
//
// The reference is a transcription of std::set<(pos,cell)> semantics for the two queries native
// answers from it: begin()->first and rbegin()->first. Randomized add/remove/commit sequences are
// mirrored move by move, comparing the maintained bounds and the returned "rescan needed" code.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <vector>
#include <set>
#include <utility>
#include <algorithm>

extern "C" {
int64_t fg209i_on_insert(uint64_t, int64_t, int64_t, int64_t);
int64_t fg209i_on_remove(uint64_t, int64_t, int64_t, int64_t);
int64_t fg209i_commit_bound(uint64_t, int64_t, int64_t, int64_t, int64_t);
}
static int checks, failures;
static void ck(bool ok,const char* w,long long a,long long b){++checks;
  if(!ok){if(failures<15)printf("FAIL %s: goose=%lld ref=%lld\n",w,a,b);++failures;}}

int main(void){
  std::mt19937 rng(209011);
  const int NSEQ = 4;
  for (int t = 0; t < 400; ++t) {
    // arena: mn[NSEQ] | mx[NSEQ] | cnt[NSEQ]; mn/mx start at -1, counts at 0
    std::vector<int64_t> a((size_t)NSEQ*3, -1);
    for (int s = 0; s < NSEQ; ++s) a[NSEQ*2 + s] = 0;
    // reference sets per sequence, exactly what native keeps
    std::vector<std::set<std::pair<int64_t,int64_t>>> ref(NSEQ);

    auto rescan = [&](int s) {
      int64_t lo = -1, hi = -1;
      if (!ref[s].empty()) { lo = ref[s].begin()->first; hi = ref[s].rbegin()->first; }
      fg209i_commit_bound((uint64_t)(uintptr_t)a.data(), s, NSEQ, lo, hi);
    };

    for (int op = 0; op < 60; ++op) {
      const int s = (int)(rng() % NSEQ);
      // a cell index is unique in native: (pos, cell) pairs are therefore always distinct, which
      // std::set relies on. A repeated (pos, cell) would make the reference silently not grow
      // while the module counted it -- that was a defect in this gate, not in the module.
      static int64_t next_cell = 0;
      if ((rng() & 1) == 0) {
        // insert a fresh cell at a fresh position
        const int64_t p = (int64_t)(rng() % 100);
        ref[s].insert({p, next_cell++});
        const int64_t r = fg209i_on_insert((uint64_t)(uintptr_t)a.data(), s, NSEQ, p);
        const int64_t mn = a[s], mx = a[NSEQ + s];
        if (ref[s].size() == 1) {
          ck(r == 1 && mn == p && mx == p, "insert first", r, 1);
        } else {
          ck(r == 0, "insert r", r, 0);
          ck(mn == ref[s].begin()->first, "insert mn", mn, ref[s].begin()->first);
          ck(mx == ref[s].rbegin()->first, "insert mx", mx, ref[s].rbegin()->first);
        }
      } else {
        // remove the current minimum entry (pos, cell) -- the module is told the position only,
        // which is what the native caller has at that point
        if (ref[s].empty()) continue;
        auto it = ref[s].begin();
        const int64_t p = it->first;
        ref[s].erase(it);
        const int64_t cnt_before = a[NSEQ*2 + s];
        const int64_t r = fg209i_on_remove((uint64_t)(uintptr_t)a.data(), s, NSEQ, p);
        if (failures < 3)
          printf("  RM t=%d s=%d p=%lld refsize_before=%zu cnt_before=%lld r=%lld refsize_now=%zu\n",
                 t, s, (long long)p, (size_t)(ref[s].size()+1), (long long)cnt_before, (long long)r, ref[s].size());
        if (ref[s].empty()) {
          if (r != 2 && failures < 6)
            printf("  DIVERGENCE t=%d s=%d p=%lld r=%lld cnt=%lld mn=%lld mx=%lld refsize=%zu\n",
                   t, s, (long long)p, (long long)r, (long long)a[NSEQ*2+s],
                   (long long)a[s], (long long)a[NSEQ+s], ref[s].size());
          ck(r == 2 && a[s] == -1 && a[NSEQ + s] == -1, "remove last", r, 2);
        } else if (r == 1) {
          rescan(s);   // the host rescans only when a bound actually moved
          ck(a[s] == ref[s].begin()->first, "rescan mn", a[s], ref[s].begin()->first);
          ck(a[NSEQ + s] == ref[s].rbegin()->first, "rescan mx", a[NSEQ + s], ref[s].rbegin()->first);
        } else {
          ck(a[s] == ref[s].begin()->first, "after rm mn", a[s], ref[s].begin()->first);
          ck(a[NSEQ + s] == ref[s].rbegin()->first, "after rm mx", a[NSEQ + s], ref[s].rbegin()->first);
        }
      }
    }
  }
  printf("FG209I_FIXTURE checks=%d failures=%d\n", checks, failures);
  if (failures) { printf("EXP209_LOCAL_GATES_FAIL\n"); return 1; }
  printf("EXP209_LOCAL_GATES_PASS\n");
  return 0;
}
