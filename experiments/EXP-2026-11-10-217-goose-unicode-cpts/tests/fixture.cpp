// Differential gate for whole-string UTF-8 decoding against unicode.cpp.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include <vector>
#include <string>
#include <stdexcept>
extern "C" { int64_t fg217c_cpts_from_utf8(const char *, int64_t, int32_t *); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b){++checks;
  if(!ok){if(failures<15)printf("FAIL %s: goose=%lld ref=%lld\n",w,a,b);++failures;}}

// native unicode_cpt_from_utf8, then the recovery loop of unicode_cpts_from_utf8
static uint32_t ref_cpt(const std::string& u, size_t& off) {
  if (!(u[off+0] & 0x80)) { auto r=(uint32_t)(uint8_t)u[off+0]; off+=1; return r; }
  if (!(u[off+0] & 0x40)) throw std::invalid_argument("c");
  if (!(u[off+0] & 0x20)) {
    if (off+1 >= u.size() || !((u[off+1]&0xc0)==0x80)) throw std::invalid_argument("2");
    auto r=((uint32_t)(u[off+0]&0x1f)<<6)|((uint32_t)u[off+1]&0x3f);
    if (r<0x80) throw std::invalid_argument("o2");
    off+=2; return r;
  }
  if (!(u[off+0] & 0x10)) {
    if (off+2 >= u.size() || !((u[off+1]&0xc0)==0x80) || !((u[off+2]&0xc0)==0x80)) throw std::invalid_argument("3");
    auto r=((uint32_t)(u[off+0]&0x0f)<<12)|((uint32_t)(u[off+1]&0x3f)<<6)|((uint32_t)u[off+2]&0x3f);
    if (r<0x800) throw std::invalid_argument("o3");
    off+=3; return r;
  }
  if (!(u[off+0] & 0x08)) {
    if (off+3 >= u.size() || !((u[off+1]&0xc0)==0x80) || !((u[off+2]&0xc0)==0x80) || !((u[off+3]&0xc0)==0x80)) throw std::invalid_argument("4");
    auto r=((uint32_t)(u[off+0]&0x07)<<18)|((uint32_t)(u[off+1]&0x3f)<<12)|((uint32_t)(u[off+2]&0x3f)<<6)|((uint32_t)u[off+3]&0x3f);
    if (r<0x10000 || r>0x10ffff) throw std::invalid_argument("r");
    off+=4; return r;
  }
  throw std::invalid_argument("5");
}
static std::vector<uint32_t> ref_all(const std::string& u) {
  std::vector<uint32_t> r; r.reserve(u.size());
  size_t off = 0;
  while (off < u.size()) {
    try { r.push_back(ref_cpt(u, off)); }
    catch (const std::invalid_argument&) { ++off; r.push_back(0xFFFD); }
  }
  return r;
}

static void run_case(const std::string& s, int t) {
  const std::vector<uint32_t> ref = ref_all(s);
  std::vector<int32_t> out(s.size() + 8, -12345);
  const int64_t n = fg217c_cpts_from_utf8(s.data(), (int64_t)s.size(), out.data());
  ck(n == (int64_t)ref.size(), "count", n, (long long)ref.size());
  if (n == (int64_t)ref.size()) {
    for (size_t i = 0; i < ref.size(); ++i)
      if (out[i] != (int32_t)ref[i]) {
        if (failures < 15) printf("FAIL case %d at %zu: goose=%d ref=%u\n", t, i, out[i], ref[i]);
        ++checks; ++failures; return;
      }
    ++checks;
  }
  ck(out[ref.size()] == -12345, "no overrun", out[ref.size()], -12345);
}

int main(void){
  std::mt19937 rng(217011);
  // valid UTF-8 strings
  for (int t = 0; t < 200; ++t) {
    std::string s;
    const int n = (int)(rng() % 12);
    for (int k = 0; k < n; ++k) {
      uint32_t cp; do { cp = rng() % 0x110000; } while (cp >= 0xd800 && cp <= 0xdfff);
      if (cp < 0x80) s.push_back((char)cp);
      else if (cp < 0x800) { s.push_back((char)(0xc0|(cp>>6))); s.push_back((char)(0x80|(cp&0x3f))); }
      else if (cp < 0x10000) { s.push_back((char)(0xe0|(cp>>12))); s.push_back((char)(0x80|((cp>>6)&0x3f))); s.push_back((char)(0x80|(cp&0x3f))); }
      else { s.push_back((char)(0xf0|(cp>>18))); s.push_back((char)(0x80|((cp>>12)&0x3f))); s.push_back((char)(0x80|((cp>>6)&0x3f))); s.push_back((char)(0x80|(cp&0x3f))); }
    }
    run_case(s, t);
  }
  // pure random bytes: recovery must emit one U+FFFD per bad byte at the right places
  for (int t = 200; t < 500; ++t) {
    std::string s;
    const int n = 1 + (int)(rng() % 10);
    for (int k = 0; k < n; ++k) s.push_back((char)(rng() % 256));
    run_case(s, t);
  }
  // truncation and continuation-at-start shapes
  for (const char* s : {"", "\xE2", "\xE2\x82", "\xE2\x82\xAC", "\x80", "\xBF\xBF", "A\xE2\x82", "\xF0\x9F\x92"}) {
    run_case(std::string(s), 999);
  }
  printf("FG217C_FIXTURE checks=%d failures=%d\n",checks,failures);
  if(failures){printf("EXP217_LOCAL_GATES_FAIL\n");return 1;}
  printf("EXP217_LOCAL_GATES_PASS\n"); return 0;
}
