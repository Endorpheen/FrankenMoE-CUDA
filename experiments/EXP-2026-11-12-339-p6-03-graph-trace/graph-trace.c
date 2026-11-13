#include "ggml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(void){ setvbuf(stdout,NULL,_IONBF,0);
 struct ggml_init_params p={256u*1024u*1024u,NULL,false};
 struct ggml_context*c=ggml_init(p);
 struct ggml_tensor*a=ggml_new_tensor_2d(c,GGML_TYPE_F32,32,16);
 struct ggml_tensor*b=ggml_new_tensor_2d(c,GGML_TYPE_F32,32,16);
 ggml_set_name(a,"a"); ggml_set_name(b,"b");
 struct ggml_tensor*t1=ggml_add(c,a,b); ggml_set_name(t1,"add");
 struct ggml_tensor*t2=ggml_scale(c,t1,2.0f); ggml_set_name(t2,"sc");
 struct ggml_tensor*t3=ggml_mul_mat(c,a,b); ggml_set_name(t3,"mm");
 struct ggml_tensor*t4=ggml_add(c,t2,t3); ggml_set_name(t4,"out");
 struct ggml_cgraph*g=ggml_new_graph(c);
 ggml_build_forward_expand(g,t4);
 printf("n_nodes=%d\n", ggml_graph_n_nodes(g));
 for(int i=0;i<ggml_graph_n_nodes(g);i++){
  struct ggml_tensor*n=ggml_graph_node(g,i);
  printf("node %d op=%d ne=[%ld,%ld,%ld,%ld] nb=[%ld,%ld] type=%d name=%s\n", i, (int)n->op,
    (long)n->ne[0],(long)n->ne[1],(long)n->ne[2],(long)n->ne[3],(long)n->nb[0],(long)n->nb[1],(int)n->type, ggml_get_name(n));
 }
 const char *path=getenv("DOT_OUT"); if(!path) path="/tmp/g.dot";
 ggml_graph_dump_dot(g,NULL,path);
 FILE *fp=fopen(path,"rb"); if(!fp){printf("dot open failed\n");return 2;}
 fseek(fp,0,SEEK_END); long n=ftell(fp); fseek(fp,0,SEEK_SET);
 char *buf=malloc(n+1); size_t rd=fread(buf,1,n,fp); buf[rd]=0; fclose(fp);
 printf("dot_len=%ld dot_head=%.24s\n", n, buf);
 free(buf);
 ggml_free(c); free(buf); return 0;
}
