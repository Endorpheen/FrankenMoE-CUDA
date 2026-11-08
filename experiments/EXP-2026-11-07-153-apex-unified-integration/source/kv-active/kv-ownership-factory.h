// worker-2/kv-ownership-factory.h
// Concrete-context factory for hybrid/model owners. Returns the Goose subclass
// when the cache is attached; otherwise falls back to the native concrete
// context so rollback keeps exact behaviour.
#pragma once
#include "llama-kv-cache.h"
#include <memory>
#include <vector>

llama_memory_context_ptr fg153_kv_context_batch(llama_kv_cache * kv,
        llama_kv_cache::slot_info_vec_t sinfos, std::vector<llama_ubatch> ubs);
