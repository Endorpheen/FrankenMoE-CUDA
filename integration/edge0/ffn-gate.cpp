// Independent native GGML reference for the actual routed/shared callback path.
#include "pack.h"
#include "gguf.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include "ggml-alloc.h"
extern "C" {
#include "runtime.h"
}
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
static void require(bool b,const char * s) { if(!b) throw std::runtime_error(s); }
static void readat(int fd,void * dst,size_t n,int64_t off) { require(pread(fd,dst,n,off)==(ssize_t)n,"read"); }
static void compute(ggml_context * c,ggml_tensor * t) { auto * g=ggml_new_graph_custom(c,4096,false);ggml_build_forward_expand(g,t);require(ggml_graph_compute_with_ctx(c,g,4)==GGML_STATUS_SUCCESS,"compute"); }
static ggml_tensor * tensor(ggml_context * c,ggml_type ty,int64_t n0,int64_t n1,int fd,int64_t off) {
 auto * t=ggml_new_tensor_2d(c,ty,n0,n1); readat(fd,t->data,ggml_nbytes(t),off); return t;
}
int main(int argc,char ** argv) { try {
 require(argc==2,"pack path"); std::string path=argv[1];auto pack=franken_edge0_open(path);
 ggml_context * meta=nullptr;auto * info=gguf_init_from_file((path+"/model-skeleton.gguf").c_str(),{true,&meta});require(info,"GGUF metadata");
 int sf=open((path+"/model-skeleton.gguf").c_str(),O_RDONLY),ef=open((path+"/experts.bin").c_str(),O_RDONLY),df=open((path+"/latent_dense_q4.bin").c_str(),O_RDONLY);
 int lf=open((path+"/labels.bin").c_str(),O_RDONLY);int32_t labels[48*512];readat(lf,labels,sizeof(labels),0);close(lf);
 for(int layer:{0,47}) {
  auto * c=ggml_init({128*1024*1024,nullptr,false});require(c,"context");
  std::array<ggml_tensor *,5>w;std::array<ggml_backend_buffer_t,5> buffers;
  const char * suffix[]={"ffn_gate_inp.weight","ffn_gate_shexp.weight","ffn_up_shexp.weight","ffn_down_shexp.weight","ffn_gate_inp_shexp.weight"};
  for(int i=0;i<5;++i) {
    std::string name="blk."+std::to_string(layer)+"."+suffix[i];auto * mt=ggml_get_tensor(meta,name.c_str());require(mt,"dense weight metadata");
    w[i]=ggml_new_tensor_2d(c,mt->type,mt->ne[0],mt->ne[1]);readat(sf,w[i]->data,ggml_nbytes(w[i]),gguf_get_data_offset(info)+gguf_get_tensor_offset(info,gguf_find_tensor(info,name.c_str())));
    buffers[i]=ggml_backend_buft_alloc_buffer(ggml_backend_cpu_buffer_type(),ggml_nbytes(w[i]));
    require(buffers[i],"CPU weight buffer");void * data=ggml_backend_buffer_get_base(buffers[i]);std::memcpy(data,w[i]->data,ggml_nbytes(w[i]));w[i]->data=data;w[i]->buffer=buffers[i];
  }
  franken_edge0_bind(*pack,layer,w[0],w[1],w[2],w[3],w[4]);
  auto * x=ggml_new_tensor_2d(c,GGML_TYPE_F32,2560,1);for(int i=0;i<2560;++i)((float*)x->data)[i]=((i*17+layer*7)%47-23)/64.0f;
  auto * logits=ggml_mul_mat(c,w[0],x);auto * probs=ggml_soft_max(c,logits);auto * ids=ggml_argsort_top_k(c,probs,10);compute(c,ids);
  int64_t gi[10];float gw[10],gp[512];require(moe_route({(float*)logits->data,512},10,1,1.0f,{gi,10},{gw,10},{gp,512})==10,"Goose route");
  float rw[10];double sum=0;for(int k=0;k<10;++k) { require(gi[k]==((int32_t*)ids->data)[k],"route IDs differ");rw[k]=((float*)probs->data)[gi[k]];sum+=rw[k]; }
  for(int k=0;k<10;++k) {rw[k]=(float)(rw[k]/std::max(sum,1.0/16384));require(std::fabs(rw[k]-gw[k])<=2e-6f,"route weights differ");}
  int sc=open((path+"/scales/l"+std::to_string(layer)+".f32").c_str(),O_RDONLY);float scales[1280];readat(sc,scales,sizeof(scales),0);close(sc);
  auto * inv=ggml_new_tensor_1d(c,GGML_TYPE_F32,1280);for(int i=0;i<1280;++i)((float*)inv->data)[i]=1.0f/scales[i];
  ggml_tensor * result=nullptr;
  for(int k=0;k<10;++k) {
    int expert=((int32_t*)ids->data)[k],group=labels[layer*512+expert];int64_t gb=1843200,base=(layer*512LL+expert)*1164800;
    auto * d=tensor(c,GGML_TYPE_Q4_0,2560,1280,df,(layer*16LL+group)*gb);
    auto * u=tensor(c,GGML_TYPE_Q4_0,1280,2560,df,(layer*16LL+8+group)*gb);
    auto * gate=tensor(c,GGML_TYPE_Q3_K,1280,640,ef,base);auto * up=tensor(c,GGML_TYPE_Q3_K,1280,640,ef,base+352000);
    auto * down=tensor(c,GGML_TYPE_IQ4_NL,640,1280,ef,base+704000);
    auto * latent=ggml_mul(c,ggml_mul_mat(c,d,x),inv);
    auto * branch=ggml_mul_mat(c,u,ggml_mul_mat(c,down,ggml_mul(c,ggml_silu(c,ggml_mul_mat(c,gate,latent)),ggml_mul_mat(c,up,latent))));
    auto * coefficient=ggml_new_tensor_1d(c,GGML_TYPE_F32,1);*(float*)coefficient->data=rw[k];branch=ggml_mul(c,branch,coefficient);
    result=result?ggml_add(c,result,branch):branch;
  }
  auto * shared=ggml_mul_mat(c,w[3],ggml_mul(c,ggml_silu(c,ggml_mul_mat(c,w[1],x)),ggml_mul_mat(c,w[2],x)));
  shared=ggml_mul(c,shared,ggml_sigmoid(c,ggml_mul_mat(c,w[4],x)));result=ggml_add(c,result,shared);compute(c,result);
  auto * candidate=franken_edge0_ffn(*pack,c,x,layer);compute(c,candidate);
  float max_error=0,peak=0;for(int i=0;i<2560;++i) {float a=((float*)candidate->data)[i],b=((float*)result->data)[i];require(std::isfinite(a)&&std::isfinite(b),"finite");max_error=std::max(max_error,std::fabs(a-b));peak=std::max(peak,std::fabs(a));require(std::fabs(a-b)<=2e-5f+2e-5f*std::fabs(b),"full FFN mismatch");}
  require(peak>0,"nonzero");std::printf("PASS full FFN layer=%d routed=10 shared=1 values=2560 max_abs_error=%.9g peak=%.9g\n",layer,max_error,peak);
  for(auto b:buffers)ggml_backend_buffer_free(b);ggml_free(c);
 }
 close(sf);close(ef);close(df);gguf_free(info);ggml_free(meta);std::puts("RESULT PASS: actual callback + Goose latent/top10/shared integration");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
