#define main fg161_unused_positive_main
#include "fixture.cpp"
#undef main
#include <thread>
#include <atomic>
int main(int argc,char**argv){if(argc!=2)return 2;API api(argv[1],true);std::atomic<int>errors{0};std::vector<std::thread>threads;for(int tid=0;tid<8;++tid)threads.emplace_back([&]{for(int step=0;step<16;++step){Mock m(api,64,512);Context x(api);auto graph=make(x,4,16,true);auto a=api.ggml_gallocr_new(&m.kind);if(!api.ggml_gallocr_alloc_graph(a,graph.graph)||!api.ggml_gallocr_alloc_graph(a,graph.graph))++errors;api.ggml_gallocr_free(a);if(m.live!=0)++errors;}});for(auto&t:threads)t.join();if(errors||sym<int64_t(*)()>(api.h,"fg161_allocator_heap_live")()!=0||sym<int64_t(*)()>(api.h,"fg157_context_heap_live")()!=0)return 3;std::puts("PASS independent allocator threads=8 graph_lifecycles=128 graph_allocations=256 backend_live=0 allocator_heaps=0");}
