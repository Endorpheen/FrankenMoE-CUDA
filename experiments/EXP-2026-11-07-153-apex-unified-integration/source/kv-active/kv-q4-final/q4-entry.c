// worker-2/kv-q4-final/q4-entry.c
#include <stdint.h>
extern int64_t q4_n_layer(int64_t);
extern int64_t q4_use_layer(int64_t,int64_t);
extern int64_t q4_n_embd_k(int64_t,int64_t);
extern int64_t q4_n_embd_v(int64_t,int64_t);
extern int64_t q4_type(int64_t,int64_t);
extern int64_t q4_kv_size(int64_t);
extern int64_t q4_nstream(int64_t);
extern int64_t q4_tag(int64_t);
extern int64_t q4_ctx(int64_t,int64_t);
extern int64_t q4_tensor3d(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
extern int64_t q4_layer_add(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
extern void    q4_set_head_all(int64_t,int64_t,int64_t);
extern int64_t q4_finalize(int64_t,int64_t);
extern void    q4_release_impl(int64_t);
extern void    q4_witness(int64_t);
extern int64_t q4_other(int64_t);
extern int64_t q4_rot_disabled(int64_t);
extern int64_t q4_quantized(int64_t,int64_t);
extern int64_t q4_head_k(int64_t);
extern int64_t q4_head_v(int64_t);
extern void    q4_copy_rot(int64_t,int64_t);
extern void    q4_set_rot(int64_t,int64_t,int64_t);
extern void    q4_hadamard(int64_t,int64_t);
extern void    q4_clear_hadamard(int64_t);

#define main fg153_q4_program_main
#include "q4-gen.c"
#undef main

int64_t fg153_q4_entry(int64_t mode, int64_t a, int64_t b, int64_t c) {
    int64_t r = gg_function_1(mode, a, b, c, 0);
    for (int i = 0; i < GG_STACK_COUNT; ++i) { gg_stacks[i].top = 0; }
    return r;
}
