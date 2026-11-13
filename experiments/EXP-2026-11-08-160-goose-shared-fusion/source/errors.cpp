#define _GNU_SOURCE 1
#include "ggml-impl.h"
#include "ggml-backend-impl.h"
#include <dlfcn.h>
#include <link.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
template<class T>T sym(void*h,const char*n){auto p=dlsym(h,n);if(!p){std::fprintf(stderr,"Missing %s\n",n);std::exit(2);}return reinterpret_cast<T>(p);}
static ggml_tensor*checked_tensor;static ggml_tensor saved;static int64_t(*contexts)();static int64_t(*graphs)();
static void rejected(const char*){_exit(std::memcmp(checked_tensor,&saved,sizeof saved)==0&&contexts()==0&&graphs()==0?81:82);}
int main(int argc,char**argv){if(argc!=2)return 2;void*h=dlmopen(LM_ID_NEWLM,argv[1],RTLD_NOW|RTLD_LOCAL);if(!h){std::fprintf(stderr,"Load %s\n",dlerror());return 2;}auto query=sym<uint64_t(*)(int64_t,const uint64_t*,int64_t)>(h,"fg160_query");auto abort_callback=sym<decltype(&ggml_set_abort_callback)>(h,"ggml_set_abort_callback");contexts=sym<decltype(contexts)>(h,"fg157_context_heap_live");graphs=sym<decltype(graphs)>(h,"fg159_graph_heap_live");
for(int test=0;test<10;++test){auto pid=fork();if(pid<0)return 3;if(!pid){ggml_tensor t{};for(int i=0;i<4;++i)t.ne[i]=1;t.op=GGML_OP_ROPE;checked_tensor=&t;saved=t;abort_callback(rejected);uint64_t args[7]={(uintptr_t)&t,16,0,0,0,0,0};int mode=10;unsigned char bytes[65]{};
switch(test){case 0:mode=10;break;case 1:mode=11;break;case 2:mode=12;args[1]=UINT32_MAX;break;case 3:mode=13;args[1]=16;break;case 4:mode=9;args[1]=(uintptr_t)bytes;args[2]=65;break;case 5:mode=9;args[1]=0;args[2]=1;break;case 6:mode=14;args[1]=UINT64_MAX;break;case 7:mode=14;t.op=GGML_OP_ADD;saved=t;args[1]=0;break;case 8:mode=17;{static int32_t pad[8]={-2,0,0,0,0,0,0,0};args[0]=0;args[1]=(uintptr_t)&t;args[2]=(uintptr_t)pad;}break;case 9:mode=17;{static int32_t pad[8]={1,0,0,0,0,0,0,0};t.ne[0]=INT64_MAX;saved=t;args[0]=0;args[1]=(uintptr_t)&t;args[2]=(uintptr_t)pad;}break;}
query(mode,args,0);_exit(83);}int status=0;if(waitpid(pid,&status,0)!=pid||!WIFEXITED(status)||WEXITSTATUS(status)!=81){std::fprintf(stderr,"FAIL shared rejection=%d status=%d\n",test,status);return 4;}}
// Zero-byte NULL copies are accepted without calling memcpy on a NULL source.
ggml_tensor t{};uint64_t args[7]={(uintptr_t)&t,0,0,0,0,0,0};if(query(9,args,0)!=0)return 5;
// Native header helpers index unchecked negative IDs;the Goose predicates reject them.
ggml_tensor nodes[2]{};ggml_tensor*ptrs[2]={nodes,nodes+1};for(int i=0;i<2;++i){nodes[i].op=i?GGML_OP_MUL:GGML_OP_ADD;nodes[i].flags=GGML_TENSOR_FLAG_COMPUTE;for(int j=0;j<4;++j)nodes[i].ne[j]=1;}nodes[1].src[0]=nodes;int32_t counts[3]={0,0,0};ggml_bitset_t used=0;ggml_tensor*keys[3]={};ggml_cgraph graph{};graph.size=2;graph.n_nodes=2;graph.nodes=ptrs;graph.use_counts=counts;graph.visited_hash_set={3,&used,keys};int idx[2]={-1,1};ggml_op ops[2]={GGML_OP_ADD,GGML_OP_MUL};int output=1;uint64_t bad[7]={(uintptr_t)&graph,(uintptr_t)idx,2,(uintptr_t)ops,(uintptr_t)&output,1,0};if(query(4,bad,0)!=0)return 6;bad[1]=UINT64_MAX;bad[2]=(uintptr_t)ops;bad[3]=2;if(query(3,bad,0)!=0)return 7;std::array<int,3>edge{-1,0,0};bad[1]=0;bad[2]=(uintptr_t)&edge;bad[3]=1;if(query(6,bad,0)!=0)return 8;
// Full hash with an absent graph node returns zero uses instead of reading SIZE_MAX.
used=7;keys[0]=(ggml_tensor*)16;keys[1]=(ggml_tensor*)32;keys[2]=(ggml_tensor*)48;bad[1]=0;if(query(0,bad,0)!=0)return 9;
// Validate a long borrowed immutable-view chain,then reject its actual cycle.
used=0;std::memset(keys,0,sizeof keys);std::memset(counts,0,sizeof counts);for(int i=0;i<2;++i){size_t slot=((uintptr_t)(nodes+i)>>4)%3;while(used&(1u<<slot))slot=(slot+1)%3;used|=1u<<slot;keys[slot]=nodes+i;counts[slot]=i?0:1;}
ggml_backend_buffer weights{};weights.usage=GGML_BACKEND_BUFFER_USAGE_WEIGHTS;ggml_tensor borrowed[64]{};for(int i=0;i<64;++i){borrowed[i].buffer=&weights;if(i+1<64)borrowed[i].view_src=borrowed+i+1;}nodes[0].view_src=borrowed;idx[0]=0;uint64_t chain[7]={(uintptr_t)&graph,(uintptr_t)idx,2,(uintptr_t)ops,(uintptr_t)&output,1,0};if(query(4,chain,0)!=1)return 10;borrowed[63].view_src=borrowed;if(query(4,chain,0)!=0)return 11;
std::puts("PASS shared malformed-input cases=15 rejection_before_mutation=10 zero_NULL_copy=1 negative_fusion_indices=3 full_absent_hash=1 borrowed_chain=64 cycle_rejected=1");}
