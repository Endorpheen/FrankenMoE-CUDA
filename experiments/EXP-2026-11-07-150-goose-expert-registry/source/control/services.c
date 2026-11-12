
struct fg143_ctl_context {const struct ggml_cgraph*graph;struct ggml_tensor*node;struct ggml_threadpool*pool;struct ggml_compute_params*params;};
static bool fg143_control_enabled=true;
__attribute__((constructor))static void fg143_control_config(void){const char*v=getenv("GGML_GOOSE_CPU_CONTROL");fg143_control_enabled=!v||strcmp(v,"0");}
bool fg143_control_is_enabled(void){return fg143_control_enabled;}
extern void fg143_cpu_control(int64_t,int64_t,int64_t,int64_t,int64_t*);
static struct ggml_tensor*fg143_control_node(struct fg143_ctl_context*x,int64_t index){if(x->graph){GGML_ASSERT(index>=0&&index<x->graph->n_nodes);return x->graph->nodes[index];}GGML_ASSERT(index==0&&x->node);return x->node;}
int64_t fg143_ctl_meta(int64_t context,int64_t index,int64_t field){
 struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;
 if(field>=50){switch(field){case 50:return x->pool?x->pool->n_threads:0;case 51:return GGML_DEFAULT_N_THREADS;case 52:
#if (defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)) || defined(__wasi__)
 return 1;
#else
 return 0;
#endif
 case 53:return CACHE_LINE_SIZE;case 54:return sizeof(struct mmid_row_mapping);case 55:return GGML_FA_TILE_Q;case 56:return GGML_FA_TILE_KV;case 57:return GGML_SOFT_MAX_UNROLL;case 58:return GGML_IM2COL_WORK_SIZE;case 59:return GGML_GLU_OP_COUNT;case 60:return GGML_UNARY_OP_COUNT;case 61:
#if defined(__AVX512VNNI__) || defined(__AVX2__) || defined(__AVX__)
 return 1;
#else
 return 0;
#endif
 case 62:return fg143_control_node(x,index)->src[0]->extra!=NULL;case 63:return fg149_tiled_enabled;case 64:return 524288;default:GGML_ABORT("invalid Goose CPU constant");}}
 const struct ggml_tensor*t=fg143_control_node(x,index);const struct ggml_tensor*a=t->src[0];
 if(field>=10&&field<14)return t->ne[field-10];
 if(field>=20&&field<32){int source=(field-20)/4,axis=(field-20)%4;return t->src[source]?t->src[source]->ne[axis]:0;}
 if(field>=2&&field<=4)return t->src[field-2]?t->src[field-2]->type:-1;
 switch(field){case 0:return t->op;case 1:return t->type;case 5:return ggml_is_quantized(t->type);case 6:return a?ggml_is_quantized(a->type):0;case 8:return ggml_get_op_params_i32(t,0);case 40:return type_traits_cpu[a->type].vec_dot_type;case 41:return ggml_type_size(type_traits_cpu[a->type].vec_dot_type);case 42:return ggml_blck_size(type_traits_cpu[a->type].vec_dot_type);case 43:return ggml_type_size(t->type);case 44:return a?ggml_type_size(a->type):0;
 case 7:switch(t->op){case GGML_OP_MAP_CUSTOM1:{struct ggml_map_custom1_op_params p;memcpy(&p,t->op_params,sizeof(p));return p.n_tasks;}case GGML_OP_MAP_CUSTOM2:{struct ggml_map_custom2_op_params p;memcpy(&p,t->op_params,sizeof(p));return p.n_tasks;}case GGML_OP_MAP_CUSTOM3:{struct ggml_map_custom3_op_params p;memcpy(&p,t->op_params,sizeof(p));return p.n_tasks;}case GGML_OP_CUSTOM:{struct ggml_custom_op_params p;memcpy(&p,t->op_params,sizeof(p));return p.n_tasks;}default:GGML_ABORT("invalid custom task metadata");}
 default:GGML_ABORT("invalid Goose CPU metadata");}
}
int64_t fg143_ctl_extra_work(int64_t context,int64_t index,int64_t threads){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;size_t size=0;if(!ggml_cpu_extra_work_size((int)threads,fg143_control_node(x,index),&size))return -1;GGML_ASSERT(size<=INT64_MAX);return(int64_t)size;}
int64_t fg143_ctl_goose_work(int64_t context,int64_t index,int64_t tasks){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;size_t size=fg143_extra_workspace(fg143_control_node(x,index),(int)tasks);GGML_ASSERT(size<=INT64_MAX);return(int64_t)size;}
uint8_t fg143_ctl_extra_compute(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;return ggml_cpu_extra_compute_forward(x->params,x->node);}
void fg143_control_dispatch(struct ggml_compute_params*p,struct ggml_tensor*t){struct fg143_ctl_context context={0};context.node=t;context.params=p;fg143_cpu_control(2,(int64_t)(uintptr_t)&context,0,0,NULL);}
int fg143_control_tasks(struct ggml_tensor*t,int threads){struct fg143_ctl_context context={0};context.node=t;int64_t out[2]={0};fg143_cpu_control(1,(int64_t)(uintptr_t)&context,0,threads,out);GGML_ASSERT(out[0]>0&&out[0]<=INT_MAX);return(int)out[0];}
struct ggml_cplan fg143_control_plan(const struct ggml_cgraph*g,int threads,struct ggml_threadpool*pool){struct fg143_ctl_context context={0};context.graph=g;context.pool=pool;int64_t out[2]={0};fg143_cpu_control(0,(int64_t)(uintptr_t)&context,g->n_nodes,threads,out);GGML_ASSERT(out[0]>=0&&out[1]>0&&out[1]<=INT_MAX);struct ggml_cplan plan={0};plan.threadpool=pool;plan.n_threads=(int)out[1];plan.work_size=(size_t)out[0];return plan;}

