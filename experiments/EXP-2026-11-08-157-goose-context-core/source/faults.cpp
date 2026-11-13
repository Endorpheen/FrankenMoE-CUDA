#define _GNU_SOURCE 1
#include "ggml.h"
#include <dlfcn.h>
#include <link.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
template<class T>T get(void*h,const char*n){auto p=dlsym(h,n);if(!p){std::fprintf(stderr,"Missing %s\n",n);std::exit(2);}return reinterpret_cast<T>(p);}
static int64_t(*live)();static uint64_t(*frees)();static const char*expected;
static void abort_checked(const char*message){_exit(live()==0&&frees()==0&&std::strstr(message,expected)?81:82);}
int main(int argc,char**argv){if(argc!=2)return 2;void*h=dlmopen(LM_ID_NEWLM,argv[1],RTLD_NOW|RTLD_LOCAL);if(!h){std::fprintf(stderr,"%s\n",dlerror());return 2;}
auto init=get<decltype(&ggml_init)>(h,"ggml_init");auto set_abort=get<decltype(&ggml_set_abort_callback)>(h,"ggml_set_abort_callback");auto fail_at=get<void(*)(int)>(h,"fg157_fail_allocation_at");auto alloc=get<void*(*)(size_t,size_t,int)>(h,"fg157_test_private_alloc");auto aligned=get<void*(*)(size_t)>(h,"ggml_aligned_malloc");live=get<decltype(live)>(h,"fg157_context_heap_live");frees=get<decltype(frees)>(h,"fg157_aligned_free_calls");
for(int test=0;test<8;++test){auto pid=fork();if(pid<0)return 3;if(!pid){set_abort(abort_checked);alignas(64) unsigned char external[4096];
switch(test){case 0:expected="code=3";fail_at(1);init({4096,nullptr,false});break;
case 1:expected="code=4";fail_at(2);init({4096,nullptr,false});break;
case 2:expected="code=3";fail_at(1);init({4096,external,false});break;
case 3:expected="code=5";init({4096,external+1,false});break;
case 4:expected="code=2";init({SIZE_MAX,nullptr,false});break;
case 5:expected="code=20";fail_at(1);alloc(32,0,0);break;
case 6:expected="code=21";fail_at(1);alloc(2,32,1);break;
case 7:expected="code=1";alloc(SIZE_MAX,2,1);break;}
_exit(83);}int status=0;if(waitpid(pid,&status,0)!=pid||!WIFEXITED(status)||WEXITSTATUS(status)!=81){std::fprintf(stderr,"FAIL fault case=%d status=%d\n",test,status);return 4;}}
if(alloc(0,0,0)||alloc(0,16,1)||alloc(16,0,1))return 5;
fail_at(1);if(aligned(64)!=nullptr||live()!=0||frees()!=0)return 6;
std::puts("PASS allocation failures=8 zero-size=3 aligned-refusal=1 descriptor_heaps=0 borrowed_frees=0");return 0;}
