/* Maps the gate's flat arguments onto the CURRENT cfg_validate(i[], f[]) door, which the shipped
 * library exports. The index map is the one the module documents; nothing is guessed.
 *
 * The old wrapper called gg_function_1, the pre-export-fn dispatcher, which no longer exists.
 */
#include <stdint.h>

typedef struct { const int64_t * data; int64_t len; } sl_i64;
typedef struct { const double  * data; int64_t len; } sl_f64;

int64_t cfg_validate(sl_i64, sl_f64);

int64_t cfg_rule_call(void * f, int64_t hm, int64_t np, int64_t nt, int64_t nc, int64_t nu,
                      int64_t neu, int64_t wc, int64_t wr, int64_t moe, int64_t spec,
                      int64_t ismtp, int64_t isng, int64_t dmax, double pmin, int64_t ngmax,
                      int64_t ngmin, int64_t vram, int64_t slots, int64_t ngl, double temp,
                      int64_t tk, double tp, int64_t ov, int64_t plog, int64_t ppf, int64_t ra,
                      int64_t rs, int64_t rsmb, int64_t iot, int64_t cmb, int64_t fc, int64_t pl,
                      int64_t ca, int64_t cfl, int64_t ccl, int64_t itw, int64_t psm,
                      double drop_cold, double sub_lambda) {
    (void) f; (void) psm;
    int64_t i[32];
    double  fl[5];
    i[0]  = np;   i[1]  = nt;   i[2]  = nc;   i[3]  = nu;   i[4]  = neu;
    i[5]  = vram; i[6]  = slots;i[7]  = ngl;  i[8]  = hm;   i[9]  = wc;
    i[10] = wr;   i[11] = tk;   i[13] = dmax; i[14] = ngmin;i[15] = ngmax;
    i[16] = moe;  i[17] = iot;  i[18] = cmb;  i[19] = ca;   i[20] = cfl;
    i[21] = ccl;  i[22] = pl;   i[23] = itw;  i[24] = plog; i[25] = ppf;
    i[26] = psm;  i[27] = ra;   i[28] = rs;   i[29] = rsmb; i[30] = ov;
    i[31] = fc;
    i[12] = (spec == 0) ? 0 : (ismtp ? 1 : (isng ? 2 : 0));
    fl[0] = temp; fl[1] = tp;   fl[2] = pmin; fl[3] = drop_cold; fl[4] = sub_lambda;
    sl_i64 si; si.data = i; si.len = 32;
    sl_f64 sf; sf.data = fl; sf.len = 5;
    return cfg_validate(si, sf);
}
