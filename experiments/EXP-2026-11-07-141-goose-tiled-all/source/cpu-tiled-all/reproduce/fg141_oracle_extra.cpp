extern "C" void fg141_oracle_y(int kind,const void*data,int64_t stride,const void*mapping,int64_t first,int count,int slots,int start,int nk,void*workspace){
 auto*w=(tiled_ws*)workspace;const auto*ids=(const int32_t*)mapping;const block_q8_K*rows[256];
 for(int i=0;i<count;++i){int64_t slot=ids[2*(first+i)]%slots,token=ids[2*(first+i)+1];rows[i]=(const block_q8_K*)data+(slot+token*slots)*stride;}
 tiled_unpack_src1_q8_K(rows,count,&w->src1,start,nk);bool bias=kind==2||kind==3||kind==6||kind==7||kind==8||kind==9||kind==10||kind==11||kind==12;
 if(nk==1){for(int row=0;row<count;row+=16)tiled_repack_src1(&w->src1,row,nk,bias);}else if(count)tiled_repack_src1(&w->src1,0,nk,bias);
}
extern "C" void fg141_oracle_micro(int kind,void*workspace,int row,int col,int out,int nk,int slab){
 auto*w=(tiled_ws*)workspace;float*buffer=w->acc+(out-row)*256;
 switch(kind){
 case 0:case 1:tiled_run_microtile<32,true,0,false>(w->src0,w->src1,row,col,nk,slab,buffer,256);break;
 case 2:tiled_run_microtile<16,false,32,true>(w->src0,w->src1,row,col,nk,slab,buffer,256);break;
 case 3:tiled_run_microtile<16,false,4,true>(w->src0,w->src1,row,col,nk,slab,buffer,256);break;
 case 4:tiled_run_microtile<16,true,0,false>(w->src0,w->src1,row,col,nk,slab,buffer,256);break;
 case 5:tiled_run_microtile<32,false,128,false>(w->src0,w->src1,row,col,nk,slab,buffer,256);break;
 case 6:case 9:case 10:case 11:tiled_run_microtile<32,false,128,true>(w->src0,w->src1,row,col,nk,slab,buffer,256);break;
 case 7:case 8:case 12:tiled_run_microtile<16,false,128,true>(w->src0,w->src1,row,col,nk,slab,buffer,256);break;
 default:abort();
 }}
extern "C" {
#include "/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/cpu-tiled-all/bridge.h"
}
extern "C" void fg141_oracle_moe(const struct fg141_plan*p,const void*data,const void*activations,const void*routes,void*scratch,float*output){
 static const enum ggml_type types[]={GGML_TYPE_Q4_K,GGML_TYPE_Q5_K,GGML_TYPE_Q6_K,GGML_TYPE_Q3_K,GGML_TYPE_Q2_K,GGML_TYPE_IQ4_XS,GGML_TYPE_IQ2_XXS,GGML_TYPE_IQ2_XS,GGML_TYPE_IQ2_S,GGML_TYPE_IQ3_XXS,GGML_TYPE_IQ3_S,GGML_TYPE_IQ1_S,GGML_TYPE_IQ1_M};
 struct ggml_tensor x={},y={},dst={};x.type=types[p->kind];x.data=(char*)data+p->origin;y.type=GGML_TYPE_Q8_K;y.data=(void*)activations;dst.type=GGML_TYPE_F32;dst.data=output;dst.src[0]=&x;dst.src[1]=&y;
 x.ne[0]=p->width;x.ne[1]=p->output_rows;x.ne[2]=x.ne[3]=1;x.nb[0]=ggml_type_size(x.type);x.nb[1]=p->stride;x.nb[2]=x.nb[3]=p->stride*p->output_rows;
 y.ne[0]=p->width;y.ne[1]=p->slots;y.ne[2]=p->n12;y.ne[3]=1;y.nb[0]=sizeof(block_q8_K);y.nb[1]=p->y_stride*sizeof(block_q8_K);y.nb[2]=y.nb[1]*p->slots;y.nb[3]=y.nb[2]*p->n12;
 dst.ne[0]=p->output_rows;dst.ne[1]=p->n11;dst.ne[2]=p->n12;dst.ne[3]=1;dst.nb[0]=4;dst.nb[1]=p->out_stride1*4;dst.nb[2]=p->out_stride2*4;dst.nb[3]=dst.nb[2]*p->n12;
 struct ggml_compute_params params={};params.ith=(int)p->ith;params.nth=(int)p->nth;
 switch(p->kind){
 case 0:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_q4_K,32,true,0,false>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 1:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_q5_K,32,true,0,false>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 2:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_q6_K,16,false,32,true>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 3:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_q3_K,16,false,4,true>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 4:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_q2_K,16,true,0,false>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 5:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_iq4_xs,32,false,128,false>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 6:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_iq2_xxs,32,false,128,true>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 7:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_iq2_xs,16,false,128,true>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 8:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_iq2_s,16,false,128,true>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 9:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_iq3_xxs,32,false,128,true>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 10:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_iq3_s,32,false,128,true>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 11:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_iq1_s,32,false,128,true>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 case 12:ggml_compute_forward_mul_mat_id_tiled_one_expert<block_iq1_m,16,false,128,true>(&params,&dst,0,p->count,(const int32_t*)routes,(char*)scratch);break;
 default:abort();
 }}
