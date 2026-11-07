#include "llama-moe-cache.h"

#include "llama-impl.h"
#include "llama-model.h"

#include "ggml-cpp.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <cstdlib>
#include <cstring>
#include "bridge.h"
#include "budget.h"
#include "lifecycle-bridge.h"
static bool fg150_cache_enabled=true;
__attribute__((constructor))static void fg150_cache_config(){const char*v=getenv("GGML_GOOSE_EXPERT_CACHE");fg150_cache_enabled=!v||strcmp(v,"0");}
static bool fg151_enabled=true;
__attribute__((constructor))static void fg151_config(){const char*v=getenv("GGML_GOOSE_EXPERT_LIFECYCLE");fg151_enabled=!v||strcmp(v,"0");}
namespace {
struct moe_cache_slru;
struct fg150_cache_request {
 moe_cache_slru*cache;int32_t layers=0,experts=0,slots=0,layer=0,slot=0;bool protect=false;
 const int32_t*ids=nullptr;size_t n_ids=0;void*fills=nullptr;size_t*hits=nullptr;int64_t extra[8]={};
};
static int64_t fg150_cache_call(int64_t mode,fg150_cache_request&q){return fg150_cache(mode,(int64_t)(uintptr_t)&q);}


// SLRU of the experts of a group of layers, the slot of each expert is kept in the slot map of its layer
struct moe_cache_slru {
    int32_t n_expert = 0;
    int32_t n_slots  = 0;

    std::vector<int32_t *> slot_map; // [n_layer] data of the slot maps, -1 if the expert is not cached
    std::vector<int32_t>   key_of;   // [n_slots] il*n_expert + expert, -1 if empty

    // probation and protected LRU lists; hits promote slots to the protected half
    std::vector<int32_t> prev;
    std::vector<int32_t> next;
    int32_t head[2] = { -1, -1 };
    int32_t tail[2] = { -1, -1 };
    std::vector<bool> is_protected;
    int32_t n_protected = 0;

    std::vector<uint32_t> seen; // [n_expert]
    uint32_t seen_gen = 0;
    std::vector<int32_t> uniq;

    void init(int32_t n_layer, int32_t n_expert, int32_t n_slots) {
        if(fg150_cache_enabled){fg150_cache_request q={};q.cache=this;q.layers=n_layer;q.experts=n_expert;q.slots=n_slots;fg150_cache_call(0,q);return;}

        this->n_expert = n_expert;
        this->n_slots  = n_slots;
        slot_map.assign(n_layer, nullptr);
        key_of.assign(n_slots, -1);
        prev.resize(n_slots);
        next.resize(n_slots);
        for (int32_t s = 0; s < n_slots; ++s) {
            prev[s] = s - 1;
            next[s] = s + 1 < n_slots ? s + 1 : -1;
        }
        head[0] = 0;
        tail[0] = n_slots - 1;
        is_protected.assign(n_slots, false);
        seen.assign(n_expert, 0);
    }

    // move slot s to the most recently used end of a segment
    void move(int32_t s, bool protect) {
        if(fg150_cache_enabled){fg150_cache_request q={};q.cache=this;q.slot=s;q.protect=protect;fg150_cache_call(1,q);return;}

        const bool from = is_protected[s];
        if (from == protect && s == tail[from]) {
            return;
        }
        if (prev[s] >= 0) {
            next[prev[s]] = next[s];
        } else {
            head[from] = next[s];
        }
        if (next[s] >= 0) {
            prev[next[s]] = prev[s];
        } else {
            tail[from] = prev[s];
        }

        prev[s] = tail[protect];
        next[s] = -1;
        if (tail[protect] >= 0) {
            next[tail[protect]] = s;
        } else {
            head[protect] = s;
        }
        tail[protect] = s;
        is_protected[s] = protect;
        n_protected += int(protect) - int(from);
    }

    void touch(int32_t s) {
        if(fg150_cache_enabled){fg150_cache_request q={};q.cache=this;q.slot=s;fg150_cache_call(2,q);return;}

        move(s, true);
        if (n_protected > n_slots/2) {
            move(head[1], false);
        }
    }

