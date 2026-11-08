// worker-2/goose-entry-kv-ownership.c
#include <stdint.h>

extern int64_t own_n_layer(int64_t);
extern int64_t own_use_layer(int64_t,int64_t);
extern int64_t own_n_embd_k(int64_t,int64_t);
extern int64_t own_n_embd_v(int64_t,int64_t);
extern int64_t own_type(int64_t,int64_t);
extern int64_t own_kv_size(int64_t);
extern int64_t own_nstream(int64_t);
extern int64_t own_tag(int64_t);
extern int64_t own_ctx(int64_t,int64_t);
extern int64_t own_tensor3d(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
extern int64_t own_layer_add(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
extern void    own_set_head_all(int64_t,int64_t,int64_t);
extern int64_t own_finalize(int64_t,int64_t);
extern void    own_release_impl(int64_t);
extern void    own_witness(int64_t);

#define main fg153_kv_ownership_program_main
#include "kv-own-gen.c"
#undef main

int64_t fg153_kv_own_entry(int64_t mode, int64_t a, int64_t b, int64_t c) {
    int64_t r = gg_function_1(mode, a, b, c, 0);
    for (int i = 0; i < GG_STACK_COUNT; ++i) { gg_stacks[i].top = 0; }
    return r;
}
