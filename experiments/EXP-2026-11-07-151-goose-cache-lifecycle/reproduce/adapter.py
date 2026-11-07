from pathlib import Path
import re
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp151-cache-lifecycle-20261107');D=A/'source/expert-cache';p=D/'llama-moe-cache.cpp';s=p.read_text();s=s.replace('#include "budget.h"','#include "budget.h"\n#include "lifecycle-bridge.h"')
pos=s.index('namespace {');s=s[:pos]+'''static bool fg151_enabled=true;
__attribute__((constructor))static void fg151_config(){const char*v=getenv("GGML_GOOSE_EXPERT_LIFECYCLE");fg151_enabled=!v||strcmp(v,"0");}
'''+s[pos:]
# State adapters contain ABI projections and direct GGML/OS services,never the original constructor or cache planner.
adapter=r'''
    bool fg151_owned=false;
    static ggml_context_ptr&fg151_context(impl&s,int64_t id){if(id==-1)return s.ctx_host;if(id==-2)return s.ctx_views;GGML_ASSERT(id>=0);return s.devices.at((size_t)id).ctx;}
    static ggml_backend_buffer_ptr&fg151_buffer(impl&s,int64_t id){if(id==-1)return s.buf_host;GGML_ASSERT(id>=0);return s.devices.at((size_t)id).buf;}
    static int64_t fg151_size(size_t value){GGML_ASSERT(value<=INT64_MAX);return(int64_t)value;}
    static int64_t fg151_read(fg151_request*r,int64_t field,int64_t i,int64_t j){auto&s=*(impl*)r->state;
        auto tensor=[](int64_t ptr){GGML_ASSERT(ptr>0);return(const ggml_tensor*)(uintptr_t)ptr;};
        switch(field){
        case 0:return fg151_size(r->backends->size());case 1:return fg151_size(s.layers.size());case 2:return fg151_size(s.devices.size());case 3:return fg151_size(s.groups.size());
        case 4:return r->model->split_mode();case 5:return r->model->hparams.n_expert;case 6:return s.n_expert_used;case 7:return r->model->hparams.no_alloc;
        case 8:return GGML_BACKEND_DEVICE_TYPE_GPU;case 9:return GGML_BACKEND_DEVICE_TYPE_IGPU;case 10:return LLAMA_SPLIT_MODE_TENSOR;case 11:return GGML_BACKEND_BUFFER_USAGE_WEIGHTS;
        case 12:return fg151_size(r->model->devices.size());case 13:return(int64_t)(uintptr_t)r->model->devices.at(i).dev;case 14:return(int64_t)(uintptr_t)r->model->dev_layer((int)i);
        case 15:return ggml_backend_dev_type(ggml_backend_get_device(r->backends->at(i)));case 16:return(int64_t)(uintptr_t)ggml_backend_get_device(r->backends->at(i));
        case 17:return(int64_t)(uintptr_t)ggml_backend_get_device(s.devices.at(i).backend);case 18:return fg151_size(s.devices.at(i).host_bytes);case 19:return(int64_t)(uintptr_t)s.devices.at(i).buf.get();case 20:return(int64_t)(uintptr_t)s.devices.at(i).buf.get();case 21:return fg151_size(s.devices.at(i).buf_size);case 22:return(int64_t)(uintptr_t)s.devices.at(i).ctx.get();case 23:return fg151_size(ggml_backend_buft_get_alignment(s.devices.at(i).buft));
        case 24:return fg151_size(r->experts.size());case 25:return(int64_t)(uintptr_t)r->experts.at(i);case 26:return s.groups.at(i).id;case 27:return fg151_size(s.groups.at(i).ref.size());case 28:return(int64_t)(uintptr_t)s.groups.at(i).ref.at(j);case 29:return fg151_size(s.groups.at(i).layers.size());case 30:return s.groups.at(i).layers.at(j);case 31:return s.groups.at(i).n_slots;case 32:return fg151_size(s.groups.at(i).host_bytes);case 33:return fg151_size(r->tensors.at(i));case 34:return fg151_size(r->host_tensors);
        case 35:return s.layers.at(i).ig;case 36:return(int64_t)(uintptr_t)s.layers.at(i).slot_map;case 37:return fg151_size(s.layers.at(i).experts.size());case 38:return(int64_t)(uintptr_t)s.layers.at(i).experts.at(j);
        case 40:return tensor(i)->type;case 41:GGML_ASSERT(j>=0&&j<4);return tensor(i)->ne[j];case 42:GGML_ASSERT(j>=0&&j<4);return fg151_size(tensor(i)->nb[j]);case 43:return(int64_t)(uintptr_t)tensor(i)->buffer;case 44:return ggml_backend_buffer_get_usage(tensor(i)->buffer);case 45:return ggml_backend_buffer_is_host(tensor(i)->buffer);case 46:return fg151_size(ggml_nbytes(tensor(i)));
        case 47:return(int64_t)(uintptr_t)ggml_get_first_tensor((ggml_context*)(uintptr_t)i);case 48:return(int64_t)(uintptr_t)ggml_get_next_tensor((ggml_context*)(uintptr_t)i,(ggml_tensor*)(uintptr_t)j);
        case 49:return(int64_t)(uintptr_t)s.ctx_host.get();case 50:return(int64_t)(uintptr_t)s.ctx_views.get();case 51:return(int64_t)(uintptr_t)s.buf_host.get();case 52:return fg151_size(s.buf_host_size);case 54:return r->model_indices.at(i);case 55:return fg151_size(ggml_tensor_overhead());case 56:return fg151_size(ggml_backend_buft_get_alignment(ggml_backend_cpu_buffer_type()));case 57:return fg151_size(ggml_backend_buffer_get_size((ggml_backend_buffer_t)(uintptr_t)i));
        case 58:{const auto&l=r->model->layers.at(i);switch(j){case 0:return(int64_t)(uintptr_t)l.ffn_gate_up_exps;case 1:return(int64_t)(uintptr_t)l.ffn_gate_exps;case 2:return(int64_t)(uintptr_t)l.ffn_up_exps;case 3:return(int64_t)(uintptr_t)l.ffn_down_exps;default:GGML_ABORT("invalid expert tensor field");}}
        default:GGML_ABORT("invalid cache lifecycle metadata");}}
    static void fg151_write(fg151_request*r,int64_t field,int64_t i,int64_t j,int64_t value){auto&s=*(impl*)r->state;GGML_UNUSED(j);switch(field){
        case 0:GGML_ASSERT(value>=0);s.devices.at(i).host_bytes=(size_t)value;return;case 1:GGML_ASSERT(value>=0);s.groups.at(i).host_bytes=(size_t)value;return;
        case 3:GGML_ASSERT(value>=0);s.devices.at(i).buf_size=(size_t)value;return;case 4:GGML_ASSERT(value>=0);s.buf_host_size=(size_t)value;return;
        case 5:GGML_ASSERT(value>=0&&value<=INT32_MAX);s.layers.at(i).ig=(int32_t)value;return;
        case 7:GGML_ASSERT(i>0);((ggml_tensor*)(uintptr_t)i)->buffer=(ggml_backend_buffer_t)(uintptr_t)value;return;
        case 8:GGML_ASSERT(value>=-1);r->model_indices.at(i)=value;return;
        default:GGML_ABORT("invalid cache lifecycle state write");}}
    static void fg151_budget_project(fg151_request&r){auto&s=*(impl*)r.state;const auto&model=*r.model;fg150_budget_request q={};q.bytes=fg151_size(r.bytes);q.used=s.n_expert_used;q.tensors.resize(s.devices.size());
        const float*split=model.tensor_split();if(split)q.split.assign(split,split+model.n_devices());
        for(size_t i=0;i<s.devices.size();i++){auto&d=s.devices[i];size_t free=0,total=0;ggml_backend_dev_memory(ggml_backend_get_device(d.backend),&free,&total);q.devices.push_back({fg151_size(d.host_bytes),fg151_size(free),fg151_size(ggml_backend_buft_get_alignment(d.buft)),r.model_indices.at(i),0});}
        for(const auto&g:s.groups){q.groups.push_back({g.id,fg151_size(g.host_bytes),fg151_size(g.layers.size()),g.ref.at(0)->ne[2],fg151_size(q.strides.size()),fg151_size(g.ref.size()),0});for(const auto*t:g.ref)q.strides.push_back(fg151_size(t->nb[2]));}
        fg150_cache(8,(int64_t)(uintptr_t)&q);
        r.tensors.resize(q.tensors.size());for(size_t i=0;i<q.tensors.size();i++){s.devices[i].split=q.devices[i].split;r.tensors[i]=(size_t)q.tensors[i];}
        r.host_tensors=(size_t)q.host_tensors;for(size_t i=0;i<q.groups.size();i++){GGML_ASSERT(q.groups[i].slots>=0&&q.groups[i].slots<=INT32_MAX);s.groups[i].n_slots=(int32_t)q.groups[i].slots;}
    }
    static int64_t fg151_service(fg151_request*r,int64_t op,int64_t a,int64_t b,int64_t d){auto&s=*(impl*)r->state;switch(op){
        case 0:r->experts.clear();return 0;case 1:GGML_ASSERT(a>0);r->experts.push_back((ggml_tensor*)(uintptr_t)a);return 0;
        case 2:{GGML_ASSERT(s.devices.size()<INT32_MAX);auto&device=s.devices.emplace_back();device.backend=r->backends->at(a);device.buft=r->bufts->at(a);r->model_indices.push_back(-1);return(int64_t)s.devices.size()-1;}
        case 3:{GGML_ASSERT(a>=0&&(size_t)a<s.devices.size()&&s.groups.size()<INT32_MAX);auto&group=s.groups.emplace_back();group.id=(int32_t)a;group.ref=r->experts;return(int64_t)s.groups.size()-1;}
        case 4:GGML_ASSERT(b>=0&&b<=INT32_MAX);s.groups.at(a).layers.push_back((int32_t)b);return 0;
        case 5:{static const char*const messages[]={"MoE cache requires a GPU backend","MoE cache does not support tensor parallelism","MoE cache requires a MoE model","MoE cache is too small to hold the experts of one token","failed to create the MoE cache context","failed to allocate the MoE cache buffers"};GGML_ASSERT(a>=0&&a<6);throw std::runtime_error(messages[a]);}
        case 6:LLAMA_LOG_WARN("%s: no layer has all of its experts in host memory, MoE cache is disabled\n","impl");return 0;
        case 7:fg151_budget_project(*r);return 0;
        case 8:{GGML_ASSERT(b>=0);ggml_init_params p={(size_t)b,nullptr,true};fg151_context(s,a).reset(ggml_init(p));return(int64_t)(uintptr_t)fg151_context(s,a).get();}
        case 9:{auto&g=s.groups.at(a);g.slru.init((int32_t)s.layers.size(),(int32_t)g.ref.at(0)->ne[2],g.n_slots);return 0;}
        case 10:{auto&g=s.groups.at(a);auto*t=g.ref.at(b);auto*bank=ggml_new_tensor_3d(s.devices.at(g.id).ctx.get(),t->type,t->ne[0],t->ne[1],d);GGML_ASSERT(bank->nb[2]==t->nb[2]);ggml_format_name(bank,"moe_cache.%zu.%s",(size_t)a,t->name);g.banks.push_back(bank);return(int64_t)(uintptr_t)bank;}
        case 11:s.layers.at(a).experts=r->experts;return 0;
        case 12:{auto&g=s.groups.at(a);auto&layer=s.layers.at(b);auto*bank=g.banks.at(d);auto*cached=ggml_view_3d(s.devices.at(g.id).ctx.get(),bank,bank->ne[0],bank->ne[1],g.n_slots,bank->nb[1],bank->nb[2],0);ggml_format_name(cached,"moe_cache.%s",layer.experts.at(d)->name);s.bindings[layer.experts.at(d)]={(int32_t)b,(int32_t)d,cached};return(int64_t)(uintptr_t)cached;}
        case 13:{auto&g=s.groups.at(a);auto&layer=s.layers.at(b);layer.slot_map=ggml_new_tensor_2d(s.ctx_host.get(),GGML_TYPE_I32,1,g.ref.at(0)->ne[2]);ggml_format_name(layer.slot_map,"moe_cache.slot_map-%d",(int)b);s.layer_of[layer.slot_map]=(int32_t)b;return(int64_t)(uintptr_t)layer.slot_map;}
        case 14:{auto type=a==-1?ggml_backend_cpu_buffer_type():s.devices.at(a).buft;auto&buffer=fg151_buffer(s,a);if(b!=0)buffer.reset(ggml_backend_buft_alloc_buffer(type,0));else buffer.reset(ggml_backend_alloc_ctx_tensors_from_buft(fg151_context(s,a).get(),type));return(int64_t)(uintptr_t)buffer.get();}
        case 15:ggml_backend_buffer_clear(fg151_buffer(s,a).get(),(uint8_t)b);return 0;
        case 16:ggml_backend_buffer_set_usage(fg151_buffer(s,a).get(),GGML_BACKEND_BUFFER_USAGE_WEIGHTS);return 0;
        case 17:fg151_context(s,a).reset();return 0;case 18:fg151_buffer(s,a).reset();return 0;
        case 19:s.groups.at(a).slru.slot_map.at(b)=(int32_t*)s.layers.at(b).slot_map->data;return 0;
        case 20:{const auto&device=s.devices.at(a);LLAMA_LOG_INFO("%s: %10s MoE cache size = %8.2f MiB for %.2f MiB of host experts\n","impl",ggml_backend_buft_name(device.buft),device.buf_size/1024.0/1024.0,device.host_bytes/1024.0/1024.0);return 0;}
        case 21:{const auto&g=s.groups.at(a);LLAMA_LOG_INFO("%s: %2zu layers, %s: %5d slots (%.1f%%)\n","impl",g.layers.size(),ggml_type_name(g.ref.back()->type),g.n_slots,100.0*g.n_slots/(g.layers.size()*g.ref.at(0)->ne[2]));return 0;}
        case 22:LLAMA_LOG_WARN("%s: MoE cache budget is too small for %zu layers, they are not cached\n","impl",s.groups.at(a).layers.size());return 0;
        case 23:s.log_stats();return 0;case 26:s.bindings.clear();s.layer_of.clear();return 0;
        default:GGML_ABORT("invalid cache lifecycle service");}}
    fg151_request fg151_prepare(){fg151_request q={};q.state=this;q.read=fg151_read;q.write=fg151_write;q.service=fg151_service;return q;}
'''
marker='    impl(const llama_model & model,';pos=s.index(marker);s=s[:pos]+adapter+'\n'+s[pos:]
needle='            n_expert_used(model.hparams.n_expert_used_max()), layers(model.layers.size()) {';assert s.count(needle)==1
s=s.replace(needle,needle+r'''
        if(fg151_enabled){
            fg151_owned=true;auto q=fg151_prepare();q.model=&model;q.backends=&backends;q.bufts=&bufts;q.bytes=size;
            try{fg151_lifecycle(0,(int64_t)(uintptr_t)&q);}catch(...){fg151_lifecycle(2,(int64_t)(uintptr_t)&q);throw;}
            return;
        }
''',1)
needle='    ~impl() {\n        log_stats();\n    }';assert s.count(needle)==1
s=s.replace(needle,'''    ~impl() {
        if(fg151_owned){auto q=fg151_prepare();fg151_lifecycle(1,(int64_t)(uintptr_t)&q);return;}
        log_stats();
    }''')