    int32_t victim(int32_t il) const {
        if(fg150_cache_enabled){fg150_cache_request q={};q.cache=const_cast<moe_cache_slru*>(this);q.layer=il;return(int32_t)fg150_cache_call(3,q);}

        for (int segment = 0; segment < 2; ++segment) {
            for (int32_t s = head[segment]; s >= 0; s = next[s]) {
                const int32_t key = key_of[s];
                // keep every expert selected by this request, including earlier fills
                if (key < 0 || key / n_expert != il || seen[key % n_expert] != seen_gen) {
                    return s;
                }
            }
        }
        GGML_ABORT("no SLRU victim");
    }

    struct fill {
        int32_t expert;
        int32_t slot;
    };

    // give a slot to each expert selected by ids in layer il, evicting from probation first
    // returns false if the ids select more distinct experts than there are slots
    bool plan(int32_t il, const int32_t * ids, size_t n_ids, std::vector<fill> & fills, size_t & n_hit) {
        if(fg150_cache_enabled){fg150_cache_request q={};q.cache=this;q.layer=il;q.ids=ids;q.n_ids=n_ids;q.fills=&fills;q.hits=&n_hit;return fg150_cache_call(4,q)!=0;}

        fills.clear();
        n_hit = 0;

        if (++seen_gen == 0) {
            std::fill(seen.begin(), seen.end(), 0);
            seen_gen = 1;
        }
        uniq.clear();
        for (size_t i = 0; i < n_ids; ++i) {
            GGML_ASSERT(ids[i] >= 0 && ids[i] < n_expert);
            if (seen[ids[i]] != seen_gen) {
                seen[ids[i]] = seen_gen;
                uniq.push_back(ids[i]);
            }
        }
        if (uniq.size() > (size_t) n_slots) {
            return false;
        }

        int32_t * slots = slot_map[il];

        // promote hits before choosing victims for misses
        for (int32_t e : uniq) {
            if (slots[e] >= 0) {
                touch(slots[e]);
                n_hit++;
            }
        }
        // sorted misses usually get consecutive slots, so the uploads can be merged
        std::sort(uniq.begin(), uniq.end());
        for (int32_t e : uniq) {
            if (slots[e] >= 0) {
                continue;
            }
            const int32_t s = victim(il);
            if (key_of[s] >= 0) {
                slot_map[key_of[s] / n_expert][key_of[s] % n_expert] = -1;
            }
            key_of[s] = il*n_expert + e;
            slots[e] = s;
            move(s, false);
            fills.push_back({ e, s });
        }
        return true;
    }
};

// gate, up, down or gate_up, down
static std::vector<ggml_tensor *> llama_moe_cache_layer_experts(const llama_layer & layer) {
    std::vector<ggml_tensor *> res;
    for (ggml_tensor * t : { layer.ffn_gate_up_exps, layer.ffn_gate_exps, layer.ffn_up_exps, layer.ffn_down_exps }) {
        if (t != nullptr) {
            res.push_back(t);
        }
    }
    return res;
}

static bool llama_moe_cache_same_layout(const std::vector<ggml_tensor *> & a, const std::vector<ggml_tensor *> & b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i]->type != b[i]->type || !ggml_are_same_shape(a[i], b[i]) || a[i]->nb[2] != b[i]->nb[2]) {
            return false;
        }
    }
    return true;
}

static bool llama_moe_cache_is_host_weight(const ggml_tensor * t) {
    return t->buffer != nullptr &&
        ggml_backend_buffer_get_usage(t->buffer) == GGML_BACKEND_BUFFER_USAGE_WEIGHTS &&
        ggml_backend_buffer_is_host(t->buffer);
}

}

struct llama_moe_cache::impl {
    // a GPU with its own budget and banks, it caches the layers assigned to it
    struct device {
        ggml_backend_t backend;
        ggml_backend_buffer_type_t buft;
        size_t host_bytes = 0; // host experts of the layers it caches
        double split = 0.0;    // share of the budget

        // banks and their views
        ggml_context_ptr ctx;
        ggml_backend_buffer_ptr buf;
        size_t buf_size = 0;
    };

    // layers of the same device with the same expert tensor layout share the banks and the LRU of a group
    struct group {
        int32_t id;                       // device
        std::vector<ggml_tensor *> ref;   // expert tensors of the first layer
        std::vector<int32_t> layers;
        std::vector<ggml_tensor *> banks; // device storage of all slots, one per expert tensor
        size_t host_bytes = 0;
        int32_t n_slots = 0;
        moe_cache_slru slru;
    };

    struct layer {
        int32_t ig = -1;                    // -1 if the layer is not cached
        ggml_tensor * slot_map = nullptr;   // I32 [1, n_expert] in host memory
        std::vector<ggml_tensor *> experts; // host expert tensors, in the order of the banks
    };

