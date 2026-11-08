// worker-2/kv-native-adapter.h
//
// Bridges the Goose KV algorithms to a REAL llama_kv_cache instance.
// The adapter accesses named private members through a TU-local
// `#define private public` include; no ABI or layout change.
#pragma once

#include "llama-memory.h"

#include <cstdint>
#include <memory>

class llama_kv_cache;
struct llama_context;

int64_t fg_kv_attach_real(llama_kv_cache * kv);
void    fg_kv_configure_real(int64_t kv_handle, llama_kv_cache * kv);
int64_t fg_kv_handle_for(const llama_kv_cache * kv);

llama_memory_context_ptr fg_goose_context_word(llama_memory_status status);
llama_memory_context_ptr fg_goose_context_full(llama_kv_cache * kv, int64_t gkv, int64_t gctx);
llama_memory_context_ptr fg_goose_context_batch(llama_kv_cache * kv, int64_t gkv, int64_t gctx);
llama_memory_context_ptr fg_goose_context_update(llama_kv_cache * kv, int64_t gkv, int64_t gctx,
        llama_context * lctx, bool do_shift, int64_t sc_count,
        const uint32_t * sc_src, const uint32_t * sc_dst);
