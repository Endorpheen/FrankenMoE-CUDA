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

        case 60:return(int64_t)(uintptr_t)ggml_backend_buffer_get_type((ggml_backend_buffer_t)(uintptr_t)i);
        case 70:{auto it=s.layer_of.find(r->src);return it==s.layer_of.end()?-1:it->second;}
        case 71:return r->layer;case 74:return(int64_t)(uintptr_t)r->backend;case 75:return ggml_graph_n_nodes(r->graph);case 76:return(int64_t)(uintptr_t)ggml_graph_node(r->graph,(int)i);case 77:return tensor(i)->op;case 78:return GGML_OP_GET_ROWS;case 79:GGML_ASSERT(j>=0&&j<GGML_MAX_SRC);return(int64_t)(uintptr_t)tensor(i)->src[j];case 80:return(int64_t)(uintptr_t)tensor(i)->view_src;case 81:return ggml_is_contiguous(tensor(i));case 83:return fg151_size(s.ids.size());case 84:return fg151_size(s.fills.size());case 85:return s.fills.at(i).expert;case 86:return s.fills.at(i).slot;case 88:return(int64_t)(uintptr_t)r->dst;case 89:return(int64_t)(uintptr_t)r->src;case 91:return(int64_t)(uintptr_t)s.devices.at(i).backend;case 94:return fg151_size(r->hits);
        case 97:{auto it=s.bindings.find(r->weight);return it==s.bindings.end()?-1:it->second.il;}
        case 98:{auto it=s.bindings.find(r->weight);GGML_ASSERT(it!=s.bindings.end());return it->second.ip;}
        case 101:{const auto&g=s.groups.at(r->group);const auto*map=g.slru.slot_map.at(r->layer);GGML_ASSERT(map&&i>=0&&i<g.slru.n_expert);return map[i];}
        case 102:return(int64_t)(uintptr_t)s.groups.at(r->group).slru.slot_map.at(r->layer);
        case 103:return(int64_t)(uintptr_t)s.groups.at(r->group).banks.at(r->bank);case 105:return s.groups.at(r->group).ref.at(0)->ne[2];case 106:return r->expert;case 107:return r->last;
        case 110:{GGML_ASSERT(i==0||i==1);const auto&st=i==0?s.stats_small:s.stats_large;if(j==0)return fg151_size(st.hits);if(j==1)return fg151_size(st.misses);GGML_ASSERT(j==2);return fg151_size(st.bytes);}
        case 111:return fg151_size(s.stats_copy.hits);case 112:return fg151_size(s.stats_copy.bytes);case 113:return(int64_t)(uintptr_t)tensor(i)->data;case 115:return ggml_nelements(tensor(i));case 116:return GGML_TYPE_I32;
        case 120:{auto it=r->memory->find((ggml_backend_buffer_type_t)(uintptr_t)i);return it==r->memory->end()?0:fg151_size(it->second);}
        default:GGML_ABORT("invalid cache lifecycle metadata");}}
    static void fg151_write(fg151_request*r,int64_t field,int64_t i,int64_t j,int64_t value){auto&s=*(impl*)r->state;GGML_UNUSED(j);switch(field){
        case 0:GGML_ASSERT(value>=0);s.devices.at(i).host_bytes=(size_t)value;return;case 1:GGML_ASSERT(value>=0);s.groups.at(i).host_bytes=(size_t)value;return;
        case 3:GGML_ASSERT(value>=0);s.devices.at(i).buf_size=(size_t)value;return;case 4:GGML_ASSERT(value>=0);s.buf_host_size=(size_t)value;return;
        case 5:GGML_ASSERT(value>=0&&value<=INT32_MAX);s.layers.at(i).ig=(int32_t)value;return;
        case 7:GGML_ASSERT(i>0);((ggml_tensor*)(uintptr_t)i)->buffer=(ggml_backend_buffer_t)(uintptr_t)value;return;
        case 8:GGML_ASSERT(value>=-1);r->model_indices.at(i)=value;return;

        case 10:GGML_ASSERT(value>=0);r->layer=value;return;case 11:GGML_ASSERT(value>=0);r->group=value;return;case 12:GGML_ASSERT(value>=0);r->bank=value;return;case 13:GGML_ASSERT(value>=0);r->offset=value;return;case 14:GGML_ASSERT(value>=0);r->transfer_size=value;return;case 15:r->selection=(const ggml_tensor*)(uintptr_t)value;return;
        case 20:{GGML_ASSERT(value>=0&&(i==0||i==1));auto&st=i==0?s.stats_small:s.stats_large;if(j==0)st.hits=(size_t)value;else if(j==1)st.misses=(size_t)value;else {GGML_ASSERT(j==2);st.bytes=(size_t)value;}return;}
        case 21:GGML_ASSERT(value>=0);s.stats_copy.hits=(size_t)value;return;case 22:GGML_ASSERT(value>=0);s.stats_copy.bytes=(size_t)value;return;
        case 23:GGML_ASSERT(value>=0);(*r->memory)[(ggml_backend_buffer_type_t)(uintptr_t)i]=(size_t)value;return;
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

        case 30:ggml_reset(s.ctx_views.get());return 0;
        case 31:{auto*t=(ggml_tensor*)(uintptr_t)a;auto*view=ggml_view_3d(s.ctx_views.get(),t,t->ne[0],t->ne[1],b,t->nb[1],t->nb[2],(size_t)r->offset);return(int64_t)(uintptr_t)view;}
        case 32:ggml_backend_view_init((ggml_tensor*)(uintptr_t)a);return 0;
        case 33:ggml_backend_tensor_copy_async(r->backend,r->backend,(ggml_tensor*)(uintptr_t)a,(ggml_tensor*)(uintptr_t)b);return 0;
        case 34:GGML_ASSERT(a>=0);s.ids.resize((size_t)a);return 0;
        case 35:ggml_backend_tensor_get_async(r->backend,r->selection,s.ids.data(),0,ggml_nbytes(r->selection));return 0;
        case 36:ggml_backend_synchronize(r->backend);return 0;
        case 37:GGML_ASSERT(a>=0);s.fills.reserve((size_t)a);s.groups.at(r->group).slru.uniq.reserve((size_t)a);return 0;
        case 38:return s.groups.at(r->group).slru.plan((int32_t)r->layer,s.ids.data(),s.ids.size(),s.fills,r->hits);
        case 39:{if(fg150_cache_enabled){fg150_cache_request q={};q.fills=&s.fills;q.n_ids=s.fills.size();q.extra[0]=a;q.extra[7]=1;return fg150_cache_call(7,q);}
            GGML_ASSERT(a>=0&&(size_t)a<s.fills.size());size_t n=1;while((size_t)a+n<s.fills.size()&&s.fills[a+n].expert==s.fills[a].expert+(int32_t)n&&s.fills[a+n].slot==s.fills[a].slot+(int32_t)n)n++;return(int64_t)n;}
        case 40:ggml_backend_tensor_set_async(r->backend,(ggml_tensor*)(uintptr_t)a,(const void*)(uintptr_t)b,(size_t)d,(size_t)r->transfer_size);return 0;
        case 41:if(a==0)GGML_ABORT("the experts of layer %d are selected in the same split as their MoE cache lookup",(int)b);GGML_ABORT("the MoE cache is too small for the experts selected in layer %d",(int)b);
        case 42:{const auto&g=s.groups.at(r->group);if(fg150_cache_enabled){fg150_cache_request q={};q.ids=g.slru.slot_map.at(r->layer);q.n_ids=g.slru.n_expert;q.extra[0]=r->expert;q.extra[1]=r->last;return fg150_cache_call(6,q);}
            const auto*slots=g.slru.slot_map.at(r->layer);int64_t n=1;while(r->expert+n<=r->last&&slots[r->expert+n]==slots[r->expert]+n)n++;return n;}
        default:GGML_ABORT("invalid cache lifecycle service");}}
    fg151_request fg151_prepare(){fg151_request q={};q.state=this;q.read=fg151_read;q.write=fg151_write;q.service=fg151_service;return q;}

