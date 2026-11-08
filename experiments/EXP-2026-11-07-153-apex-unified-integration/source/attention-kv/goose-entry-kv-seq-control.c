// worker-2/goose-entry-kv-seq-control.c
//
// Binds the ACTUAL generated entry of kv-seq-gen.c.
// Generated C defines static gg_function_1(mode,a,b,c,d,e,base). Include it
// (renaming main) and expose a typed callable. Do not also link the generated
// file separately.

#include <stdint.h>

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
extern void    fg_kv_slot_push(int64_t,int64_t,int64_t);
extern int64_t fg_kv_slot_len(int64_t,int64_t);
extern void    fg_kv_zero_cells(int64_t,int64_t,int64_t,int64_t);
extern void    fg_kv_clear_data(int64_t);
extern int64_t fg_kv_other_pos(int64_t,int64_t,int64_t);

#define main fg_goose_kv_seq_program_main
#include "kv-seq-gen.c"
#undef main

int64_t fg_goose_kv_seq_entry(int64_t mode, int64_t a, int64_t b, int64_t c,
                              int64_t d, int64_t e) {
    int64_t r = gg_function_1(mode, a, b, c, d, e, 0);
    for (int i = 0; i < GG_STACK_COUNT; ++i) { gg_stacks[i].top = 0; }
    return r;
}
