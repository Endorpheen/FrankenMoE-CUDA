#define _GNU_SOURCE 1
#include "ggml.h"
#include <dlfcn.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
template<class T>T sym(void*h,const char*n){return reinterpret_cast<T>(dlsym(h,n));}
int main(int argc,char**argv){if(argc!=2)return 2;void*h=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL|RTLD_DEEPBIND);if(!h)return 2;auto init=sym<decltype(&ggml_init)>(h,"ggml_init");auto release=sym<decltype(&ggml_free)>(h,"ggml_free");auto tensor=sym<decltype(&ggml_new_tensor_1d)>(h,"ggml_new_tensor_1d");auto add=sym<decltype(&ggml_add)>(h,"ggml_add");auto graph=sym<decltype(&ggml_new_graph_custom)>(h,"ggml_new_graph_custom");auto expand=sym<decltype(&ggml_build_forward_expand)>(h,"ggml_build_forward_expand");auto count=sym<decltype(&ggml_graph_n_nodes)>(h,"ggml_graph_n_nodes");auto clear=sym<decltype(&ggml_graph_clear)>(h,"ggml_graph_clear");auto heaps=sym<int64_t(*)()>(h,"fg159_graph_heap_live");std::atomic<int>errors{0};std::vector<std::thread>ts;for(int t=0;t<8;++t)ts.emplace_back([&]{for(int j=0;j<32;++j){auto c=init({65536,nullptr,true});auto a=tensor(c,GGML_TYPE_F32,8),b=tensor(c,GGML_TYPE_F32,8),out=add(c,a,b);auto*g=graph(c,17,false);expand(g,out);if(count(g)!=1)++errors;clear(g);expand(g,out);if(count(g)!=1)++errors;release(c);}});for(auto&t:ts)t.join();if(errors||heaps()!=0)return 3;std::puts("PASS concurrent independent inference graphs threads=8 cycles=256 expand_clear_reuse=512 live_graph_heaps=0");}
