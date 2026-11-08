#include "bridge.h"
#include "../hybrid-memory/frame.hpp"
#include "llama-graph.h"
#include "llama-adapter.h"
#include "models.h"
#include "llama-io.h"
#include "ggml-backend.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <stdexcept>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line=%d %s\n",__LINE__,#x);abort();}}while(0)
struct Writer final:llama_io_write_i{
 std::vector<unsigned char> bytes;
 void write(const void *p,size_t n)override{auto *b=(const unsigned char*)p;bytes.insert(bytes.end(),b,b+n);}
 void write_tensor(ggml_tensor *t,size_t o,size_t n)override{size_t old=bytes.size();bytes.resize(old+n);ggml_backend_tensor_get(t,bytes.data()+old,o,n);}
 size_t n_bytes()override{return bytes.size();}
};
struct Reader final:llama_io_read_i{
 const std::vector<unsigned char>&bytes;size_t at=0,limit;
 explicit Reader(const std::vector<unsigned char>&v,size_t max=SIZE_MAX):bytes(v),limit(max){}
 void read(void *p,size_t n)override{if(n>bytes.size()-at||n>limit-at)throw std::runtime_error("fixture read failure");memcpy(p,bytes.data()+at,n);at+=n;}
 void read_tensor(ggml_tensor*t,size_t o,size_t n)override{std::vector<unsigned char> b(n);read(b.data(),n);ggml_backend_tensor_set(t,b.data(),o,n);}
 size_t n_bytes()override{return at;}
};
static uint64_t mix(uint64_t h,int64_t v){return(h^(uint64_t)v)*1099511628211ULL;}
static std::unique_ptr<llama_memory_hybrid> make(llama_model &m){return std::make_unique<llama_memory_hybrid>(m,GGML_TYPE_F16,GGML_TYPE_F16,false,64,16,0,LLAMA_SWA_TYPE_NONE,GGML_TYPE_F32,GGML_TYPE_F32,8,4,1,false,true);}
static uint64_t state(llama_memory_hybrid&m,uint64_t h){for(int seq=0;seq<4;++seq){h=mix(h,m.seq_pos_min(seq));h=mix(h,m.seq_pos_max(seq));}for(auto &v:m.memory_breakdown())h=mix(h,v.second);return h;}

static uint64_t graph_inputs(llama_model_qwen35moe &model,llama_memory_hybrid_context *memory,const llama_ubatch &batch,uint64_t h){
 llm_graph_result result(512);llama_adapter_cvec cvec;llama_adapter_loras loras;llm_graph_params p={};p.arch=LLM_ARCH_QWEN35MOE;p.hparams=model.hparams;p.cparams={};p.cparams.n_ctx=64;p.cparams.n_ctx_seq=64;p.cparams.n_seq_max=4;p.cparams.kv_unified=true;p.cparams.causal_attn=true;p.cparams.flash_attn=true;p.cparams.ctx_type=LLAMA_CONTEXT_TYPE_DEFAULT;p.ubatch=batch;p.n_outputs=batch.n_tokens;p.cvec=&cvec;p.loras=&loras;p.mctx=memory;p.res=&result;llm_graph_context graph(p);
 auto*input=graph.build_inp_mem_hybrid();CHECK(input&&result.inputs.size()==1);CHECK(input->can_reuse(p));auto changed=p;changed.ubatch.n_tokens+=1;CHECK(!input->can_reuse(changed));CHECK(input->can_reuse(p));
 auto*mask=input->inp_attn->self_kq_mask;auto old=mask->ne[2];mask->ne[2]=2;CHECK(!input->can_reuse(p));mask->ne[2]=old;CHECK(input->can_reuse(p));
 auto*buffer=ggml_backend_alloc_ctx_tensors_from_buft(result.get_ctx(),ggml_backend_cpu_buffer_type());CHECK(buffer);ggml_backend_buffer_clear(buffer,0);input->set_input(&batch);
 std::vector<ggml_tensor*> tensors={input->inp_rs->s_copy,input->inp_rs->s_copy_main,input->inp_rs->s_copy_tail,input->inp_attn->self_k_idxs,input->inp_attn->self_v_idxs,input->inp_attn->self_kq_mask};
 for(auto*t:tensors){CHECK(t);h=mix(h,t->type);h=mix(h,t->op);for(int k=0;k<4;++k){h=mix(h,t->ne[k]);h=mix(h,t->nb[k]);}h=mix(h,t->view_offs);for(const char*name=t->name;*name;++name)h=mix(h,*name);std::vector<unsigned char> bytes(ggml_nbytes(t));if(!bytes.empty())ggml_backend_tensor_get(t,bytes.data(),0,bytes.size());for(auto b:bytes)h=mix(h,b);}
 h=mix(h,input->inp_rs->head);h=mix(h,input->inp_rs->rs_z);CHECK(input->inp_attn->self_k_rot==nullptr&&input->inp_attn->self_v_rot==nullptr);ggml_backend_buffer_free(buffer);return h;
}

