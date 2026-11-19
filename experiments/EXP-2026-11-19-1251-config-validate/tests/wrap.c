#include "/tmp/cfg.c"
typedef int64_t (*fn_rule)();
int64_t cfg_rule_call(void * f, int64_t hm, int64_t np, int64_t nt, int64_t nc, int64_t nu,
                      int64_t neu, int64_t wc, int64_t wr, int64_t moe, int64_t spec,
                      int64_t ismtp, int64_t isng, int64_t dmax, double pmin, int64_t ngmax,
                      int64_t ngmin, int64_t vram, int64_t slots, int64_t ngl, double temp,
                      int64_t tk, double tp, int64_t ov, int64_t plog, int64_t ppf, int64_t ra) {
    (void) f;
    return gg_function_1(hm,np,nt,nc,nu,neu,wc,wr,moe,spec,ismtp,isng,dmax,pmin,ngmax,ngmin,
                         vram,slots,ngl,temp,tk,tp,ov,plog,ppf,ra,210,0);
}
