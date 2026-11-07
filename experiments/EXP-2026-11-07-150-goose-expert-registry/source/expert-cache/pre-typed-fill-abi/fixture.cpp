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
static bool fg150_cache_enabled=true;
__attribute__((constructor))static void fg150_cache_config(){const char*v=getenv("GGML_GOOSE_EXPERT_CACHE");fg150_cache_enabled=!v||strcmp(v,"0");}
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


} // namespace
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

#include <cstdio>
#include <limits>
static uint32_t rng=0x6c2f7391;
static uint32_t random_word(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static size_t checks=0;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line=%d case=%zu\n",__LINE__,checks);abort();}}while(0)
static void same(const moe_cache_slru&a,const moe_cache_slru&b){
 CHECK(a.n_expert==b.n_expert&&a.n_slots==b.n_slots&&a.key_of==b.key_of&&a.prev==b.prev&&a.next==b.next&&a.n_protected==b.n_protected&&a.seen==b.seen&&a.seen_gen==b.seen_gen&&a.uniq==b.uniq&&a.is_protected==b.is_protected);
 for(int k=0;k<2;k++)CHECK(a.head[k]==b.head[k]&&a.tail[k]==b.tail[k]);
 for(size_t l=0;l<a.slot_map.size();l++)for(int e=0;e<a.n_expert;e++)CHECK(a.slot_map[l][e]==b.slot_map[l][e]);
 std::vector<bool> visited(a.n_slots,false);int protected_count=0;
 for(int k=0;k<2;k++){int32_t prior=-1;size_t n=0;for(int32_t s=a.head[k];s>=0;s=a.next[s]){CHECK(s<a.n_slots&&!visited[s]&&n++<(size_t)a.n_slots);visited[s]=true;CHECK(a.prev[s]==prior&&a.is_protected[s]==(k==1));prior=s;protected_count+=k;}CHECK(prior==a.tail[k]);}
 for(bool v:visited)CHECK(v);CHECK(protected_count==a.n_protected);
}
static void budget_cases(){
 for(int test=0;test<2000;test++){
  fg150_budget_request goose={};goose.bytes=random_word()%(8*1024*1024);goose.used=1+random_word()%8;
  int devices=1+random_word()%4;goose.devices.resize(devices);goose.tensors.resize(devices);
  for(int i=0;i<devices;i++)goose.devices[i]={0,(int64_t)(random_word()%(16*1024*1024)),int64_t(32)<<(random_word()%4),i,0};
  for(int i=0;i<1+(test%6);i++){
   int device=random_word()%devices,layers=1+random_word()%8,experts=1+random_word()%32,banks=1+random_word()%4;int64_t host=0,first=goose.strides.size();
   for(int j=0;j<banks;j++){int64_t stride=64+(random_word()%1024);goose.strides.push_back(stride);host+=stride*experts*layers;}
   goose.groups.push_back({device,host,layers,experts,first,banks,0});goose.devices[device].host+=host;
  }
  if(test%3)for(int i=0;i<devices;i++)goose.split.push_back(test%3==1?0.f:float(random_word()%100));
  if(test%7==0)for(auto&d:goose.devices)d.free_bytes=0;
  auto native=goose;bool by_free=native.split.empty()||std::all_of(native.split.begin(),native.split.end(),[](float x){return x==0.f;});double sum=0;
  for(auto&d:native.devices){if(d.host==0)continue;d.split=by_free?double(d.free_bytes):double(native.split[d.split_index]);sum+=d.split;}
  if(sum==0)for(auto&d:native.devices){d.split=d.host>0?1.:0.;sum+=d.split;}
  for(auto&g:native.groups){auto&d=native.devices[g.device];size_t budget=(size_t)(double(native.bytes)*d.split/sum*g.host/d.host);
   auto bytes=[&](int slots){size_t n=0;for(int b=0;b<g.banks;b++){size_t raw=native.strides[g.first+b]*(slots+1);n+=(raw+d.alignment-1)/d.alignment*d.alignment;}return n;};
   while(g.slots<g.layers*g.experts&&bytes(g.slots+1)<=budget)g.slots++;
   if(g.slots<native.used){g.slots=0;continue;}native.tensors[g.device]+=g.banks*(1+g.layers);native.host_tensors+=g.layers;
  }
  CHECK(fg150_cache(8,(int64_t)(uintptr_t)&goose)==(native.host_tensors>0));
  CHECK(goose.tensors==native.tensors&&goose.host_tensors==native.host_tensors);
  for(int i=0;i<devices;i++)CHECK(goose.devices[i].split==native.devices[i].split);
  for(size_t i=0;i<goose.groups.size();i++)CHECK(goose.groups[i].slots==native.groups[i].slots);
  checks++;
 }
}

int main(){
 for(int layers:{1,3,8})for(int experts:{1,4,17,64})for(int slots:{1,2,7,17,32}){
  moe_cache_slru native,goose;fg150_cache_enabled=false;native.init(layers,experts,slots);fg150_cache_enabled=true;goose.init(layers,experts,slots);
  std::vector<std::vector<int32_t>> nmap(layers,std::vector<int32_t>(experts,-1)),gmap=nmap;
  for(int l=0;l<layers;l++){native.slot_map[l]=nmap[l].data();goose.slot_map[l]=gmap[l].data();}same(native,goose);
  for(int step=0;step<120;step++){
   int l=random_word()%layers;size_t n=random_word()%(experts*2+1);std::vector<int32_t> ids(n);for(auto&x:ids)x=random_word()%experts;
   if(step==45||step==90){native.seen_gen=goose.seen_gen=UINT32_MAX;}
   std::vector<moe_cache_slru::fill> nf,gf;size_t nh=99,gh=99;
   fg150_cache_enabled=false;bool nr=native.plan(l,ids.data(),ids.size(),nf,nh);
   fg150_cache_enabled=true;bool gr=goose.plan(l,ids.data(),ids.size(),gf,gh);
   CHECK(nr==gr&&nh==gh&&nf.size()==gf.size());for(size_t i=0;i<nf.size();i++)CHECK(nf[i].expert==gf[i].expert&&nf[i].slot==gf[i].slot);same(native,goose);checks++;
  }
 }
 // Eligibility boundaries and grouped copies/uploads use their independent original formulas.
 for(int64_t tokens:{-1,0,1,4,32,33})for(int64_t used:{0,1,8,32})for(int64_t slots:{1,4,32,64}){
  fg150_cache_request q={};q.extra[0]=0;q.extra[1]=1;q.extra[2]=0;q.extra[3]=tokens;q.extra[4]=used;q.extra[5]=slots;q.extra[6]=64;q.extra[7]=32;
  bool expected=tokens>0&&tokens<=32&&std::min(tokens*used,int64_t(64))<=slots;CHECK(bool(fg150_cache_call(5,q))==expected);checks++;
 }
 for(int t=0;t<1000;t++){
  std::vector<int32_t> slots(64);for(auto&x:slots)x=(int32_t)(random_word()%33)-1;
  int e=random_word()%64,last=e+random_word()%(64-e);int64_t n=0;if(slots[e]>=0){n=1;while(e+n<=last&&slots[e+n]==slots[e]+n)n++;}
  fg150_cache_request q={};q.ids=slots.data();q.n_ids=slots.size();q.extra[0]=e;q.extra[1]=last;CHECK(fg150_cache_call(6,q)==n);checks++;
  std::vector<moe_cache_slru::fill> fills;for(int i=0;i<20;i++)fills.push_back({int(random_word()%64),int(random_word()%64)});
  for(int i=0;i<20;i++){size_t count=1;while(i+count<fills.size()&&fills[i+count].expert==fills[i].expert+(int)count&&fills[i+count].slot==fills[i].slot+(int)count)count++;
   q={};q.ids=(const int32_t*)fills.data();q.n_ids=fills.size();q.extra[0]=i;q.extra[7]=1;CHECK(fg150_cache_call(7,q)==(int64_t)count);checks++;}
 }
 budget_cases();
 printf("{\"status\":\"PASS_EXPERT_CACHE_NATIVE_DIFFERENTIAL\",\"cases\":%zu}\n",checks);
}
