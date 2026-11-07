from pathlib import Path
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp151-cache-lifecycle-20261107');D=A/'source/expert-cache';src=(D/'llama-moe-cache.cpp').read_text();prefix=src[:src.index('llama_moe_cache::llama_moe_cache(')].replace('struct llama_moe_cache::impl {','struct impl {');tail=src[src.index('extern "C" int64_t fg150_cache_get'):]
header=r'''
#include "llama-model.h"
#include "llama-moe-cache.h"
#include "llama-impl.h"
#include "ggml-cpp.h"
#include "ggml-backend-impl.h"
#include <map>
#include <vector>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <set>
#include <array>
static void check(bool value,const char*what){if(!value){fprintf(stderr,"CHECK FAILED: %s\n",what);abort();}}
static std::set<ggml_context*>live_contexts;
static std::map<ggml_backend_buffer_t,void(*)(ggml_backend_buffer_t)>live_buffers;
static size_t context_calls=0,buffer_calls=0,fail_context=0,fail_buffer=0;
static size_t buffer_requests=0,fail_request=0;
static ggml_context*tracked_init(ggml_init_params p){context_calls++;if(fail_context&&context_calls==fail_context)return nullptr;auto*c=ggml_init(p);if(c)check(live_contexts.insert(c).second,"unique context allocation");return c;}
struct tracked_context_deleter{void operator()(ggml_context*c)const{if(c){std::set<const ggml_tensor*>owned;for(auto*t=ggml_get_first_tensor(c);t;t=ggml_get_next_tensor(c,t))owned.insert(t);
 for(auto*other:live_contexts)if(other!=c)for(auto*t=ggml_get_first_tensor(other);t;t=ggml_get_next_tensor(other,t))check(!t->view_src||owned.count(t->view_src)==0,"temporary views released before their parent context");
 check(live_contexts.erase(c)==1,"context freed exactly once");ggml_free(c);}}};
using tracked_context_ptr=std::unique_ptr<ggml_context,tracked_context_deleter>;
static void tracked_buffer_free(ggml_backend_buffer_t buffer){auto it=live_buffers.find(buffer);check(it!=live_buffers.end(),"buffer freed exactly once");auto fn=it->second;live_buffers.erase(it);if(fn)fn(buffer);}
static ggml_backend_buffer_t tracked_alloc(ggml_backend_buffer_type_t type,size_t size){buffer_calls++;if(fail_buffer&&buffer_calls==fail_buffer)return nullptr;auto real=ggml_backend_cpu_buffer_type();auto*b=real->iface.alloc_buffer(real,size);if(b){check(live_buffers.emplace(b,b->iface.free_buffer).second,"unique buffer allocation");b->buft=type;b->iface.free_buffer=tracked_buffer_free;}return b;}
static ggml_backend_buffer_t track_result(ggml_backend_buffer_t b){if(b&&live_buffers.find(b)==live_buffers.end()){live_buffers.emplace(b,b->iface.free_buffer);b->iface.free_buffer=tracked_buffer_free;}return b;}
static ggml_backend_buffer_t tracked_alloc_ctx(ggml_context*c,ggml_backend_buffer_type_t type){buffer_requests++;if(fail_request&&buffer_requests==fail_request)return nullptr;return track_result(ggml_backend_alloc_ctx_tensors_from_buft(c,type));}
static ggml_backend_buffer_t tracked_alloc_buft(ggml_backend_buffer_type_t type,size_t size){buffer_requests++;if(fail_request&&buffer_requests==fail_request)return nullptr;return track_result(ggml_backend_buft_alloc_buffer(type,size));}
static enum ggml_backend_dev_type fake_gpu_type(ggml_backend_dev_t){return GGML_BACKEND_DEVICE_TYPE_GPU;}
struct fg151_test_model{
 llama_hparams hparams={};std::vector<llama_layer>layers;std::vector<llama_device>devices;std::vector<ggml_backend_dev_t>layer_devices;std::vector<float>splits;llama_split_mode mode=LLAMA_SPLIT_MODE_LAYER;
 size_t n_devices()const{return devices.size();}const float*tensor_split()const{return splits.empty()?nullptr:splits.data();}llama_split_mode split_mode()const{return mode;}ggml_backend_dev_t dev_layer(int i)const{return layer_devices.at(i);}
};
#define llama_model fg151_test_model
#define ggml_context_ptr tracked_context_ptr
#define ggml_init tracked_init
#define ggml_backend_alloc_ctx_tensors_from_buft tracked_alloc_ctx
#define ggml_backend_buft_alloc_buffer tracked_alloc_buft
'''
main=r'''
#undef ggml_init
#undef ggml_backend_alloc_ctx_tensors_from_buft
#undef ggml_backend_buft_alloc_buffer
#undef ggml_context_ptr
#undef llama_model
static void switches(bool goose){fg151_enabled=goose;fg150_cache_enabled=goose;}
static void compare_cache(const impl&a,const impl&b){
 check(a.devices.size()==b.devices.size()&&a.groups.size()==b.groups.size()&&a.layers.size()==b.layers.size(),"cache dimensions");check(a.buf_host_size==b.buf_host_size,"host buffer size");
 for(size_t i=0;i<a.devices.size();i++){check(a.devices[i].host_bytes==b.devices[i].host_bytes&&a.devices[i].buf_size==b.devices[i].buf_size&&a.devices[i].split==b.devices[i].split,"device accounting");}
 for(size_t i=0;i<a.groups.size();i++){const auto&x=a.groups[i];const auto&y=b.groups[i];check(x.id==y.id&&x.n_slots==y.n_slots&&x.layers==y.layers&&x.host_bytes==y.host_bytes&&x.banks.size()==y.banks.size(),"bank grouping/budget");check(x.slru.key_of==y.slru.key_of&&x.slru.prev==y.slru.prev&&x.slru.next==y.slru.next&&x.slru.is_protected==y.slru.is_protected&&x.slru.n_protected==y.slru.n_protected,"SLRU states");
  for(size_t j=0;j<x.banks.size();j++){auto*p=x.banks[j];auto*q=y.banks[j];check(p->type==q->type&&ggml_are_same_shape(p,q)&&ggml_nbytes(p)==ggml_nbytes(q),"bank layout");if(p->data&&q->data)check(!memcmp(p->data,q->data,ggml_nbytes(p)),"bank bytes");}}
 for(size_t i=0;i<a.layers.size();i++){check(a.layers[i].ig==b.layers[i].ig,"layer placement");auto*x=a.layers[i].slot_map;auto*y=b.layers[i].slot_map;check(bool(x)==bool(y),"slot map presence");if(x&&y&&x->data&&y->data)check(!memcmp(x->data,y->data,ggml_nbytes(x)),"slot map contents");}
 check(a.stats_small.hits==b.stats_small.hits&&a.stats_small.misses==b.stats_small.misses&&a.stats_small.bytes==b.stats_small.bytes&&a.stats_large.hits==b.stats_large.hits&&a.stats_large.misses==b.stats_large.misses&&a.stats_large.bytes==b.stats_large.bytes&&a.stats_copy.hits==b.stats_copy.hits&&a.stats_copy.bytes==b.stats_copy.bytes,"all cache statistics");
}
static void verify_resident(const impl&cache,int layer){const auto&l=cache.layers.at(layer);if(l.ig<0)return;const auto&g=cache.groups.at(l.ig);const auto*map=(const int32_t*)l.slot_map->data;for(size_t bank=0;bank<l.experts.size();bank++){auto*w=l.experts[bank];for(int e=0;e<g.slru.n_expert;e++)if(map[e]>=0){check(map[e]<g.n_slots,"resident slot range");auto*expected=(const unsigned char*)w->data+e*w->nb[2];auto*actual=(const unsigned char*)g.banks[bank]->data+map[e]*g.banks[bank]->nb[2];check(!memcmp(expected,actual,w->nb[2]),"resident expert bytes");}}}
static size_t cases=0;
static void one_case(int devices,int layers,int banks,bool no_alloc,enum ggml_type type,size_t budget,bool explicit_split){
 std::vector<ggml_backend_t>backends;std::vector<ggml_backend_dev_t>original_devices;std::vector<ggml_backend_device>fakes(devices);std::vector<ggml_backend_buffer_type_t>types;std::vector<ggml_backend_buffer_type>tracked_types(devices);
 for(int i=0;i<devices;i++){auto*b=ggml_backend_cpu_init();check(b,"CPU backend primitive");backends.push_back(b);original_devices.push_back(b->device);fakes[i]=*b->device;fakes[i].iface.get_type=fake_gpu_type;b->device=&fakes[i];tracked_types[i]=*ggml_backend_cpu_buffer_type();tracked_types[i].iface.alloc_buffer=tracked_alloc;types.push_back(&tracked_types[i]);}
 fg151_test_model model;model.hparams.n_expert=8;model.hparams.n_layer_all=layers;model.hparams.n_expert_used_arr.fill(2);model.hparams.no_alloc=no_alloc;model.layers.resize(layers);model.layer_devices.resize(layers);
 for(int i=0;i<devices;i++)model.devices.push_back({false,&fakes[i]});if(explicit_split)for(int i=0;i<devices;i++)model.splits.push_back(float(i+1));
 ggml_context_ptr weights_ctx(ggml_init({ggml_tensor_overhead()*size_t(layers*banks+1),nullptr,true}));check(bool(weights_ctx),"weights metadata");
 for(int layer=0;layer<layers;layer++){auto&l=model.layers[layer];model.layer_devices[layer]=&fakes[layer%devices];for(int bank=0;bank<banks;bank++){auto*t=ggml_new_tensor_3d(weights_ctx.get(),type,type==GGML_TYPE_F32?16:256,8,8);ggml_format_name(t,"test.%d.%d",layer,bank);if(banks==2&&bank==0)l.ffn_gate_up_exps=t;else if(bank==0)l.ffn_gate_exps=t;else if(bank==1&&banks==3)l.ffn_up_exps=t;else l.ffn_down_exps=t;}}
 ggml_backend_buffer_ptr weights(ggml_backend_alloc_ctx_tensors_from_buft(weights_ctx.get(),ggml_backend_cpu_buffer_type()));check(bool(weights),"weights allocation");ggml_backend_buffer_set_usage(weights.get(),GGML_BACKEND_BUFFER_USAGE_WEIGHTS);
 for(int layer=0;layer<layers;layer++){auto ts=llama_moe_cache_layer_experts(model.layers[layer]);for(size_t bank=0;bank<ts.size();bank++){auto*t=ts[bank];for(size_t i=0;i<ggml_nbytes(t);i++)((unsigned char*)t->data)[i]=(unsigned char)((i*13+layer*37+bank*71)%251);}}
 size_t context_before=live_contexts.size(),buffers_before=live_buffers.size();
 {
  switches(false);impl native(model,backends,types,budget);switches(true);impl goose(model,backends,types,budget);compare_cache(native,goose);
  std::map<ggml_backend_buffer_type_t,size_t>native_memory,goose_memory;
  for(const auto&d:native.devices)if(d.buf)native_memory[ggml_backend_buffer_get_type(d.buf.get())]+=d.buf_size;if(native.buf_host)native_memory[ggml_backend_buffer_get_type(native.buf_host.get())]+=native.buf_host_size;
  auto req=goose.fg151_prepare();req.memory=&goose_memory;fg151_lifecycle(6,(int64_t)(uintptr_t)&req);check(native_memory==goose_memory,"Goose memory breakdown");cases++;
  if(!no_alloc){
   for(int step=0;step<9;step++)for(int layer=0;layer<layers;layer++){
    if(native.layers[layer].ig<0)continue;auto mapn=native.layers[layer].slot_map;auto mapg=goose.layers[layer].slot_map;
    ggml_context_ptr request(ggml_init({ggml_tensor_overhead()*16+2*ggml_graph_overhead_custom(8,false),nullptr,true}));
    auto*ids=ggml_new_tensor_1d(request.get(),GGML_TYPE_I32,2);auto*dstn=ggml_dup_tensor(request.get(),mapn);auto*dstg=ggml_dup_tensor(request.get(),mapg);auto*lookupn=ggml_get_rows(request.get(),dstn,ids);auto*lookupg=ggml_get_rows(request.get(),dstg,ids);auto*gn=ggml_new_graph_custom(request.get(),8,false);auto*gg=ggml_new_graph_custom(request.get(),8,false);ggml_build_forward_expand(gn,lookupn);ggml_build_forward_expand(gg,lookupg);
    ggml_backend_buffer_ptr rb(ggml_backend_alloc_ctx_tensors_from_buft(request.get(),ggml_backend_cpu_buffer_type()));check(bool(rb),"request buffer");((int32_t*)ids->data)[0]=(step+layer)%8;((int32_t*)ids->data)[1]=(step*3+layer)%8;
    auto backend=backends[layer%devices];switches(false);bool nr=native.copy(backend,mapn,dstn,gn);switches(true);bool gr=goose.copy(backend,mapg,dstg,gg);check(nr&&gr,"copy recognized");ggml_backend_synchronize(backend);check(!memcmp(dstn->data,dstg->data,ggml_nbytes(dstn)),"uploaded slot map");compare_cache(native,goose);for(int l=0;l<layers;l++){verify_resident(native,l);verify_resident(goose,l);}cases++;
    const auto&ln=native.layers[layer];const auto&lg=goose.layers[layer];for(size_t bank=0;bank<ln.experts.size();bank++)for(int e=0;e<8;e++){
     ggml_context_ptr viewctx(ggml_init({ggml_tensor_overhead()*2,nullptr,true}));auto*readn=ggml_dup_tensor(viewctx.get(),ln.experts[bank]);auto*readg=ggml_dup_tensor(viewctx.get(),lg.experts[bank]);ggml_backend_buffer_ptr readbuf(ggml_backend_alloc_ctx_tensors_from_buft(viewctx.get(),ggml_backend_cpu_buffer_type()));ggml_backend_buffer_clear(readbuf.get(),0);
     switches(false);int64_t n=native.copy_experts(backend,ln.experts[bank],readn,e,7);switches(true);int64_t g=goose.copy_experts(backend,lg.experts[bank],readg,e,7);check(n==g,"cached read span");ggml_backend_synchronize(backend);check(!memcmp(readn->data,readg->data,ggml_nbytes(readn)),"cached read bytes");cases++;
    }
   }
   compare_cache(native,goose);
  }
 }
 check(live_contexts.size()==context_before&&live_buffers.size()==buffers_before,"all cache resources released");
 // Resource failure checks are local to the new owner:both null-return allocation and rollback cleanup.
 for(bool goose:{false,true})for(int stage=1;stage<=devices+2;stage++){
  switches(goose);context_calls=buffer_calls=0;fail_context=stage;bool caught=false;try{impl candidate(model,backends,types,budget);}catch(const std::runtime_error&){caught=true;}fail_context=0;check(caught,"context failure propagated");check(live_contexts.size()==context_before&&live_buffers.size()==buffers_before,"context failure cleanup");cases++;
 }
 for(bool goose:{false,true})for(int stage=1;stage<=devices+1;stage++){
  if(no_alloc&&!goose)continue;
  switches(goose);context_calls=buffer_calls=buffer_requests=0;fail_request=stage;bool caught=false;try{impl candidate(model,backends,types,budget);}catch(const std::runtime_error&){caught=true;}fail_request=0;check(caught,"buffer failure propagated");check(live_contexts.size()==context_before&&live_buffers.size()==buffers_before,"buffer failure cleanup");cases++;
 }
 for(int i=0;i<devices;i++){backends[i]->device=original_devices[i];ggml_backend_free(backends[i]);}
}
int main(){
 for(int devices:{1,2})for(int layers:{2,3})for(int banks:{2,3})for(bool no_alloc:{false,true})for(bool split:{false,true}){
  one_case(devices,layers,banks,no_alloc,GGML_TYPE_F32,32768,split);
 }
 one_case(1,2,3,false,GGML_TYPE_Q4_K,32768,false);
 check(live_contexts.empty()&&live_buffers.empty(),"global resource balance");printf("{\"status\":\"PASS_LOCAL_NATIVE_GOOSE_CACHE_LIFECYCLE\",\"cases\":%zu,\"real_ggml_resources\":true,\"fake_model_metadata\":true,\"gpu_primitives\":\"CPU substitute;not CUDA or Apex\"}\n",cases);
}
'''
(D/'lifecycle-fixture.cpp').write_text(header+prefix+tail+main)
print('Local fixture prepared from actual controller bodies with synthetic metadata and real GGML allocations/copies')
