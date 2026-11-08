// worker-2/goose-entry-kv-lifecycle.c
//
// Binds the ACTUAL generated entry of kv-life-gen.c.
// The generated C defines static gg_function_1(mode,a,b,c,d,e,f,base); this
// translation unit includes it (renaming main) and exposes a typed callable.
// Do not also link kv-lifecycle.c separately: it has its own main.
//
// Parent regenerates kv-life-gen.c after the pad rename, then
// compiles this wrapper instead of the generated file.

#include <stdint.h>
#include <stdatomic.h>
static _Atomic uint64_t fg153_life_calls[32];
uint64_t fg153_kv_life_calls(int mode){return mode>=0&&mode<32?atomic_load(fg153_life_calls+mode):0;}

// Declarations for the Goose externs used by the generated module.
extern int64_t fg_w2_get(int64_t,int64_t,int64_t);
extern void    fg_w2_put(int64_t,int64_t,int64_t,int64_t);
extern int64_t fg_w2_service(int64_t,int64_t,int64_t);
extern int64_t fg_w2_set_create(void);
extern void    fg_w2_set_destroy(int64_t);
extern int64_t fg_w2_set_insert(int64_t,int64_t);
extern int64_t fg_w2_set_erase(int64_t,int64_t);
extern int64_t fg_w2_set_first(int64_t);
extern int64_t fg_w2_set_last(int64_t);
extern int64_t fg_w2_set_size(int64_t);
extern int64_t fg_w2_set_key(int64_t,int64_t);

extern int64_t fg_kv_get(int64_t,int64_t,int64_t);
extern void    fg_kv_put(int64_t,int64_t,int64_t,int64_t);
extern int64_t fg_kv_service(int64_t,int64_t,int64_t);
extern int64_t fg_kv_obj(int64_t);
extern int64_t fg_kv_create(int64_t,int64_t);
extern void    fg_kv_destroy(int64_t);
extern void    fg_kv_vec_push(int64_t,int64_t);
extern void    fg_kv_slot_push(int64_t,int64_t,int64_t);
extern int64_t fg_kv_slot_len(int64_t,int64_t);
extern void    fg_kv_slot_clear(int64_t,int64_t);
extern int64_t fg_kv_swa_masked(int64_t,int64_t,int64_t,int64_t,int64_t);
extern void    fg_kv_sync(int64_t,int64_t);
extern void    fg_kv_tensor_copy(int64_t,int64_t,int64_t,int64_t,int64_t);
extern int64_t fg_kv_shift_compute(int64_t,int64_t);

extern int64_t fg_kv_ub_n_tokens(int64_t);
extern int64_t fg_kv_ub_n_seqs_unq(int64_t);
extern int64_t fg_kv_ub_seq_id_unq(int64_t,int64_t);
extern int64_t fg_kv_ub_pos(int64_t,int64_t);
extern int64_t fg_kv_ub_n_seq_id(int64_t,int64_t);
extern int64_t fg_kv_ub_seq_id(int64_t,int64_t,int64_t);
extern int64_t fg_kv_ub_token(int64_t,int64_t);
extern int64_t fg_kv_ub_has_token(int64_t);
extern int64_t fg_kv_ub_is_pos_2d(int64_t);
extern int64_t fg_kv_ub_is_mixed(int64_t);
extern int64_t fg_kv_ub_type(int64_t,int64_t);
extern void    fg_kv_ub_destroy(int64_t);
extern void    fg_kv_balloc_reset(int64_t);
extern int64_t fg_kv_balloc_split(int64_t,int64_t,int64_t);
extern int64_t fg_kv_balloc_n_used(int64_t);
extern int64_t fg_kv_balloc_n_tokens(int64_t);
extern int64_t fg_kv_test_ubatch(int64_t);
extern void    fg_kv_test_ub_set_pos(int64_t,int64_t,int64_t);
extern void    fg_kv_test_ub_set_seq(int64_t,int64_t,int64_t,int64_t);
extern void    fg_kv_test_ub_set_seq_unq(int64_t,int64_t,int64_t);
extern int64_t fg_kv_test_balloc(int64_t);
extern void    fg_kv_test_ubatch_free(int64_t);
extern void    fg_kv_balloc_destroy(int64_t);

#define main fg_goose_kv_lifecycle_program_main
#include "kv-life-gen.c"
#undef main

int64_t fg_goose_kv_lifecycle_entry(int64_t mode, int64_t a, int64_t b, int64_t c,
                                    int64_t d, int64_t e, int64_t f) {
    if(mode>=0&&mode<32)atomic_fetch_add(fg153_life_calls+mode,1);
    int64_t r = gg_function_1(mode, a, b, c, d, e, f, 0);
    return r;
}
