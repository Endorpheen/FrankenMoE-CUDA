#include "canonical-io.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include "ggml-alloc.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
extern "C" int64_t fgc_live_allocs(void);
extern "C" int64_t fgc_live_bytes(void);
#define C(x) do {if (!(x)) {std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static int64_t h(void *p) {return (int64_t)(intptr_t)p;}
int main() {
 ggml_cpu_init();
 ggml_init_params ip={16384,nullptr,true};auto ctx=ggml_init(ip);C(ctx);
 auto src=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,4);auto dst=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,4);
 auto s0=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,2);auto s1=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,2);
 auto d0=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,2);auto d1=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,2);
 auto buf=ggml_backend_alloc_ctx_tensors_from_buft(ctx,ggml_backend_cpu_buffer_type());C(buf);
 float in[4]={1.25f,-2.5f,3.75f,9.0f};ggml_backend_tensor_set(src,in,0,sizeof in);ggml_backend_tensor_set(s0,in,0,8);ggml_backend_tensor_set(s1,in+2,0,8);
 int64_t w=fgcio_create(5,0,0,0);C(w);
 for (int variant=0;variant<4;variant++) {
   if (variant==2) {C(fgcio_write_tensor(w,h(s0),8,0)==0);C(fgcio_write_tensor(w,h(s1),8,0)==0);}
   else C(fgcio_write_tensor(w,h(src),16,0)==0);
   C(fgcio_flush(w)==0);
   int64_t r=fgcio_create(6,0,0,w);C(r);
   if (variant==1 || variant==3) {C(fgcio_read_tensor(r,h(d0),8,0)==0);C(fgcio_read_tensor(r,h(d1),8,0)==0);}
   else C(fgcio_read_tensor(r,h(dst),16,0)==0);
   C(fgcio_flush(r)==0);float out[4]={};
   if (variant==1 || variant==3) {ggml_backend_tensor_get(d0,out,0,8);ggml_backend_tensor_get(d1,out+2,0,8);}else ggml_backend_tensor_get(dst,out,0,16);
   C(!std::memcmp(in,out,16));C(fgcio_dispose(r)==0);
 }
 C(fgcio_dispose(w)==0);C(fgc_live_allocs()==0 && fgc_live_bytes()==0);
 ggml_backend_buffer_free(buf);ggml_free(ctx);
 std::puts("PASS real GGML canonical device IO repeated/split/geometry-change and zero owned allocations");return 0;
}
