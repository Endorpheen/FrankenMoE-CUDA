#include "bridge.h"
#include "ggml-cpu/binary-ops.h"
#include "ggml-cpu/unary-ops.h"
#include <cstdlib>
#include <cstring>
static bool fg_el_enabled=true;
__attribute__((constructor))static void fg_el_config(void){const char*v=getenv("GGML_GOOSE_ELEMENTWISE");fg_el_enabled=!v||strcmp(v,"0");}
typedef void(*native_op)(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_abs(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_sgn(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_neg(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_step(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_tanh(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_elu(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_relu(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_sigmoid(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_hardsigmoid(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_exp(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_hardswish(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_sqr(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_sqrt(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_sin(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_cos(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_log(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_expm1(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_softplus(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_floor(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_ceil(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_round(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_trunc(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_u_xielu(const ggml_compute_params*,ggml_tensor*);
static native_op fg_el_native_u[]={fg_el_native_u_abs,fg_el_native_u_sgn,fg_el_native_u_neg,fg_el_native_u_step,fg_el_native_u_tanh,fg_el_native_u_elu,fg_el_native_u_relu,fg_el_native_u_sigmoid,fg_el_native_u_hardsigmoid,fg_el_native_u_exp,fg_el_native_u_hardswish,fg_el_native_u_sqr,fg_el_native_u_sqrt,fg_el_native_u_sin,fg_el_native_u_cos,fg_el_native_u_log,fg_el_native_u_expm1,fg_el_native_u_softplus,fg_el_native_u_floor,fg_el_native_u_ceil,fg_el_native_u_round,fg_el_native_u_trunc,fg_el_native_u_xielu};
extern "C" void fg_el_native_b_add(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_b_sub(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_b_mul(const ggml_compute_params*,ggml_tensor*);
extern "C" void fg_el_native_b_div(const ggml_compute_params*,ggml_tensor*);
static native_op fg_el_native_b[]={fg_el_native_b_add,fg_el_native_b_sub,fg_el_native_b_mul,fg_el_native_b_div};
static fg_el_shape shape(const ggml_tensor*t){
 fg_el_shape s={};
 const size_t size=ggml_type_size(t->type);
 switch(t->type){case GGML_TYPE_F32:s.kind=0;break;case GGML_TYPE_F16:s.kind=1;break;case GGML_TYPE_BF16:s.kind=2;break;default:GGML_ABORT("unsupported elementwise type");}
 for(int i=0;i<4;++i){GGML_ASSERT(t->nb[i]%size==0);s.ne[i]=t->ne[i];s.stride[i]=t->nb[i]/size;}
 return s;
}
static void apply(int mode,int op,const ggml_compute_params*params,ggml_tensor*dst){
 if(!fg_el_enabled){(mode?fg_el_native_b[op]:fg_el_native_u[op])(params,dst);return;}
 const ggml_tensor*a=dst->src[0],*b=mode?dst->src[1]:nullptr;
 GGML_ASSERT(ggml_are_same_shape(a,dst));
 GGML_ASSERT(a->nb[0]==ggml_type_size(a->type)&&dst->nb[0]==ggml_type_size(dst->type));
 if(mode)GGML_ASSERT(ggml_can_repeat(b,a));
 else GGML_ASSERT((op==22?(ggml_is_contiguous_1(a)&&ggml_is_contiguous_1(dst)):(ggml_is_contiguous_rows(a)&&ggml_is_contiguous_rows(dst))));
 fg_el_plan p={};p.a=shape(a);p.d=shape(dst);if(b)p.b=shape(b);p.ith=params->ith;p.nth=params->nth;p.mode=mode;p.op=op;
 if(!mode&&op==22)for(int i=0;i<4;++i)p.params[i]=ggml_get_op_params_f32(dst,i+1);
 fg_element_compute(&p,a->data,ggml_nbytes(a)/ggml_type_size(a->type),b?b->data:nullptr,b?ggml_nbytes(b)/ggml_type_size(b->type):0,dst->data,ggml_nbytes(dst)/ggml_type_size(dst->type));
}
extern "C" void ggml_compute_forward_abs(const ggml_compute_params*p,ggml_tensor*d){apply(0,0,p,d);}
extern "C" void ggml_compute_forward_sgn(const ggml_compute_params*p,ggml_tensor*d){apply(0,1,p,d);}
extern "C" void ggml_compute_forward_neg(const ggml_compute_params*p,ggml_tensor*d){apply(0,2,p,d);}
extern "C" void ggml_compute_forward_step(const ggml_compute_params*p,ggml_tensor*d){apply(0,3,p,d);}
extern "C" void ggml_compute_forward_tanh(const ggml_compute_params*p,ggml_tensor*d){apply(0,4,p,d);}
extern "C" void ggml_compute_forward_elu(const ggml_compute_params*p,ggml_tensor*d){apply(0,5,p,d);}
extern "C" void ggml_compute_forward_relu(const ggml_compute_params*p,ggml_tensor*d){apply(0,6,p,d);}
extern "C" void ggml_compute_forward_sigmoid(const ggml_compute_params*p,ggml_tensor*d){apply(0,7,p,d);}
extern "C" void ggml_compute_forward_hardsigmoid(const ggml_compute_params*p,ggml_tensor*d){apply(0,8,p,d);}
extern "C" void ggml_compute_forward_exp(const ggml_compute_params*p,ggml_tensor*d){apply(0,9,p,d);}
extern "C" void ggml_compute_forward_hardswish(const ggml_compute_params*p,ggml_tensor*d){apply(0,10,p,d);}
extern "C" void ggml_compute_forward_sqr(const ggml_compute_params*p,ggml_tensor*d){apply(0,11,p,d);}
extern "C" void ggml_compute_forward_sqrt(const ggml_compute_params*p,ggml_tensor*d){apply(0,12,p,d);}
extern "C" void ggml_compute_forward_sin(const ggml_compute_params*p,ggml_tensor*d){apply(0,13,p,d);}
extern "C" void ggml_compute_forward_cos(const ggml_compute_params*p,ggml_tensor*d){apply(0,14,p,d);}
extern "C" void ggml_compute_forward_log(const ggml_compute_params*p,ggml_tensor*d){apply(0,15,p,d);}
extern "C" void ggml_compute_forward_expm1(const ggml_compute_params*p,ggml_tensor*d){apply(0,16,p,d);}
extern "C" void ggml_compute_forward_softplus(const ggml_compute_params*p,ggml_tensor*d){apply(0,17,p,d);}
extern "C" void ggml_compute_forward_floor(const ggml_compute_params*p,ggml_tensor*d){apply(0,18,p,d);}
extern "C" void ggml_compute_forward_ceil(const ggml_compute_params*p,ggml_tensor*d){apply(0,19,p,d);}
extern "C" void ggml_compute_forward_round(const ggml_compute_params*p,ggml_tensor*d){apply(0,20,p,d);}
extern "C" void ggml_compute_forward_trunc(const ggml_compute_params*p,ggml_tensor*d){apply(0,21,p,d);}
extern "C" void ggml_compute_forward_xielu(const ggml_compute_params*p,ggml_tensor*d){apply(0,22,p,d);}
extern "C" void ggml_compute_forward_add_non_quantized(const ggml_compute_params*p,ggml_tensor*d){apply(1,0,p,d);}
extern "C" void ggml_compute_forward_sub(const ggml_compute_params*p,ggml_tensor*d){apply(1,1,p,d);}
extern "C" void ggml_compute_forward_mul(const ggml_compute_params*p,ggml_tensor*d){apply(1,2,p,d);}
extern "C" void ggml_compute_forward_div(const ggml_compute_params*p,ggml_tensor*d){apply(1,3,p,d);}
