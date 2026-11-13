// Differential gate for the recurrent seq_keep module against llama-memory-recurrent.cpp:302-332.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <vector>
#include <set>

extern "C" {
int64_t fg210k_seq_keep(uint64_t, int64_t, int64_t, int64_t, int64_t, int64_t *);
}
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b){++checks;
  if(!ok){if(failures<15)printf("FAIL %s: goose=%lld ref=%lld\n",w,a,b);++failures;}}

struct Cell { int64_t pos=-1, src=-1, tail=-1; std::set<int> seq; };

static int64_t ref_seq_keep(std::vector<Cell> & cells, int w, int s, int64_t head, int64_t & used) {
    const int64_t n = (int64_t) cells.size();
    int64_t new_head = n;
    for (int64_t i = 0; i < n; ++i) {
        if (i != s) cells[i].tail = -1;
        if (!cells[i].seq.count(s)) {
            if (cells[i].pos >= 0) used--;
            cells[i].pos = -1; cells[i].src = -1; cells[i].seq.clear();
            if (new_head == n) new_head = i;
        } else {
            cells[i].seq.clear(); cells[i].seq.insert(s);
        }
    }
    if (new_head != n && new_head < head) head = new_head;
    return head;
}

int main(void){
  std::mt19937 rng(210011);
  const int W = 4;
  for (int t = 0; t < 400; ++t) {
    const int n = 1 + (int)(rng() % 10);
    const int ns = 4;
    std::vector<Cell> rc(n);
    std::vector<int64_t> a((size_t)(3*n + n*W), 0);
    int64_t used = 0;
    for (int i = 0; i < n; ++i) {
      const bool occ = (rng() % 3) != 0;
      rc[i].pos = occ ? (int64_t)(rng() % 100) : -1;
      rc[i].src = rc[i].pos >= 0 ? (int64_t)(rng() % n) : -1;
      rc[i].tail = (rng() % 3) == 0 ? (int64_t)(rng() % n) : -1;
      for (int s = 0; s < ns; ++s) if ((rng() % 3) == 0) rc[i].seq.insert(s);
      if (rc[i].pos >= 0) used++;
      a[i] = rc[i].pos;
      a[n + i] = rc[i].tail;
      a[2*n + i] = rc[i].src;
      for (int s = 0; s < ns; ++s)
        if (rc[i].seq.count(s)) a[3*n + (size_t)i*W + s/64] |= (int64_t)(1ULL << (s%64));
    }
    const int64_t head = (int64_t)(rng() % (unsigned)n);
    const int s = (int)(rng() % ns);

    std::vector<Cell> ref = rc; int64_t ref_used = used;
    const int64_t ref_head = ref_seq_keep(ref, W, s, head, ref_used);

    int64_t got_used = used;
    const int64_t got_head = fg210k_seq_keep((uint64_t)(uintptr_t)a.data(), n, W, s, head, &got_used);

    ck(got_head == ref_head, "head", got_head, ref_head);
    ck(got_used == ref_used, "used", got_used, ref_used);
    for (int i = 0; i < n; ++i) {
      ck(a[i] == ref[i].pos, "pos", a[i], ref[i].pos);
      ck(a[n+i] == ref[i].tail, "tail", a[n+i], ref[i].tail);
      ck(a[2*n+i] == ref[i].src, "src", a[2*n+i], ref[i].src);
      for (int q = 0; q < ns; ++q) {
        const bool has = (a[3*n + (size_t)i*W + q/64] >> (q%64)) & 1;
        ck(has == (ref[i].seq.count(q) != 0), "seqbit", has, ref[i].seq.count(q)!=0);
      }
    }
  }
  printf("FG210K_FIXTURE checks=%d failures=%d\n",checks,failures);
  if(failures){printf("EXP210_LOCAL_GATES_FAIL\n");return 1;}
  printf("EXP210_LOCAL_GATES_PASS\n"); return 0;
}
