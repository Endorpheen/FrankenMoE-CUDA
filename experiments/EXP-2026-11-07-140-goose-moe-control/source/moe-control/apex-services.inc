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
 if(type==GGML_TYPE_Q8_K)fg_selfhost_quant_q8k(src,dst,values);
 else if(type==GGML_TYPE_Q8_0)fg139_quantize_q8_0(src,dst,values);
 else type_traits_cpu[type].from_float(src,dst,values);
}
void fg140_reset(int64_t c,int64_t expert,int32_t value){struct fg140_context*x=fg140_context_get(c);GGML_ASSERT(expert>=0&&expert<x->plan->experts);atomic_store_explicit((atomic_int*)(x->chunks+expert),value,memory_order_seq_cst);}
void fg140_barrier(int64_t c){struct fg140_context*x=fg140_context_get(c);ggml_barrier(x->params->threadpool);}
void fg140_ready(int64_t c,int64_t expert){GGML_UNUSED(c);GGML_UNUSED(expert);}
uint8_t fg140_tiled(int64_t c,int64_t expert,int64_t rows){struct fg140_context*x=fg140_context_get(c);struct fg_moe_context old={x->params,x->dst,x->counts,x->rows,x->chunks,x->scratch};return fg_moe_try_tiled((int64_t)(uintptr_t)&old,expert,rows);}
void fg140_chunk(int64_t c,int64_t expert,int64_t first,int64_t last,int64_t cfirst,int64_t clast,int64_t fused){
 struct fg140_context*x=fg140_context_get(c);GGML_ASSERT(!fused&&expert>=0&&expert<x->plan->experts);
 const struct ggml_tensor*w=x->dst->src[0],*y=x->dst->src[1],*ids=x->dst->src[2];
 const enum ggml_type vtype=type_traits_cpu[w->type].vec_dot_type;
 ggml_compute_forward_mul_mat_id_one_chunk(x->dst,w,y,ids,expert,first,last,cfirst,clast,(const char*)w->data+expert*w->nb[2],x->rows,ggml_row_size(vtype,y->ne[0]),ggml_is_contiguous(y),y->type==vtype?y->data:x->params->wdata);
}
void fg140_matrix(int64_t c,int64_t weight,int64_t input,int64_t output,int64_t width,int64_t rows){
 struct fg140_context*x=fg140_context_get(c);const struct ggml_tensor*w=x->dst->src[0],*y=x->dst->src[1];struct fg140_plan*p=x->plan;
 GGML_ASSERT(weight>=0&&input>=0&&output>=0&&rows>0&&width==p->ne10);
 const size_t wbytes=(size_t)(rows-1)*w->nb[1]+(size_t)p->weight_row_bytes;
 const size_t ybytes=(size_t)p->row_size,obytes=(size_t)rows*4;
 GGML_ASSERT((uint64_t)weight<=ggml_nbytes(w)&&wbytes<=ggml_nbytes(w)-(uint64_t)weight);
 const void*activation=p->need_quant?x->params->wdata:y->data;
 const size_t capacity=p->need_quant?(size_t)p->row_size*p->ne11*p->ne12*p->ne13:ggml_nbytes(y);
 GGML_ASSERT((uint64_t)input<=capacity&&ybytes<=capacity-(uint64_t)input);
 GGML_ASSERT((uint64_t)output<=ggml_nbytes(x->dst)&&obytes<=ggml_nbytes(x->dst)-(uint64_t)output);
 struct fg_moe_context old={x->params,x->dst,x->counts,x->rows,x->chunks,x->scratch};
 fg_moe_matrix((int64_t)(uintptr_t)&old,(int64_t)(uintptr_t)((const char*)w->data+weight),(int64_t)(uintptr_t)((const char*)activation+input),(int64_t)(uintptr_t)((char*)x->dst->data+output),width,rows);
#if FG140_TRACE
 atomic_fetch_add_explicit(w->type==GGML_TYPE_Q4_K?&fg140_q4_calls:&fg140_q6_calls,1,memory_order_relaxed);
#endif
}
int32_t fg140_claim(int64_t c,int64_t expert){struct fg140_context*x=fg140_context_get(c);GGML_ASSERT(expert>=0&&expert<x->plan->experts);return atomic_fetch_add_explicit((atomic_int*)(x->chunks+expert),1,memory_order_relaxed);}
void fg140_iq_stats(int64_t c,int64_t index){GGML_UNUSED(c);GGML_UNUSED(index);}
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
static bool fg140_dispatch(const struct ggml_compute_params*params,struct ggml_tensor*dst){
 const struct ggml_tensor*w=dst->src[0],*y=dst->src[1],*ids=dst->src[2];
 if(!fg140_enabled||params->use_ref||w->extra!=NULL||w->ne[0]<=0||w->ne[1]<=0||w->ne[2]<=0||ids->type!=GGML_TYPE_I32||ids->nb[0]%4||ids->nb[1]%4)return false;
 const enum ggml_type vec=type_traits_cpu[w->type].vec_dot_type;
 if(y->type!=vec&&y->type!=GGML_TYPE_F32)return false;
 GGML_ASSERT(w->nb[0]==ggml_type_size(w->type)&&y->nb[0]==ggml_type_size(y->type));
 GGML_ASSERT(dst->nb[0]==4&&dst->nb[0]<=dst->nb[1]&&dst->nb[1]<=dst->nb[2]&&dst->nb[2]<=dst->nb[3]);
 struct fg140_plan p={0};p.ith=params->ith;p.nth=params->nth;p.experts=w->ne[2];p.real_experts=w->ne[2];p.ids_per_token=ids->ne[0];p.tokens=ids->ne[1];p.need_quant=y->type!=vec;p.block_size=ggml_blck_size(vec);p.block_bytes=ggml_type_size(vec);p.ne10=y->ne[0];p.ne11=y->ne[1];p.ne12=y->ne[2];p.ne13=y->ne[3];p.nb10=y->nb[0];p.nb11=y->nb[1];p.nb12=y->nb[2];p.nb13=y->nb[3];p.output_rows=w->ne[1];p.row_size=ggml_row_size(vec,y->ne[0]);p.ids_stride0=ids->nb[0]/4;p.ids_stride1=ids->nb[1]/4;p.source_contiguous=ggml_is_contiguous(y);p.disable_chunking=ggml_is_numa();p.tiled_bytes=ggml_tiled_wdata_size(params->nth,dst);p.cache_line=CACHE_LINE_SIZE;p.wdata_mod=(uintptr_t)params->wdata%(CACHE_LINE_SIZE>64?CACHE_LINE_SIZE:64);p.wdata_bytes=params->wsize;p.dst_bytes=ggml_nbytes(dst);p.apex_math=fg_moe_enabled&&(w->type==GGML_TYPE_Q4_K||w->type==GGML_TYPE_Q6_K);p.weight_row_bytes=ggml_row_size(w->type,w->ne[0]);p.nb01=w->nb[1];p.nb02=w->nb[2];p.dst_nb1=dst->nb[1];p.dst_nb2=dst->nb[2];
 fg140_entry(&p,0,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0);
 struct fg140_context context={params,dst,&p,(int64_t*)((char*)params->wdata+p.counts_offset),(struct mmid_row_mapping*)((char*)params->wdata+p.rows_offset),(void*)((char*)params->wdata+p.chunks_offset),(char*)params->wdata+p.scratch_offset};p.context=(int64_t)(uintptr_t)&context;
 fg140_entry(&p,1,context.counts,p.experts,(struct fg140_row*)context.rows,p.experts*p.ids_per_token*p.tokens,ids->data,ggml_nbytes(ids)/4,NULL,0,NULL,0,dst->data,ggml_nbytes(dst));return true;
}
static void ggml_compute_forward_mul_mat_id(const struct ggml_compute_params*params,struct ggml_tensor*dst){if(!fg140_dispatch(params,dst))fg140_previous_mul_mat_id(params,dst);}
