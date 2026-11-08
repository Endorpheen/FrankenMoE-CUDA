// worker-2/kv-active-closure/q4a-entry.c
#include <stdint.h>
extern int64_t q4a_n_layer(int64_t);
extern int64_t q4a_use_layer(int64_t,int64_t);
extern int64_t q4a_n_embd_k(int64_t,int64_t);
extern int64_t q4a_n_embd_v(int64_t,int64_t);
extern int64_t q4a_type(int64_t,int64_t);
extern int64_t q4a_kv_size(int64_t);
extern int64_t q4a_nstream(int64_t);
extern int64_t q4a_tag(int64_t);
extern int64_t q4a_offload(int64_t);
extern int64_t q4a_vtrans(int64_t);
extern int64_t q4a_cpu_buft(void);
extern int64_t q4a_dev_buft(int64_t,int64_t);
extern int64_t q4a_lazy_buft(int64_t,int64_t);
extern int64_t q4a_ctx(int64_t,int64_t,int64_t);
extern int64_t q4a_tensor3d(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
extern int64_t q4a_layer_add(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
extern void    q4a_set_head_all(int64_t,int64_t,int64_t);
extern void    q4a_set_lazy(int64_t,int64_t);
extern int64_t q4a_finalize(int64_t,int64_t);
extern void    q4a_release_impl(int64_t);
extern void    q4a_witness(int64_t);
extern int64_t q4a_rot_disabled(int64_t);
extern int64_t q4a_quantized(int64_t,int64_t);
extern int64_t q4a_head_k(int64_t);
extern int64_t q4a_head_v(int64_t);
extern void    q4a_set_rot(int64_t,int64_t,int64_t);
extern void    q4a_h_begin(int64_t,int64_t);
extern void    q4a_h_end(int64_t,int64_t);
extern int64_t q4a_h_get(int64_t,int64_t,int64_t,int64_t);
extern void    q4a_h_set(int64_t,int64_t,int64_t,int64_t,int64_t);
extern int64_t q4a_inv_sqrt(int64_t);

#define main fg153_q4a_program_main
#include "q4a-gen.c"
#undef main

int64_t fg153_q4a_entry(int64_t mode, int64_t a, int64_t b, int64_t c) {
    int64_t r = gg_function_1(mode, a, b, c, 0);
    return r;
}
