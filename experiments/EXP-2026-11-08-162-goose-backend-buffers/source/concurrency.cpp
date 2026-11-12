#define main fg162_unused_allocator_main
#include "fixture.cpp"
#undef main
#include <thread>
#include <atomic>
int main(int argc,char**argv){if(argc!=2)return 2;API a(argv[1],true);std::atomic<int>errors{0};std::vector<std::thread>threads;for(int t=0;t<8;++t)threads.emplace_back([&]{for(int step=0;step<16;++step){Mock m(a,64,128);Context x(a);auto*p=x.t(17,"root");a.ggml_view_1d(x.c,p,8,4);for(int i=0;i<4;++i)x.t(17+i,"extra");auto b=a.ggml_backend_alloc_ctx_tensors_from_buft(x.c,&m.kind);if(!b)++errors;a.ggml_backend_buffer_free(b);if(m.live)++errors;}});for(auto&t:threads)t.join();if(errors||sym<int64_t(*)()>(a.h,"fg162_buffer_heap_live")()!=0||sym<int64_t(*)()>(a.h,"fg157_context_heap_live")()!=0)return 3;std::puts("PASS independent backend buffer threads=8 bulk_lifecycles=128 borrowed_views=128 backend_live=0 heap_live=0");}
