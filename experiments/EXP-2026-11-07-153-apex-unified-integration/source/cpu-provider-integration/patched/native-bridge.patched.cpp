#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-cpu-impl.h"
#include "ops.h"
#include "fg143-bridge.h"
#include "stateful/bridge.h"
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <cstdio>
#include <climits>
static bool fg143_enabled=true;
static std::atomic<uint64_t> fg143_native_calls[GGML_OP_COUNT];
__attribute__((constructor))static void fg143_config(){const char*v=std::getenv("GGML_GOOSE_CPU_BACKEND");fg143_enabled=!v||std::strcmp(v,"0");}
static fg143_tensor fg143_shape(const ggml_tensor*t){fg143_tensor s={};if(t){for(int i=0;i<4;++i){s.ne[i]=t->ne[i];s.nb[i]=t->nb[i];}s.kind=t->type;s.bytes=ggml_nbytes(t);}return s;}
static float fg143_float(const ggml_tensor*t,int at){float f;std::memcpy(&f,(const char*)t->op_params+at*4,4);return f;}
static fg143_node fg143_describe(const ggml_tensor*t,int ith,int nth){
 fg143_node p={};p.x=fg143_shape(t->src[0]);p.y=fg143_shape(t->src[1]);p.z=fg143_shape(t->src[2]);p.d=fg143_shape(t);p.op=t->op;p.ith=ith;p.nth=nth;
 for(int i=0;i<16;++i)p.ip[i]=t->op_params[i];for(int i=0;i<8;++i)p.fp[i]=fg143_float(t,i);p.ip[13]=1;
#if defined(__AVX2__) && defined(__FMA__)
 p.ip[12]=1;
#else
 p.ip[12]=0;
#endif

 switch(t->op){
 case GGML_OP_UNARY:for(int i=0;i<4;++i)p.fp[i]=fg143_float(t,i+1);break;
 case GGML_OP_GLU:p.ip[2]=t->src[1]!=nullptr;p.fp[0]=fg143_float(t,2);p.fp[1]=fg143_float(t,3);break;
 case GGML_OP_GROUP_NORM:p.fp[0]=fg143_float(t,1);break;
 case GGML_OP_SOFT_MAX:p.ip[0]=t->src[1]!=nullptr;p.ip[1]=t->src[2]!=nullptr;break;
 case GGML_OP_ROPE:case GGML_OP_ROPE_BACK:
  p.ip[0]=t->op_params[1];p.ip[1]=t->op_params[2];p.ip[2]=t->op_params[4];p.ip[3]=t->op_params[15];for(int i=0;i<4;++i)p.ip[4+i]=t->op_params[11+i];p.ip[8]=t->src[2]!=nullptr;for(int i=0;i<6;++i)p.fp[i]=fg143_float(t,5+i);break;
 case GGML_OP_WIN_PART:p.ip[0]=t->op_params[2];p.ip[1]=t->op_params[0];break;
 default:break;
 }
 return p;
}
static bool fg143_scalar(enum ggml_type t){return t==GGML_TYPE_F32||t==GGML_TYPE_F16||t==GGML_TYPE_BF16||t==GGML_TYPE_I8||t==GGML_TYPE_I16||t==GGML_TYPE_I32||t==GGML_TYPE_I64||t==GGML_TYPE_F64;}
extern "C" size_t fg143_extra_workspace(const ggml_tensor*t,int nth){if(!fg143_enabled)return 0;auto p=fg143_describe(t,0,nth);int64_t info[4]={};fg143_workspace(&p,info);GGML_ASSERT(info[3]>=0&&(uint64_t)info[3]<=SIZE_MAX);return(size_t)info[3];}
extern "C" uint8_t fg006_try(const ggml_compute_params*params,ggml_tensor*t);
extern "C" uint8_t fg143_try(const ggml_compute_params*params,ggml_tensor*t){
 if(fg006_try(params,t))return 1;
 if(!fg143_enabled||params->use_ref||!fg143_scalar(t->type))return 0;

 if(t->op==GGML_OP_GATED_DELTA_NET||t->op==GGML_OP_SSM_SCAN||t->op==GGML_OP_RWKV_WKV6||t->op==GGML_OP_RWKV_WKV7||t->op==GGML_OP_GATED_LINEAR_ATTN){fg143_state_plan state={};const void*inputs[7]={};for(int i=0;i<7;++i){state.source[i]=fg143_shape(t->src[i]);inputs[i]=t->src[i]?t->src[i]->data:nullptr;}state.dst=fg143_shape(t);state.ith=params->ith;state.nth=params->nth;state.op=t->op;state.snapshots=t->op_params[0];for(int i=0;i<4;++i)state.fp[i]=fg143_float(t,i);return fg143_state_compute(&state,inputs,t->data);}
 auto p=fg143_describe(t,params->ith,params->nth);int64_t info[4]={};fg143_workspace(&p,info);GGML_ASSERT(info[3]>=0&&(uint64_t)info[3]<=params->wsize);void*work=params->wdata;int64_t*indices=info[1]?(int64_t*)((char*)work+info[2]):nullptr;
 return fg143_compute(&p,t->src[0]?t->src[0]->data:nullptr,t->src[1]?t->src[1]->data:nullptr,t->src[2]?t->src[2]->data:nullptr,t->data,(float*)work,info[0],indices,info[1]);
}
__attribute__((destructor))static void fg143_fallback_report(){for(int i=0;i<GGML_OP_COUNT;++i){auto n=fg143_native_calls[i].load();if(n)std::fprintf(stderr,"FG143_NATIVE op=%d calls=%llu enabled=%d\n",i,(unsigned long long)n,fg143_enabled);}}
extern "C" void fg143_native_ggml_compute_forward_dup(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_dup(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_dup(p,t);}
extern "C" void fg143_native_ggml_compute_forward_add(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_add(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_add(p,t);}
extern "C" void fg143_native_ggml_compute_forward_add_id(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_add_id(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_add_id(p,t);}
extern "C" void fg143_native_ggml_compute_forward_add1(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_add1(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_add1(p,t);}
extern "C" void fg143_native_ggml_compute_forward_acc(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_acc(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_acc(p,t);}
extern "C" void fg143_native_ggml_compute_forward_sum(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_sum(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_sum(p,t);}
extern "C" void fg143_native_ggml_compute_forward_sum_rows(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_sum_rows(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_sum_rows(p,t);}
extern "C" void fg143_native_ggml_compute_forward_cumsum(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_cumsum(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_cumsum(p,t);}
extern "C" void fg143_native_ggml_compute_forward_mean(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_mean(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_mean(p,t);}
extern "C" void fg143_native_ggml_compute_forward_argmax(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_argmax(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_argmax(p,t);}
extern "C" void fg143_native_ggml_compute_forward_count_equal(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_count_equal(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_count_equal(p,t);}
extern "C" void fg143_native_ggml_compute_forward_repeat(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_repeat(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_repeat(p,t);}
extern "C" void fg143_native_ggml_compute_forward_repeat_back(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_repeat_back(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_repeat_back(p,t);}
extern "C" void fg143_native_ggml_compute_forward_concat(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_concat(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_concat(p,t);}
extern "C" void fg143_native_ggml_compute_forward_silu_back(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_silu_back(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_silu_back(p,t);}
extern "C" void fg143_native_ggml_compute_forward_norm(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_norm(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_norm(p,t);}
extern "C" void fg143_native_ggml_compute_forward_rms_norm(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_rms_norm(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_rms_norm(p,t);}
extern "C" void fg143_native_ggml_compute_forward_rms_norm_back(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_rms_norm_back(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_rms_norm_back(p,t);}
extern "C" void fg143_native_ggml_compute_forward_group_norm(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_group_norm(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_group_norm(p,t);}
extern "C" void fg143_native_ggml_compute_forward_l2_norm(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_l2_norm(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_l2_norm(p,t);}
extern "C" void fg143_native_ggml_compute_forward_out_prod(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_out_prod(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_out_prod(p,t);}
extern "C" void fg143_native_ggml_compute_forward_scale(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_scale(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_scale(p,t);}
extern "C" void fg143_native_ggml_compute_forward_set(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_set(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_set(p,t);}
extern "C" void fg143_native_ggml_compute_forward_cpy(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_cpy(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_cpy(p,t);}
extern "C" void fg143_native_ggml_compute_forward_cont(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_cont(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_cont(p,t);}
extern "C" void fg143_native_ggml_compute_forward_get_rows(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_get_rows(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_get_rows(p,t);}
extern "C" void fg143_native_ggml_compute_forward_get_rows_back(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_get_rows_back(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_get_rows_back(p,t);}
extern "C" void fg143_native_ggml_compute_forward_set_rows(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_set_rows(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_set_rows(p,t);}
extern "C" void fg143_native_ggml_compute_forward_diag(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_diag(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_diag(p,t);}
extern "C" void fg143_native_ggml_compute_forward_diag_mask_inf(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_diag_mask_inf(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_diag_mask_inf(p,t);}
extern "C" void fg143_native_ggml_compute_forward_diag_mask_zero(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_diag_mask_zero(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_diag_mask_zero(p,t);}
extern "C" void fg143_native_ggml_compute_forward_soft_max(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_soft_max(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_soft_max(p,t);}
extern "C" void fg143_native_ggml_compute_forward_soft_max_ext_back(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_soft_max_ext_back(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_soft_max_ext_back(p,t);}
extern "C" void fg143_native_ggml_compute_forward_rope(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_rope(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_rope(p,t);}
extern "C" void fg143_native_ggml_compute_forward_rope_back(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_rope_back(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_rope_back(p,t);}
extern "C" void fg143_native_ggml_compute_forward_clamp(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_clamp(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_clamp(p,t);}
extern "C" void fg143_native_ggml_compute_forward_conv_transpose_1d(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_conv_transpose_1d(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_conv_transpose_1d(p,t);}
extern "C" void fg143_native_ggml_compute_forward_im2col(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_im2col(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_im2col(p,t);}
extern "C" void fg143_native_ggml_compute_forward_im2col_back_f32(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_im2col_back_f32(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_im2col_back_f32(p,t);}
extern "C" void fg143_native_ggml_compute_forward_im2col_3d(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_im2col_3d(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_im2col_3d(p,t);}
extern "C" void fg143_native_ggml_compute_forward_col2im_1d(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_col2im_1d(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_col2im_1d(p,t);}
extern "C" void fg143_native_ggml_compute_forward_conv_2d(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_conv_2d(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_conv_2d(p,t);}
extern "C" void fg143_native_ggml_compute_forward_conv_3d(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_conv_3d(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_conv_3d(p,t);}
extern "C" void fg143_native_ggml_compute_forward_conv_transpose_2d(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_conv_transpose_2d(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_conv_transpose_2d(p,t);}
extern "C" void fg143_native_ggml_compute_forward_conv_2d_dw(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_conv_2d_dw(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_conv_2d_dw(p,t);}
extern "C" void fg143_native_ggml_compute_forward_pool_1d(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_pool_1d(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_pool_1d(p,t);}
extern "C" void fg143_native_ggml_compute_forward_pool_2d(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_pool_2d(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_pool_2d(p,t);}
extern "C" void fg143_native_ggml_compute_forward_pool_2d_back(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_pool_2d_back(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_pool_2d_back(p,t);}
extern "C" void fg143_native_ggml_compute_forward_upscale(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_upscale(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_upscale(p,t);}
extern "C" void fg143_native_ggml_compute_forward_pad(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_pad(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_pad(p,t);}
extern "C" void fg143_native_ggml_compute_forward_pad_reflect_1d(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_pad_reflect_1d(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_pad_reflect_1d(p,t);}
extern "C" void fg143_native_ggml_compute_forward_roll(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_roll(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_roll(p,t);}
extern "C" void fg143_native_ggml_compute_forward_arange(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_arange(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_arange(p,t);}
extern "C" void fg143_native_ggml_compute_forward_timestep_embedding(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_timestep_embedding(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_timestep_embedding(p,t);}
extern "C" void fg143_native_ggml_compute_forward_argsort(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_argsort(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_argsort(p,t);}
extern "C" void fg143_native_ggml_compute_forward_top_k(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_top_k(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_top_k(p,t);}
extern "C" void fg143_native_ggml_compute_forward_leaky_relu(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_leaky_relu(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_leaky_relu(p,t);}
extern "C" void fg143_native_ggml_compute_forward_tri(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_tri(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_tri(p,t);}
extern "C" void fg143_native_ggml_compute_forward_fill(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_fill(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_fill(p,t);}
extern "C" void fg143_native_ggml_compute_forward_flash_attn_ext(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_flash_attn_ext(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_flash_attn_ext(p,t);}
extern "C" void fg143_native_ggml_compute_forward_ssm_conv(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_ssm_conv(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_ssm_conv(p,t);}
extern "C" void fg143_native_ggml_compute_forward_ssm_scan(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_ssm_scan(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_ssm_scan(p,t);}
extern "C" void fg143_native_ggml_compute_forward_win_part(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_win_part(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_win_part(p,t);}
extern "C" void fg143_native_ggml_compute_forward_win_unpart(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_win_unpart(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_win_unpart(p,t);}
extern "C" void fg143_native_ggml_compute_forward_unary(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_unary(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_unary(p,t);}
extern "C" void fg143_native_ggml_compute_forward_glu(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_glu(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_glu(p,t);}
extern "C" void fg143_native_ggml_compute_forward_get_rel_pos(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_get_rel_pos(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_get_rel_pos(p,t);}
extern "C" void fg143_native_ggml_compute_forward_add_rel_pos(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_add_rel_pos(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_add_rel_pos(p,t);}
extern "C" void fg143_native_ggml_compute_forward_rwkv_wkv6(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_rwkv_wkv6(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_rwkv_wkv6(p,t);}
extern "C" void fg143_native_ggml_compute_forward_rwkv_wkv7(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_rwkv_wkv7(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_rwkv_wkv7(p,t);}
extern "C" void fg143_native_ggml_compute_forward_solve_tri(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_solve_tri(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_solve_tri(p,t);}
extern "C" void fg143_native_ggml_compute_forward_gla(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_gla(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_gla(p,t);}
extern "C" void fg143_native_ggml_compute_forward_gated_delta_net(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_gated_delta_net(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_gated_delta_net(p,t);}
extern "C" void fg143_native_ggml_compute_forward_lightning_indexer(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_lightning_indexer(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_lightning_indexer(p,t);}
extern "C" void fg143_native_ggml_compute_forward_dsv4_hc_comb(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_dsv4_hc_comb(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_dsv4_hc_comb(p,t);}
extern "C" void fg143_native_ggml_compute_forward_dsv4_hc_pre(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_dsv4_hc_pre(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_dsv4_hc_pre(p,t);}
extern "C" void fg143_native_ggml_compute_forward_dsv4_hc_post(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_dsv4_hc_post(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_dsv4_hc_post(p,t);}
extern "C" void fg143_native_ggml_compute_forward_map_custom1(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_map_custom1(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_map_custom1(p,t);}
extern "C" void fg143_native_ggml_compute_forward_map_custom2(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_map_custom2(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_map_custom2(p,t);}
extern "C" void fg143_native_ggml_compute_forward_map_custom3(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_map_custom3(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_map_custom3(p,t);}
extern "C" void fg143_native_ggml_compute_forward_custom(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_custom(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_custom(p,t);}
extern "C" void fg143_native_ggml_compute_forward_cross_entropy_loss(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_cross_entropy_loss(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_cross_entropy_loss(p,t);}
extern "C" void fg143_native_ggml_compute_forward_cross_entropy_loss_back(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_cross_entropy_loss_back(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_cross_entropy_loss_back(p,t);}
extern "C" void fg143_native_ggml_compute_forward_opt_step_adamw(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_opt_step_adamw(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_opt_step_adamw(p,t);}
extern "C" void fg143_native_ggml_compute_forward_fwht(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_fwht(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_fwht(p,t);}
extern "C" void fg143_native_ggml_compute_forward_opt_step_sgd(const ggml_compute_params*,ggml_tensor*);
extern "C" void ggml_compute_forward_opt_step_sgd(const ggml_compute_params*p,ggml_tensor*t){if(fg143_try(p,t))return;fg143_native_calls[t->op].fetch_add(1,std::memory_order_relaxed);fg143_native_ggml_compute_forward_opt_step_sgd(p,t);}

extern "C" uint8_t fg143_backend_is_enabled(void){return fg143_enabled;}
extern "C" uint8_t fg143_apply_fused(const ggml_compute_params*params,const ggml_tensor*norm,ggml_tensor*mul,int slot){
 if(!fg143_enabled||params->use_ref)return 0;GGML_ASSERT(slot==0||slot==1);const ggml_tensor*w=mul->src[slot];auto p=fg143_describe(mul,params->ith,params->nth);p.x=fg143_shape(norm->src[0]);p.y=fg143_shape(w);p.z={};p.op=GGML_OP_RMS_NORM;p.fp[0]=fg143_float(norm,0);p.ip[14]=1;return fg143_compute(&p,norm->src[0]->data,w->data,nullptr,mul->data,nullptr,0,nullptr,0);
}
