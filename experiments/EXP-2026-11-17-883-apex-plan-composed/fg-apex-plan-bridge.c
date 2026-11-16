// Bridge for Apex's own architecture plan (goose/model/qwen35moe.goose + qgate-split.goose).
//
// Both modules are FFI-free except q35_rope_multi, which calls fg158_rope_multi - a thin wrapper over
// ggml_rope_multi provided alongside this file. The generated C defines goose_init per module, so each is
// compiled with that symbol renamed and started here on first use; the SLRU module already owns goose_init
// in this DSO.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int64_t * data; int64_t len; } sl_i64;

int64_t q35_kind(sl_i64 p, int64_t il, int64_t flags_base);
int64_t q35_plan(sl_i64 p, int64_t flags_base, sl_i64 kinds);
int64_t q35_shape(sl_i64 p, int64_t kind, int64_t tid, sl_i64 out);
int64_t qg_split(int64_t c, int64_t qfull, int64_t head_dim, int64_t n_head, int64_t n_tokens,
                 int64_t elem, sl_i64 out);

void fg_q35_goose_init(int argc, char ** argv);
void fg_qg_goose_init(int argc, char ** argv);

static int g_started = 0;
static void ensure_init(void) {
    if (!g_started) {
        fg_q35_goose_init(0, (char **) 0);
        fg_qg_goose_init(0, (char **) 0);
        g_started = 1;
    }
}

// Default on (rule 10): the switch restores the previous behaviour only.
int fg_q35s_enabled(void) {
    const char * v = getenv("GGML_GOOSE_APEX_PLAN");
    return !(v && strcmp(v, "0") == 0);
}

int64_t fg_q35s_plan(const int64_t * p, int64_t np, int64_t flags_base, int64_t * kinds, int64_t nk) {
    ensure_init();
    return q35_plan((sl_i64) { (int64_t *) (uintptr_t) p, np }, flags_base,
                    (sl_i64) { kinds, nk });
}

int64_t fg_q35s_kind(const int64_t * p, int64_t np, int64_t il, int64_t flags_base) {
    ensure_init();
    return q35_kind((sl_i64) { (int64_t *) (uintptr_t) p, np }, il, flags_base);
}

int64_t fg_q35s_shape(const int64_t * p, int64_t np, int64_t kind, int64_t tid, int64_t * out, int64_t nout) {
    ensure_init();
    return q35_shape((sl_i64) { (int64_t *) (uintptr_t) p, np }, kind, tid, (sl_i64) { out, nout });
}

int64_t fg_q35s_qg_split(int64_t c, int64_t qfull, int64_t head_dim, int64_t n_head, int64_t n_tokens,
                         int64_t elem, int64_t * out, int64_t nout) {
    ensure_init();
    return qg_split(c, qfull, head_dim, n_head, n_tokens, elem, (sl_i64) { out, nout });
}
