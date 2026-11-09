#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <vector>
extern "C" {
int64_t fg207p_has_bit(uint64_t, int64_t, int64_t, int64_t);
int64_t fg207p_seq_pos_min(uint64_t, uint64_t, int64_t, int64_t, int64_t);
int64_t fg207p_seq_pos_max(uint64_t, uint64_t, int64_t, int64_t, int64_t);
}
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b){++checks; if(!ok){if(failures<15)printf("FAIL %s: goose=%lld ref=%lld\n",w,a,b);++failures;}}
int main(void){
  std::mt19937 rng(207011);
  const int W = 4;   // LLAMA_MAX_SEQ=256
  for (int t = 0; t < 500; ++t) {
    const int n = 1 + (int)(rng() % 20);
    std::vector<int64_t> pos(n);
    std::vector<int64_t> seqw((size_t)n*W, 0);
    for (int i = 0; i < n; ++i) pos[i] = (rng()%3)==0 ? -1 : (int64_t)(rng()%200);
    // assign random sequence bits
    for (int i = 0; i < n; ++i)
      for (int s = 0; s < 256; ++s)
        if ((rng() % 4) == 0) seqw[(size_t)i*W + s/64] |= (int64_t)(1ULL << (s%64));

    for (int s = 0; s < 256; s += 17) {
      // reference: min/max over cells that carry s, using the native ordered set semantics
      int64_t mn = -1, mx = -1;
      for (int i = 0; i < n; ++i) {
        const bool has = (seqw[(size_t)i*W + s/64] >> (s%64)) & 1;
        ck((fg207p_has_bit((uint64_t)(uintptr_t)seqw.data(), i, s, W) != 0) == has, "has_bit", i, has);
        if (!has) continue;
        // native seq_pos never holds an empty cell: rm() clears the bits before pos becomes -1
        if (pos[i] == -1) continue;
        const int64_t p = pos[i];
        if (mn == -1 || p < mn) mn = p;
        if (mx == -1 || p > mx) mx = p;
      }
      ck(fg207p_seq_pos_min((uint64_t)(uintptr_t)pos.data(),(uint64_t)(uintptr_t)seqw.data(),n,s,W) == mn, "min", 0, mn);
      ck(fg207p_seq_pos_max((uint64_t)(uintptr_t)pos.data(),(uint64_t)(uintptr_t)seqw.data(),n,s,W) == mx, "max", 0, mx);
    }
  }
  // empty sequence must give -1 for both
  {
    std::vector<int64_t> pos(4, 5), seqw(4*W, 0);
    ck(fg207p_seq_pos_min((uint64_t)(uintptr_t)pos.data(),(uint64_t)(uintptr_t)seqw.data(),4,7,W) == -1, "min empty", 0, -1);
    ck(fg207p_seq_pos_max((uint64_t)(uintptr_t)pos.data(),(uint64_t)(uintptr_t)seqw.data(),4,7,W) == -1, "max empty", 0, -1);
  }
  printf("FG207P_FIXTURE checks=%d failures=%d\n",checks,failures);
  if(failures){printf("EXP207_LOCAL_GATES_FAIL\n");return 1;}
  printf("EXP207_LOCAL_GATES_PASS\n");return 0;
}