    struct binding {
        int32_t il;
        int32_t ip;           // index of the bank
        ggml_tensor * cached; // view of the bank used in place of the host experts
    };

    struct stats {
        size_t hits   = 0;
        size_t misses = 0;
        size_t bytes  = 0;
    };

    static constexpr int64_t max_batch = 32;

    int32_t n_expert_used;

    stats stats_small; // up to 8 tokens per ubatch
    stats stats_large;
    stats stats_copy;  // experts copied from the cache for large batches

    std::vector<device> devices;
    std::vector<group> groups;
    std::vector<layer> layers;
    std::unordered_map<const ggml_tensor *, binding> bindings; // host experts -> cached experts
    std::unordered_map<const ggml_tensor *, int32_t> layer_of; // slot map -> layer

    std::vector<int32_t> ids;
    std::vector<moe_cache_slru::fill> fills;

    // slot maps in host memory
    ggml_context_ptr ctx_host;
    ggml_backend_buffer_ptr buf_host;
    size_t buf_host_size = 0;

    // views used by copy_experts
    ggml_context_ptr ctx_views;


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

    impl(const llama_model & model, const std::vector<ggml_backend_t> & backends, const std::vector<ggml_backend_buffer_type_t> & bufts, size_t size) :
            n_expert_used(model.hparams.n_expert_used_max()), layers(model.layers.size()) {
        if(fg151_enabled){
            fg151_owned=true;auto q=fg151_prepare();q.model=&model;q.backends=&backends;q.bufts=&bufts;q.bytes=size;
            try{fg151_lifecycle(0,(int64_t)(uintptr_t)&q);}catch(...){fg151_lifecycle(2,(int64_t)(uintptr_t)&q);throw;}
            return;
        }

        for (size_t i = 0; i < backends.size(); ++i) {
            const auto dev_type = ggml_backend_dev_type(ggml_backend_get_device(backends[i]));
            if (dev_type == GGML_BACKEND_DEVICE_TYPE_GPU || dev_type == GGML_BACKEND_DEVICE_TYPE_IGPU) {
                auto & d = devices.emplace_back();
                d.backend = backends[i];
                d.buft    = bufts[i];
            }
        }
        if (devices.empty()) {
            throw std::runtime_error("MoE cache requires a GPU backend");
        }
        if (model.split_mode() == LLAMA_SPLIT_MODE_TENSOR) {
            throw std::runtime_error("MoE cache does not support tensor parallelism");
        }
        if (model.hparams.n_expert == 0 || n_expert_used == 0) {
            throw std::runtime_error("MoE cache requires a MoE model");
        }

        // only cache layers that keep all of their experts in host memory, on the device the layer is assigned to
        for (size_t il = 0; il < model.layers.size(); ++il) {
            auto experts = llama_moe_cache_layer_experts(model.layers[il]);
            if (experts.empty() || !std::all_of(experts.begin(), experts.end(), llama_moe_cache_is_host_weight)) {
                continue;
            }
            const auto it_dev = std::find_if(devices.begin(), devices.end(), [&](const device & d) { return ggml_backend_get_device(d.backend) == model.dev_layer(il); });
            if (it_dev == devices.end()) {
                continue;
            }
            const int32_t id = (int32_t) (it_dev - devices.begin());
            auto it = std::find_if(groups.begin(), groups.end(), [&](const group & g) { return g.id == id && llama_moe_cache_same_layout(g.ref, experts); });
            if (it == groups.end()) {
                groups.emplace_back();
                it = groups.end() - 1;
                it->id  = id;
                it->ref = experts;
            }
            it->layers.push_back(il);
            for (const ggml_tensor * t : experts) {
                it->host_bytes         += ggml_nbytes(t);
                devices[id].host_bytes += ggml_nbytes(t);
            }
        }
        if (groups.empty()) {
            LLAMA_LOG_WARN("%s: no layer has all of its experts in host memory, MoE cache is disabled\n", __func__);
            return;
        }

        // one extra slot at the end, CUDA MMQ can read past the last expert
        auto alloc_size = [&](const group & g, int32_t n_slots) {
            const size_t alignment = ggml_backend_buft_get_alignment(devices[g.id].buft);
            size_t res = 0;
            for (const ggml_tensor * t : g.ref) {
                res += GGML_PAD(t->nb[2]*(n_slots + 1), alignment);
            }
            return res;
        };

        // the budget is split among the devices with host experts like the layers, by the tensor split or by default by free memory
        std::vector<size_t> n_tensors(devices.size(),0);
        size_t n_tensors_host=0;
        if(fg150_cache_enabled){
            fg150_budget_request q={};GGML_ASSERT(size<=INT64_MAX);q.bytes=(int64_t)size;q.used=n_expert_used;q.tensors.resize(devices.size());
            const float*split=model.tensor_split();if(split)q.split.assign(split,split+model.n_devices());
            for(const auto&d:devices){size_t free=0,total=0;ggml_backend_dev_t dev=ggml_backend_get_device(d.backend);ggml_backend_dev_memory(dev,&free,&total);int64_t index=-1;
                for(size_t i=0;i<model.devices.size();i++)if(model.devices[i].dev==dev){index=i;break;}
                GGML_ASSERT(d.host_bytes<=INT64_MAX&&free<=INT64_MAX);q.devices.push_back({(int64_t)d.host_bytes,(int64_t)free,(int64_t)ggml_backend_buft_get_alignment(d.buft),index,0});}
            for(const auto&g:groups){GGML_ASSERT(g.host_bytes<=INT64_MAX);fg150_budget_group group={g.id,(int64_t)g.host_bytes,(int64_t)g.layers.size(),g.ref[0]->ne[2],(int64_t)q.strides.size(),(int64_t)g.ref.size(),0};q.groups.push_back(group);
                for(const auto*t:g.ref){GGML_ASSERT(t->nb[2]<=INT64_MAX);q.strides.push_back((int64_t)t->nb[2]);}}
            fg150_cache(8,(int64_t)(uintptr_t)&q);
            for(size_t i=0;i<devices.size();i++){devices[i].split=q.devices[i].split;n_tensors[i]=(size_t)q.tensors[i];}
            n_tensors_host=(size_t)q.host_tensors;
            for(size_t i=0;i<groups.size();i++){auto&g=groups[i];g.n_slots=(int32_t)q.groups[i].slots;
                if(g.n_slots==0){LLAMA_LOG_WARN("%s: MoE cache budget is too small for %zu layers, they are not cached\n",__func__,g.layers.size());continue;}
                g.slru.init(model.layers.size(),g.ref[0]->ne[2],g.n_slots);}
        }else {
        const float * tensor_split = model.tensor_split();
        const bool split_by_free = tensor_split == nullptr ||
            std::all_of(tensor_split, tensor_split + model.n_devices(), [](float x) { return x == 0.0f; });
        double split_sum = 0.0;
        for (device & d : devices) {
            if (d.host_bytes == 0) {
                continue;
            }
            ggml_backend_dev_t dev = ggml_backend_get_device(d.backend);
            if (split_by_free) {
                size_t free;
                size_t total;
                ggml_backend_dev_memory(dev, &free, &total);
                d.split = (double) free;
            } else {
                const auto it = std::find_if(model.devices.begin(), model.devices.end(), [&](const llama_device & ld) { return ld.dev == dev; });
                GGML_ASSERT(it != model.devices.end());
                d.split = (double) tensor_split[it - model.devices.begin()];
            }
            split_sum += d.split;
        }
        if (split_sum == 0.0) {
            // the devices do not report their free memory
            for (device & d : devices) {
                d.split    = d.host_bytes > 0 ? 1.0 : 0.0;
                split_sum += d.split;
            }
        }

        // within a device the budget is split by the size of the experts, so each group caches the same fraction of its experts
        for (group & g : groups) {
            const device & d = devices[g.id];
            const int32_t n_expert  = g.ref[0]->ne[2];
            const size_t  budget    = (size_t) ((double) size*d.split/split_sum*g.host_bytes/d.host_bytes);
            const int32_t max_slots = g.layers.size()*n_expert;
            while (g.n_slots < max_slots && alloc_size(g, g.n_slots + 1) <= budget) {
                g.n_slots++;
            }
            if (g.n_slots < n_expert_used) {
                LLAMA_LOG_WARN("%s: MoE cache budget is too small for %zu layers, they are not cached\n", __func__, g.layers.size());
                g.n_slots = 0;
                continue;
            }
            g.slru.init(model.layers.size(), n_expert, g.n_slots);
            n_tensors[g.id] += g.ref.size()*(1 + g.layers.size());
            n_tensors_host  += g.layers.size();
        }
        }
        if (n_tensors_host == 0) {
            throw std::runtime_error("MoE cache is too small to hold the experts of one token");
        }

        auto init_ctx = [](size_t n_tensors) {
            ggml_init_params params = {
                /*.mem_size   =*/ n_tensors*ggml_tensor_overhead(),
                /*.mem_buffer =*/ nullptr,
                /*.no_alloc   =*/ true,
            };
            ggml_context_ptr res(ggml_init(params));
            if (!res) {
                throw std::runtime_error("failed to create the MoE cache context");
            }
            return res;
        };
        for (size_t id = 0; id < devices.size(); ++id) {
            if (n_tensors[id] > 0) {
                devices[id].ctx = init_ctx(n_tensors[id]);
            }
        }
        ctx_host  = init_ctx(n_tensors_host);
        ctx_views = init_ctx(2);

        ggml_backend_buffer_type_t buft_host = ggml_backend_cpu_buffer_type();
        const size_t alignment_host = ggml_backend_buft_get_alignment(buft_host);

        for (size_t ig = 0; ig < groups.size(); ++ig) {
            group & g = groups[ig];
            if (g.n_slots == 0) {
                continue;
            }
            ggml_context * ctx = devices[g.id].ctx.get();
            for (const ggml_tensor * t : g.ref) {
                ggml_tensor * bank = ggml_new_tensor_3d(ctx, t->type, t->ne[0], t->ne[1], g.n_slots + 1);
                GGML_ASSERT(bank->nb[2] == t->nb[2]);
                ggml_format_name(bank, "moe_cache.%zu.%s", ig, t->name);
                g.banks.push_back(bank);
            }
            for (int32_t il : g.layers) {
                layer & l = layers[il];
                l.ig      = (int32_t) ig;
                l.experts = llama_moe_cache_layer_experts(model.layers[il]);
                for (size_t ip = 0; ip < l.experts.size(); ++ip) {
                    ggml_tensor * bank   = g.banks[ip];
                    ggml_tensor * cached = ggml_view_3d(ctx, bank, bank->ne[0], bank->ne[1], g.n_slots, bank->nb[1], bank->nb[2], 0);
                    ggml_format_name(cached, "moe_cache.%s", l.experts[ip]->name);
                    bindings[l.experts[ip]] = { il, (int32_t) ip, cached };
                }
                l.slot_map = ggml_new_tensor_2d(ctx_host.get(), GGML_TYPE_I32, 1, g.ref[0]->ne[2]);
                ggml_format_name(l.slot_map, "moe_cache.slot_map-%d", il);
                layer_of[l.slot_map] = il;
                buf_host_size += GGML_PAD(ggml_nbytes(l.slot_map), alignment_host);
            }
            devices[g.id].buf_size += alloc_size(g, g.n_slots);
        }

        if (model.hparams.no_alloc) {
            // only used to measure the memory use, see llama_context::memory_breakdown
            for (device & d : devices) {
                if (!d.ctx) {
                    continue;
                }
                d.buf.reset(ggml_backend_buft_alloc_buffer(d.buft, 0));
                for (ggml_tensor * t = ggml_get_first_tensor(d.ctx.get()); t != nullptr; t = ggml_get_next_tensor(d.ctx.get(), t)) {
                    t->buffer = d.buf.get();
                }
            }
            buf_host.reset(ggml_backend_buft_alloc_buffer(buft_host, 0));
            for (ggml_tensor * t = ggml_get_first_tensor(ctx_host.get()); t != nullptr; t = ggml_get_next_tensor(ctx_host.get(), t)) {
                t->buffer = buf_host.get();
            }
        } else {
            for (device & d : devices) {
                if (!d.ctx) {
                    continue;
                }
                d.buf.reset(ggml_backend_alloc_ctx_tensors_from_buft(d.ctx.get(), d.buft));
                if (!d.buf) {
                    throw std::runtime_error("failed to allocate the MoE cache buffers");
                }
                ggml_backend_buffer_clear(d.buf.get(), 0);
                d.buf_size = ggml_backend_buffer_get_size(d.buf.get());
            }
            buf_host.reset(ggml_backend_alloc_ctx_tensors_from_buft(ctx_host.get(), buft_host));
            if (!buf_host) {
                throw std::runtime_error("failed to allocate the MoE cache buffers");
            }
            ggml_backend_buffer_clear(buf_host.get(), 0xff); // all slots are -1
            buf_host_size = ggml_backend_buffer_get_size(buf_host.get());

            for (group & g : groups) {
                for (int32_t il : g.layers) {
                    if (layers[il].slot_map != nullptr) {
                        g.slru.slot_map[il] = (int32_t *) layers[il].slot_map->data;
                    }
                }
            }
        }

        // as weights, the ops that read the banks run on the device and the slot maps are copied with the copy callback
        for (device & d : devices) {
            if (d.buf) {
                ggml_backend_buffer_set_usage(d.buf.get(), GGML_BACKEND_BUFFER_USAGE_WEIGHTS);
            }
        }
        ggml_backend_buffer_set_usage(buf_host.get(), GGML_BACKEND_BUFFER_USAGE_WEIGHTS);

        for (size_t id = 0; id < devices.size(); ++id) {
            const device & d = devices[id];
            if (d.host_bytes == 0) {
                continue;
            }
            LLAMA_LOG_INFO("%s: %10s MoE cache size = %8.2f MiB for %.2f MiB of host experts\n", __func__,
                ggml_backend_buft_name(d.buft), d.buf_size/1024.0/1024.0, d.host_bytes/1024.0/1024.0);
            for (const group & g : groups) {
                if (g.id == (int32_t) id) {
                    LLAMA_LOG_INFO("%s: %2zu layers, %s: %5d slots (%.1f%%)\n", __func__,
                        g.layers.size(), ggml_type_name(g.ref.back()->type), g.n_slots, 100.0*g.n_slots/(g.layers.size()*g.ref[0]->ne[2]));
                }
            }
        }
    }

