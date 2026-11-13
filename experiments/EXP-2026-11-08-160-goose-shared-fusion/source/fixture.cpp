#define _GNU_SOURCE 1
#include "ggml-impl.h"
#include "ggml-backend-impl.h"
#include <dlfcn.h>
#include <link.h>
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#define METHODS(X) X(ggml_init) X(ggml_free) X(ggml_get_first_tensor) X(ggml_get_next_tensor) X(ggml_used_mem) X(ggml_get_mem_buffer) X(ggml_new_tensor_4d) X(ggml_new_graph_custom) X(ggml_build_forward_expand) X(ggml_add) X(ggml_dup) X(ggml_view_tensor) X(ggml_set_name) X(ggml_rope) X(ggml_rope_set_offset) X(ggml_swiglu_oai) X(ggml_swiglu_clamp) X(ggml_argsort_top_k) X(ggml_pad_ext) X(ggml_pad_ext_circular) X(ggml_set_abort_callback)
template<class T>T sym(void*h,const char*n){auto p=dlsym(h,n);if(!p){std::fprintf(stderr,"Missing %s %s\n",n,dlerror());std::exit(2);}return reinterpret_cast<T>(p);}
struct API{void*h;uint64_t(*probe)(int,const uint64_t*);
#define FIELD(n) decltype(&::n) n;
METHODS(FIELD)
#undef FIELD
explicit API(const char*p){h=dlmopen(LM_ID_NEWLM,p,RTLD_NOW|RTLD_LOCAL);if(!h){std::fprintf(stderr,"Load %s\n",dlerror());std::exit(2);}probe=sym<decltype(probe)>(h,"fg160_probe");
#define LOAD(n) n=sym<decltype(n)>(h,#n);
METHODS(LOAD)
#undef LOAD
}uint64_t q(int mode,uint64_t a=0,uint64_t b=0,uint64_t c=0,uint64_t d=0,uint64_t e=0,uint64_t f=0,uint64_t g=0){uint64_t args[]={a,b,c,d,e,f,g};return probe(mode,args);}};
extern "C" void ggml_abort(const char*file,int line,const char*format,...){std::fprintf(stderr,"Fixture native invariant %s:%d %s\n",file,line,format);std::abort();}
static size_t checks,decisions;static const char*phase;static void req(bool p,const char*m){++checks;if(!p){std::fprintf(stderr,"FAIL %s: %s\n",phase,m);std::exit(3);}}template<class T>static uint64_t u(T*p){return (uintptr_t)p;}
static void decisions_suite(API&n,API&g){phase="fusion decisions";ggml_tensor nodes[6]{};ggml_tensor*ptrs[6];for(int i=0;i<6;++i){ptrs[i]=nodes+i;nodes[i].op=(ggml_op)(GGML_OP_ADD+i);for(int d=0;d<4;++d)nodes[i].ne[d]=4;nodes[i].flags=GGML_TENSOR_FLAG_COMPUTE;if(i)nodes[i].src[0]=nodes+i-1;}
ggml_hash_set hs{17,nullptr,nullptr};std::vector<ggml_bitset_t>bits(1);std::vector<ggml_tensor*>keys(17);std::vector<int32_t>uses(17);hs.used=bits.data();hs.keys=keys.data();for(int i=0;i<6;++i){auto pos=ggml_hash_insert(&hs,nodes+i);uses[pos]=i<5?1:0;}ggml_cgraph graph{};graph.size=6;graph.n_nodes=6;graph.nodes=ptrs;graph.use_counts=uses.data();graph.visited_hash_set=hs;std::array<ggml_op,6>ops;std::array<int,6>idxs;for(int i=0;i<6;++i){ops[i]=nodes[i].op;idxs[i]=i;}int outputs[]={5};std::array<std::array<int,3>,3>edges{{{{1,0,0}},{{2,0,1}},{{3,0,2}}}};
auto same=[&](int mode,uint64_t a,uint64_t b=0,uint64_t c=0,uint64_t d=0,uint64_t e=0,uint64_t f=0){auto x=n.q(mode,a,b,c,d,e,f),y=g.q(mode,a,b,c,d,e,f);++decisions;req(x==y,"native predicate parity");return y;};
for(int variant=0;variant<12;++variant){auto saved=nodes[2];auto olduse=uses[ggml_hash_find(&hs,nodes+2)];auto oldidx=idxs[2];auto oldop=ops[2];
switch(variant){case 1:nodes[2].flags|=GGML_TENSOR_FLAG_OUTPUT;break;case 2:nodes[2].flags=0;break;case 3:nodes[2].view_src=nodes+1;break;case 4:++uses[ggml_hash_find(&hs,nodes+2)];break;case 5:nodes[2].src[0]=nullptr;break;case 6:nodes[2].ne[1]=1;break;case 7:ops[2]=GGML_OP_NONE;break;case 8:idxs[2]=6;break;case 9:nodes[2].src[1]=nodes+1;nodes[2].src[0]=nullptr;break;case 10:nodes[2].flags|=GGML_TENSOR_FLAG_PARAM;break;case 11:nodes[2].src[9]=nodes;break;}
for(int i=0;i<6;++i){same(0,u(&graph),i);for(int k=0;k<3;++k)same(1,u(&graph),i,k);}for(int length=0;length<=6;++length){same(2,u(&graph),u(idxs.data()),u(ops.data()),length);same(3,u(&graph),0,u(ops.data()),length);if(variant!=8){same(4,u(&graph),u(idxs.data()),length,u(ops.data()),u(outputs),1);same(5,u(&graph),0,length,u(ops.data()),u(outputs),1);}}if(variant!=8)for(int count=0;count<=3;++count)same(6,u(&graph),0,u(edges.data()),count);
nodes[2]=saved;uses[ggml_hash_find(&hs,nodes+2)]=olduse;idxs[2]=oldidx;ops[2]=oldop;}
// Check views outside the subgraph:only immutable weight buffers qualify.
ggml_backend_buffer fake{};fake.usage=GGML_BACKEND_BUFFER_USAGE_WEIGHTS;ggml_tensor external{};external.buffer=&fake;auto*saved=nodes[2].view_src;nodes[2].view_src=&external;for(int flags:{0,(int)GGML_TENSOR_FLAG_PARAM})for(int usage:{(int)GGML_BACKEND_BUFFER_USAGE_ANY,(int)GGML_BACKEND_BUFFER_USAGE_WEIGHTS,(int)GGML_BACKEND_BUFFER_USAGE_COMPUTE}){external.flags=flags;fake.usage=(ggml_backend_buffer_usage)usage;same(4,u(&graph),u(idxs.data()),6,u(ops.data()),u(outputs),1);}nodes[2].view_src=saved;
// Valid sequential windows and shape/edge perturbations use every intermediate output position.
for(int first=0;first<6;++first)for(int len=0;len<=6-first;++len){same(3,u(&graph),first,u(ops.data()+first),len);int last=first+(len?len-1:0);same(5,u(&graph),first,len,u(ops.data()+first),u(&last),1);}
}
static void params(API&n,API&g){phase="parameter bits";struct Guard{uint64_t before;ggml_tensor t;uint64_t after;}a{},b{};a.before=b.before=0x123456789abcdef0;a.after=b.after=0x0fedcba987654321;std::array<unsigned char,GGML_MAX_OP_PARAMS>bytes{};for(size_t i=0;i<bytes.size();++i)bytes[i]=(unsigned char)(i*73+17);for(size_t len=0;len<=bytes.size();++len){std::memset(a.t.op_params,0xa5,sizeof a.t.op_params);std::memset(b.t.op_params,0xa5,sizeof b.t.op_params);n.q(9,u(&a.t),u(bytes.data()),len);g.q(9,u(&b.t),u(bytes.data()),len);req(std::memcmp(&a,&b,sizeof a)==0,"bulk params and tails");}
std::array<uint32_t,10>patterns={0,0x80000000,1,0xffffffff,0x7fffffff,0x3f800000,0x7f800000,0xff800000,0x7fc12345,0x7f812345};for(uint32_t value:patterns)for(uint32_t i=0;i<GGML_MAX_OP_PARAMS/4;++i){n.q(12,u(&a.t),i,value);g.q(12,u(&b.t),i,value);req(n.q(10,u(&a.t),i)==g.q(10,u(&b.t),i),"signed integer slot");req(n.q(11,u(&a.t),i)==g.q(11,u(&b.t),i),"float slot bits");n.q(13,u(&a.t),i,value);g.q(13,u(&b.t),i,value);req(std::memcmp(&a,&b,sizeof a)==0,"float setters preserve payload/canaries");}}
struct Context{API&a;ggml_context*c;Context(API&api):a(api),c(a.ggml_init({1024*1024,nullptr,true})){}~Context(){a.ggml_free(c);}ggml_tensor*t(int64_t n0=8,int64_t n1=4,int64_t n2=2,int64_t n3=2,ggml_type ty=GGML_TYPE_F32){return a.ggml_new_tensor_4d(c,ty,n0,n1,n2,n3);}};
static void compare(Context&n,Context&g){req(n.a.ggml_used_mem(n.c)==g.a.ggml_used_mem(g.c),"node arena bytes");std::unordered_map<ggml_tensor*,ggml_tensor*>map;map[nullptr]=nullptr;std::vector<ggml_tensor*>aa,bb;for(auto*t=n.a.ggml_get_first_tensor(n.c);t;t=n.a.ggml_get_next_tensor(n.c,t))aa.push_back(t);for(auto*t=g.a.ggml_get_first_tensor(g.c);t;t=g.a.ggml_get_next_tensor(g.c,t))bb.push_back(t);req(aa.size()==bb.size(),"intermediate count");for(size_t i=0;i<aa.size();++i)map[aa[i]]=bb[i];auto abase=(uintptr_t)n.a.ggml_get_mem_buffer(n.c),bbase=(uintptr_t)g.a.ggml_get_mem_buffer(g.c);for(size_t i=0;i<aa.size();++i){auto*a=aa[i],*b=bb[i];req(a->op==b->op&&a->type==b->type&&a->flags==b->flags,"node type/op/flags");req(std::memcmp(a->ne,b->ne,sizeof a->ne)==0&&std::memcmp(a->nb,b->nb,sizeof a->nb)==0,"node shape/strides");req(std::memcmp(a->op_params,b->op_params,sizeof a->op_params)==0&&std::memcmp(a->name,b->name,sizeof a->name)==0,"node parameter/name bytes");req(map.at(a->view_src)==b->view_src&&a->view_offs==b->view_offs,"node aliases");for(int s=0;s<GGML_MAX_SRC;++s)req(map.at(a->src[s])==b->src[s],"node source links");req((uintptr_t)a-abase==(uintptr_t)b-bbase,"node placement");}}
static void composition(API&n,API&g){phase="remaining node composition";Context a(n),b(g);for(Context*x:{&a,&b}){auto&api=x->a;auto c=x->c;auto t=x->t(),other=x->t();api.ggml_swiglu_oai(c,t,other,1.7f,7.0f);api.ggml_swiglu_clamp(c,t,other,8.0f);for(int k:{0,1,4,8})api.ggml_argsort_top_k(c,t,k);for(int l:{0,1,-1})for(int r:{0,2}){api.ggml_pad_ext(c,t,l,r,1,0,0,0,0,0);api.ggml_pad_ext_circular(c,t,l,r,0,1,0,0,0,0);}auto pos=x->t(2,1,1,1,GGML_TYPE_I32);auto rope=api.ggml_rope(c,t,pos,8,0);api.ggml_rope_set_offset(rope,3);}compare(a,b);}
int main(int argc,char**argv){if(argc!=3)return 2;API n(argv[1]),g(argv[2]);phase="oracle isolation";req(dlsym(n.h,"fg160_query")==nullptr,"native oracle must not load EXP160");req(dlsym(g.h,"fg160_query")!=nullptr,"candidate must load EXP160");decisions_suite(n,g);params(n,g);composition(n,g);auto witness=sym<uint64_t(*)(int)>(g.h,"fg160_caller_seen")(0);bool disabled=std::getenv("GGML_GOOSE_FUSION")&&std::strcmp(std::getenv("GGML_GOOSE_FUSION"),"0")==0;if(disabled)req((witness&((UINT64_C(1)<<18)-1))==0,"native fusion/params rollback");else req((witness&UINT64_C(0x3fe7f))==UINT64_C(0x3fe7f),"all active fusion/parameter/composition entry modes");req(sym<int64_t(*)()>(g.h,"fg157_context_heap_live")()==0&&sym<int64_t(*)()>(g.h,"fg159_graph_heap_live")()==0,"no live context or graph heaps");std::printf("PASS shared fusion/params decisions=%zu checks=%zu core_modes=0x%llx exact_parameter_bits=1 native_oracle=EXP159\n",decisions,checks,(unsigned long long)witness);}
