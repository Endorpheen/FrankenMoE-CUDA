// Bridge for the ubatch assembly decisions (EXP349) into llama_batch_allocr::ubatch_add.
//
// The module decides the shape flags, the payload sizes, the output-id list and the unique sequence
// list; the bridge copies those out of its journal into plain arrays. Payload copies (token, embd,
// state, positions, seq_id pointers) stay in llama.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls;
static void fg349s_tick(void){
    const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG349S_UBATCH_ADD first call calls=1\n");
    if ((n % 100) == 0) fprintf(stderr, "FG349S_UBATCH_ADD calls=%llu\n", n);
}

static int32_t id64_of(const int32_t * idxs, int64_t i){ return idxs ? idxs[i] : 0; }
static void g_fail(int64_t c){ fprintf(stderr, "FG349S validation code %lld\n", (long long)c); abort(); }
static void   g_put(int64_t p, int64_t k, int64_t i, uint64_t v){ ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t) v; }
static uint64_t g_get(int64_t p, int64_t k, int64_t i){ return (uint64_t) ((const int64_t *)(uintptr_t)p)[k*16 + i]; }
static void   g_stw(uint64_t b, int64_t o, int64_t v){ *(int64_t *)(uintptr_t)(b + (uint64_t) o) = v; }
static int64_t g_ld(uint64_t b, int64_t o){ return *(const int64_t *)(uintptr_t)(b + (uint64_t) o); }

#define fg349k_get   g_get
#define fg349k_put   g_put
#define fg349k_ld_i64 g_ld
#define fg349k_stw   g_stw
#define fg349k_fail  g_fail
#define gg_init_globals fg349s_init_globals
#define main fg349s_generated_main
#include "host-qualified.c"
#undef main

// out_sizes: [embd_all, state_all, pos_all]
// out_flags: [mixed, use_token, use_embd, has_state]
// out_outids_n: number of output rows
// returns 0 on success
int64_t fg349s_ubatch(int64_t n, int64_t n_batch, const int32_t * idxs, const int32_t * logits,
                      const int32_t * nseqid, const int32_t * seqid, int64_t n_seq_max,
                      const int32_t * is_embd, int64_t n_embd,
                      int64_t has_embd_vec, int64_t has_state, int64_t has_token, int64_t has_embdv,
                      int64_t n_pos, int32_t * out_sizes, int32_t * out_flags, int32_t * out_nseqid,
                      int32_t * out_output, int32_t * out_type, int32_t * out_seq_unq,
                      int32_t * out_seq_idx, int32_t * out_outids, int32_t * out_outids_n) {
    fg349s_tick();

    // the module indexes these arrays by batch index, so they are copied whole
    const int64_t nb = n_batch > 0 ? n_batch : 1;
    int64_t * ix64 = (int64_t *) malloc(sizeof(int64_t) * (size_t)(n > 0 ? n : 1));
    int64_t * lg64 = (int64_t *) malloc(sizeof(int64_t) * (size_t) nb);
    int64_t * ns64 = (int64_t *) malloc(sizeof(int64_t) * (size_t) nb);
    int64_t * sq64 = (int64_t *) malloc(sizeof(int64_t) * (size_t)(nb*4));
    int64_t * em64 = (int64_t *) malloc(sizeof(int64_t) * (size_t) nb);
    if (!ix64 || !lg64 || !ns64 || !sq64 || !em64) abort();
    for (int64_t i = 0; i < n; ++i) ix64[i] = idxs[i];
    for (int64_t i = 0; i < nb; ++i) {
        lg64[i] = logits ? logits[i] : 0;
        ns64[i] = nseqid ? nseqid[i] : 1;
        em64[i] = is_embd ? is_embd[i] : 0;
        for (int64_t j = 0; j < 4; ++j) sq64[i*4 + j] = seqid ? seqid[(size_t)(i*4 + j)] : 0;
    }

    const int64_t cap = 4 * n + 8 * (int64_t) n_seq_max + 32;
    int64_t * journal = (int64_t *) malloc(sizeof(int64_t) * 3 * (size_t) cap);
    int64_t * cnt = (int64_t *) calloc(4, sizeof(int64_t));
    int64_t * pkt = (int64_t *) calloc(8 * 16, sizeof(int64_t));
    if (!journal || !cnt || !pkt) abort();

    pkt[0*16 + 0] = n;
    pkt[0*16 + 1] = (int64_t)(uintptr_t) ix64;
    pkt[0*16 + 2] = (int64_t)(uintptr_t) lg64;
    pkt[0*16 + 3] = (int64_t)(uintptr_t) ns64;
    pkt[0*16 + 4] = (int64_t)(uintptr_t) sq64;
    pkt[0*16 + 5] = n_seq_max;
    pkt[0*16 + 6] = (int64_t)(uintptr_t) em64;
    pkt[0*16 + 7] = n_embd;
    pkt[0*16 + 8] = has_embd_vec;
    pkt[0*16 + 9] = has_state;
    pkt[0*16 + 10] = has_token;
    pkt[0*16 + 11] = has_embdv;
    pkt[0*16 + 12] = n_pos;
    pkt[1*16 + 0] = (int64_t)(uintptr_t) journal;
    pkt[1*16 + 1] = (int64_t)(uintptr_t) cnt;

    (void) fg349ds_gg_function_1((int64_t)(uintptr_t) pkt, 1);

    *out_outids_n = 0;
    const int64_t total = cnt[0];
    for (int64_t i = 0; i < total; ++i) {
        const int64_t kind = journal[i*3];
        const int64_t a = journal[i*3 + 1];
        const int64_t b = journal[i*3 + 2];
        switch (kind) {
            case 0: out_flags[a] = (int32_t) b; break;
            case 1: out_sizes[a] = (int32_t) b; break;
            case 2: out_nseqid[a] = (int32_t) b; break;
            case 3: out_outids[(*out_outids_n)++] = (int32_t) b; break;
            case 4: out_seq_unq[a] = (int32_t) b; break;
            case 5: out_seq_idx[a] = (int32_t) b; break;
            case 6: out_output[a] = (int32_t) b; break;
            default: break;
        }
    }
    if (out_type) {
        for (int64_t i = 0; i < n; ++i) out_type[i] = (int32_t) (em64[(size_t) id64_of(idxs, i)] ? 1 : 0);
    }

    free(pkt); free(cnt); free(journal);
    free(em64); free(sq64); free(ns64); free(lg64); free(ix64);
    return 0;
}

int fg349s_enabled(void) {
    const char * f = getenv("GGML_GOOSE_UBATCH_ADD");
    return !(f && strcmp(f, "0") == 0);
}
