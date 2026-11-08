// worker-2/kv-ownership-hooks.h
// Hooks injected into the hash-bound patched llama-kv-cache.cpp.
#pragma once
#include <cstdint>

extern "C" {
int  fg153_kv_own_init(void * kv, const void * model, const void * hparams,
                       int type_k, int type_v, int offload,
                       uint32_t kv_size, uint32_t n_stream,
                       const void * filter, const char * tag);
void fg153_kv_own_release(void * kv);
int  fg153_kv_own_enabled(void);
void fg153_kv_own_force_oom(int64_t after);
void fg153_kv_own_reset_oom(void);
int64_t fg153_kv_witness(void);
}
