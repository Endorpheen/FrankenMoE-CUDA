extern "C" int64_t fg150_cache_get(int64_t context,int64_t field,int64_t index){auto&r=*(fg150_cache_request*)(uintptr_t)context;GGML_ASSERT(index>=0);
 if(field>=40&&field<48)return r.extra[field-40];
 if(field==34){GGML_ASSERT(r.n_ids<=INT64_MAX);return r.n_ids;}
 if(field==18){GGML_ASSERT((size_t)index<(r.extra[7]==1?r.n_ids*2:r.n_ids));return r.ids[index];}
 auto&c=*r.cache;switch(field){
 case 0:return c.n_expert;case 1:return c.n_slots;case 2:return c.slot_map.size();case 3:return c.n_protected;case 4:return c.seen_gen;case 5:return c.uniq.size();case 6:return*r.hits;
 case 10:return c.key_of.at(index);case 11:return c.prev.at(index);case 12:return c.next.at(index);case 13:return c.is_protected.at(index);case 14:return c.seen.at(index);case 15:return c.uniq.at(index);case 16:GGML_ASSERT(index<2);return c.head[index];case 17:GGML_ASSERT(index<2);return c.tail[index];
 case 18:GGML_ASSERT((size_t)index<r.n_ids);return r.ids[index];case 19:GGML_ASSERT(c.n_expert>0&&index/c.n_expert<(int64_t)c.slot_map.size());GGML_ASSERT(c.slot_map[index/c.n_expert]);return c.slot_map[index/c.n_expert][index%c.n_expert];
 case 30:return r.layers;case 31:return r.experts;case 32:return r.slots;case 33:return r.layer;case 34:GGML_ASSERT(r.n_ids<=INT64_MAX);return r.n_ids;case 35:return r.slot;case 36:return r.protect;default:GGML_ABORT("invalid cache metadata");}}
extern "C" void fg150_cache_put(int64_t context,int64_t field,int64_t index,int64_t value){auto&r=*(fg150_cache_request*)(uintptr_t)context;auto&c=*r.cache;GGML_ASSERT(index>=0&&value>=INT32_MIN&&value<=UINT32_MAX);switch(field){
 case 0:c.n_expert=value;return;case 1:c.n_slots=value;return;case 3:c.n_protected=value;return;case 4:c.seen_gen=(uint32_t)value;return;case 6:*r.hits=value;return;
 case 10:c.key_of.at(index)=value;return;case 11:c.prev.at(index)=value;return;case 12:c.next.at(index)=value;return;case 13:GGML_ASSERT(value==0||value==1);c.is_protected.at(index)=value;return;case 14:c.seen.at(index)=(uint32_t)value;return;case 15:c.uniq.at(index)=value;return;
 case 16:GGML_ASSERT(index<2);c.head[index]=value;return;case 17:GGML_ASSERT(index<2);c.tail[index]=value;return;
 case 19:GGML_ASSERT(c.n_expert>0&&index/c.n_expert<(int64_t)c.slot_map.size());GGML_ASSERT(c.slot_map[index/c.n_expert]);c.slot_map[index/c.n_expert][index%c.n_expert]=value;return;
 default:GGML_ABORT("invalid cache state write");}}
extern "C" void fg150_cache_action(int64_t context,int64_t op,int64_t a,int64_t b){auto&r=*(fg150_cache_request*)(uintptr_t)context;auto&c=*r.cache;switch(op){
 case 0:c.slot_map.assign(r.layers,nullptr);c.key_of.resize(r.slots);c.prev.resize(r.slots);c.next.resize(r.slots);c.is_protected.resize(r.slots);c.seen.resize(r.experts);c.uniq.clear();return;
 case 1:c.uniq.clear();return;case 2:GGML_ASSERT(a>=0&&a<c.n_expert);c.uniq.push_back((int32_t)a);return;
 case 3:((std::vector<moe_cache_slru::fill>*)r.fills)->clear();*r.hits=0;return;
 case 4:GGML_ASSERT(a>=0&&a<c.n_expert&&b>=0&&b<c.n_slots);((std::vector<moe_cache_slru::fill>*)r.fills)->push_back({(int32_t)a,(int32_t)b});return;
 default:GGML_ABORT("invalid cache storage action");}}
