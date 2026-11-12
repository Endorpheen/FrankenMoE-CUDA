// Differential fixture for the Goose server startup/config policy (EXP445).
//
// Right arm: a direct C transcription of the native statements in
// tools/server/server.cpp (lines 159-200) and server-context.cpp (n_ctx_slot).
// Left arm: the Goose module emitted by the pinned FrankenGoose compiler.
// Both arms consume the same parameter tuple and write eight i64 words.
#include <stdio.h>
#include "ffi-adapter.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

void fg445_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

enum {
    W_VALID = 0, W_N_BATCH, W_N_UBATCH, W_N_PARALLEL,
    W_KV_UNIFIED, W_N_CTX, W_ALIAS_USE, W_N_CTX_SLOT,
};

struct P {
    int64_t has_model, embedding;
    int64_t n_batch, n_ubatch, n_parallel, kv_unified;
    int64_t n_ctx, kv_per_slot, fit_min_ctx;
    int64_t alias_empty, model_name_len, ctx_seq, ctx_train;
};

static int64_t min64(int64_t a, int64_t b) { return a < b ? a : b; }

// Verbatim native behavior. The validity word is the parse-time contract
// mirrored by the Goose module (positive batch, non-negative ctx/pool, usable
// slot context); it is not an extra native adjustment.
static void native_rules(const struct P * p, int64_t out[8]) {
    int64_t valid = 1;
    int64_t n_batch = p->n_batch;
    int64_t n_ubatch = p->n_ubatch;
    int64_t n_parallel = p->n_parallel;
    int64_t kv_unified = p->kv_unified;
    int64_t n_ctx = p->n_ctx;
    int64_t alias_use = 0;

    if (n_batch <= 0 || n_ubatch <= 0) valid = 0;
    if (n_ctx < 0 || p->kv_per_slot < 0) valid = 0;
    if (p->ctx_seq <= 0 || p->ctx_train <= 0) valid = 0;

    const int is_router_server = !p->has_model;
    if (!is_router_server) {
        if (p->embedding && n_batch > n_ubatch) {
            n_batch = n_ubatch;                       // server.cpp:166
        }
        if (n_parallel < 0) {                         // server.cpp:169-173
            n_parallel = 4;
            kv_unified = 1;
        }
    }

    const int ctx_pool_auto_sized = p->kv_per_slot > 0 && n_ctx == 0 &&
                                    (uint32_t) p->fit_min_ctx != UINT32_MAX;
    if (ctx_pool_auto_sized) {                        // server.cpp:181-187
        n_ctx = n_parallel * p->kv_per_slot;
    }
    if (p->alias_empty && p->model_name_len > 0) {    // server.cpp:190-193
        alias_use = 1;
    }
    if (n_parallel <= 0) valid = 0;

    int64_t slot = p->ctx_seq;                        // server-context.cpp:4932
    if (p->kv_per_slot > 0) slot = min64(slot, p->kv_per_slot);
    slot = min64(slot, p->ctx_train);

    out[W_VALID] = valid;
    out[W_N_BATCH] = n_batch;
    out[W_N_UBATCH] = n_ubatch;
    out[W_N_PARALLEL] = n_parallel;
    out[W_KV_UNIFIED] = kv_unified;
    out[W_N_CTX] = n_ctx;
    out[W_ALIAS_USE] = alias_use;
    out[W_N_CTX_SLOT] = slot;
}

static int checks = 0, fails = 0, cases = 0;

static void one(const struct P * p) {
    int64_t ref[8] = {0}, go[8] = {0};
    native_rules(p, ref);
    fg445_decide(p->has_model, p->embedding, p->n_batch, p->n_ubatch, p->n_parallel,
                 p->kv_unified, p->n_ctx, p->kv_per_slot, p->fit_min_ctx, p->alias_empty,
                 p->model_name_len, p->ctx_seq, p->ctx_train, (uint64_t) (uintptr_t) go);
    ++cases;
    for (int w = 0; w < 8; ++w) {
        ++checks;
        if (ref[w] != go[w]) {
            ++fails;
            if (fails <= 5) {
                printf("FAIL case=%d word=%d native=%lld goose=%lld  "
                       "[model=%lld emb=%lld nb=%lld nub=%lld np=%lld ku=%lld nc=%lld pool=%lld fit=%lld "
                       "alias=%lld namelen=%lld ctxseq=%lld ctxtrain=%lld]\n",
                       cases, w, (long long) ref[w], (long long) go[w],
                       (long long) p->has_model, (long long) p->embedding,
                       (long long) p->n_batch, (long long) p->n_ubatch,
                       (long long) p->n_parallel, (long long) p->kv_unified,
                       (long long) p->n_ctx, (long long) p->kv_per_slot,
                       (long long) p->fit_min_ctx, (long long) p->alias_empty,
                       (long long) p->model_name_len, (long long) p->ctx_seq,
                       (long long) p->ctx_train);
            }
        }
    }
}

