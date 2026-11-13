#include "fattn-mma-f16.cuh"
#include "mma-iter-primitives.cuh"
#include "mma-iter-functions-generated.cuh"
#include "mma-process-primitives.cuh"
#include "mma-process-functions-generated.cuh"
#include "mma-entry-adapter.cuh"
#include "mma-entry-device-functions-generated.cuh"
#include "mma-entry-kernel.cuh"
#include "mask-fixups-primitives.cuh"
#include "mask-fixups-device-functions-generated.cuh"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
template<int N>__global__ void uni(float*d,const float2*m,int nq,int heads,int kh,int nb,int gqa,int bpt,int64_t a,int64_t b,int64_t c){FG175F_UNIFORM((uint64_t)(uintptr_t)d,(uint64_t)(uintptr_t)m,nq,heads,kh,nb,gqa,bpt,a,b,c,N,8,blockIdx.x,blockIdx.y,blockIdx.z,threadIdx.x,1);}
template<int N>__global__ void gen(float*d,const float2*m,int nq,int heads,int gqa,int total,int64_t a,int64_t b,int64_t c,int64_t e){FG175F_GENERAL((uint64_t)(uintptr_t)d,(uint64_t)(uintptr_t)m,nq,heads,gqa,total,a,b,c,e,N,8,blockIdx.x,blockIdx.y,blockIdx.z,gridDim.x,threadIdx.x,1);}
static void ck(cudaError_t e){if(e!=cudaSuccess){fprintf(stderr,"CUDA %s\n",cudaGetErrorString(e));exit(2);}}
struct Buf{unsigned char*p;size_t n;Buf(size_t size):n(size){ck(cudaMalloc(&p,n+128));ck(cudaMemset(p,0xA5,n+128));}~Buf(){ck(cudaFree(p));}template<class T>T*data(){return(T*)(p+64);}std::vector<unsigned char>get(){std::vector<unsigned char>h(n+128);ck(cudaMemcpy(h.data(),p,h.size(),cudaMemcpyDeviceToHost));return h;}};
static void same(const std::vector<unsigned char>&a,const std::vector<unsigned char>&b,const char*w,int N,int nb){for(size_t i=0;i<a.size();i++)if(a[i]!=b[i]){uint32_t aa=0,bb=0;memcpy(&aa,a.data()+(i&~3),4);memcpy(&bb,b.data()+(i&~3),4);fprintf(stderr,"FAIL %s N%d blocks%d byte%zu %08x %08x\n",w,N,nb,i,aa,bb);exit(3);}for(int i=0;i<64;i++)if(b[i]!=0xA5||b[b.size()-1-i]!=0xA5){fprintf(stderr,"FAIL guard\n");exit(3);}}
template<int N>void test(int nq,int nkv,int nb,bool sinks){const int nt=(nq+N-1)/N,ntdst=nt*2,ik=nkv/32;std::vector<float>q(256*nq*16);std::vector<half>k(256*nkv*2),v(k.size()),mask(nq*nkv);std::vector<float>ss(16);for(size_t i=0;i<q.size();i++)q[i]=((int)(i%31)-15)/32.f;for(size_t i=0;i<k.size();i++){k[i]=__float2half(((int)(i%17)-8)/64.f);v[i]=__float2half(((int)(i%29)-14)/128.f);}for(size_t i=0;i<mask.size();i++)mask[i]=__float2half(i%nkv<(size_t)nkv-3?0.f:-INFINITY);for(int i=0;i<16;i++)ss[i]=(i-8)/8.f;Buf Q(q.size()*4),K(k.size()*2),V(v.size()*2),M(mask.size()*2),S(64),a(q.size()*4),b(a.n),fa(nb*N*8*130*8),fb(fa.n);ck(cudaMemcpy(Q.data<float>(),q.data(),q.size()*4,cudaMemcpyHostToDevice));ck(cudaMemcpy(K.data<half>(),k.data(),k.size()*2,cudaMemcpyHostToDevice));ck(cudaMemcpy(V.data<half>(),v.data(),v.size()*2,cudaMemcpyHostToDevice));ck(cudaMemcpy(M.data<half>(),mask.data(),mask.size()*2,cudaMemcpyHostToDevice));ck(cudaMemcpy(S.data<float>(),ss.data(),64,cudaMemcpyHostToDevice));const auto qi=Q.get(),ki=K.get(),vi=V.get(),mi=M.get(),si=S.get();
#define ARGS(side,meta) (const char*)Q.data<float>(),(const char*)K.data<half>(),(const char*)V.data<half>(),(const char*)M.data<half>(),sinks?(const char*)S.data<float>():nullptr,nullptr,side.data<float>(),meta.data<float2>(),1.f/16,0.f,1.f,1.f,16,0.f,256,init_fastdiv_values(nq),16,1,1024,1024*nq,1024*nq*16,256,nkv,2,1,512,512*nkv,(int64_t)(512*nkv*2),512,512*nkv,(int64_t)(512*nkv*2),nq,1,1,2*nkv,2*nkv*nq,(int64_t)(2*nkv*nq)
flash_attn_ext_f16<256,256,N,8,false,false,false><<<nb,dim3(32,4),33792>>>(ARGS(a,fa));fg175m_kernel<N><<<nb,dim3(32,4),33792>>>(ARGS(b,fb));ck(cudaGetLastError());ck(cudaDeviceSynchronize());same(a.get(),b.get(),"main-dst",N,nb);same(fa.get(),fb.get(),"main-meta",N,nb);
#undef ARGS
if(nb%ntdst==0&&nb>ntdst){flash_attn_stream_k_fixup_uniform<256,N,8><<<dim3(ntdst,N,8),256>>>(a.data<float>(),fa.data<float2>(),nq,16,2,nb,8,nb/ntdst,init_fastdiv_values(nt*2),init_fastdiv_values(nt),init_fastdiv_values(nt));uni<N><<<dim3(ntdst,N,8),256>>>(b.data<float>(),fb.data<float2>(),nq,16,2,nb,8,nb/ntdst,nt*2,nt,nt);}else if(ntdst%nb!=0){flash_attn_stream_k_fixup_general<256,N,8><<<dim3(nb,N,8),256>>>(a.data<float>(),fa.data<float2>(),nq,16,8,ik*ntdst,init_fastdiv_values(ik*nt*2),init_fastdiv_values(ik*nt),init_fastdiv_values(ik*nt),init_fastdiv_values(ik));gen<N><<<dim3(nb,N,8),256>>>(b.data<float>(),fb.data<float2>(),nq,16,8,ik*ntdst,ik*nt*2,ik*nt,ik*nt,ik);}ck(cudaGetLastError());ck(cudaDeviceSynchronize());same(a.get(),b.get(),"fixed-dst",N,nb);if(qi!=Q.get()||ki!=K.get()||vi!=V.get()||mi!=M.get()||si!=S.get()){fprintf(stderr,"FAIL readonly input\n");exit(3);}printf("PASS wholeMMA N%d nq%d nkv%d blocks%d sink%d\n",N,nq,nkv,nb,sinks);}
int main(){test<4>(4,64,2,false);test<8>(8,96,2,false);test<4>(7,96,4,true);test<8>(13,64,4,true);test<4>(4,64,4,false);test<8>(8,96,4,true);test<4>(4,96,3,true);test<8>(13,96,3,false);puts("PASS8 whole stream-K prefill kernel and original uniform/general fixups; no full-runtime claim");}
