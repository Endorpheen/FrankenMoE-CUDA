// Real raw weights, exact CPU activation packing, fixed pre-existing tolerance.
#include "ggml.h"
#include "ggml-cpu.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <string>
#include <algorithm>
extern "C" int e0c_gemv(int,const void*,size_t,const void*,size_t,float*,int,int);
static void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
static void read(const std::string&p,int64_t off,void*out,size_t n){std::ifstream f(p,std::ios::binary);require((bool)f,"open");f.seekg(off);f.read((char*)out,n);require(f.gcount()==(std::streamsize)n,"read");}
int main(int argc,char**argv){try{
 require(argc==2,"pack argument");ggml_cpu_init();std::string pack=argv[1];std::vector<int32_t>labels(48*512);read(pack+"/labels.bin",0,labels.data(),labels.size()*4);int count=0;
 for(int layer:{0,47})for(int group:{0,3,7})for(int i=0;i<5;++i){
  int expert=-1;for(int e=0;e<512;++e)if(labels[layer*512+e]==group){expert=e;break;}require(expert>=0,"group");
  auto*c=ggml_init({64*1024*1024,nullptr,false});require(c,"context");auto ty=i<2?GGML_TYPE_Q4_0:(i<4?GGML_TYPE_Q3_K:GGML_TYPE_IQ4_NL);
  int n=i==0?2560:(i==4?640:1280),rows=i==1?2560:(i>=2&&i<4?640:1280);
  auto*w=ggml_new_tensor_2d(c,ty,n,rows);int64_t base=(layer*512LL+expert)*1164800;
  read(pack+(i<2?"/latent_dense_q4.bin":"/experts.bin"),i<2?(layer*16LL+i*8+group)*1843200:base+(i==2?0:(i==3?352000:704000)),w->data,ggml_nbytes(w));
  auto*x=ggml_new_tensor_2d(c,GGML_TYPE_F32,n,1);auto*y=ggml_mul_mat(c,w,x);auto*g=ggml_new_graph(c);ggml_build_forward_expand(g,y);
  auto at=ggml_get_type_traits_cpu(ty)->vec_dot_type;auto quant=ggml_get_type_traits_cpu(at)->from_float;require(quant,"CPU pack function");std::vector<uint8_t>act(ggml_row_size(at,n));std::vector<float>out(rows);
  for(float amp:{0.125f,1.0f,8.0f}){
   for(int j=0;j<n;++j)((float*)x->data)[j]=amp*((j*17+layer*7+group*3)%47-23)/64.0f;
   require(ggml_graph_compute_with_ctx(c,g,8)==GGML_STATUS_SUCCESS,"CPU compute");quant((float*)x->data,act.data(),n);
   int err=e0c_gemv(ty,w->data,ggml_nbytes(w),act.data(),act.size(),out.data(),n,rows);require(err==0,"CUDA compute");float max_abs=0,peak=0;int bad=0;
   for(int j=0;j<rows;++j){float a=((float*)y->data)[j],b=out[j];require(std::isfinite(b),"finite");max_abs=std::max(max_abs,std::fabs(a-b));peak=std::max(peak,std::fabs(b));if(std::fabs(a-b)>2e-5f+2e-5f*std::fabs(a))++bad;}
   std::printf("CASE layer=%d group=%d expert=%d matrix=%d weight_type=%d activation_type=%d amplitude=%g max_abs=%.9g peak=%.9g bad=%d\n",layer,group,expert,i,ty,at,amp,max_abs,peak,bad);std::fflush(stdout);require(bad==0&&peak>0,"fixed numerical gate");++count;
  }ggml_free(c);
 }std::printf("RESULT PASS cases=%d; model test still required\n",count);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"RESULT FAIL %s\n",e.what());return 1;}}
