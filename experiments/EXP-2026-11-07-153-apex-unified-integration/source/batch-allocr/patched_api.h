#ifndef FG153_PATCHED_API_H
#define FG153_PATCHED_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct fg153p_row {
    uint32_t n_tokens, n_seq_tokens, n_seqs;
    int32_t  equal;
    int32_t  has_token;
    int32_t  token[512];
    int32_t  pos[2048];
    int32_t  n_seq_id[512];
    int32_t  seq_flat[2048];
    int32_t  batch_idxs[512];
    double   embd_sum;
} fg153p_row;

#define FG153P_API __attribute__((visibility("default")))

/* mode: 0 simple, 1 equal, 2 seq. Returns ubatch count or -1. */
FG153P_API int fg153p_run_case(int mode, uint32_t n_ubatch, int sequential, uint32_t n_keep_tail,
                    const int64_t * token, const int64_t * pos,
                    const int64_t * n_seq_id, const int64_t * seq_flat,
                    const int64_t * seq_off, const int64_t * logits,
                    int64_t n, int64_t n_seq_max, int embd_all, int mark_all_used,
                    fg153p_row * rows, int max_rows);

typedef struct fg153p_init_out {
    int32_t ok;
    int32_t n_tokens;
    int32_t n_outputs;
    int32_t token[512];
    int32_t pos[2048];
    int32_t n_seq_id[512];
    int32_t seq_flat[2048];
    int32_t seq_off[513];
    int32_t logits[512];
} fg153p_init_out;

FG153P_API int fg153p_init_case(const int64_t * token, const int64_t * pos,
                     const int64_t * n_seq_id, const int64_t * seq_flat,
                     const int64_t * seq_off, const int64_t * logits,
                     int64_t n, int64_t n_seq_max, int64_t n_vocab,
                     int output_all, fg153p_init_out * out);

/* Diagnostics: calls into Goose, native fallbacks (env=0), ubatches built. */
FG153P_API void fg153p_counters(uint64_t * calls, uint64_t * fallbacks, uint64_t * ubatches);
FG153P_API int64_t fg153p_sizeof(void);

#ifdef __cplusplus
}
#endif
#endif
