#define main fg165_positive_main
#include "fixture.cpp"
#undef main
#include <sys/wait.h>
#include <unistd.h>
static API*active;static int scenario;
static void rejected(const char*){auto heaps=sym<int64_t(*)()>(active->h,"fg165_registry_heap_live");auto handles=sym<int64_t(*)()>(active->h,"fg165_registry_handles_live");if(handles()!=0)_exit(83);if(heaps()!=(scenario==5?1:0))_exit(84);sym<void(*)()>(active->h,"fg165_probe_finish")();if(heaps()!=0)_exit(85);_exit(81);}
int main(int argc,char**argv){if(argc!=2)return 2;API a(argv[1],true);a.ggml_backend_reg_count();auto inject=sym<void(*)(int)>(a.h,"fg165_fail_allocation_at");for(int test=0;test<6;test++){auto p=fork();if(p<0)return 3;if(!p){active=&a;scenario=test;a.ggml_set_abort_callback(rejected);std::vector<std::string>log;Provider f(a,log,"test");switch(test){case 0:a.ggml_backend_reg_get(0);break;case 1:a.ggml_backend_dev_get(SIZE_MAX);break;case 2:a.ggml_backend_reg_by_name(nullptr);break;case 3:a.ggml_backend_device_register(nullptr);break;case 4:inject(1);a.ggml_backend_register(&f.reg);break;case 5:inject(2);a.ggml_backend_register(&f.reg);break;}_exit(86);}int st=0;if(waitpid(p,&st,0)!=p||!WIFEXITED(st)||WEXITSTATUS(st)!=81){fprintf(stderr,"FAIL registry rejection=%d status=%d\n",test,st);return 4;}}puts("PASS rejection_cases=6 bounds/null/OOM=1 partial_owned_state_releasable=1 heaps_and_handles=0");}
