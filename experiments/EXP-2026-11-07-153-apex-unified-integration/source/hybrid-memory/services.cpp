#include "frame.hpp"
#include "llama-impl.h"
#include <cstdlib>
#include <cstring>
static FG153HybridFrame &hf(int64_t p){GGML_ASSERT(p);return *(FG153HybridFrame*)(intptr_t)p;}
static llama_memory_i *child(FG153HybridFrame &f,int64_t side){GGML_ASSERT(side==0||side==1);return side?(llama_memory_i*)f.recr:(llama_memory_i*)f.attn;}
static llama_memory_context_i *context(FG153HybridFrame &f,int64_t side){GGML_ASSERT(side==0||side==1);auto *v=side?f.cr:f.ca;GGML_ASSERT(v&&v->get());return v->get();}
extern "C" int fg153_hybrid_enabled(void){auto *v=getenv("GGML_GOOSE_HYBRID");return !v||strcmp(v,"0")!=0;}
extern "C" int64_t fg153_hm_get(int64_t p,int64_t k,int64_t i){auto &f=hf(p);switch(k){
case 0:return f.batch->get_n_used();case 1:return f.batch->get_n_tokens();case 2:return f.attn->get_n_stream();case 3:return f.recr->n_rs_seq;
case 10:return f.status;case 11:return *f.next;case 12:return f.contexts->size();case 20:return f.breakdown.size();
case 21:return(intptr_t)f.breakdown.at(i).first;case 22:return f.breakdown.at(i).second;
default:GGML_ABORT("invalid Goose hybrid field");}}
extern "C" void fg153_hm_put(int64_t p,int64_t k,int64_t,int64_t v){auto &f=hf(p);GGML_ASSERT(k==11&&v>=0);*f.next=(size_t)v;}
extern "C" int64_t fg153_hm_call(int64_t op,int64_t p,int64_t a,int64_t b,int64_t c,int64_t d){if(op==99)GGML_ABORT("Goose hybrid invariant failed");auto &f=hf(p);switch(op){
case 0:f.batch->split_reset();return 0;
case 1:f.current=f.batch->split_seq(a);return f.current.n_tokens;
case 2:f.current=f.batch->split_equal(a,b,c);return f.current.n_tokens;
case 3:f.ubatches.push_back(std::move(f.current));return 0;
case 5:return f.recr->prepare(f.ubatches);
case 6:f.slots=f.attn->prepare(f.ubatches);return !f.slots.empty();
case 7:return(intptr_t)new llama_memory_hybrid_context(f.hybrid,std::move(f.slots),std::move(f.ubatches));
case 8:if(a)LLAMA_LOG_ERROR("Goose hybrid: failed to prepare %s ubatches\n",a==1?"recurrent":"attention");return(intptr_t)new llama_memory_hybrid_context(LLAMA_MEMORY_STATUS_FAILED_PREPARE);
case 9:return(intptr_t)new llama_memory_hybrid_context(f.hybrid);
case 10:return(intptr_t)new llama_memory_hybrid_context(f.hybrid,(llama_context*)(intptr_t)a,b);
case 11:return f.attn->get_can_shift();
case 12:child(f,a)->clear(b);return 0;
case 13:return child(f,a)->seq_rm(b,c,d);
case 14:child(f,a)->seq_cp(b,c,d,f.fourth);return 0;
case 15:child(f,a)->seq_keep(b);return 0;
case 16:child(f,a)->seq_add(b,c,d,f.fourth);return 0;
case 17:child(f,a)->seq_div(b,c,d,f.fourth);return 0;
case 18:return child(f,a)->seq_pos_min(b);
case 19:return child(f,a)->seq_pos_max(b);
case 30:return context(f,a)->next();case 31:return context(f,a)->apply();
case 33:return(intptr_t)&f.contexts->at(a);
case 42:child(f,a)->state_write(*(llama_io_write_i*)f.io,b,c);return 0;
case 43:try{child(f,a)->state_read(*(llama_io_read_i*)f.io,b,c);return 0;}catch(...){f.failure=std::current_exception();return 1;}
case 44:GGML_ASSERT(f.failure);std::rethrow_exception(f.failure);
case 45:f.attn->state_clear(a);return 0;
case 46:{auto values=child(f,a)->memory_breakdown();f.breakdown.assign(values.begin(),values.end());return 0;}
case 48:return f.result[(ggml_backend_buffer_type_t)(intptr_t)a];
case 49:GGML_ASSERT(b>=0);f.result[(ggml_backend_buffer_type_t)(intptr_t)a]=(size_t)b;return 0;
default:GGML_ABORT("invalid Goose hybrid primitive");}}