    ggml_backend_t backend(int32_t il) const {
        if(fg151_owned){auto q=const_cast<impl*>(this)->fg151_prepare();q.layer=il;return(ggml_backend_t)(uintptr_t)fg151_lifecycle(5,(int64_t)(uintptr_t)&q);}

        return devices[groups[layers[il].ig].id].backend;
    }

    ~impl() {
        if(fg151_owned){auto q=fg151_prepare();fg151_lifecycle(1,(int64_t)(uintptr_t)&q);return;}
        log_stats();
    }

    ggml_tensor * get_slot_map(int32_t il, int64_t n_tokens, int64_t n_expert_used) const {
        if(fg150_cache_enabled){
            fg150_cache_request q={};q.extra[0]=il;q.extra[1]=layers.size();q.extra[2]=(il>=0&&(size_t)il<layers.size())?layers[il].ig:-1;
            q.extra[3]=n_tokens;q.extra[4]=n_expert_used;q.extra[7]=max_batch;
            if(q.extra[2]>=0){q.extra[5]=groups[layers[il].ig].n_slots;q.extra[6]=layers[il].slot_map->ne[1];}
            return fg150_cache_call(5,q)?layers[il].slot_map:nullptr;
        }
        if (il < 0 || il >= (int32_t) layers.size() || layers[il].ig < 0) {
            return nullptr;
        }
        const layer & l = layers[il];

        // large batches use most experts of a layer, so they gain little from the cache and would evict the experts used in generation
        if (n_tokens == 0 || n_tokens > max_batch || std::min(n_tokens*n_expert_used, l.slot_map->ne[1]) > groups[l.ig].n_slots) {
            return nullptr;
        }
        return l.slot_map;
    }

