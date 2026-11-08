#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cuda.h"
#include <vector>
#include <cstdio>
#include <cmath>
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 auto backend=ggml_backend_cuda_init(0);if(!backend)return 3;
 auto ctx=ggml_init({4*1024*1024,nullptr,true});if(!ctx)return 4;
 auto x=ggml_new_tensor_3d(ctx,GGML_TYPE_F32,11,128,1);
 auto w=ggml_new_tensor_2d(ctx,GGML_TYPE_F32,4,128);
 auto conv=ggml_ssm_conv(ctx,x,w);
 auto norm=ggml_rms_norm(ctx,conv,1e-5f);
 auto silu=ggml_silu(ctx,norm);
 auto mul=ggml_mul(ctx,norm,silu);
 auto half=ggml_cast(ctx,mul,GGML_TYPE_F16);
 auto full=ggml_cast(ctx,half,GGML_TYPE_F32);
 auto quant=ggml_cast(ctx,full,GGML_TYPE_Q4_0);
 auto result=ggml_cast(ctx,quant,GGML_TYPE_F32);
 auto graph=ggml_new_graph(ctx);ggml_build_forward_expand(graph,result);
 auto buf=ggml_backend_alloc_ctx_tensors(ctx,backend);if(!buf)return 5;
 std::vector<float> xv(11*128),wv(4*128);
 for(size_t i=0;i<xv.size();++i)xv[i]=float(int(i%31)-15)*0.03125f;
 for(size_t i=0;i<wv.size();++i)wv[i]=float(int(i%13)-6)*0.0625f;
 ggml_backend_tensor_set(x,xv.data(),0,xv.size()*4);ggml_backend_tensor_set(w,wv.data(),0,wv.size()*4);
 if(ggml_backend_graph_compute(backend,graph)!=GGML_STATUS_SUCCESS)return 6;
 ggml_backend_synchronize(backend);
 std::vector<float> out(ggml_nelements(result));ggml_backend_tensor_get(result,out.data(),0,out.size()*4);
 for(float v:out)if(!std::isfinite(v))return 7;
 FILE *f=fopen(argv[1],"wb");if(!f)return 8;
 for(auto stage : {conv,norm,silu,mul,full,result}) { std::vector<float> temp(ggml_nelements(stage)); ggml_backend_tensor_get(stage,temp.data(),0,temp.size()*4); fwrite(temp.data(),4,temp.size(),f); }
 fclose(f);
 printf("PASS joined CUDA graph SSM-conv/RMS/SiLU/MUL/F16/Q4 movement elements=%zu\n",out.size());
 ggml_backend_buffer_free(buf);ggml_free(ctx);ggml_backend_free(backend);return 0;
}
