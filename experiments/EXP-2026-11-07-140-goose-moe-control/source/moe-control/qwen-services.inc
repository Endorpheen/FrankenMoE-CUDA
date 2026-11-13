struct fg140_context {
 const struct ggml_compute_params*params;struct ggml_tensor*dst;struct fg140_plan*plan;
 int64_t*counts;struct mmid_row_mapping*rows;char(*chunks)[CACHE_LINE_SIZE];char*scratch;
};
static bool fg140_enabled=true;
#ifndef FG140_TRACE
#define FG140_TRACE 1
#endif
#if FG140_TRACE
static atomic_uint_fast64_t fg140_events[7],fg140_q4_calls,fg140_q6_calls;
#endif
__attribute__((constructor))static void fg140_config(void){const char*v=getenv("GGML_GOOSE_MOE_CONTROL");fg140_enabled=!v||strcmp(v,"0");}
static struct fg140_context*fg140_context_get(int64_t c){return(struct fg140_context*)(uintptr_t)c;}
void fg140_quantize(int64_t c,int64_t source,int64_t target,int64_t values){
 struct fg140_context*x=fg140_context_get(c);const struct ggml_tensor*y=x->dst->src[1];const struct fg140_plan*p=x->plan;
 GGML_ASSERT(source>=0&&target>=0&&values>=0&&values%p->block_size==0);
 GGML_ASSERT((uint64_t)source<=ggml_nbytes(y)&&(uint64_t)values*4<=ggml_nbytes(y)-(uint64_t)source);
 const size_t nbytes=(size_t)(values/p->block_size)*p->block_bytes;
 const size_t capacity=(size_t)p->row_size*p->ne11*p->ne12*p->ne13;
 GGML_ASSERT((uint64_t)target<=capacity&&nbytes<=capacity-(uint64_t)target);
 const enum ggml_type type=type_traits_cpu[x->dst->src[0]->type].vec_dot_type;
 const float*src=(const float*)((const char*)y->data+source);void*dst=(char*)x->params->wdata+target;
 if(type==GGML_TYPE_Q8_K)fg139_quantize(1,src,dst,values);
 else if(type==GGML_TYPE_Q8_0)fg139_quantize(0,src,dst,values);
 else type_traits_cpu[type].from_float(src,dst,values);
}
void fg140_reset(int64_t c,int64_t expert,int32_t value){struct fg140_context*x=fg140_context_get(c);GGML_ASSERT(expert>=0&&expert<x->plan->experts);atomic_store_explicit((atomic_int*)(x->chunks+expert),value,memory_order_seq_cst);}
void fg140_barrier(int64_t c){struct fg140_context*x=fg140_context_get(c);ggml_barrier(x->params->threadpool);}
void fg140_ready(int64_t c,int64_t expert){struct fg140_context*x=fg140_context_get(c);if(g_expert_ready_hook)g_expert_ready_hook(x->dst->src[0],expert,g_expert_ready_hook_data);}
uint8_t fg140_tiled(int64_t c,int64_t expert,int64_t rows){struct fg140_context*x=fg140_context_get(c);const struct ggml_tensor*ids=x->dst->src[2];return ggml_compute_forward_mul_mat_id_tiled(x->params,x->dst,expert,rows,(const int32_t*)(x->rows+expert*ids->ne[0]*ids->ne[1]),x->scratch);}
void fg140_chunk(int64_t c,int64_t expert,int64_t first,int64_t last,int64_t cfirst,int64_t clast,int64_t fused){
 struct fg140_context*x=fg140_context_get(c);GGML_ASSERT(expert>=0&&expert<x->plan->experts);
 const struct ggml_tensor*w=x->dst->src[0],*y=x->dst->src[1],*ids=x->dst->src[2];const enum ggml_type vtype=type_traits_cpu[w->type].vec_dot_type;
 const char*weights=(const char*)w->data+expert*w->nb[2];const void*input=y->type==vtype?y->data:x->params->wdata;const size_t row_size=ggml_row_size(vtype,y->ne[0]);
 if(fused){
#if defined(__AVX2__) && defined(__FMA__)
  GGML_ASSERT(cfirst==0&&clast>=2&&clast<=3);ggml_compute_forward_mul_mat_id_iq2s_one_chunk_e46(x->dst,w,y,ids,expert,first,last,weights,x->rows,clast,row_size,input);
#else
  GGML_ABORT("IQ2_S verification fusion unavailable");
#endif
 }else ggml_compute_forward_mul_mat_id_one_chunk(x->dst,w,y,ids,expert,first,last,cfirst,clast,weights,x->rows,row_size,ggml_is_contiguous(y),input);
}
void fg140_matrix(int64_t c,int64_t weight,int64_t input,int64_t output,int64_t width,int64_t rows){GGML_UNUSED(c);GGML_UNUSED(weight);GGML_UNUSED(input);GGML_UNUSED(output);GGML_UNUSED(width);GGML_UNUSED(rows);GGML_ABORT("Apex-specific matrix service not used by Qwen controller");}
int32_t fg140_claim(int64_t c,int64_t expert){struct fg140_context*x=fg140_context_get(c);GGML_ASSERT(expert>=0&&expert<x->plan->experts);return atomic_fetch_add_explicit((atomic_int*)(x->chunks+expert),1,memory_order_relaxed);}
void fg140_iq_stats(int64_t c,int64_t index){GGML_UNUSED(c);GGML_ASSERT(index>=0&&index<15);iq2s_fused_stats_count((int)index,1);}
void fg140_witness(int64_t c,int64_t kind,int64_t value){
 GGML_UNUSED(c);
#if FG140_TRACE
 GGML_ASSERT(kind>=0&&kind<7&&value>=0);atomic_fetch_add_explicit(fg140_events+kind,(uint64_t)value,memory_order_relaxed);
#else
 GGML_UNUSED(kind);GGML_UNUSED(value);
#endif
}
#if FG140_TRACE
__attribute__((destructor))static void fg140_report(void){fprintf(stderr,"FG140_CONTROL entries=%llu quant_blocks=%llu grouped_routes=%llu chunks=%llu fused_chunks=%llu tiled_delegations=%llu layout_bytes=%llu enabled=%d apex_q4_calls=%llu apex_q6_calls=%llu\n",(unsigned long long)atomic_load(fg140_events),(unsigned long long)atomic_load(fg140_events+1),(unsigned long long)atomic_load(fg140_events+2),(unsigned long long)atomic_load(fg140_events+3),(unsigned long long)atomic_load(fg140_events+4),(unsigned long long)atomic_load(fg140_events+5),(unsigned long long)atomic_load(fg140_events+6),fg140_enabled,(unsigned long long)atomic_load(&fg140_q4_calls),(unsigned long long)atomic_load(&fg140_q6_calls));}
#endif
void fg140_previous_cold(const struct ggml_compute_params*,struct ggml_tensor*);
static bool fg140_dispatch(const struct ggml_compute_params*params,struct ggml_tensor*dst,int mode){
 const struct ggml_tensor*w=dst->src[0],*y=dst->src[1],*ids=dst->src[2],*mask=mode?dst->src[3]:NULL,*lut=mode==2?dst->src[4]:NULL;
 if(!fg140_enabled||params->use_ref||w->extra!=NULL||w->ne[0]<=0||w->ne[1]<=0||w->ne[2]<=0||ids->type!=GGML_TYPE_I32||ids->nb[0]%4||ids->nb[1]%4)return false;
 const enum ggml_type vec=type_traits_cpu[w->type].vec_dot_type;if(y->type!=vec&&y->type!=GGML_TYPE_F32)return false;
 GGML_ASSERT(w->nb[0]==ggml_type_size(w->type)&&y->nb[0]==ggml_type_size(y->type));
 GGML_ASSERT(dst->nb[0]==4&&dst->nb[0]<=dst->nb[1]&&dst->nb[1]<=dst->nb[2]&&dst->nb[2]<=dst->nb[3]);
 if(mode)GGML_ASSERT(mask&&mask->type==GGML_TYPE_I32&&ggml_is_contiguous(mask));
 if(mode==2)GGML_ASSERT(lut&&lut->type==GGML_TYPE_I32&&ggml_is_contiguous(lut));
 struct fg140_plan p={0};p.mode=mode;p.ith=params->ith;p.nth=params->nth;p.experts=w->ne[2];p.real_experts=mode?ggml_nelements(mask):w->ne[2];p.ids_per_token=ids->ne[0];p.tokens=ids->ne[1];p.need_quant=y->type!=vec;p.block_size=ggml_blck_size(vec);p.block_bytes=ggml_type_size(vec);p.ne10=y->ne[0];p.ne11=y->ne[1];p.ne12=y->ne[2];p.ne13=y->ne[3];p.nb10=y->nb[0];p.nb11=y->nb[1];p.nb12=y->nb[2];p.nb13=y->nb[3];p.output_rows=w->ne[1];p.row_size=ggml_row_size(vec,y->ne[0]);p.ids_stride0=ids->nb[0]/4;p.ids_stride1=ids->nb[1]/4;p.source_contiguous=ggml_is_contiguous(y);p.is_iq2s=w->type==GGML_TYPE_IQ2_S;p.disable_chunking=ggml_is_numa();p.tiled_bytes=mode?0:ggml_tiled_wdata_size(params->nth,dst);p.cache_line=CACHE_LINE_SIZE;p.wdata_mod=(uintptr_t)params->wdata%(CACHE_LINE_SIZE>64?CACHE_LINE_SIZE:64);p.wdata_bytes=params->wsize;p.dst_bytes=ggml_nbytes(dst);p.apex_math=0;p.weight_row_bytes=ggml_row_size(w->type,w->ne[0]);p.nb01=w->nb[1];p.nb02=w->nb[2];p.dst_nb1=dst->nb[1];p.dst_nb2=dst->nb[2];
#if defined(__AVX2__) && defined(__FMA__)
 p.fused_available=iq2s_fused_mode()==1&&atomic_load_explicit(&g_iq2s_fused_window,memory_order_relaxed);
#endif
 fg140_entry(&p,0,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0);
 struct fg140_context context={params,dst,&p,(int64_t*)((char*)params->wdata+p.counts_offset),(struct mmid_row_mapping*)((char*)params->wdata+p.rows_offset),(void*)((char*)params->wdata+p.chunks_offset),(char*)params->wdata+p.scratch_offset};p.context=(int64_t)(uintptr_t)&context;
 fg140_entry(&p,1,context.counts,p.experts,(struct fg140_row*)context.rows,p.experts*p.ids_per_token*p.tokens,ids->data,ggml_nbytes(ids)/4,mode?mask->data:NULL,mode?ggml_nbytes(mask)/4:0,mode==2?lut->data:NULL,mode==2?ggml_nbytes(lut)/4:0,dst->data,ggml_nbytes(dst));return true;
}
static void ggml_compute_forward_mul_mat_id(const struct ggml_compute_params*params,struct ggml_tensor*dst){if(!fg140_dispatch(params,dst,0))fg140_previous_mul_mat_id(params,dst);}
void ggml_compute_forward_mul_mat_id_cold(const struct ggml_compute_params*params,struct ggml_tensor*dst){if(!fg140_dispatch(params,dst,dst->src[4]?2:1))fg140_previous_cold(params,dst);}
