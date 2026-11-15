// S1.6 host bridge: the switched P9-03/P9-04 stream decisions.
//
// Four decisions of the runtime are exposed here as six entry points, so the
// staged build can carry both implementations and the build flag picks one:
//
//   xl_  expert cache      fg_xl_touch        touch/promote  (expert_stream_source.cpp:917)
//                          fg_xl_evict_tail   evict          (expert_stream_source.cpp:761)
//                          fg_xl_set_budget   budget         (expert_stream_source.cpp:675)
//   dw_  dense placement   fg_dw_subtract     complement     (dense_weights.cpp:98)
//   ov_  overlap           fg_ov_wave_list    wave choice    (expert_stream_source.cpp:1188)
//   rs_  row source        fg_rs_table_verdict verdict       (router_hook.cpp:195)
//
// Every fg_<name> is the production entry: with FG_GOOSE_STREAM it enters the
// Goose module, without it it runs the native transcription. The transcription
// is compiled in BOTH configurations as fg_native_<name>, which is what the
// differential gate compares against; switching the flag off is a rebuild, not
// a revert, and no native decision is deleted.
//
// Not switched, deliberately: pread, FileReader, O_DIRECT, vm_commit/vm_evict,
// the lane pool and the ready flags. Those are I/O and kernel calls, not policy.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Layout-identical to the generated Goose sl_i64/sl_u8 ({pointer, length}).
// The static_asserts in fg-stream-policy.cpp keep the two in step.
typedef struct {
    int64_t * data;
    int64_t len;
} fg_i64;

typedef struct {
    uint8_t * data;
    int64_t len;
} fg_u8;

// --- the switched entry points ---------------------------------------------
//
// Return values keep the encoding the accepted gates use:
//   touch       1 hit, 0 miss, -1 page commit failed
//   evict_tail  the evicted id, -1 on an empty cache
//   set_budget  the budget in force after the call
//   subtract    the number of output ranges
//   wave_list   the number of (projection, expert) pairs
//   verdict     1 row-gathered, 0 keeps its dense policy

int64_t fg_xl_touch(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev, fg_i64 next,
                    fg_i64 ever, fg_i64 st, fg_i64 sacc, int64_t n_expert,
                    int64_t il, int64_t e, int64_t promote, int64_t co_proj,
                    int64_t commit_ok);

int64_t fg_xl_evict_tail(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev, fg_i64 next,
                         fg_i64 ever, fg_i64 st, fg_i64 sacc, int64_t n_expert);

int64_t fg_xl_set_budget(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev, fg_i64 next,
                         fg_i64 ever, fg_i64 st, fg_i64 sacc, int64_t budget,
                         int64_t cur, int64_t total, int64_t cache_off,
                         int64_t n_expert);

int64_t fg_dw_subtract(fg_i64 ra, fg_i64 rb, int64_t n, int64_t a, int64_t b,
                       fg_i64 oa, fg_i64 ob);

int64_t fg_ov_wave_list(int64_t wave_mask, fg_i64 staged, int64_t n_staged,
                        fg_u8 miss, fg_i64 out);

int64_t fg_rs_table_verdict(int64_t gathered_refs, int64_t disq_refs);

// --- the native transcription, always compiled -----------------------------

int64_t fg_native_xl_touch(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev, fg_i64 next,
                           fg_i64 ever, fg_i64 st, fg_i64 sacc, int64_t n_expert,
                           int64_t il, int64_t e, int64_t promote, int64_t co_proj,
                           int64_t commit_ok);

int64_t fg_native_xl_evict_tail(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev,
                                fg_i64 next, fg_i64 ever, fg_i64 st, fg_i64 sacc,
                                int64_t n_expert);

int64_t fg_native_xl_set_budget(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev,
                                fg_i64 next, fg_i64 ever, fg_i64 st, fg_i64 sacc,
                                int64_t budget, int64_t cur, int64_t total,
                                int64_t cache_off, int64_t n_expert);

int64_t fg_native_dw_subtract(fg_i64 ra, fg_i64 rb, int64_t n, int64_t a, int64_t b,
                              fg_i64 oa, fg_i64 ob);

int64_t fg_native_ov_wave_list(int64_t wave_mask, fg_i64 staged, int64_t n_staged,
                               fg_u8 miss, fg_i64 out);

int64_t fg_native_rs_table_verdict(int64_t gathered_refs, int64_t disq_refs);

// --- selection -------------------------------------------------------------

// One-time initialisation of the selected implementation. On a Goose build this
// is the module's own goose_init(); on a native build it is a no-op. A real
// integration must call it once before the first decision, from the source's
// init, not from a request path.
void fg_stream_init(void);

// How many times a switched entry point actually entered the Goose module.
// Zero on a build without FG_GOOSE_STREAM.
int64_t fg_stream_goose_calls(void);

// "goose" or "native": which implementation the switched entries run.
const char * fg_stream_mode(void);

#ifdef __cplusplus
}
#endif
