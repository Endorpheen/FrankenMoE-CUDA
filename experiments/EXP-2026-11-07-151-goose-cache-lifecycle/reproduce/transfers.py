from pathlib import Path
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp151-cache-lifecycle-20261107');D=A/'source/expert-cache';p=D/'lifecycle-bridge.h';s=p.read_text().replace('#include <vector>','#include <vector>\n#include <map>')
s=s.replace(' int64_t(*read)', ''' ggml_backend_t backend=nullptr;const ggml_tensor*src=nullptr;ggml_tensor*dst=nullptr;const ggml_tensor*weight=nullptr;ggml_cgraph*graph=nullptr;
 const ggml_tensor*lookup=nullptr;const ggml_tensor*selection=nullptr;
 int64_t layer=-1,group=-1,bank=-1,expert=0,last=0,offset=0,transfer_size=0;size_t hits=0;
 std::map<ggml_backend_buffer_type_t,size_t>*memory=nullptr;
 int64_t(*read)''');p.write_text(s)
copy=r'''fn copy_read(c:i64)->i64 {
    let layer=get(c,97,0,0);if layer<0 {return 0;}
    put(c,10,0,0,layer);let group=get(c,35,layer,0);assert(group>=0);put(c,11,0,0,group);put(c,12,0,0,get(c,98,0,0));
    let device=get(c,26,group,0);if get(c,74,0,0)!=get(c,91,device,0)||get(c,102,0,0)==0 {return 0;}
    let first=get(c,106,0,0);let last=get(c,107,0,0);let experts=get(c,105,0,0);
    if first<0||last<first||last>=experts||get(c,101,first,0)<0 {return 0;}
    let count=action(c,42,0,0,0);assert(count>0);
    let bank=get(c,103,0,0);let dst=get(c,88,0,0);let slot=get(c,101,first,0);
    let reset=action(c,30,0,0,0);
    put(c,13,0,0,mul(slot,get(c,42,bank,2)));let src_view=action(c,31,bank,count,0);
    put(c,13,0,0,mul(first,get(c,42,dst,2)));let dst_view=action(c,31,dst,count,1);
    assert(src_view!=0&&dst_view!=0);let a=action(c,32,src_view,0,0);let b=action(c,32,dst_view,0,0);let copied=action(c,33,src_view,dst_view,0);
    put(c,21,0,0,add(get(c,111,0,0),count));put(c,22,0,0,add(get(c,112,0,0),get(c,46,src_view,0)));return count;
}
fn upload(c:i64,tensor:i64,source:i64,offset:i64,bytes:i64) {
    assert(source>0&&offset>=0&&bytes>=0);assert(add(offset,bytes)<=get(c,46,tensor,0));
    put(c,14,0,0,bytes);let copied=action(c,40,tensor,source,offset);
}
fn copy_upload(c:i64)->i64 {
    let layer=get(c,70,0,0);if layer<0 {return 0;}
    put(c,10,0,0,layer);let group=get(c,35,layer,0);assert(group>=0);put(c,11,0,0,group);
    assert(get(c,74,0,0)==get(c,91,get(c,26,group,0),0));
    let nodes=get(c,75,0,0);let dst=get(c,88,0,0);var lookup:i64=0;
    var i:i64=0;while i<nodes&&lookup==0 {let node=get(c,76,i,0);if get(c,77,node,0)==get(c,78,0,0)&&get(c,79,node,0)==dst {lookup=node;}i+=1;}
    assert(lookup!=0);let selection=get(c,79,lookup,1);assert(selection!=0);let view=get(c,80,selection,0);
    for i in nodes {let node=get(c,76,i,0);if node==selection||node==view {let ignored=action(c,41,0,layer,0);assert(false);}}
    assert(get(c,81,selection,0)!=0&&get(c,40,selection,0)==get(c,116,0,0));put(c,15,0,0,selection);
    let elements=get(c,115,selection,0);assert(elements>=0);let resized=action(c,34,elements,0,0);
    let experts=get(c,105,0,0);let capacity=if elements<experts {elements}else {experts};let reserved=action(c,37,capacity,0,0);
    let downloaded=action(c,35,0,0,0);let synchronized=action(c,36,0,0,0);
    if action(c,38,0,0,0)==0 {let ignored=action(c,41,1,layer,0);assert(false);}
    var bytes:i64=0;let fills=get(c,84,0,0);
    for bank_index in get(c,37,layer,0) {
        put(c,12,0,0,bank_index);let weights=get(c,38,layer,bank_index);let bank=get(c,103,0,0);let expert_bytes=get(c,42,weights,2);var current:i64=0;
        while current<fills {
            let count=action(c,39,current,0,0);assert(count>0&&count<=fills-current);
            let source_offset=mul(get(c,85,current,0),expert_bytes);let destination_offset=mul(get(c,86,current,0),expert_bytes);let span=mul(count,expert_bytes);
            assert(add(source_offset,span)<=get(c,46,weights,0));let source=add(get(c,113,weights,0),source_offset);
            upload(c,bank,source,destination_offset,span);bytes=add(bytes,span);current=add(current,count);
        }
    }
    let bucket=if get(c,83,0,0)<=mul(8,get(c,6,0,0)) {0}else {1};
    put(c,20,bucket,0,add(get(c,110,bucket,0),get(c,94,0,0)));put(c,20,bucket,1,add(get(c,110,bucket,1),fills));put(c,20,bucket,2,add(get(c,110,bucket,2),bytes));
    let source=get(c,89,0,0);upload(c,dst,get(c,113,source,0),0,get(c,46,source,0));return 1;
}
fn breakdown(c:i64) {
    for device in get(c,2,0,0) {let buffer=get(c,20,device,0);if buffer!=0 {let kind=get(c,60,buffer,0);put(c,23,kind,0,add(get(c,120,kind,0),get(c,21,device,0)));}}
    let buffer=get(c,51,0,0);if buffer!=0 {let kind=get(c,60,buffer,0);put(c,23,kind,0,add(get(c,120,kind,0),get(c,52,0,0)));}
}
'''
p=D/'lifecycle.goose';s=p.read_text();pos=s.index('fn entry(');s=s[:pos]+copy+s[pos:];s=s.replace('    assert(mode==2);cleanup(c);return 0;', '''    if mode==2 {cleanup(c);return 0;}
    if mode==3 {return copy_read(c);}
    if mode==4 {return copy_upload(c);}
    if mode==5 {let layer=get(c,71,0,0);let group=get(c,35,layer,0);assert(group>=0);return get(c,91,get(c,26,group,0),0);}
    assert(mode==6);breakdown(c);return 0;''');p.write_text(s)
