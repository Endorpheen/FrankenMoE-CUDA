#include "ggml-quants.h"
#include "matrix-bridge.h"
#include <stdlib.h>
#include <string.h>
static int fg139_dequant_active=1;
__attribute__((constructor))static void fg139_dequant_config(void){const char*v=getenv("GGML_GOOSE_MOE_MATH");fg139_dequant_active=!v||strcmp(v,"0");}
extern void fg139_native_dequantize_row_q2_0(const block_q2_0*,float*,int64_t);
void dequantize_row_q2_0(const block_q2_0*x,float*y,int64_t n){if(!fg139_dequant_active)fg139_native_dequantize_row_q2_0(x,y,n);else fg139_dequantize(0,x,y,n);}
extern void fg139_native_dequantize_row_q5_0(const block_q5_0*,float*,int64_t);
void dequantize_row_q5_0(const block_q5_0*x,float*y,int64_t n){if(!fg139_dequant_active)fg139_native_dequantize_row_q5_0(x,y,n);else fg139_dequantize(1,x,y,n);}
extern void fg139_native_dequantize_row_iq4_nl(const block_iq4_nl*,float*,int64_t);
void dequantize_row_iq4_nl(const block_iq4_nl*x,float*y,int64_t n){if(!fg139_dequant_active)fg139_native_dequantize_row_iq4_nl(x,y,n);else fg139_dequantize(2,x,y,n);}
extern void fg139_native_dequantize_row_iq2_xxs(const block_iq2_xxs*,float*,int64_t);
void dequantize_row_iq2_xxs(const block_iq2_xxs*x,float*y,int64_t n){if(!fg139_dequant_active)fg139_native_dequantize_row_iq2_xxs(x,y,n);else fg139_dequantize(3,x,y,n);}
extern void fg139_native_dequantize_row_iq2_s(const block_iq2_s*,float*,int64_t);
void dequantize_row_iq2_s(const block_iq2_s*x,float*y,int64_t n){if(!fg139_dequant_active)fg139_native_dequantize_row_iq2_s(x,y,n);else fg139_dequantize(4,x,y,n);}
