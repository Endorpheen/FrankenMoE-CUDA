#include "ggml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char**argv){
 int c = atoi(argv[1]);
 struct ggml_init_params p={64u*1024u*1024u,NULL,false};
 struct ggml_context*ctx=ggml_init(p);
 struct ggml_tensor*a=ggml_new_tensor_2d(ctx,GGML_TYPE_F32,64,32);
 printf("case %d\n", c); fflush(stdout);
 switch(c){
  case 1: ggml_new_tensor_1d(ctx,(enum ggml_type)GGML_TYPE_COUNT,16); break;         /* invalid type */
  case 2: ggml_new_tensor_1d(ctx,GGML_TYPE_F32,-4); break;                           /* negative ne */
  case 3: ggml_new_tensor_2d(ctx,GGML_TYPE_F32,1LL<<40,1LL<<40); break;              /* overflow */
  case 4: ggml_view_2d(ctx,a,32,16,a->nb[1],ggml_nbytes(a)+64); break;               /* offset past end */
  case 5: ggml_reshape_2d(ctx,a,10,10); break;                                       /* element count mismatch */
  case 6: ggml_permute(ctx,a,1,1,2,3); break;                                        /* duplicate axis */
  case 7: ggml_view_3d(ctx,ggml_new_tensor_3d(ctx,GGML_TYPE_Q4_K,256,8,3),128,4,2,144,1152,8); break; /* block-alignment */
 }
 printf("no abort\n");
 ggml_free(ctx); return 0;
}