    ggml_tensor * get_experts(const ggml_tensor * w) const {
        const auto it = bindings.find(w);
        return it != bindings.end() ? it->second.cached : nullptr;
    }

    int64_t copy_experts(ggml_backend_t backend, const ggml_tensor * w, ggml_tensor * dst, int64_t e, int64_t last) {
        if(fg151_owned){auto q=fg151_prepare();q.backend=backend;q.weight=w;q.dst=dst;q.expert=e;q.last=last;return fg151_lifecycle(3,(int64_t)(uintptr_t)&q);}

        const auto it = bindings.find(w);
        if (it == bindings.end()) {
            return 0;
        }
        const binding & b = it->second;
        const group & g = groups[layers[b.il].ig];
        if (backend != devices[g.id].backend) {
            return 0;
        }

        // large batches only read the cache, so the experts used in generation stay in it
        const int32_t * slots = g.slru.slot_map[b.il];
        if (slots == nullptr || slots[e] < 0) {
            return 0;
        }
        int64_t n = 1;
        if(fg150_cache_enabled){fg150_cache_request q={};q.ids=slots;q.n_ids=g.slru.n_expert;q.extra[0]=e;q.extra[1]=last;n=fg150_cache_call(6,q);GGML_ASSERT(n>0);}
        else {while(e+n<=last&&slots[e+n]==slots[e]+n){n++;}}

        ggml_tensor * bank = g.banks[b.ip];
        ggml_reset(ctx_views.get());
        ggml_tensor * src_view = ggml_view_3d(ctx_views.get(), bank, bank->ne[0], bank->ne[1], n, bank->nb[1], bank->nb[2], slots[e]*bank->nb[2]);
        ggml_tensor * dst_view = ggml_view_3d(ctx_views.get(), dst,  dst->ne[0],  dst->ne[1],  n, dst->nb[1],  dst->nb[2],  e*dst->nb[2]);
        ggml_backend_view_init(src_view);
        ggml_backend_view_init(dst_view);
        ggml_backend_tensor_copy_async(backend, backend, src_view, dst_view);

        stats_copy.hits  += n;
        stats_copy.bytes += ggml_nbytes(src_view);

        return n;
    }

