#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include <stdio.h>
int main(void) {
 struct ggml_context * c=ggml_init((struct ggml_init_params){.mem_size=4*1024*1024,.no_alloc=true});
 if(!c)return 1;
 struct ggml_tensor *x=ggml_new_tensor_2d(c,GGML_TYPE_F32,32,8);
 ggml_set_input(x);
 struct ggml_tensor *z=ggml_scale(c,x,0.5f);ggml_set_output(z);
 struct ggml_cgraph *g=ggml_new_graph(c);ggml_build_forward_expand(g,z);
 ggml_gallocr_t a=ggml_gallocr_new(ggml_backend_cpu_buffer_type());size_t size=0;
 ggml_gallocr_reserve_n_size(a,g,NULL,NULL,&size);
 printf("PUBLIC_RESERVE bytes=%zu\n",size);
 ggml_gallocr_free(a);ggml_free(c);return size?0:2;
}
