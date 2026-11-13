// Differential gate for UTF-8 codepoint decoding against unicode.cpp:30-74.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <random>
#include <vector>
#include <string>
#include <stdexcept>

extern "C" {
int64_t fg215u_cpt_from_utf8(const char *, int64_t, int64_t, int64_t *);
}
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b){++checks;
  if(!ok){if(failures<15)printf("FAIL %s: goose=%lld ref=%lld\n",w,a,b);++failures;}}

// native transcription, unicode.cpp:30-74
static uint32_t ref_cpt(const std::string& u, size_t& off) {
  if (!(u[off+0] & 0x80)) { auto r=(uint32_t)(uint8_t)u[off+0]; off+=1; return r; }
  if (!(u[off+0] & 0x40)) throw std::invalid_argument("cont");
  if (!(u[off+0] & 0x20)) {
    if (off+1 >= u.size() || !((u[off+1]&0xc0)==0x80)) throw std::invalid_argument("2");
    auto r=((uint32_t)(u[off+0]&0x1f)<<6)|((uint32_t)u[off+1]&0x3f);
    if (r<0x80) throw std::invalid_argument("overlong2");
    off+=2; return r;
  }
  if (!(u[off+0] & 0x10)) {
    if (off+2 >= u.size() || !((u[off+1]&0xc0)==0x80) || !((u[off+2]&0xc0)==0x80)) throw std::invalid_argument("3");
    auto r=((uint32_t)(u[off+0]&0x0f)<<12)|((uint32_t)(u[off+1]&0x3f)<<6)|((uint32_t)u[off+2]&0x3f);
    if (r<0x800) throw std::invalid_argument("overlong3");
    off+=3; return r;
  }
  if (!(u[off+0] & 0x08)) {
    if (off+3 >= u.size() || !((u[off+1]&0xc0)==0x80) || !((u[off+2]&0xc0)==0x80) || !((u[off+3]&0xc0)==0x80)) throw std::invalid_argument("4");
    auto r=((uint32_t)(u[off+0]&0x07)<<18)|((uint32_t)(u[off+1]&0x3f)<<12)|((uint32_t)(u[off+2]&0x3f)<<6)|((uint32_t)u[off+3]&0x3f);
    if (r<0x10000 || r>0x10ffff) throw std::invalid_argument("range");
    off+=4; return r;
  }
  throw std::invalid_argument("5");
}

int main(void){
  std::mt19937 rng(215011);
  // valid sequences from real codepoints plus random byte soup
  for (int t = 0; t < 600; ++t) {
    std::string s;
    if (t % 3 == 0) {
      // random bytes, often malformed
      const int n = 1 + (int)(rng() % 5);
      for (int i = 0; i < n; ++i) s.push_back((char)(rng() % 256));
    } else {
      // well-formed codepoint encoded as UTF-8
      uint32_t cp;
      do { cp = rng() % 0x110000; } while (cp >= 0xd800 && cp <= 0xdfff);
      if (cp < 0x80) s.push_back((char)cp);
      else if (cp < 0x800) { s.push_back((char)(0xc0|(cp>>6))); s.push_back((char)(0x80|(cp&0x3f))); }
      else if (cp < 0x10000) { s.push_back((char)(0xe0|(cp>>12))); s.push_back((char)(0x80|((cp>>6)&0x3f))); s.push_back((char)(0x80|(cp&0x3f))); }
      else { s.push_back((char)(0xf0|(cp>>18))); s.push_back((char)(0x80|((cp>>12)&0x3f))); s.push_back((char)(0x80|((cp>>6)&0x3f))); s.push_back((char)(0x80|(cp&0x3f))); }
    }
    size_t roff = 0; bool refOk = true; uint32_t refVal = 0;
    try { refVal = ref_cpt(s, roff); } catch (const std::exception&) { refOk = false; }
    int64_t adv = -1;
    // safe mode: catch the Goose trap with a fork. The child's `adv` is a copy, so the value is
    // sent back through a second pipe rather than read from the parent's variable.
    int fd[2]; if (pipe(fd)!=0) { ++failures; continue; }
    int ad[2]; if (pipe(ad)!=0) { ++failures; continue; }
    pid_t pid = fork();
    if (pid == 0) {
      close(fd[0]); dup2(fd[1],2); close(fd[1]);
      close(ad[0]);
      const int64_t v = fg215u_cpt_from_utf8(s.data(), (int64_t)s.size(), 0, &adv);
      struct { int64_t adv; int64_t val; } out2 = { adv, v };
      ssize_t w = write(ad[1], &out2, sizeof out2); (void) w;
      close(ad[1]);
      _exit(0);
    }
    close(ad[1]);
    close(fd[1]);
    char b[128]={0}; ssize_t n=read(fd[0],b,sizeof b-1); close(fd[0]);
    struct { int64_t adv; int64_t val; } back = {-1, -1};
    ssize_t rn = read(ad[0], &back, sizeof back); (void) rn; close(ad[0]);
    int st=0; waitpid(pid,&st,0);
    const bool gooseOk = !(WIFSIGNALED(st) && WTERMSIG(st)==SIGABRT);
    ck(gooseOk == refOk, "validity", gooseOk, refOk);
    if (refOk && gooseOk) {
      ck(back.adv == (int64_t)roff, "advance", back.adv, (long long)roff);
      ck((uint32_t)back.val == refVal, "codepoint", back.val, (long long)refVal);
    }
  }
  printf("FG215U_FIXTURE checks=%d failures=%d\n",checks,failures);
  if(failures){printf("EXP215_LOCAL_GATES_FAIL\n");return 1;}
  printf("EXP215_LOCAL_GATES_PASS\n"); return 0;
}