void fg143_ctl_leaf_dup(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_dup(x->params,x->node);}
void fg143_ctl_leaf_add(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_add(x->params,x->node);}
void fg143_ctl_leaf_add_id(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_add_id(x->params,x->node);}
void fg143_ctl_leaf_add1(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_add1(x->params,x->node);}
void fg143_ctl_leaf_acc(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_acc(x->params,x->node);}
void fg143_ctl_leaf_sub(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_sub(x->params,x->node);}
void fg143_ctl_leaf_mul(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_mul(x->params,x->node);}
void fg143_ctl_leaf_div(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_div(x->params,x->node);}
void fg143_ctl_leaf_sqr(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_sqr(x->params,x->node);}
void fg143_ctl_leaf_sqrt(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_sqrt(x->params,x->node);}
void fg143_ctl_leaf_log(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_log(x->params,x->node);}
void fg143_ctl_leaf_sin(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_sin(x->params,x->node);}
void fg143_ctl_leaf_cos(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_cos(x->params,x->node);}
void fg143_ctl_leaf_sum(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_sum(x->params,x->node);}
void fg143_ctl_leaf_sum_rows(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_sum_rows(x->params,x->node);}
void fg143_ctl_leaf_cumsum(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_cumsum(x->params,x->node);}
void fg143_ctl_leaf_mean(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_mean(x->params,x->node);}
void fg143_ctl_leaf_argmax(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_argmax(x->params,x->node);}
void fg143_ctl_leaf_count_equal(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_count_equal(x->params,x->node);}
void fg143_ctl_leaf_repeat(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_repeat(x->params,x->node);}
void fg143_ctl_leaf_repeat_back(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_repeat_back(x->params,x->node);}
void fg143_ctl_leaf_concat(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_concat(x->params,x->node);}
void fg143_ctl_leaf_silu_back(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_silu_back(x->params,x->node);}
void fg143_ctl_leaf_norm(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_norm(x->params,x->node);}
void fg143_ctl_leaf_rms_norm(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_rms_norm(x->params,x->node);}
void fg143_ctl_leaf_rms_norm_back(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_rms_norm_back(x->params,x->node);}
void fg143_ctl_leaf_group_norm(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_group_norm(x->params,x->node);}
void fg143_ctl_leaf_l2_norm(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_l2_norm(x->params,x->node);}
void fg143_ctl_leaf_mul_mat(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_mul_mat(x->params,x->node);}
void fg143_ctl_leaf_mul_mat_id(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_mul_mat_id(x->params,x->node);}
void fg143_ctl_leaf_out_prod(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_out_prod(x->params,x->node);}
void fg143_ctl_leaf_scale(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_scale(x->params,x->node);}
void fg143_ctl_leaf_set(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_set(x->params,x->node);}
void fg143_ctl_leaf_cpy(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_cpy(x->params,x->node);}
void fg143_ctl_leaf_cont(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_cont(x->params,x->node);}
void fg143_ctl_leaf_get_rows(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_get_rows(x->params,x->node);}
void fg143_ctl_leaf_get_rows_back(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_get_rows_back(x->params,x->node);}
void fg143_ctl_leaf_set_rows(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_set_rows(x->params,x->node);}
void fg143_ctl_leaf_diag(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_diag(x->params,x->node);}
void fg143_ctl_leaf_diag_mask_inf(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_diag_mask_inf(x->params,x->node);}
void fg143_ctl_leaf_diag_mask_zero(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_diag_mask_zero(x->params,x->node);}
void fg143_ctl_leaf_soft_max(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_soft_max(x->params,x->node);}
void fg143_ctl_leaf_soft_max_back(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_soft_max_ext_back(x->params,x->node);}
void fg143_ctl_leaf_rope(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_rope(x->params,x->node);}
void fg143_ctl_leaf_rope_back(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_rope_back(x->params,x->node);}
void fg143_ctl_leaf_clamp(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_clamp(x->params,x->node);}
void fg143_ctl_leaf_conv_transpose_1d(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_conv_transpose_1d(x->params,x->node);}
void fg143_ctl_leaf_im2col(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_im2col(x->params,x->node);}
void fg143_ctl_leaf_im2col_back(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_im2col_back_f32(x->params,x->node);}
void fg143_ctl_leaf_im2col_3d(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_im2col_3d(x->params,x->node);}
void fg143_ctl_leaf_col2im_1d(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_col2im_1d(x->params,x->node);}
void fg143_ctl_leaf_conv_2d(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_conv_2d(x->params,x->node);}
void fg143_ctl_leaf_conv_3d(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_conv_3d(x->params,x->node);}
void fg143_ctl_leaf_conv_2d_dw(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_conv_2d_dw(x->params,x->node);}
void fg143_ctl_leaf_conv_transpose_2d(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_conv_transpose_2d(x->params,x->node);}
void fg143_ctl_leaf_pool_1d(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_pool_1d(x->params,x->node);}
void fg143_ctl_leaf_pool_2d(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_pool_2d(x->params,x->node);}
void fg143_ctl_leaf_pool_2d_back(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_pool_2d_back(x->params,x->node);}
void fg143_ctl_leaf_upscale(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_upscale(x->params,x->node);}
void fg143_ctl_leaf_pad(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_pad(x->params,x->node);}
void fg143_ctl_leaf_pad_reflect_1d(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_pad_reflect_1d(x->params,x->node);}
void fg143_ctl_leaf_roll(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_roll(x->params,x->node);}
void fg143_ctl_leaf_arange(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_arange(x->params,x->node);}
void fg143_ctl_leaf_timestep_embedding(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_timestep_embedding(x->params,x->node);}
void fg143_ctl_leaf_argsort(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_argsort(x->params,x->node);}
void fg143_ctl_leaf_top_k(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_top_k(x->params,x->node);}
void fg143_ctl_leaf_leaky_relu(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_leaky_relu(x->params,x->node);}
void fg143_ctl_leaf_tri(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_tri(x->params,x->node);}
void fg143_ctl_leaf_fill(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_fill(x->params,x->node);}
void fg143_ctl_leaf_flash_attn_ext(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_flash_attn_ext(x->params,x->node);}
void fg143_ctl_leaf_flash_attn_back(int64_t context,int64_t masked){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_flash_attn_back(x->params,masked!=0,x->node);}
void fg143_ctl_leaf_ssm_conv(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_ssm_conv(x->params,x->node);}
void fg143_ctl_leaf_ssm_scan(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_ssm_scan(x->params,x->node);}
void fg143_ctl_leaf_win_part(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_win_part(x->params,x->node);}
void fg143_ctl_leaf_win_unpart(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_win_unpart(x->params,x->node);}
void fg143_ctl_leaf_unary(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_unary(x->params,x->node);}
void fg143_ctl_leaf_glu(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_glu(x->params,x->node);}
void fg143_ctl_leaf_get_rel_pos(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_get_rel_pos(x->params,x->node);}
void fg143_ctl_leaf_add_rel_pos(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_add_rel_pos(x->params,x->node);}
void fg143_ctl_leaf_rwkv_wkv6(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_rwkv_wkv6(x->params,x->node);}
void fg143_ctl_leaf_gated_linear_attn(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_gla(x->params,x->node);}
void fg143_ctl_leaf_rwkv_wkv7(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_rwkv_wkv7(x->params,x->node);}
void fg143_ctl_leaf_solve_tri(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_solve_tri(x->params,x->node);}
void fg143_ctl_leaf_gated_delta_net(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_gated_delta_net(x->params,x->node);}
void fg143_ctl_leaf_lightning_indexer(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_lightning_indexer(x->params,x->node);}
void fg143_ctl_leaf_dsv4_hc_comb(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_dsv4_hc_comb(x->params,x->node);}
void fg143_ctl_leaf_dsv4_hc_pre(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_dsv4_hc_pre(x->params,x->node);}
void fg143_ctl_leaf_dsv4_hc_post(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_dsv4_hc_post(x->params,x->node);}
void fg143_ctl_leaf_map_custom1(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_map_custom1(x->params,x->node);}
void fg143_ctl_leaf_map_custom2(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_map_custom2(x->params,x->node);}
void fg143_ctl_leaf_map_custom3(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_map_custom3(x->params,x->node);}
void fg143_ctl_leaf_custom(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_custom(x->params,x->node);}
void fg143_ctl_leaf_cross_entropy_loss(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_cross_entropy_loss(x->params,x->node);}
void fg143_ctl_leaf_cross_entropy_loss_back(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_cross_entropy_loss_back(x->params,x->node);}
void fg143_ctl_leaf_opt_step_adamw(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_opt_step_adamw(x->params,x->node);}
void fg143_ctl_leaf_opt_step_sgd(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;ggml_compute_forward_opt_step_sgd(x->params,x->node);}

static bool fg150_expert_policy=true;
static atomic_uint_fast64_t fg150_scratch_calls;
__attribute__((constructor))static void fg150_expert_config(void){const char*v=getenv("GGML_GOOSE_EXPERT_POLICY");fg150_expert_policy=!v||strcmp(v,"0");}
static size_t fg150_expert_scratch(int threads,struct ggml_tensor*t){
 if(!fg150_expert_policy)return ggml_tiled_wdata_size(threads,t);
 struct fg143_ctl_context c={0};c.node=t;int64_t out[2]={0};fg143_cpu_control(5,(int64_t)(uintptr_t)&c,0,threads,out);
 GGML_ASSERT(out[0]>=0);atomic_fetch_add_explicit(&fg150_scratch_calls,1,memory_order_relaxed);return(size_t)out[0];
}
__attribute__((destructor))static void fg150_scratch_report(void){fprintf(stderr,"FG150_EXPERT scratch_calls=%llu enabled=%d\n",(unsigned long long)atomic_load(&fg150_scratch_calls),fg150_expert_policy);}
