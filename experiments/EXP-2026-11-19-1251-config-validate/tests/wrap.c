#include "/tmp/cfg.c"   /* produced by build.sh from src/validate.goose */
typedef int64_t (*fn_rule)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
                           int64_t,int64_t,int64_t,int64_t,double,int64_t,double,int64_t,int64_t);
int64_t cfg_rule_call(fn_rule f, int64_t hm, int64_t np, int64_t nt, int64_t nc, int64_t nu,
                      int64_t neu, int64_t wc, int64_t wr, int64_t moe, int64_t spec,
                      int64_t vram, int64_t slots, int64_t ngl, double temp, int64_t tk,
                      double tp, int64_t ov) {
    (void) f;
    return gg_function_1(hm,np,nt,nc,nu,neu,wc,wr,moe,spec,vram,slots,ngl,temp,tk,tp,ov,210,0);
}
