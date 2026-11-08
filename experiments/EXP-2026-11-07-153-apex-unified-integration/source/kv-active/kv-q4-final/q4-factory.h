// worker-2/kv-q4-final/q4-factory.h
#pragma once
#include "llama-kv-cache.h"
#include <memory>
#include <vector>
llama_memory_context_ptr fg153_q4_context_batch(llama_kv_cache * kv,
        llama_kv_cache::slot_info_vec_t sinfos, std::vector<llama_ubatch> ubs);
