#include "ggml-cpu/quants.h"
#include "matrix-bridge.h"
#include <stdlib.h>
#include <string.h>
static int fg139_math_active=1;
int fg139_math_enabled(void);
__attribute__((constructor))static void fg139_math_config(void){const char*v=getenv("GGML_GOOSE_MOE_MATH");fg139_math_active=!v||strcmp(v,"0");}
__attribute__((visibility("hidden")))int fg139_math_enabled(void){return fg139_math_active;}
extern void fg139_quantize(int,const float*,void*,int64_t);
void quantize_row_q8_0(const float*x,void*y,int64_t n){fg139_quantize(0,x,y,n);}
void quantize_row_q8_K(const float*x,void*y,int64_t n){fg139_quantize(1,x,y,n);}
extern void fg139_native_ggml_vec_dot_q2_0_q8_0(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc);
void ggml_vec_dot_q2_0_q8_0(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc){if(!fg139_math_active||nrc!=1){fg139_native_ggml_vec_dot_q2_0_q8_0(n,s,bs,x,bx,y,by,nrc);return;}fg139_dot(0,n,x,y,s);}
extern void fg139_native_ggml_vec_dot_q5_0_q8_0(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc);
void ggml_vec_dot_q5_0_q8_0(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc){if(!fg139_math_active||nrc!=1){fg139_native_ggml_vec_dot_q5_0_q8_0(n,s,bs,x,bx,y,by,nrc);return;}fg139_dot(1,n,x,y,s);}
extern void fg139_native_ggml_vec_dot_iq4_nl_q8_0(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc);
void ggml_vec_dot_iq4_nl_q8_0(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc){if(!fg139_math_active||nrc!=1){fg139_native_ggml_vec_dot_iq4_nl_q8_0(n,s,bs,x,bx,y,by,nrc);return;}fg139_dot(2,n,x,y,s);}
extern void fg139_native_ggml_vec_dot_iq2_xxs_q8_K(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc);
void ggml_vec_dot_iq2_xxs_q8_K(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc){if(!fg139_math_active||nrc!=1){fg139_native_ggml_vec_dot_iq2_xxs_q8_K(n,s,bs,x,bx,y,by,nrc);return;}fg139_dot(3,n,x,y,s);}
extern void fg139_native_ggml_vec_dot_iq2_s_q8_K(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc);
void ggml_vec_dot_iq2_s_q8_K(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc){if(!fg139_math_active||nrc!=1){fg139_native_ggml_vec_dot_iq2_s_q8_K(n,s,bs,x,bx,y,by,nrc);return;}fg139_dot(4,n,x,y,s);}
