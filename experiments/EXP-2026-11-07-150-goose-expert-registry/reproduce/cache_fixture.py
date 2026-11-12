from pathlib import Path
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');d=A/'source/expert-cache';s=(d/'llama-moe-cache.cpp').read_text();prefix=s[:s.index('// gate, up, down or gate_up, down')]
fixture=r'''
} // namespace
'''+(d/'services.cpp').read_text()+r'''
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
   q={};q.fills=&fills;q.n_ids=fills.size();q.extra[0]=i;q.extra[7]=1;CHECK(fg150_cache_call(7,q)==(int64_t)count);checks++;}
 }
 printf("{\"status\":\"PASS_EXPERT_CACHE_NATIVE_DIFFERENTIAL\",\"cases\":%zu}\n",checks);
}
'''
fixture=fixture.replace('int main(){', (d/'budget-fixture.inc').read_text()+'\nint main(){').replace(' printf(', ' budget_cases();\n printf(')
(d/'fixture.cpp').write_text(prefix+fixture);print('cache fixture created')