    bool copy(ggml_backend_t backend, const ggml_tensor * src, ggml_tensor * dst, ggml_cgraph * graph) {
        if(fg151_owned){auto q=fg151_prepare();q.backend=backend;q.src=src;q.dst=dst;q.graph=graph;return fg151_lifecycle(4,(int64_t)(uintptr_t)&q)!=0;}

        const auto it = layer_of.find(src);
        if (it == layer_of.end()) {
            return false;
        }
        const int32_t il = it->second;
        const layer & l = layers[il];
        group & g = groups[l.ig];

        GGML_ASSERT(backend == devices[g.id].backend);

        // the get_rows that looks up the slots of the selected experts
        const int n_nodes = ggml_graph_n_nodes(graph);
        const ggml_tensor * lookup = nullptr;
        for (int i = 0; i < n_nodes && lookup == nullptr; ++i) {
            const ggml_tensor * node = ggml_graph_node(graph, i);
            if (node->op == GGML_OP_GET_ROWS && node->src[0] == dst) {
                lookup = node;
            }
        }
        GGML_ASSERT(lookup != nullptr);

        // the selected experts must be computed in an earlier split
        // the scheduler starts a new split at the lookup because it reads a host weight, but only if the split already has inputs
        const ggml_tensor * sel = lookup->src[1];
        for (int i = 0; i < n_nodes; ++i) {
            const ggml_tensor * node = ggml_graph_node(graph, i);
            if (node == sel || node == sel->view_src) {
                GGML_ABORT("the experts of layer %d are selected in the same split as their MoE cache lookup", il);
            }
        }
        GGML_ASSERT(ggml_is_contiguous(sel));

        ids.resize(ggml_nelements(sel));
        ggml_backend_tensor_get_async(backend, sel, ids.data(), 0, ggml_nbytes(sel));
        ggml_backend_synchronize(backend);

        size_t n_hit = 0;
        if (!g.slru.plan(il, ids.data(), ids.size(), fills, n_hit)) {
            GGML_ABORT("the MoE cache is too small for the experts selected in layer %d", il);
        }

        // upload the missing experts, consecutive experts going to consecutive slots are uploaded together
        size_t bytes = 0;
        for (size_t ip = 0; ip < l.experts.size(); ++ip) {
            const ggml_tensor * w    = l.experts[ip];
            ggml_tensor       * bank = g.banks[ip];
            const size_t expert_size = w->nb[2];
            for (size_t i = 0; i < fills.size();) {
                size_t n = 1;
                if(fg150_cache_enabled){
                    static_assert(sizeof(moe_cache_slru::fill)==2*sizeof(int32_t),"fill ABI");
                    fg150_cache_request q={};q.fills=&fills;q.n_ids=fills.size();q.extra[0]=i;q.extra[7]=1;n=(size_t)fg150_cache_call(7,q);
                }else {while(i+n<fills.size()&&fills[i+n].expert==fills[i].expert+(int32_t)n&&fills[i+n].slot==fills[i].slot+(int32_t)n){n++;}}
                ggml_backend_tensor_set_async(backend, bank, (const uint8_t *) w->data + fills[i].expert*expert_size, fills[i].slot*expert_size, n*expert_size);
                bytes += n*expert_size;
                i += n;
            }
        }

        stats & st = ids.size() <= (size_t) 8*n_expert_used ? stats_small : stats_large;
        st.hits   += n_hit;
        st.misses += fills.size();
        st.bytes  += bytes;

        // the next copy synchronizes the backend before it changes the slot map again
        ggml_backend_tensor_set_async(backend, dst, src->data, 0, ggml_nbytes(src));

        return true;
    }

