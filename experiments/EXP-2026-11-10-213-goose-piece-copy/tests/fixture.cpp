#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <vector>
extern "C" {
int64_t fg213p_lstrip_count(uint64_t, int64_t, int64_t);
int64_t fg213p_try_copy(uint64_t, int64_t, int64_t, uint64_t, int64_t);
}
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b){++checks;
  if(!ok){if(failures<15)printf("FAIL %s: goose=%lld ref=%lld\n",w,a,b);++failures;}}

// native: llama-vocab.cpp:3980-3994
static int32_t ref_try_copy(const char* token, size_t size, int32_t lstrip, char* buf, int32_t length) {
  for (int32_t i = 0; i < lstrip && size && *token == ' '; ++i) { token++; size--; }
  if (length < (int32_t) size) return -(int32_t) size;
  memcpy(buf, token, size);
  return (int32_t) size;
}

int main(void){
  std::mt19937 rng(213011);
  for (int t = 0; t < 500; ++t) {
    const int n = (int)(rng() % 24);
    std::vector<char> piece(n);
    for (auto& c : piece) c = (rng() % 3 == 0) ? ' ' : (char)('a' + rng() % 26);
    const int32_t lstrip = (int32_t)(rng() % 6);
    const int32_t avail  = (int32_t)(rng() % 32) - 4;

    char refbuf[64] = {0};
    const int32_t ref = ref_try_copy(piece.data(), (size_t)n, lstrip, refbuf, avail);
    char gotbuf[64] = {0};
    const int64_t got = fg213p_try_copy((uint64_t)(uintptr_t)piece.data(), n, lstrip,
                                        (uint64_t)(uintptr_t)gotbuf, avail);
    ck(got == ref, "try_copy result", got, ref);
    if (ref >= 0) ck(memcmp(gotbuf, refbuf, (size_t)ref) == 0, "try_copy bytes", 0, 0);

    // lstrip_count must agree with native's skip
    int32_t nskip = 0;
    { const char* p = piece.data(); size_t s = (size_t)n;
      for (int32_t i = 0; i < lstrip && s && *p == ' '; ++i) { p++; s--; nskip++; } }
    ck(fg213p_lstrip_count((uint64_t)(uintptr_t)piece.data(), n, lstrip) == nskip, "lstrip", 0, nskip);
  }
  // explicit edge cases
  { char b[8]={0}; char o[8]={0};
    ck(fg213p_try_copy((uint64_t)(uintptr_t)b,0,0,(uint64_t)(uintptr_t)o,0)==0,"empty",0,0); }
  { char b[4]={'a','b','c','d'}; char o[4]={0};
    ck(fg213p_try_copy((uint64_t)(uintptr_t)b,4,0,(uint64_t)(uintptr_t)o,2)==-4,"overflow",0,-4); }
  { char b[4]={' ',' ','x','y'}; char o[4]={0};
    ck(fg213p_try_copy((uint64_t)(uintptr_t)b,4,2,(uint64_t)(uintptr_t)o,4)==2,"strip2",0,2); }
  printf("FG213P_FIXTURE checks=%d failures=%d\n",checks,failures);
  if(failures){printf("EXP213_LOCAL_GATES_FAIL\n");return 1;}
  printf("EXP213_LOCAL_GATES_PASS\n"); return 0;
}
