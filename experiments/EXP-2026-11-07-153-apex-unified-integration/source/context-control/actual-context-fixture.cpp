#include "llama-context.h"
#include "llama-model.h"
#include "models.h"
#include <cstdio>
#include <vector>
int main(){
 llama_model_params mp={};llama_model_qwen35moe model(mp);model.arch=LLM_ARCH_QWEN35MOE;
 auto &h=model.hparams;h.vocab_only=true;h.no_alloc=true;h.n_layer_all=4;h.n_embd=128;h.n_ctx_train=256;h.n_head_arr.fill(2);h.n_head_kv_arr.fill(1);h.n_embd_head_k_full=64;h.n_embd_head_v_full=64;h.n_rot_full=64;model.layers.resize(4);
 for(int seqs:{1,4}){
  auto params=llama_context_default_params();params.n_ctx=256;params.n_batch=32;params.n_ubatch=16;params.n_seq_max=seqs;params.n_threads=2;params.n_threads_batch=4;
  llama_context ctx(model,params);const auto &p=ctx.get_cparams();
  std::printf("CTX seq=%d ctx=%u batch=%u ubatch=%u seqmax=%u threads=%d/%d\n",seqs,p.n_ctx,p.n_batch,p.n_ubatch,p.n_seq_max,p.n_threads,p.n_threads_batch);
  const size_t size=ctx.state_get_size();std::vector<uint8_t> bytes(size);auto n=ctx.state_get_data(bytes.data(),bytes.size());if(n!=size)return 2;
  uint64_t hash=1469598103934665603ULL;for(auto b:bytes){hash^=b;hash*=1099511628211ULL;}
  if(ctx.state_set_data(bytes.data(),bytes.size())!=n)return 3;
  std::printf("STATE bytes=%zu hash=%016llx\n",n,(unsigned long long)hash);
 }
 return 0;
}