    void log_stats() const {
        auto log = [](const char * name, const stats & st) {
            const size_t n = st.hits + st.misses;
            if (n == 0) {
                return;
            }
            LLAMA_LOG_INFO("llama_moe_cache: %s: hits = %zu, misses = %zu, hit rate = %.2f%%, uploaded = %.2f MiB\n",
                name, st.hits, st.misses, 100.0*st.hits/n, st.bytes/1024.0/1024.0);
        };
        log("ubatch <= 8", stats_small);
        log("ubatch  > 8", stats_large);
        if (stats_copy.hits > 0) {
            LLAMA_LOG_INFO("llama_moe_cache: large batches: %zu experts copied from the cache, %.2f MiB\n", stats_copy.hits, stats_copy.bytes/1024.0/1024.0);
        }
    }
};

llama_moe_cache::llama_moe_cache(const llama_model & model, const std::vector<ggml_backend_t> & backends, const std::vector<ggml_backend_buffer_type_t> & bufts, size_t size) :
    pimpl(new impl(model, backends, bufts, size)) {
}

llama_moe_cache::~llama_moe_cache() = default;

ggml_backend_t llama_moe_cache::backend(int32_t il) const {
    return pimpl->backend(il);
}

ggml_tensor * llama_moe_cache::get_slot_map(int32_t il, int64_t n_tokens, int64_t n_expert_used) const {
    return pimpl->get_slot_map(il, n_tokens, n_expert_used);
}

