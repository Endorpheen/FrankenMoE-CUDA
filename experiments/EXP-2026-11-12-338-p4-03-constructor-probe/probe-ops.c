#include "ggml.h"
#include <stdio.h>
#include <string.h>
static void d(const char*t, struct ggml_tensor*x){
 printf("%s %s ne=[%ld,%ld,%ld,%ld] nb=[%ld,%ld,%ld,%ld] nb_=%zu contig=%d view=%d data_null=%d op=%d\n", t,
  x?ggml_type_name(x->type):"-",
  x?(long)x->ne[0]:0, x?(long)x->ne[1]:0, x?(long)x->ne[2]:0, x?(long)x->ne[3]:0,
  x?(long)x->nb[0]:0, x?(long)x->nb[1]:0, x?(long)x->nb[2]:0, x?(long)x->nb[3]:0,
  x?ggml_nbytes(x):0, x?(int)ggml_is_contiguous(x):0, x?(x->view_src!=NULL):0, x?(x->data==NULL):0, x?(int)x->op:0);
}
int main(void){
 struct ggml_init_params p={256u*1024u*1024u,NULL,false};
 struct ggml_context*c=ggml_init(p);
 struct ggml_tensor*a=ggml_new_tensor_2d(c,GGML_TYPE_F32,64,32);
 struct ggml_tensor*b=ggml_new_tensor_2d(c,GGML_TYPE_F32,64,32);
 ggml_set_name(a,"a"); ggml_set_name(b,"b");
 d("a",a); d("b",b);
 printf("name a=%s b=%s a2=%s\n", ggml_get_name(a), ggml_get_name(b), ggml_get_name(a));
 struct ggml_tensor*ct=ggml_cont(c,a); d("cont",ct);
 struct ggml_tensor*dp=ggml_dup(c,a); d("dup",dp);
 ggml_set_zero(a);
 struct ggml_tensor*sc=ggml_scale(c,a,3.0f); d("scale",sc);
 struct ggml_tensor*ad=ggml_add(c,a,b); d("add",ad);
 struct ggml_tensor*mm=ggml_mul_mat(c,a,b); d("mulmat",mm);
 struct ggml_tensor*vw=ggml_view_2d(c,a,32,16,a->nb[1],64); d("view2d",vw);
 printf("view2d src_is_a=%d offs=%zu\n", vw->view_src==a, (size_t)vw->view_offs);
 struct ggml_cgraph*g=ggml_new_graph(c);
 ggml_build_forward_expand(g,ad); ggml_build_forward_expand(g,mm);
 printf("graph n_nodes=%d\n", ggml_graph_n_nodes(g));
 char buf[256]; ggml_graph_dump_dot(g,NULL,buf);
 printf("dot_len=%zu\n", strlen(buf));
 printf("used=%zu\n", ggml_used_mem(c));
 ggml_free(c); return 0;
}
