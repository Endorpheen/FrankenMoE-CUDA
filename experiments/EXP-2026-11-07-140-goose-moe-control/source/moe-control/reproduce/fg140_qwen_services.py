from pathlib import Path
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');text=(r/'source/moe-control/apex-services.inc').read_text()
def body(start,end,new):
 global text
 a=text.index(start);b=text.index(end,a);text=text[:a]+new+'\n'+text[b:]
text=text.replace('if(type==GGML_TYPE_Q8_K)fg_selfhost_quant_q8k(src,dst,values);\n else if(type==GGML_TYPE_Q8_0)fg139_quantize_q8_0(src,dst,values);','if(type==GGML_TYPE_Q8_K)fg139_quantize(1,src,dst,values);\n else if(type==GGML_TYPE_Q8_0)fg139_quantize(0,src,dst,values);')
body('void fg140_ready(', 'uint8_t fg140_tiled(','''void fg140_ready(int64_t c,int64_t expert){struct fg140_context*x=fg140_context_get(c);if(g_expert_ready_hook)g_expert_ready_hook(x->dst->src[0],expert,g_expert_ready_hook_data);}''')
body('uint8_t fg140_tiled(', 'void fg140_chunk(','''uint8_t fg140_tiled(int64_t c,int64_t expert,int64_t rows){struct fg140_context*x=fg140_context_get(c);const struct ggml_tensor*ids=x->dst->src[2];return ggml_compute_forward_mul_mat_id_tiled(x->params,x->dst,expert,rows,(const int32_t*)(x->rows+expert*ids->ne[0]*ids->ne[1]),x->scratch);}''')
body('void fg140_chunk(', 'void fg140_matrix(','''void fg140_chunk(int64_t c,int64_t expert,int64_t first,int64_t last,int64_t cfirst,int64_t clast,int64_t fused){
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
}''')
body('void fg140_matrix(', 'int32_t fg140_claim(','''void fg140_matrix(int64_t c,int64_t weight,int64_t input,int64_t output,int64_t width,int64_t rows){GGML_UNUSED(c);GGML_UNUSED(weight);GGML_UNUSED(input);GGML_UNUSED(output);GGML_UNUSED(width);GGML_UNUSED(rows);GGML_ABORT("Apex-specific matrix service not used by Qwen controller");}''')
body('void fg140_iq_stats(', 'void fg140_witness(','''void fg140_iq_stats(int64_t c,int64_t index){GGML_UNUSED(c);GGML_ASSERT(index>=0&&index<15);iq2s_fused_stats_count((int)index,1);}''')
a=text.index('static bool fg140_dispatch(');text=text[:a]+'''void fg140_previous_cold(const struct ggml_compute_params*,struct ggml_tensor*);
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
'''
Path('/tmp/fg140_qwen_services.inc').write_text(text)