ggml_tensor * llama_moe_cache::get_experts(const ggml_tensor * w) const {
    return pimpl->get_experts(w);
}

bool llama_moe_cache::copy(ggml_backend_t backend, const ggml_tensor * src, ggml_tensor * dst, ggml_cgraph * graph) {
    return pimpl->copy(backend, src, dst, graph);
}

int64_t llama_moe_cache::copy_experts(ggml_backend_t backend, const ggml_tensor * w, ggml_tensor * dst, int64_t e, int64_t last) {
    return pimpl->copy_experts(backend, w, dst, e, last);
}

std::map<ggml_backend_buffer_type_t, size_t> llama_moe_cache::memory_breakdown() const {
    std::map<ggml_backend_buffer_type_t, size_t> res;
    if(pimpl->fg151_owned){auto q=pimpl->fg151_prepare();q.memory=&res;fg151_lifecycle(6,(int64_t)(uintptr_t)&q);return res;}
    for (const auto & d : pimpl->devices) {
        if (d.buf) {
            res[ggml_backend_buffer_get_type(d.buf.get())] += d.buf_size;
        }
    }
    if (pimpl->buf_host) {
        res[ggml_backend_buffer_get_type(pimpl->buf_host.get())] += pimpl->buf_host_size;
    }
    return res;
}

extern "C" int64_t fg150_cache_get(int64_t context,int64_t field,int64_t index){auto&r=*(fg150_cache_request*)(uintptr_t)context;GGML_ASSERT(index>=0);
 if(field>=40&&field<48)return r.extra[field-40];
 if(field==34){GGML_ASSERT(r.n_ids<=INT64_MAX);return r.n_ids;}
 if(field==18){
  if(r.extra[7]==1){auto&fills=*(const std::vector<moe_cache_slru::fill>*)r.fills;GGML_ASSERT((size_t)(index/2)<fills.size());const auto&fill=fills.at(index/2);return index%2?fill.slot:fill.expert;}
  GGML_ASSERT((size_t)index<r.n_ids);return r.ids[index];}
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

extern "C" int64_t fg151_meta(int64_t c,int64_t f,int64_t i,int64_t j){auto*q=(fg151_request*)(uintptr_t)c;GGML_ASSERT(q&&q->read);return q->read(q,f,i,j);}
extern "C" void fg151_put(int64_t c,int64_t f,int64_t i,int64_t j,int64_t v){auto*q=(fg151_request*)(uintptr_t)c;GGML_ASSERT(q&&q->write);q->write(q,f,i,j,v);}
extern "C" int64_t fg151_action(int64_t c,int64_t op,int64_t a,int64_t b,int64_t d){auto*q=(fg151_request*)(uintptr_t)c;GGML_ASSERT(q&&q->service);return q->service(q,op,a,b,d);}
