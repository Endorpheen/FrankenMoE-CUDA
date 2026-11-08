// worker-2/kv-active-closure/q4a-hooks.h
#pragma once
#include <cstdint>
extern "C" {
int  fg153_q4a_init(void * kv, const void * model, const void * hparams,
                    int type_k, int type_v, int offload, int v_trans,
                    uint32_t kv_size, uint32_t n_stream,
                    const void * filter, const char * tag);
void fg153_q4a_release(void * kv);
void fg153_q4a_ensure_mapped(void * kv, uint32_t n_kv);
int  fg153_q4a_enabled(void);
void fg153_q4a_force_oom(int64_t after);
void fg153_q4a_reset_oom(void);
int64_t fg153_q4a_witness(void);
}
