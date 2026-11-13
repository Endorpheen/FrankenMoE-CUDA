// Differential gate for unicode_tolower against the native lower_bound over the real table.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <vector>
#include <utility>
#include <algorithm>

// the real project table and the native function
#include <unicode-data.h>
static uint32_t ref_tolower(uint32_t cpt) {
    auto it = std::lower_bound(unicode_map_lowercase.begin(), unicode_map_lowercase.end(), cpt,
        [](const std::pair<uint32_t, uint32_t> & p, uint32_t v) { return p.first < v; });
    if (it != unicode_map_lowercase.end() && it->first == cpt) return it->second;
    return cpt;
}
extern "C" { int64_t fg218t_tolower(int64_t, const void *, int64_t); }

static int checks, failures;
static void ck(bool ok,long long a,long long b,const char*w){++checks;
  if(!ok){if(failures<15)printf("FAIL %s: goose=%lld ref=%lld\n",w,a,b);++failures;}}

int main(void){
  // the real map is a std::initializer_list, so it has no contiguous data() of its own; copy it
  // once into a vector to get a stable pointer for the Goose side
  const std::vector<std::pair<uint32_t,uint32_t>> table(unicode_map_lowercase.begin(),
                                                        unicode_map_lowercase.end());
  const int64_t n = (int64_t)table.size();
  const void * tab = (const void *)table.data();
  // the table layout the Goose module assumes must match the real vector's
  if (sizeof(std::pair<uint32_t,uint32_t>) != 8 || sizeof(uint32_t) != 4) {
    printf("layout mismatch: pair=%zu u32=%zu\n", sizeof(std::pair<uint32_t,uint32_t>), sizeof(uint32_t));
    printf("EXP218_LOCAL_GATES_FAIL\n"); return 1;
  }
  // every key in the table, plus its neighbours
  for (size_t i = 0; i < table.size(); ++i) {
    for (int d = -1; d <= 1; ++d) {
      const int64_t c = (int64_t)table[i].first + d;
      if (c < 0) continue;
      ck(fg218t_tolower(c, tab, n) == (int64_t)ref_tolower((uint32_t)c), fg218t_tolower(c, tab, n),
         (long long)ref_tolower((uint32_t)c), "key");
    }
  }
  // random codepoints across the whole range, most of which are unmapped
  std::mt19937 rng(218011);
  for (int t = 0; t < 4000; ++t) {
    const int64_t c = (int64_t)(rng() % 0x110000);
    ck(fg218t_tolower(c, tab, n) == (int64_t)ref_tolower((uint32_t)c), fg218t_tolower(c, tab, n),
       (long long)ref_tolower((uint32_t)c), "rand");
  }
  // boundaries of the range
  for (int64_t c : {(int64_t)0, (int64_t)0x7f, (int64_t)0x10ffff}) {
    ck(fg218t_tolower(c, tab, n) == (int64_t)ref_tolower((uint32_t)c), fg218t_tolower(c, tab, n),
       (long long)ref_tolower((uint32_t)c), "bnd");
  }
  // ASCII letters, where a broken search would show up first
  for (int64_t c = 'A'; c <= 'Z'; ++c)
    ck(fg218t_tolower(c, tab, n) == (int64_t)ref_tolower((uint32_t)c), fg218t_tolower(c, tab, n),
       (long long)ref_tolower((uint32_t)c), "ascii");
  printf("FG218T_FIXTURE table=%lld checks=%d failures=%d\n",(long long)n,checks,failures);
  if(failures){printf("EXP218_LOCAL_GATES_FAIL\n");return 1;}
  printf("EXP218_LOCAL_GATES_PASS\n"); return 0;
}
