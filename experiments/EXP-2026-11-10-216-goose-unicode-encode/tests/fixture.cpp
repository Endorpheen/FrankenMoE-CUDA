// Differential gate for UTF-8 encoding against unicode.cpp:1262-1290.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <string>
extern "C" { int64_t fg216e_cpt_to_utf8(int64_t, char *, int64_t); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b){++checks;
  if(!ok){if(failures<15)printf("FAIL %s: goose=%lld ref=%lld\n",w,a,b);++failures;}}

static std::string ref_enc(uint32_t cpt){
  std::string r;
  if (cpt <= 0x7f) { r.push_back((char)cpt); return r; }
  if (cpt >= 0x80 && cpt <= 0x7ff) {
    r.push_back((char)(0xc0|((cpt>>6)&0x1f))); r.push_back((char)(0x80|(cpt&0x3f))); return r; }
  if (cpt >= 0x800 && cpt <= 0xffff) {
    r.push_back((char)(0xe0|((cpt>>12)&0x0f))); r.push_back((char)(0x80|((cpt>>6)&0x3f)));
    r.push_back((char)(0x80|(cpt&0x3f))); return r; }
  if (cpt >= 0x10000 && cpt <= 0x10ffff) {
    r.push_back((char)(0xf0|((cpt>>18)&0x07))); r.push_back((char)(0x80|((cpt>>12)&0x3f)));
    r.push_back((char)(0x80|((cpt>>6)&0x3f))); r.push_back((char)(0x80|(cpt&0x3f))); return r; }
  return r;   // native returns an empty string above U+10FFFF
}

int main(void){
  std::mt19937 rng(216011);
  for (int t = 0; t < 800; ++t) {
    const uint32_t cpt = (t < 400) ? (rng() % 0x110000) : (rng() % 0x120000);
    const std::string ref = ref_enc(cpt);
    char got[8]; memset(got,0,sizeof got);
    const int64_t n = fg216e_cpt_to_utf8((int64_t)cpt, got, 8);
    ck(n == (int64_t)ref.size(), "length", n, (long long)ref.size());
    if (n == (int64_t)ref.size()) ck(memcmp(got, ref.data(), ref.size()) == 0, "bytes", 0, 0);
  }
  // boundary codepoints explicitly
  for (uint32_t c : {0u, 0x7fu, 0x80u, 0x7ffu, 0x800u, 0xffffu, 0x10000u, 0x10ffffu, 0x110000u}) {
    const std::string ref = ref_enc(c);
    char got[8]; memset(got,0,sizeof got);
    const int64_t n = fg216e_cpt_to_utf8((int64_t)c, got, 8);
    ck(n == (int64_t)ref.size(), "bnd len", n, (long long)ref.size());
    if (n > 0) ck(memcmp(got, ref.data(), ref.size()) == 0, "bnd bytes", 0, 0);
  }
  printf("FG216E_FIXTURE checks=%d failures=%d\n",checks,failures);
  if(failures){printf("EXP216_LOCAL_GATES_FAIL\n");return 1;}
  printf("EXP216_LOCAL_GATES_PASS\n"); return 0;
}