p=D/'lifecycle-unit.c';s=p.read_text().replace('calls[3]','calls[7]').replace('mode>=3','mode>=7').replace('i<3','i<7');p.write_text(s)
p=D/'llama-moe-cache.cpp';s=p.read_text();marker='        default:GGML_ABORT("invalid cache lifecycle metadata");'
extra=r'''
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
'''
assert s.count(marker)==1;s=s.replace(marker,extra+marker)
marker='        default:GGML_ABORT("invalid cache lifecycle state write");'
extra=r'''
        case 10:GGML_ASSERT(value>=0);r->layer=value;return;case 11:GGML_ASSERT(value>=0);r->group=value;return;case 12:GGML_ASSERT(value>=0);r->bank=value;return;case 13:GGML_ASSERT(value>=0);r->offset=value;return;case 14:GGML_ASSERT(value>=0);r->transfer_size=value;return;case 15:r->selection=(const ggml_tensor*)(uintptr_t)value;return;
        case 20:{GGML_ASSERT(value>=0&&(i==0||i==1));auto&st=i==0?s.stats_small:s.stats_large;if(j==0)st.hits=(size_t)value;else if(j==1)st.misses=(size_t)value;else {GGML_ASSERT(j==2);st.bytes=(size_t)value;}return;}
        case 21:GGML_ASSERT(value>=0);s.stats_copy.hits=(size_t)value;return;case 22:GGML_ASSERT(value>=0);s.stats_copy.bytes=(size_t)value;return;
        case 23:GGML_ASSERT(value>=0);(*r->memory)[(ggml_backend_buffer_type_t)(uintptr_t)i]=(size_t)value;return;
'''
assert s.count(marker)==1;s=s.replace(marker,extra+marker)
marker='        default:GGML_ABORT("invalid cache lifecycle service");'
extra=r'''
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
'''
assert s.count(marker)==1;s=s.replace(marker,extra+marker)
def hook(signature,body):
 global s
 assert s.count(signature)==1,(signature,s.count(signature));s=s.replace(signature,signature+'\n'+body,1)
hook('    ggml_backend_t backend(int32_t il) const {', '''        if(fg151_owned){auto q=const_cast<impl*>(this)->fg151_prepare();q.layer=il;return(ggml_backend_t)(uintptr_t)fg151_lifecycle(5,(int64_t)(uintptr_t)&q);}
''')
hook('    int64_t copy_experts(ggml_backend_t backend, const ggml_tensor * w, ggml_tensor * dst, int64_t e, int64_t last) {','''        if(fg151_owned){auto q=fg151_prepare();q.backend=backend;q.weight=w;q.dst=dst;q.expert=e;q.last=last;return fg151_lifecycle(3,(int64_t)(uintptr_t)&q);}
''')
hook('    bool copy(ggml_backend_t backend, const ggml_tensor * src, ggml_tensor * dst, ggml_cgraph * graph) {','''        if(fg151_owned){auto q=fg151_prepare();q.backend=backend;q.src=src;q.dst=dst;q.graph=graph;return fg151_lifecycle(4,(int64_t)(uintptr_t)&q)!=0;}
''')
needle='''    std::map<ggml_backend_buffer_type_t, size_t> res;
    for (const auto & d : pimpl->devices) {''';assert s.count(needle)==1
s=s.replace(needle,'''    std::map<ggml_backend_buffer_type_t, size_t> res;
    if(pimpl->fg151_owned){auto q=pimpl->fg151_prepare();q.memory=&res;fg151_lifecycle(6,(int64_t)(uintptr_t)&q);return res;}
    for (const auto & d : pimpl->devices) {''')
p.write_text(s);start=s.index('    bool fg151_owned=false;');end=s.index('    impl(const llama_model & model,',start);(D/'lifecycle-adapters.inc').write_text(s[start:end])
print('Expert download/synchronize/SLRU/upload/copy and memory-accounting orchestration moved into the same Goose controller')
