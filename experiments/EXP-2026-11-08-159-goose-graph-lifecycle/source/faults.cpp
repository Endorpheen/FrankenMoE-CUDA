#define _GNU_SOURCE 1
#include "ggml-impl.h"
#include <dlfcn.h>
#include <link.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
template<class T>T sym(void*h,const char*n){auto p=dlsym(h,n);if(!p){std::fprintf(stderr,"Missing %s %s\n",n,dlerror());std::exit(2);}return reinterpret_cast<T>(p);}
static int64_t(*heaps)();static const char*expected;
static void rejected(const char*m){_exit(heaps()==0&&std::strstr(m,expected)?81:82);}
int main(int argc,char**argv){if(argc!=2)return 2;void*h=dlmopen(LM_ID_NEWLM,argv[1],RTLD_NOW|RTLD_LOCAL);if(!h)return 2;auto init=sym<decltype(&ggml_init)>(h,"ggml_init");auto set_abort=sym<decltype(&ggml_set_abort_callback)>(h,"ggml_set_abort_callback");auto tensor=sym<decltype(&ggml_new_tensor_1d)>(h,"ggml_new_tensor_1d");auto dup=sym<decltype(&ggml_dup)>(h,"ggml_dup");auto graph=sym<decltype(&ggml_new_graph_custom)>(h,"ggml_new_graph_custom");auto expand=sym<decltype(&ggml_build_forward_expand)>(h,"ggml_build_forward_expand");auto node=sym<decltype(&ggml_graph_node)>(h,"ggml_graph_node");auto hash_new=sym<void(*)(ggml_hash_set*,size_t)>(h,"fg159_probe_hash_new");auto hop=sym<size_t(*)(int,ggml_hash_set*,ggml_tensor*)>(h,"fg159_probe_hash_op");auto view=sym<void(*)(ggml_cgraph*,ggml_cgraph*,int,int)>(h,"fg159_probe_graph_view");auto fail_at=sym<void(*)(int)>(h,"fg159_fail_allocation_at");heaps=sym<decltype(heaps)>(h,"fg159_graph_heap_live");
for(int test=0;test<9;++test){auto pid=fork();if(pid<0)return 3;if(!pid){set_abort(rejected);ggml_hash_set hs{};if(test==0||test==1){expected=test==0?"code=6":"code=7";fail_at(test+1);hash_new(&hs,17);_exit(83);}auto c=init({1024*1024,nullptr,true});auto t=tensor(c,GGML_TYPE_F32,8);auto*g=graph(c,test==3?0:17,false);
switch(test){case 2:expected="code=13";fail_at(1);expand(g,t);break;
case 3:expected="code=14";expand(g,t);break;
case 4:expected="code=23";node(g,-1);break;
case 5:expected="code=21";{ggml_cgraph v{};view(&v,g,-1,0);}break;
case 6:expected="code=10";t->src[0]=t;expand(g,t);break;
case 7:expected="code=8";graph(c,SIZE_MAX,false);break;
case 8:expected="code=5";{ggml_bitset_t used=3;ggml_tensor*keys[2]={(ggml_tensor*)16,(ggml_tensor*)48};ggml_hash_set full{2,&used,keys};hop(1,&full,(ggml_tensor*)80);}break;}
_exit(83);}int status=0;if(waitpid(pid,&status,0)!=pid||!WIFEXITED(status)||WEXITSTATUS(status)!=81){std::fprintf(stderr,"FAIL graph fault=%d status=%d\n",test,status);return 4;}}
std::puts("PASS graph allocation/error cleanup cases=9 hash_oom=2 scratch_oom=1 capacity_index_slice_cycle_size_full_hash=6 live_graph_heaps=0");}