s+='''
extern "C" int64_t fg151_meta(int64_t c,int64_t f,int64_t i,int64_t j){auto*q=(fg151_request*)(uintptr_t)c;GGML_ASSERT(q&&q->read);return q->read(q,f,i,j);}
extern "C" void fg151_put(int64_t c,int64_t f,int64_t i,int64_t j,int64_t v){auto*q=(fg151_request*)(uintptr_t)c;GGML_ASSERT(q&&q->write);q->write(q,f,i,j,v);}
extern "C" int64_t fg151_action(int64_t c,int64_t op,int64_t a,int64_t b,int64_t d){auto*q=(fg151_request*)(uintptr_t)c;GGML_ASSERT(q&&q->service);return q->service(q,op,a,b,d);}
''';p.write_text(s)
p=D/'lifecycle.goose';s=p.read_text().replace('    assert(mode==1);let logged=action(c,23,0,0,0);cleanup(c);return 0;', '    if mode==1 {let logged=action(c,23,0,0,0);cleanup(c);return 0;}\n    assert(mode==2);cleanup(c);return 0;');p.write_text(s)
p=D/'lifecycle-unit.c';s=p.read_text().replace('calls[2]','calls[3]').replace('mode>=2','mode>=3').replace('i<2','i<3');p.write_text(s)
(D/'lifecycle-adapters.inc').write_text(adapter)
print('Native constructor/destructor now default to real Goose control;old bodies are explicit rollback,ABI services contain no original executor')
