#include "models.h"
#include "llama-memory-recurrent.h"
#include "llama-kv-cache.h"
#include "llama-adapter.h"
#include "llama-moe-cache.h"
#include "bridge.h"
#include "owner.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>
static int64_t handle(const void *p){return (int64_t)(intptr_t)p;}
static ggml_tensor *tensor(int64_t p){return (ggml_tensor *)(intptr_t)p;}
static ApexOwner &owner(int64_t e){return *(ApexOwner *)(intptr_t)e;}
extern "C" int64_t fg153_am_p_add(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_add(owner(a0).ctx0,tensor(a1),tensor(a2)));}
extern "C" int64_t fg153_am_p_argsort_top_k(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_argsort_top_k(owner(a0).ctx0,tensor(a1),(int)a2));}
extern "C" void fg153_am_p_build_forward_expand(int64_t a0,int64_t a1){ggml_build_forward_expand(owner(a0).gf,tensor(a1));}
extern "C" void fg153_am_p_build_forward_order(int64_t a0,int64_t a1){ggml_build_forward_order(owner(a0).gf,tensor(a1));}
extern "C" int64_t fg153_am_p_cast(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_cast(owner(a0).ctx0,tensor(a1),(enum   ggml_type)a2));}
extern "C" int64_t fg153_am_p_clamp(int64_t a0,int64_t a1,float a2,float a3){return handle(ggml_clamp(owner(a0).ctx0,tensor(a1),a2,a3));}
extern "C" int64_t fg153_am_p_concat(int64_t a0,int64_t a1,int64_t a2,int64_t a3){return handle(ggml_concat(owner(a0).ctx0,tensor(a1),tensor(a2),(int)a3));}
extern "C" int64_t fg153_am_p_cont(int64_t a0,int64_t a1){return handle(ggml_cont(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_cont_2d(int64_t a0,int64_t a1,int64_t a2,int64_t a3){return handle(ggml_cont_2d(owner(a0).ctx0,tensor(a1),a2,a3));}
extern "C" int64_t fg153_am_p_cont_4d(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5){return handle(ggml_cont_4d(owner(a0).ctx0,tensor(a1),a2,a3,a4,a5));}
extern "C" int64_t fg153_am_p_cpy(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_cpy(owner(a0).ctx0,tensor(a1),tensor(a2)));}
extern "C" int64_t fg153_am_p_cumsum(int64_t a0,int64_t a1){return handle(ggml_cumsum(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_diag(int64_t a0,int64_t a1){return handle(ggml_diag(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_div(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_div(owner(a0).ctx0,tensor(a1),tensor(a2)));}
extern "C" int64_t fg153_am_p_dup(int64_t a0,int64_t a1){return handle(ggml_dup(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_exp(int64_t a0,int64_t a1){return handle(ggml_exp(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_fill(int64_t a0,int64_t a1,float a2){return handle(ggml_fill(owner(a0).ctx0,tensor(a1),a2));}
extern "C" int64_t fg153_am_p_flash_attn_ext(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,float a5,float a6,float a7){return handle(ggml_flash_attn_ext(owner(a0).ctx0,tensor(a1),tensor(a2),tensor(a3),tensor(a4),a5,a6,a7));}
extern "C" void fg153_am_p_flash_attn_ext_add_sinks(int64_t a0,int64_t a1){ggml_flash_attn_ext_add_sinks(tensor(a0),tensor(a1));}
extern "C" void fg153_am_p_flash_attn_ext_set_n_kv_max(int64_t a0,int64_t a1){ggml_flash_attn_ext_set_n_kv_max(tensor(a0),(int32_t)a1);}
extern "C" int64_t fg153_am_p_gated_delta_net(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5,int64_t a6,int64_t a7){return handle(ggml_gated_delta_net(owner(a0).ctx0,tensor(a1),tensor(a2),tensor(a3),tensor(a4),tensor(a5),tensor(a6),a7));}
extern "C" int64_t fg153_am_p_get_rows(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_get_rows(owner(a0).ctx0,tensor(a1),tensor(a2)));}
extern "C" bool fg153_am_p_is_contiguous(int64_t a0){return ggml_is_contiguous(tensor(a0));}
extern "C" int64_t fg153_am_p_mul(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_mul(owner(a0).ctx0,tensor(a1),tensor(a2)));}
extern "C" int64_t fg153_am_p_mul_mat(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_mul_mat(owner(a0).ctx0,tensor(a1),tensor(a2)));}
extern "C" int64_t fg153_am_p_mul_mat_id(int64_t a0,int64_t a1,int64_t a2,int64_t a3){return handle(ggml_mul_mat_id(owner(a0).ctx0,tensor(a1),tensor(a2),tensor(a3)));}
extern "C" int64_t fg153_am_p_neg(int64_t a0,int64_t a1){return handle(ggml_neg(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_nelements(int64_t a0){return ggml_nelements(tensor(a0));}
extern "C" int64_t fg153_am_p_new_tensor_1d(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_new_tensor_1d(owner(a0).ctx0,(enum   ggml_type)a1,a2));}
extern "C" int64_t fg153_am_p_new_tensor_2d(int64_t a0,int64_t a1,int64_t a2,int64_t a3){return handle(ggml_new_tensor_2d(owner(a0).ctx0,(enum   ggml_type)a1,a2,a3));}
extern "C" int64_t fg153_am_p_pad(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5){return handle(ggml_pad(owner(a0).ctx0,tensor(a1),(int)a2,(int)a3,(int)a4,(int)a5));}
extern "C" int64_t fg153_am_p_permute(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5){return handle(ggml_permute(owner(a0).ctx0,tensor(a1),(int)a2,(int)a3,(int)a4,(int)a5));}
extern "C" bool fg153_am_p_prec_set_acc(int64_t a0,int64_t a1){return ggml_prec_set_acc(tensor(a0),(enum ggml_prec)a1);}
extern "C" int64_t fg153_am_p_repeat(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_repeat(owner(a0).ctx0,tensor(a1),tensor(a2)));}
extern "C" int64_t fg153_am_p_repeat_4d(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5){return handle(ggml_repeat_4d(owner(a0).ctx0,tensor(a1),a2,a3,a4,a5));}
extern "C" int64_t fg153_am_p_reshape_1d(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_reshape_1d(owner(a0).ctx0,tensor(a1),a2));}
extern "C" int64_t fg153_am_p_reshape_2d(int64_t a0,int64_t a1,int64_t a2,int64_t a3){return handle(ggml_reshape_2d(owner(a0).ctx0,tensor(a1),a2,a3));}
extern "C" int64_t fg153_am_p_reshape_3d(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4){return handle(ggml_reshape_3d(owner(a0).ctx0,tensor(a1),a2,a3,a4));}
extern "C" int64_t fg153_am_p_reshape_4d(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5){return handle(ggml_reshape_4d(owner(a0).ctx0,tensor(a1),a2,a3,a4,a5));}
extern "C" int64_t fg153_am_p_rms_norm(int64_t a0,int64_t a1,float a2){return handle(ggml_rms_norm(owner(a0).ctx0,tensor(a1),a2));}
extern "C" int64_t fg153_am_p_rope_multi(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5,int64_t a6,int64_t a7,float a8,float a9,float a10,float a11,float a12,float a13){return handle(ggml_rope_multi(owner(a0).ctx0,tensor(a1),tensor(a2),tensor(a3),(int)a4,owner(a0).rope_sections.data(),(int)a6,(int)a7,a8,a9,a10,a11,a12,a13));}
extern "C" int64_t fg153_am_p_scale(int64_t a0,int64_t a1,float a2){return handle(ggml_scale(owner(a0).ctx0,tensor(a1),a2));}
extern "C" int64_t fg153_am_p_scale_inplace(int64_t a0,int64_t a1,float a2){return handle(ggml_scale_inplace(owner(a0).ctx0,tensor(a1),a2));}
extern "C" int64_t fg153_am_p_set_inplace(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5,int64_t a6){return handle(ggml_set_inplace(owner(a0).ctx0,tensor(a1),tensor(a2),a3,a4,a5,a6));}
extern "C" void fg153_am_p_set_input(int64_t a0){ggml_set_input(tensor(a0));}
extern "C" int64_t fg153_am_p_set_rows(int64_t a0,int64_t a1,int64_t a2,int64_t a3){return handle(ggml_set_rows(owner(a0).ctx0,tensor(a1),tensor(a2),tensor(a3)));}
extern "C" int64_t fg153_am_p_sigmoid(int64_t a0,int64_t a1){return handle(ggml_sigmoid(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_silu(int64_t a0,int64_t a1){return handle(ggml_silu(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_soft_max(int64_t a0,int64_t a1){return handle(ggml_soft_max(owner(a0).ctx0,tensor(a1)));}
extern "C" void fg153_am_p_soft_max_add_sinks(int64_t a0,int64_t a1){ggml_soft_max_add_sinks(tensor(a0),tensor(a1));}
extern "C" int64_t fg153_am_p_soft_max_ext(int64_t a0,int64_t a1,int64_t a2,float a3,float a4){return handle(ggml_soft_max_ext(owner(a0).ctx0,tensor(a1),tensor(a2),a3,a4));}
extern "C" int64_t fg153_am_p_softplus(int64_t a0,int64_t a1){return handle(ggml_softplus(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_solve_tri(int64_t a0,int64_t a1,int64_t a2,bool a3,bool a4,bool a5){return handle(ggml_solve_tri(owner(a0).ctx0,tensor(a1),tensor(a2),a3,a4,a5));}
extern "C" int64_t fg153_am_p_ssm_conv(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_ssm_conv(owner(a0).ctx0,tensor(a1),tensor(a2)));}
extern "C" int64_t fg153_am_p_sub(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_sub(owner(a0).ctx0,tensor(a1),tensor(a2)));}
extern "C" int64_t fg153_am_p_sum_rows(int64_t a0,int64_t a1){return handle(ggml_sum_rows(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_swiglu_split(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_swiglu_split(owner(a0).ctx0,tensor(a1),tensor(a2)));}
extern "C" int64_t fg153_am_p_tanh(int64_t a0,int64_t a1){return handle(ggml_tanh(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_transpose(int64_t a0,int64_t a1){return handle(ggml_transpose(owner(a0).ctx0,tensor(a1)));}
extern "C" int64_t fg153_am_p_tri(int64_t a0,int64_t a1,int64_t a2){return handle(ggml_tri(owner(a0).ctx0,tensor(a1),(enum ggml_tri_type)a2));}
extern "C" int64_t fg153_am_p_view_1d(int64_t a0,int64_t a1,int64_t a2,int64_t a3){return handle(ggml_view_1d(owner(a0).ctx0,tensor(a1),a2,a3));}
extern "C" int64_t fg153_am_p_view_2d(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5){return handle(ggml_view_2d(owner(a0).ctx0,tensor(a1),a2,a3,a4,a5));}
extern "C" int64_t fg153_am_p_view_3d(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5,int64_t a6,int64_t a7){return handle(ggml_view_3d(owner(a0).ctx0,tensor(a1),a2,a3,a4,a5,a6,a7));}
extern "C" int64_t fg153_am_p_view_4d(int64_t a0,int64_t a1,int64_t a2,int64_t a3,int64_t a4,int64_t a5,int64_t a6,int64_t a7,int64_t a8,int64_t a9){return handle(ggml_view_4d(owner(a0).ctx0,tensor(a1),a2,a3,a4,a5,a6,a7,a8,a9));}
