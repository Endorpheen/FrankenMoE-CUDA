from pathlib import Path
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=(r/'source/moe-quantized/qwen-graph-fixture.c').read_text();s=s.replace('GGML_TYPE_IQ2_S};','GGML_TYPE_IQ2_S,GGML_TYPE_Q4_K,GGML_TYPE_Q6_K,GGML_TYPE_F32,GGML_TYPE_F16,GGML_TYPE_BF16};');s=s.replace('static uint64_t cases,values;','static uint64_t cases,values;\nstatic int padded_ids,prequant_input,window_override=-1,mask_style;')
needle=' size_t bytes=ggml_nbytes(t),block=ggml_type_size(t->type);'
replacement=''' if(t->type==GGML_TYPE_Q4_K||t->type==GGML_TYPE_Q6_K||t->type==GGML_TYPE_F32||t->type==GGML_TYPE_F16||t->type==GGML_TYPE_BF16){
  int64_t n=ggml_nelements(t);float*source=malloc((size_t)n*4);if(!source)abort();for(int64_t i=0;i<n;++i)source[i]=((int32_t)(next()>>8)-8388608)*0.0000003f;
  size_t got=ggml_quantize_chunk(t->type,source,t->data,0,t->ne[1]*t->ne[2],t->ne[0],NULL);if(got!=ggml_nbytes(t))abort();free(source);return;
 }
'''+needle
assert needle in s;s=s.replace(needle,replacement)
s=s.replace('int width=kind<3?640:2560,outputs[]=', 'int width=kind==7?33:kind>=8?64:kind<3?640:2560,outputs[]=')
old=' struct ggml_tensor*ids=ggml_new_tensor_2d(ctx,GGML_TYPE_I32,4,tokens);for(int t=0;t<tokens;++t)for(int j=0;j<4;++j)((int32_t*)ids->data)[t*4+j]=j;'
new=''' if(prequant_input){const struct ggml_type_traits_cpu*traits=ggml_get_type_traits_cpu(w->type);enum ggml_type target=traits->vec_dot_type;if(target!=GGML_TYPE_F32){
  struct ggml_tensor*q=ggml_new_tensor_3d(ctx,target,width,slots,tokens);ggml_from_float_t convert=ggml_get_type_traits_cpu(target)->from_float;if(!convert)abort();
  for(int t=0;t<tokens;++t)for(int j=0;j<slots;++j)convert((const float*)((char*)x->data+t*x->nb[2]+j*x->nb[1]),(char*)q->data+t*q->nb[2]+j*q->nb[1],width);x=q;
 }}
 struct ggml_tensor*ids_storage=ggml_new_tensor_2d(ctx,GGML_TYPE_I32,padded_ids?8:4,tokens);memset(ids_storage->data,0x5a,ggml_nbytes(ids_storage));
 struct ggml_tensor*ids=padded_ids?ggml_view_2d(ctx,ids_storage,4,tokens,ids_storage->nb[1],0):ids_storage;
 for(int t=0;t<tokens;++t)for(int j=0;j<4;++j)*(int32_t*)((char*)ids->data+t*ids->nb[1]+j*ids->nb[0])=j;'''
assert old in s;s=s.replace(old,new)
s=s.replace('int32_t m[]={1,1,0,1,0};memcpy(mask->data,m,sizeof(m));','int32_t m[]={1,1,0,1,0};if(mask_style)for(int i=0;i<5;++i)m[i]=mask_style==1?0:1;memcpy(mask->data,m,sizeof(m));')
s=s.replace('int32_t l[]={2,1,-1,0,-1};memcpy(lut->data,l,sizeof(l));','int32_t l[]={2,1,-1,0,-1};if(mask_style==2)for(int i=0;i<5;++i)l[i]=0;memcpy(lut->data,l,sizeof(l));')
s=s.replace('int window=kind==4&&!mode&&(tokens==2||tokens==3);','int window=window_override>=0?window_override:kind==4&&!mode&&(tokens==2||tokens==3);')
old=' fclose(f);for(int i=0;i<3;++i)ggml_threadpool_free(pools[i]);'
new=''' window_override=0;
 for(int kind=5;kind<10;++kind)for(int mode=0;mode<3;++mode)for(int q=0;q<2;++q)for(int pad=0;pad<2;++pad){prequant_input=q;padded_ids=pad;moe(f,kind,7,2,mode,sequence++);}
 padded_ids=1;
 for(int kind=0;kind<5;++kind)for(int mode=0;mode<3;++mode)for(int q=0;q<2;++q){prequant_input=q;moe(f,kind,7,2,mode,sequence++);}
 prequant_input=1;
 for(int mode=0;mode<3;++mode)for(int window=0;window<2;++window)for(int tok=2;tok<=3;++tok){window_override=window;moe(f,4,tok,2,mode,sequence++);}
 window_override=0;
 for(int kind=0;kind<5;++kind)for(int mode=1;mode<3;++mode)for(int style=1;style<=2;++style){mask_style=style;moe(f,kind,7,2,mode,sequence++);}
'''+old
assert old in s;s=s.replace(old,new);(r/'source/moe-control/qwen-fixture-expanded.c').write_text(s)
