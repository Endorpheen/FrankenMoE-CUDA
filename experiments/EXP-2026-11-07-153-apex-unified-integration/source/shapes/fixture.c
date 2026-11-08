#include "ggml.h"
#include "bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)do{if(!(x)){fprintf(stderr,"shape mismatch line %d case=%d mode=%d\n",__LINE__,i,mode);exit(2);}}while(0)
static uint64_t state=0x1531530001ULL;
static uint64_t rng(void){state^=state<<13;state^=state>>7;state^=state<<17;return state;}
int main(void){unsigned count=0;for(int i=0;i<2048;i++){struct ggml_tensor a={0},b={0};a.type=b.type=i%3==0?GGML_TYPE_Q4_K:GGML_TYPE_F32;
 struct fg153_shape x={0},y={0};x.type_size=y.type_size=ggml_type_size(a.type);x.block_size=y.block_size=ggml_blck_size(a.type);
 for(int axis=0;axis<4;axis++){a.ne[axis]=(int64_t)(rng()%5);b.ne[axis]=(int64_t)(rng()%5);a.nb[axis]=rng()%1024;b.nb[axis]=rng()%1024;}
 if(i%3==0){a.ne[0]*=256;b.ne[0]*=256;}if(i%4==0){a.ne[1]=a.ne[2]=a.ne[3]=1;}if(i%5==0)b=a;
 if(i%7==0){a.nb[0]=x.type_size;a.nb[1]=x.type_size*a.ne[0]/x.block_size;for(int axis=2;axis<4;axis++)a.nb[axis]=a.nb[axis-1]*a.ne[axis-1];}
 memcpy(x.ne,a.ne,sizeof(x.ne));memcpy(x.nb,a.nb,sizeof(x.nb));memcpy(y.ne,b.ne,sizeof(y.ne));memcpy(y.nb,b.nb,sizeof(y.nb));
 for(int mode=0;mode<11;mode++){int64_t expected=0;switch(mode){case 0:expected=ggml_is_scalar(&a);break;case 1:expected=ggml_is_vector(&a);break;case 2:expected=ggml_is_matrix(&a);break;case 3:expected=ggml_is_3d(&a);break;case 4:expected=ggml_n_dims(&a);break;case 5:expected=ggml_is_transposed(&a);break;case 6:expected=ggml_is_permuted(&a);break;case 7:expected=ggml_are_same_shape(&a,&b);break;case 8:expected=ggml_are_same_stride(&a,&b);break;case 9:expected=ggml_can_repeat(&a,&b);break;case 10:expected=ggml_is_contiguous(&a);break;}CHECK(fg153_shape_entry(mode,&x,&y,0,4)==expected);count++;}
 for(int m=0;m<=2;m++){int mode=10;int expected=m==0?ggml_is_contiguous_0(&a):m==1?ggml_is_contiguous_1(&a):ggml_is_contiguous_2(&a);CHECK(fg153_shape_entry(10,&x,&y,m,4)==expected);count++;}
 }
 printf("{\"status\":\"PASS_SHAPE_NATIVE_DIFFERENTIAL\",\"cases\":%u}\n",count);return 0;}
