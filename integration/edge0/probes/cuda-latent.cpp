// A minimal numerical probe, not a full model or throughput benchmark.
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-cuda.h"
#include "ggml-backend.h"
#include "ggml-alloc.h"
extern "C" {
#include "runtime.h"
}
#include <vector>
#include <array>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <string>
static void require(bool ok,const char * s){if(!ok)throw std::runtime_error(s);}
static int64_t h(const void * p){return(int64_t)(intptr_t)p;}
static void read(const std::string & p,int64_t off,void * out,size_t n){std::ifstream f(p,std::ios::binary);require((bool)f,"open");f.seekg(off);f.read((char*)out,n);require(f.gcount()==(std::streamsize)n,"read");}
int main(int argc,char **argv){try{
 require(argc==2,"pack path");std::string pack=argv[1];goose_init(0,nullptr);
 auto * gpu=ggml_backend_cuda_init(0);require(gpu,"CUDA init");std::vector<uint8_t>labels(98304);read(pack+"/labels.bin",0,labels.data(),labels.size());
 for(int layer:{0,47})for(int group:{0,3,7}){
  int expert=-1;for(int i=0;i<512;++i){int32_t g;std::memcpy(&g,labels.data()+(layer*512+i)*4,4);if(g==group){expert=i;break;}}require(expert>=0,"expert group");
  auto * c=ggml_init({64*1024*1024,nullptr,false});auto * d=ggml_init({2*1024*1024,nullptr,true});require(c&&d,"contexts");
  ggml_tensor * w[5],*gw[5];const int64_t group_bytes=1843200,base=(layer*512LL+expert)*1164800;
  for(int i=0;i<5;++i){
   auto ty=i<2?GGML_TYPE_Q4_0:(i<4?GGML_TYPE_Q3_K:GGML_TYPE_IQ4_NL);int64_t n0=i==0?2560:(i==4?640:1280),n1=i==1?2560:(i>=2&&i<4?640:1280);
   w[i]=ggml_new_tensor_2d(c,ty,n0,n1);gw[i]=ggml_new_tensor_2d(d,ty,n0,n1);
   read(pack+(i<2?"/latent_dense_q4.bin":"/experts.bin"),i<2?(layer*16LL+i*8+group)*group_bytes:base+(i==2?0:(i==3?352000:704000)),w[i]->data,ggml_nbytes(w[i]));
  }
  float scales[1280];read(pack+"/scales/l"+std::to_string(layer)+".f32",0,scales,sizeof(scales));auto * inv=ggml_new_tensor_1d(c,GGML_TYPE_F32,1280);auto * gi=ggml_new_tensor_1d(d,GGML_TYPE_F32,1280);
  for(int i=0;i<1280;++i)((float*)inv->data)[i]=1.0f/scales[i];auto * x=ggml_new_tensor_2d(c,GGML_TYPE_F32,2560,1);auto * gx=ggml_new_tensor_2d(d,GGML_TYPE_F32,2560,1);
  for(int i=0;i<2560;++i)((float*)x->data)[i]=((i*17+layer*7)%47-23)/64.0f;
  ggml_tensor * ref[8];ref[0]=ggml_mul_mat(c,w[0],x);ref[1]=ggml_mul(c,ref[0],inv);ref[2]=ggml_mul_mat(c,w[2],ref[1]);ref[3]=ggml_mul_mat(c,w[3],ref[1]);ref[4]=ggml_silu(c,ref[2]);ref[5]=ggml_mul(c,ref[4],ref[3]);ref[6]=ggml_mul_mat(c,w[4],ref[5]);ref[7]=ggml_mul_mat(c,w[1],ref[6]);
  auto * cg=ggml_new_graph(c);ggml_build_forward_expand(cg,ref[7]);require(ggml_graph_compute_with_ctx(c,cg,4)==GGML_STATUS_SUCCESS,"CPU reference");
  int64_t wh[5],work[16],nodes[8];for(int i=0;i<5;++i)wh[i]=h(gw[i]);require(e0_latent_expert(h(d),h(gx),h(gi),{wh,5},{work,16},{nodes,8})!=0,"Goose graph");
  auto * graph=ggml_new_graph(d);for(int i=0;i<8;++i)ggml_set_output((ggml_tensor*)(intptr_t)nodes[i]);ggml_build_forward_expand(graph,(ggml_tensor*)(intptr_t)nodes[7]);
  auto * buf=ggml_backend_alloc_ctx_tensors(d,gpu);require(buf,"CUDA allocation");for(int i=0;i<5;++i)ggml_backend_tensor_set(gw[i],w[i]->data,0,ggml_nbytes(w[i]));ggml_backend_tensor_set(gi,inv->data,0,5120);ggml_backend_tensor_set(gx,x->data,0,10240);
  require(ggml_backend_graph_compute(gpu,graph)==GGML_STATUS_SUCCESS,"CUDA compute");bool pass=true;
  for(int stage=0;stage<8;++stage){auto * t=(ggml_tensor*)(intptr_t)nodes[stage];std::vector<float>out(ggml_nelements(t));ggml_backend_tensor_get(t,out.data(),0,ggml_nbytes(t));float max_abs=0,peak=0;int bad=0;
   for(size_t i=0;i<out.size();++i){float a=((float*)ref[stage]->data)[i],b=out[i];require(std::isfinite(b),"CUDA nonfinite");float e=std::fabs(a-b);max_abs=std::max(max_abs,e);peak=std::max(peak,std::fabs(b));if(e>2e-5f+2e-5f*std::fabs(a))++bad;}
   std::printf("CASE layer=%d group=%d expert=%d stage=%d values=%zu max_abs=%.9g peak=%.9g outside_final_tolerance=%d\n",layer,group,expert,stage,out.size(),max_abs,peak,bad);if(stage==7)pass=bad==0&&peak>0;
  }
  ggml_backend_buffer_free(buf);ggml_free(d);ggml_free(c);if(!pass){std::puts("RESULT REJECTED: CUDA final output fails the fixed existing tolerance");ggml_backend_free(gpu);return 3;}
 }
 ggml_backend_free(gpu);std::puts("RESULT PASS: minimal CUDA latent final boundary, model gate still required");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
