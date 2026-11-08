#define _GNU_SOURCE 1
#include "ggml.h"
#include <dlfcn.h>
#include <link.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

template<class T> T symbol(void * h,const char * n){auto p=dlsym(h,n);if(!p){std::fprintf(stderr,"Missing ABI symbol %s: %s\n",n,dlerror());std::exit(2);}return reinterpret_cast<T>(p);}
struct API {
 void *h;
 decltype(&ggml_init) init;
 decltype(&ggml_free) free;
 decltype(&ggml_new_tensor) tensor;
 decltype(&ggml_blck_size) block;
 decltype(&ggml_type_size) size;
 API(const char *p):h(dlmopen(LM_ID_NEWLM,p,RTLD_NOW|RTLD_LOCAL)){
  if(!h){std::fprintf(stderr,"dlmopen: %s\n",dlerror());std::exit(2);}
  init=symbol<decltype(init)>(h,"ggml_init");free=symbol<decltype(free)>(h,"ggml_free");tensor=symbol<decltype(tensor)>(h,"ggml_new_tensor");block=symbol<decltype(block)>(h,"ggml_blck_size");size=symbol<decltype(size)>(h,"ggml_type_size");
 }
};
static uint64_t rng=0x8f5a7162b4c39de1ULL;
static uint64_t next(){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
int main(int argc,char **argv){
 if(argc!=3)return 2;API native(argv[1]),goose(argv[2]);
 auto ctx=native.init({16<<20,nullptr,true});if(!ctx)return 3;
 using B=bool(*)(const ggml_tensor*);using I=int64_t(*)(const ggml_tensor*);using N=int(*)(const ggml_tensor*);using Z=size_t(*)(const ggml_tensor*);using P=bool(*)(const ggml_tensor*,const ggml_tensor*);
 struct BoolFn{const char*name;B n,g;};std::vector<BoolFn> bools;
 for(auto s:{"ggml_is_scalar","ggml_is_vector","ggml_is_matrix","ggml_is_3d","ggml_is_empty","ggml_is_transposed","ggml_is_permuted","ggml_is_contiguous","ggml_is_contiguous_0","ggml_is_contiguous_1","ggml_is_contiguous_2","ggml_is_contiguous_to_1","ggml_is_contiguous_to_2","ggml_is_contiguous_to_3","ggml_is_contiguous_rows","ggml_is_contiguous_channels","ggml_is_contiguously_allocated"})bools.push_back({s,symbol<B>(native.h,s),symbol<B>(goose.h,s)});
 struct IntFn{const char*name;I n,g;};std::vector<IntFn> ints;for(auto s:{"ggml_nelements","ggml_nrows"})ints.push_back({s,symbol<I>(native.h,s),symbol<I>(goose.h,s)});
 struct SizeFn{const char*name;Z n,g;};std::vector<SizeFn> sizes;for(auto s:{"ggml_nbytes","ggml_nbytes_pad","ggml_element_size"})sizes.push_back({s,symbol<Z>(native.h,s),symbol<Z>(goose.h,s)});
 struct PairFn{const char*name;P n,g;};std::vector<PairFn> pairs;for(auto s:{"ggml_are_same_shape","ggml_are_same_stride","ggml_can_repeat"})pairs.push_back({s,symbol<P>(native.h,s),symbol<P>(goose.h,s)});
 auto nd=symbol<N>(native.h,"ggml_n_dims"),gd=symbol<N>(goose.h,"ggml_n_dims");auto nr=symbol<decltype(&ggml_row_size)>(native.h,"ggml_row_size"),gr=symbol<decltype(&ggml_row_size)>(goose.h,"ggml_row_size");
 size_t cases=0,checks=0;int formats=0;
 for(int type=0;type<GGML_TYPE_COUNT;++type){auto ty=static_cast<ggml_type>(type);const auto bs=native.block(ty);if(bs<=0||native.size(ty)==0)continue;++formats;
  for(int i=0;i<384;++i){int64_t dims[4]={bs*(1+(i%5)),1+(i%4),1+((i/4)%3),1+((i/12)%3)};auto *original=native.tensor(ctx,ty,4,dims);ggml_tensor a=*original,b=*original;
   if(i%3==0){int d=(i/3)%4;a.ne[d]=1;}
   if(i%7==0)a.ne[(i/7)%4]=0;
   if(i%11==0){a.nb[(i/11)%4]+=128;}
   if(i%13==0){auto n=a.ne[1];a.ne[1]=a.ne[2];a.ne[2]=n;auto z=a.nb[1];a.nb[1]=a.nb[2];a.nb[2]=z;}
   if(i%17==0)a.nb[(i/17)%4]=std::numeric_limits<size_t>::max()-127;
   if(i%19==0)a.nb[(i/19)%4]=0;
   if(i%2==0)b=a;else{for(int d=0;d<4;++d)b.ne[d]=a.ne[d]*(1+int64_t(next()%4));b.nb[1]+=16;}
   for(auto &f:bools){++checks;if(f.n(&a)!=f.g(&a)){std::printf("FAIL %s type=%d case=%d\n",f.name,type,i);return 4;}}
   for(auto &f:ints){++checks;if(f.n(&a)!=f.g(&a)){std::printf("FAIL %s type=%d case=%d\n",f.name,type,i);return 4;}}
   for(auto &f:sizes){++checks;auto n=f.n(&a),g=f.g(&a);if(n!=g){std::printf("FAIL %s type=%d case=%d native=%zu goose=%zu\n",f.name,type,i,n,g);return 4;}}
   for(auto &f:pairs){++checks;if(f.n(&a,&b)!=f.g(&a,&b)){std::printf("FAIL %s type=%d case=%d\n",f.name,type,i);return 4;}}
   checks+=2;if(nd(&a)!=gd(&a)||nr(ty,dims[0])!=gr(ty,dims[0])){std::printf("FAIL dims/row_size type=%d case=%d\n",type,i);return 4;}++cases;
  }
 }
 native.free(ctx);
 // Normal constructors exercise the internal row/matmul/out-product predicates.
 for(API *api:{&native,&goose}){auto c=api->init({1<<20,nullptr,true});int64_t d[4]={64,5,1,1},v[4]={64,1,1,1},q[4]={64,7,1,1},o[4]={3,5,1,1},scalar[4]={1,1,1,1};auto a=api->tensor(c,GGML_TYPE_F32,4,d);auto b=api->tensor(c,GGML_TYPE_F32,4,q);auto w=api->tensor(c,GGML_TYPE_F32,4,o);auto s=api->tensor(c,GGML_TYPE_F32,4,scalar);auto h=api->tensor(c,GGML_TYPE_F16,4,d);auto hv=api->tensor(c,GGML_TYPE_F16,4,v);
  auto mm=symbol<decltype(&ggml_mul_mat)>(api->h,"ggml_mul_mat")(c,a,b);auto op=symbol<decltype(&ggml_out_prod)>(api->h,"ggml_out_prod")(c,a,w);auto add=symbol<decltype(&ggml_add1)>(api->h,"ggml_add1")(c,a,s);auto cast=symbol<decltype(&ggml_add_cast)>(api->h,"ggml_add_cast")(c,h,hv,GGML_TYPE_F32);if(!mm||!op||!add||!cast)return 5;api->free(c);
 }
 const auto seen=symbol<uint64_t(*)(void)>(goose.h,"fg155_geometry_seen")();const char *setting=std::getenv("GGML_GOOSE_TENSOR_GEOMETRY");const bool rollback=setting&&std::strcmp(setting,"0")==0;if(seen!=(rollback?0:0xffffffULL)){std::printf("FAIL execution coverage mask=%llx\n",(unsigned long long)seen);return 6;}
 std::printf("PASS independent native differential formats=%d tensor_cases=%zu checks=%zu Goose_modes=0x%llx\n",formats,cases,checks,(unsigned long long)seen);return 0;
}