int main(void) {
    goose_init(0, 0);
    // Exhaustive grid over every branch of the three router/sizing decisions.
    const int64_t has_model[]  = {0, 1};
    const int64_t embedding[]  = {0, 1};
    const int64_t n_batch[]    = {1, 8, 2048};
    const int64_t n_ubatch[]   = {1, 8, 512};
    const int64_t n_parallel[] = {-1, 0, 1, 4, 8};
    const int64_t kv_unified[] = {0, 1};
    const int64_t n_ctx[]      = {0, 4096};
    const int64_t kv_pool[]    = {0, 1, 1024};
    const int64_t fit_min[]    = {0, 4294967295LL};
    const int64_t alias_empty[]= {0, 1};
    const int64_t name_len[]   = {0, 12};
    const int64_t ctx_seq[]    = {512, 32768};
    const int64_t ctx_train[]  = {4096, 131072};

    for (size_t a = 0; a < 2; ++a)
    for (size_t b = 0; b < 2; ++b)
    for (size_t c = 0; c < 3; ++c)
    for (size_t d = 0; d < 3; ++d)
    for (size_t e = 0; e < 5; ++e)
    for (size_t f = 0; f < 2; ++f)
    for (size_t g = 0; g < 2; ++g)
    for (size_t h = 0; h < 3; ++h)
    for (size_t i = 0; i < 2; ++i)
    for (size_t j = 0; j < 2; ++j)
    for (size_t k = 0; k < 2; ++k)
    for (size_t l = 0; l < 2; ++l)
    for (size_t m = 0; m < 2; ++m) {
        struct P p = {
            has_model[a], embedding[b], n_batch[c], n_ubatch[d], n_parallel[e], kv_unified[f],
            n_ctx[g], kv_pool[h], fit_min[i], alias_empty[j], name_len[k], ctx_seq[l], ctx_train[m],
        };
        one(&p);
    }

    // Boundary corpus around the clamps.
    const struct P edge[] = {
        {1, 1, 2048, 512, -1, 0, 0, 1024, 0, 1, 0, 32768, 131072},   // Apex-like, auto parallel
        {1, 0, 2048, 512, 4, 1, 0, 1024, 0, 0, 12, 32768, 131072},   // Apex accepted shape
        {0, 1, 2048, 512, -1, 0, 0, 1024, 0, 1, 12, 32768, 131072},  // router: no model adjustments
        {1, 1, 512, 512, 4, 1, 0, 0, 4294967295LL, 1, 12, 4096, 4096},// fit pinned min ctx
        {1, 1, 2048, 2048, 8, 0, 4096, 1024, 0, 0, 12, 8192, 4096},  // -c wins, slot capped by train
        {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1},                      // router + np=0 -> invalid
        {1, 0, 0, 1, 4, 1, 0, 0, 0, 1, 12, 4096, 4096},               // invalid batch
        {1, 0, 1, 1, 4, 1, -1, 0, 0, 1, 12, 4096, 4096},              // invalid negative ctx
        {1, 0, 1, 1, 4, 1, 0, 0, 0, 1, 12, 0, 4096},                  // invalid ctx_seq
        {1, 0, 1, 1, 4, 1, 0, 0, 0, 1, 12, 4096, 0},                  // invalid ctx_train
        {1, 1, 300, 200, 2, 0, 0, 4096, 0, 1, 1, 100000, 262144},     // clamp + large pool
        {1, 1, 300, 200, 2, 0, 0, 4096, 77, 1, 1, 100000, 262144},    // same, fit min set
    };
    for (size_t i = 0; i < sizeof(edge) / sizeof(edge[0]); ++i) one(&edge[i]);

    printf("FG445_STARTUP cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
