// Differential gate for the sampler threshold module against llama-sampler.cpp.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <random>
#include <vector>
#include <algorithm>

extern "C" {
int64_t fg212s_top_k_cut(int64_t, int64_t);
int64_t fg212s_min_p_scan(uint64_t, int64_t, float, int64_t, uint64_t);
int64_t fg212s_top_p_scan(uint64_t, int64_t, float, int64_t);
}
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b){++checks;
  if(!ok){if(failures<15)printf("FAIL %s: goose=%lld ref=%lld\n",w,a,b);++failures;}}

// native: llama-sampler.cpp:321-338
static int64_t ref_top_k(int64_t k, int64_t size){
  if (k <= 0) return size;
  return std::min(k, size);
}
// native: 1768-1784, unsorted branch
static int64_t ref_min_p(const std::vector<float>& lg, float p, int64_t min_keep, std::vector<int64_t>& keep){
  const int64_t n = (int64_t) lg.size();
  keep.assign(n, 0);
  if (p <= 0.0f) { for (auto& k : keep) k = 1; return n; }
  if (n == 0) return -1;
  float mx = -FLT_MAX;
  for (float v : lg) mx = std::max(mx, v);
  const float mn = mx + logf(p);
  int64_t kept = 0;
  for (int64_t i = 0; i < n; ++i) if (lg[i] >= mn) { keep[i] = 1; ++kept; }
  if (kept == 0 || kept < min_keep) return -1;
  return kept;
}
// native: 1578-1598
static int64_t ref_top_p(const std::vector<float>& pv, float p, int64_t min_keep){
  const int64_t n = (int64_t) pv.size();
  if (p >= 1.0f) return n;
  float cum = 0.0f;
  for (int64_t i = 0; i < n; ++i) {
    cum += pv[i];
    if (cum >= p && i + 1 >= min_keep) return i + 1;
  }
  return n;
}

int main(void){
  std::mt19937 rng(212011);

  for (int t = 0; t < 400; ++t) {
    const int64_t n = (int64_t)(rng() % 40) + 1;
    const int64_t k = (int64_t)(rng() % 50) - 5;
    ck(fg212s_top_k_cut(k, n) == ref_top_k(k, n), "top_k", fg212s_top_k_cut(k,n), ref_top_k(k,n));
  }

  for (int t = 0; t < 400; ++t) {
    const int64_t n = 1 + (int64_t)(rng() % 32);
    std::vector<float> lg(n);
    for (auto& v : lg) v = (float)((int)(rng() % 2000) - 1000) / 100.0f;
    const float p = (float)(1 + (int)(rng() % 9)) / 10.0f;
    const int64_t mk = (int64_t)(rng() % 4);
    std::vector<int64_t> rk;
    const int64_t ref = ref_min_p(lg, p, mk, rk);
    std::vector<int64_t> gk(n, 0);
    const int64_t got = fg212s_min_p_scan((uint64_t)(uintptr_t)lg.data(), n, p, mk,
                                          (uint64_t)(uintptr_t)gk.data());
    ck(got == ref, "min_p count", got, ref);
    if (ref > 0) for (int64_t i = 0; i < n; ++i) ck(gk[i] == rk[i], "min_p keep", gk[i], rk[i]);
  }

  for (int t = 0; t < 400; ++t) {
    const int64_t n = 1 + (int64_t)(rng() % 32);
    std::vector<float> pv(n);
    float s = 0.0f;
    for (auto& v : pv) { v = (float)((int)(rng() % 100) + 1); s += v; }
    for (auto& v : pv) v /= s;                       // descending order is the caller's job
    std::sort(pv.begin(), pv.end(), std::greater<float>());
    const float p = (float)(1 + (int)(rng() % 10)) / 10.0f;
    const int64_t mk = (int64_t)(rng() % 4);
    const int64_t ref = ref_top_p(pv, p, mk);
    const int64_t got = fg212s_top_p_scan((uint64_t)(uintptr_t)pv.data(), n, p, mk);
    ck(got == ref, "top_p", got, ref);
  }

  printf("FG212S_FIXTURE checks=%d failures=%d\n",checks,failures);
  if(failures){printf("EXP212_LOCAL_GATES_FAIL\n");return 1;}
  printf("EXP212_LOCAL_GATES_PASS\n"); return 0;
}
