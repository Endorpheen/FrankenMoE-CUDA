#define _POSIX_C_SOURCE 200112L
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include "ggml-alloc.h"
#include "ggml-cpu/quants.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern ggml_backend_buffer_type_t fg153_repack_buft(void);
static uint64_t hash(const void *v,size_t n) {const uint8_t*p=v;uint64_t h=1469598103934665603ULL;while(n--){h^=*p++;h*=1099511628211ULL;}return h;}
int main() {
 ggml_cpu_init();
 for(int nth=1;nth<=8;nth=nth==1?4:nth==4?8:9) {
  struct ggml_init_params ip={8*1024*1024,NULL,false},wp={ggml_tensor_overhead(),NULL,true};
  struct ggml_context*c=ggml_init(ip),*wc=ggml_init(wp);
  struct ggml_tensor*w=ggml_new_tensor_3d(wc,GGML_TYPE_Q4_K,512,16,3);
  struct ggml_backend_buffer*wb=ggml_backend_alloc_ctx_tensors_from_buft(wc,fg153_repack_buft());if(!wb)return 2;
  uint8_t*raw=malloc(ggml_nbytes(w));float row[512];
  for(int r=0;r<48;r++){for(int k=0;k<512;k++)row[k]=(float)(((k*13+r*7)%43)-21)*0.125f;quantize_row_q4_K(row,raw+r*ggml_row_size(GGML_TYPE_Q4_K,512),512);}
  ggml_backend_tensor_set(w,raw,0,ggml_nbytes(w));free(raw);
  struct ggml_tensor*a=ggml_new_tensor_3d(c,GGML_TYPE_F32,512,2,4),*ids=ggml_new_tensor_2d(c,GGML_TYPE_I32,2,4);
  for(int k=0;k<4096;k++)((float*)a->data)[k]=(float)(((k*19)%61)-30)*0.0625f;
  for(int t=0;t<4;t++){((int32_t*)ids->data)[t*2]=t%3;((int32_t*)ids->data)[t*2+1]=(t+1)%3;}
  struct ggml_tensor*out=ggml_mul_mat_id(c,w,a,ids);struct ggml_cgraph*g=ggml_new_graph(c);ggml_build_forward_expand(g,out);
  ggml_backend_t b=ggml_backend_cpu_init();ggml_backend_cpu_set_n_threads(b,nth);int st=ggml_backend_graph_compute(b,g);
  if(st!=GGML_STATUS_SUCCESS)return 3;printf("t%d %016llx\n",nth,(unsigned long long)hash(out->data,ggml_nbytes(out)));
  ggml_backend_free(b);ggml_free(c);ggml_backend_buffer_free(wb);ggml_free(wc);
 }return 0;
}
