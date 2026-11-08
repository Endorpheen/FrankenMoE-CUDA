#pragma once
#include "llama-memory-hybrid.h"
#include "bridge.h"
#include <exception>
struct FG153HybridFrame {
 llama_memory_hybrid *hybrid=nullptr;
 llama_kv_cache *attn=nullptr;
 llama_memory_recurrent *recr=nullptr;
 llama_batch_allocr *batch=nullptr;
 llama_ubatch current={};
 std::vector<llama_ubatch> ubatches;
 llama_kv_cache::slot_info_vec_t slots;
 int64_t fourth=0;
 const llama_memory_context_ptr *ca=nullptr,*cr=nullptr;
 size_t *next=nullptr;
 const std::vector<llama_ubatch> *contexts=nullptr;
 llama_memory_status status=LLAMA_MEMORY_STATUS_SUCCESS;
 void *io=nullptr;
 std::exception_ptr failure;
 std::vector<std::pair<ggml_backend_buffer_type_t,size_t>> breakdown;
 std::map<ggml_backend_buffer_type_t,size_t> result;
 explicit FG153HybridFrame(const llama_memory_hybrid *m):hybrid(const_cast<llama_memory_hybrid*>(m)),attn(m->get_mem_attn()),recr(m->get_mem_recr()){}
 FG153HybridFrame(const llama_memory_context_ptr &a,const llama_memory_context_ptr &r,size_t &i,const std::vector<llama_ubatch> &u,llama_memory_status s):ca(&a),cr(&r),next(&i),contexts(&u),status(s){}
};