static uint64_t run(llama_model_qwen35moe &model,bool goose,bool embd_all){setenv("GGML_GOOSE_HYBRID",goose?"1":"0",1);setenv("GGML_GOOSE_HYBRID_INPUT",goose?"1":"0",1);setenv("FG153_GOOSE_HOOKS",goose?"1":"0",1);auto memory=make(model);auto &m=*memory;uint64_t h=1469598103934665603ULL;
 auto full=m.init_full();CHECK(full->get_status()==LLAMA_MEMORY_STATUS_SUCCESS);auto*fc=dynamic_cast<llama_memory_hybrid_context*>(full.get());CHECK(fc&&fc->get_recr()->get_head()==0);h=mix(h,fc->get_recr()->get_size());
 auto update=m.init_update(nullptr,false);h=mix(h,update->get_status());CHECK(update->get_status()==LLAMA_MEMORY_STATUS_NO_UPDATE);h=mix(h,m.get_can_shift());
 llama_batch_ext batch(32,128,128,4,&m,100,1);for(int seq=0;seq<2;++seq){for(int pos=0;pos<8;++pos){int i=batch.add_token(seq);CHECK(i>=0);CHECK(batch.set_token_id(i,pos+1));llama_pos p=pos;CHECK(batch.set_token_pos(i,&p));CHECK(batch.set_output(i,true));}}
 llama_batch_allocr allocr(1);CHECK(allocr.init(batch,model.vocab,true));auto context=m.init_batch(allocr,8,embd_all);CHECK(context->get_status()==LLAMA_MEMORY_STATUS_SUCCESS);int total=0,parts=0;
 do{const auto &u=context->get_ubatch();CHECK(u.n_tokens>0);total+=u.n_tokens;++parts;h=mix(h,u.n_tokens);h=mix(h,u.n_seqs);h=mix(h,u.n_seq_tokens);CHECK(context->apply());h=state(m,h);h=graph_inputs(model,dynamic_cast<llama_memory_hybrid_context*>(context.get()),u,h);}while(context->next());CHECK(total==16&&parts>0);
 m.seq_cp(0,2,0,-1);h=state(m,h);CHECK(m.seq_rm(2,0,-1));h=state(m,h);m.seq_keep(0);h=state(m,h);m.seq_add(0,0,-1,1);h=state(m,h);m.seq_div(0,0,-1,2);h=state(m,h);
 Writer w;m.state_write(w,-1,0);h=mix(h,w.bytes.size());m.clear(true);h=state(m,h);Reader good(w.bytes);m.state_read(good,-1,0);CHECK(good.n_bytes()==w.bytes.size());h=state(m,h);
 Writer attn;m.get_mem_attn()->state_write(attn,-1,0);CHECK(attn.bytes.size()<w.bytes.size());bool failed=false;try{Reader bad(w.bytes,attn.bytes.size()+1);m.state_read(bad,-1,0);}catch(const std::exception&){failed=true;}CHECK(failed);h=state(m,h);CHECK(m.get_mem_attn()->seq_pos_min(0)==-1&&m.get_mem_attn()->seq_pos_max(0)==-1);CHECK(m.seq_pos_max(0)==-1);
 m.clear(false);h=state(m,h);return h;
}
int main(){llama_model_params mp={};llama_model_qwen35moe model(mp);model.arch=LLM_ARCH_QWEN35MOE;auto &h=model.hparams;h.n_layer_all=4;h.n_embd=128;h.n_head_arr.fill(2);h.n_head_kv_arr.fill(1);h.n_embd_head_k_full=64;h.n_embd_head_v_full=64;h.n_rot_full=64;h.n_ctx_train=256;h.ssm_d_conv=4;h.ssm_d_inner=64;h.ssm_d_state=16;h.ssm_n_group=2;h.ssm_dt_rank=4;model.layers.resize(4);for(int i=0;i<4;++i)h.is_recr_impl[i]=(i+1)%4!=0;
 for(bool all:{false,true}){auto native=run(model,false,all);auto goose=run(model,true,all);CHECK(native==goose);printf("PASS hybrid native/Goose all=%d hash=%016llx\n",all,(unsigned long long)native);}
 for(int a=0;a<4;++a)for(int b=0;b<4;++b){CHECK(fg153_hybrid(12,0,a,b,0,0)==llama_memory_status_combine((llama_memory_status)a,(llama_memory_status)b));CHECK((fg153_hybrid(13,0,a,0,0,0)!=0)==llama_memory_status_is_fail((llama_memory_status)a));}
 for(auto point:std::vector<std::pair<int,int>>{{0,10},{0,11},{1,1},{1,6},{1,7},{1,32},{2,0},{2,1}}){auto n=fg153_recurrent_witness(point.first,point.second);CHECK(n>0);fprintf(stderr,"FG153_RECURRENT family=%d mode=%d calls=%llu\n",point.first,point.second,(unsigned long long)n);} puts("PASS real CPU hybrid buffers, split/apply/sequence/checkpoint/failure cleanup; no model inference");}
