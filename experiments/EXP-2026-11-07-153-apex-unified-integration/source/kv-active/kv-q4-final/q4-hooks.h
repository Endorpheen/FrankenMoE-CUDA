// worker-2/kv-q4-final/q4-hooks.h
#pragma once
#include <cstdint>
extern "C" {
int  fg153_q4_init(void * kv, const void * model, const void * hparams,
                   int type_k, int type_v, int offload,
                   uint32_t kv_size, uint32_t n_stream,
                   const void * filter, const char * tag);
void fg153_q4_release(void * kv);
int  fg153_q4_enabled(void);
void fg153_q4_force_oom(int64_t after);
void fg153_q4_reset_oom(void);
int64_t fg153_q4_witness(void);
}
