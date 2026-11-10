// Differential gate for the llama_kv_cell_ext decision against a transcription of the native block.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <random>
#include <vector>
extern "C" { int64_t fg228e_build_ext(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
                                      int64_t,int64_t,int64_t,int64_t,int32_t*); }
#define LLAMA_TOKEN_NULL (-1)
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b){++checks;
  if(!ok){if(failures<15)printf("FAIL %s goose=%lld ref=%lld\n",w,a,b);++failures;}}

struct Ref { bool set; int32_t x, y, tok; };
static Ref ref_ext(int has_ext,int pos_2d,int px,int py,int has_token,int tok_i,int is_mixed,
                   int type_i,int ple_n_heads,int ple_image,int ple_eos) {
    Ref r{false,0,0,LLAMA_TOKEN_NULL};
    if (!has_ext) return r;
    r.set = true;
    if (pos_2d) { r.x = px; r.y = py; }
    const bool is_embd = !has_token || (is_mixed && type_i);
    if (!is_embd) r.tok = tok_i;
    else if (ple_n_heads > 0) r.tok = ple_image != 0 ? ple_image : ple_eos;
    return r;
}

int main(void){
  std::mt19937 rng(228011);
  // every combination of the boolean-ish inputs, several value sets: exhaustive over the decision shape
  const int vals_px[] = {0, 7, -3, 4096};
  const int vals_tok[] = {0, 42, -5, 151000};
  for (int has_ext=0; has_ext<2; ++has_ext)
  for (int pos_2d=0; pos_2d<2; ++pos_2d)
  for (int has_token=0; has_token<2; ++has_token)
  for (int is_mixed=0; is_mixed<2; ++is_mixed)
  for (int type_i=0; type_i<2; ++type_i)
  for (int ple=0; ple<2; ++ple) {
    for (int vi=0; vi<4; ++vi) {
      const int px=vals_px[vi], py=vals_px[(vi+1)%4], tok_i=vals_tok[vi];
      const int img = (vi%2) ? (900+vi) : 0;
      const int eos = 1;
      const int32_t pnh = ple ? (vi%2 ? 4 : 1) : 0;
      Ref e = ref_ext(has_ext,pos_2d,px,py,has_token,tok_i,is_mixed,type_i,pnh,img,eos);
      int32_t out[4] = {-777,-777,-777,-777};
      const int64_t got_set = fg228e_build_ext(has_ext,pos_2d,px,py,has_token,tok_i,is_mixed,type_i,
                                               pnh,img,eos,out);
      ck(got_set == (e.set?1:0), "set", got_set, e.set?1:0);
      if (e.set) {
        ck(out[0]==e.x, "x", out[0], e.x);
        ck(out[1]==e.y, "y", out[1], e.y);
        ck(out[2]==e.tok, "tok", out[2], e.tok);
      } else {
        ck(out[0]==-777 && out[1]==-777 && out[2]==-777, "no write when unset", out[0], -777);
      }
      ck(out[3]==-777, "no overrun", out[3], -777);
    }
  }
  printf("FG228E_FIXTURE checks=%d failures=%d\n",checks,failures);
  if(failures){printf("EXP228_LOCAL_GATES_FAIL\n");return 1;}
  printf("EXP228_LOCAL_GATES_PASS\n"); return 0;
}
