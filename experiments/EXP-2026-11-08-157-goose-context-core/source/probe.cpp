#define _GNU_SOURCE 1
#include "ggml.h"
#include <dlfcn.h>
#include <link.h>
#include <cstdio>
#include <cstdlib>
template<class T>T get(void*h,const char*s){auto p=dlsym(h,s);if(!p)std::exit(2);return reinterpret_cast<T>(p);}
static void hex(const char*s){if(!s){std::printf("-");return;}for(auto*p=(const unsigned char*)s;*p;++p)std::printf("%02x",*p);}
int main(int argc,char**argv){if(argc!=2)return 2;auto h=dlmopen(LM_ID_NEWLM,argv[1],RTLD_NOW|RTLD_LOCAL);if(!h)return 3;auto traits=get<decltype(&ggml_get_type_traits)>(h,"ggml_get_type_traits");std::printf("C %d %d %d %d\n",GGML_TYPE_COUNT,GGML_OP_COUNT,GGML_UNARY_OP_COUNT,GGML_GLU_OP_COUNT);for(int i=0;i<GGML_TYPE_COUNT;++i){auto*p=traits((ggml_type)i);std::printf("T %d %lld %lld %zu %d %d %d ",i,(long long)p->blck_size,(long long)p->blck_size_interleave,p->type_size,p->is_quantized,p->to_float!=nullptr,p->from_float_ref!=nullptr);hex(p->type_name);std::printf("\n");}
 using Label=const char*(*)(int);const char*names[]={"ggml_op_name","ggml_op_symbol","ggml_unary_op_name","ggml_glu_op_name"};int counts[]={GGML_OP_COUNT,GGML_OP_COUNT,GGML_UNARY_OP_COUNT,GGML_GLU_OP_COUNT};for(int g=0;g<4;++g){auto label=get<Label>(h,names[g]);for(int i=0;i<counts[g];++i){std::printf("L %d %d ",g,i);hex(label(i));std::printf("\n");}}auto status=get<Label>(h,"ggml_status_to_string");for(int i=0;i<5;++i){std::printf("L 4 %d ",i);hex(status(i-2));std::printf("\n");}return 0;}
