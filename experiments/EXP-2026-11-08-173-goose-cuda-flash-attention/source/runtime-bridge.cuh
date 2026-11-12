#pragma once
#include "host-ffi.h"
#include <atomic>
#include <mutex>
#include <cstring>
void fg175_native_ggml_cuda_flash_attn_ext(ggml_backend_cuda_context&,ggml_tensor*);
bool fg175_native_ggml_cuda_flash_attn_ext_supported(int,const ggml_tensor*);
size_t fg175_native_ggml_cuda_flash_attn_ext_get_alloc_size(int,const ggml_tensor*);
struct fg175_packet{int64_t role,dev;const ggml_tensor*dst;ggml_backend_cuda_context*ctx;uint64_t v[8][64]{};float f[5]{};ggml_cuda_pool_alloc<int>kv;ggml_cuda_pool_alloc<float>tmp;ggml_cuda_pool_alloc<float2>meta;fg175_packet(int64_t r,int64_t d,const ggml_tensor*t,ggml_backend_cuda_context*c=nullptr):role(r),dev(d),dst(t),ctx(c){}};
static std::atomic<uint64_t> fg175fa_host_seen{0};
static std::mutex fg175fa_init_locks[GGML_CUDA_MAX_DEVICES][3];
static std::atomic<int64_t>fg175fa_init[GGML_CUDA_MAX_DEVICES][3]{};
static fg175_packet&fg175_p(int64_t p){GGML_ASSERT(p);return*(fg175_packet*)(uintptr_t)p;}
static const ggml_tensor&fg175_t(int64_t t){GGML_ASSERT(t);return*(const ggml_tensor*)(uintptr_t)t;}
static fattn_kernel_t fg175_kernel(const fg175_packet&p){const fattn_kernel_t k[]={fg175v_kernel<1>,fg175v_kernel<2>,fg175m_kernel<4>,fg175m_kernel<8>};GGML_ASSERT(p.v[3][36]<4);return k[p.v[3][36]];}
extern "C" int64_t fg175h_arg(int64_t p,int64_t k){auto&x=fg175_p(p);switch(k){case 0:return x.role;case 1:return x.dev;case 2:return(int64_t)(uintptr_t)x.dst;default:GGML_ABORT("FA arg");}}
extern "C" int64_t fg175h_plan(int64_t p){return p;}
extern "C" void fg175h_put(int64_t p,int64_t k,int64_t i,uint64_t v){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<64);fg175_p(p).v[k][i]=v;}
extern "C" uint64_t fg175h_get(int64_t p,int64_t k,int64_t i){GGML_ASSERT(k>=0&&k<8&&i>=0&&i<64);return fg175_p(p).v[k][i];}
extern "C" void fg175h_putf(int64_t p,int64_t i,float v){GGML_ASSERT(i>=0&&i<5);fg175_p(p).f[i]=v;}
extern "C" int64_t fg175h_ti(int64_t t,int64_t k,int64_t i){const auto&x=fg175_t(t);switch(k){case 0:GGML_ASSERT(i>=0&&i<GGML_MAX_SRC);return(int64_t)(uintptr_t)x.src[i];case 1:return(int64_t)(uintptr_t)x.data;case 2:return x.type;case 3:GGML_ASSERT(i>=0&&i<4);return x.ne[i];case 4:GGML_ASSERT(i>=0&&i<4);return x.nb[i];case 5:return x.op;case 7:return ggml_nbytes(&x);case 8:return(int64_t)(uintptr_t)x.view_src;case 9:return x.view_offs;default:GGML_ABORT("FA tensor field");}}
extern "C" float fg175h_tpf(int64_t t,int64_t i){float v;GGML_ASSERT(i>=0&&i<GGML_MAX_OP_PARAMS/4);memcpy(&v,(const char*)fg175_t(t).op_params+4*i,4);return v;}
extern "C" int64_t fg175h_tpi(int64_t t,int64_t i){int32_t v;GGML_ASSERT(i>=0&&i<GGML_MAX_OP_PARAMS/4);memcpy(&v,(const char*)fg175_t(t).op_params+4*i,4);return v;}
extern "C" int64_t fg175h_tc(int64_t k){const ggml_type t[]={GGML_TYPE_F32,GGML_TYPE_F16,GGML_TYPE_BF16,GGML_TYPE_Q4_0,GGML_TYPE_Q4_1,GGML_TYPE_Q5_0,GGML_TYPE_Q5_1,GGML_TYPE_Q8_0};GGML_ASSERT(k>=0&&k<8);return t[k];}
extern "C" int64_t fg175h_oc(int64_t k){GGML_ASSERT(k==0);return GGML_OP_FLASH_ATTN_EXT;}
extern "C" int64_t fg175h_ndev(){return ggml_cuda_info().device_count;}
extern "C" int64_t fg175h_dev(int64_t d,int64_t k){GGML_ASSERT(d>=0&&d<ggml_cuda_info().device_count);const auto&x=ggml_cuda_info().devices[d];switch(k){case 0:return x.cc;case 1:return x.nsm;case 4:return x.warp_size;default:GGML_ABORT("FA device field");}}
extern "C" int64_t fg175h_carch(int64_t d){return ggml_cuda_highest_compiled_arch(ggml_cuda_info().devices[d].cc);}
extern "C" int64_t fg175h_blck(int64_t t){return ggml_blck_size((ggml_type)t);}
extern "C" int64_t fg175h_tsize(int64_t t){return ggml_type_size((ggml_type)t);}
extern "C" int64_t fg175h_nel(int64_t t){return ggml_nelements(&fg175_t(t));}
extern "C" int64_t fg175h_nrows(int64_t t){return ggml_nrows(&fg175_t(t));}
extern "C" void fg175h_setdev(int64_t d){ggml_cuda_set_device(d);}
extern "C" int64_t fg175h_occ(int64_t p){auto&x=fg175_p(p);int n=0;CUDA_CHECK(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&n,fg175_kernel(x),x.v[3][6],x.v[3][9]));return n;}
extern "C" int64_t fg175h_setattr(int64_t p){auto&x=fg175_p(p);return cudaFuncSetAttribute(fg175_kernel(x),cudaFuncAttributeMaxDynamicSharedMemorySize,x.v[3][9]);}
extern "C" int64_t fg175h_memo(int64_t d,int64_t i){GGML_ASSERT(d>=0&&d<GGML_CUDA_MAX_DEVICES&&i>=1&&i<=2);return fg175fa_init[d][i].load(std::memory_order_relaxed);}
extern "C" void fg175h_memo_set(int64_t d,int64_t i,int64_t v){GGML_ASSERT(d>=0&&d<GGML_CUDA_MAX_DEVICES&&i>=1&&i<=2);fg175fa_init[d][i].store(v,std::memory_order_relaxed);}
extern "C" void fg175h_lock(int64_t d,int64_t i){GGML_ASSERT(d>=0&&d<GGML_CUDA_MAX_DEVICES&&i>=1&&i<=2);fg175fa_init_locks[d][i].lock();}
extern "C" void fg175h_unlock(int64_t d,int64_t i){GGML_ASSERT(d>=0&&d<GGML_CUDA_MAX_DEVICES&&i>=1&&i<=2);fg175fa_init_locks[d][i].unlock();}
extern "C" int64_t fg175h_pool(int64_t p,int64_t slot){auto&x=fg175_p(p);GGML_ASSERT(x.ctx);switch(slot){case 0:GGML_ASSERT(x.v[3][27]>0&&x.v[3][27]<=SIZE_MAX/sizeof(int));return(int64_t)(uintptr_t)x.kv.alloc(x.ctx->pool(),x.v[3][27]);case 1:GGML_ASSERT(x.v[3][25]>0&&x.v[3][25]<=SIZE_MAX/sizeof(float));return(int64_t)(uintptr_t)x.tmp.alloc(x.ctx->pool(),x.v[3][25]);case 2:GGML_ASSERT(x.v[3][26]>0&&x.v[3][26]<=SIZE_MAX/sizeof(float2));return(int64_t)(uintptr_t)x.meta.alloc(x.ctx->pool(),x.v[3][26]);default:GGML_ABORT("FA pool slot");}}
extern "C" int64_t fg175h_conv(int64_t p,int64_t){auto&x=fg175_p(p);GGML_ASSERT(x.ctx);auto&v=x.v[6];dim3 grid(v[9],v[10],v[11]);if(v[14]==1)fg175q_contig<<<grid,v[12],0,x.ctx->stream()>>>((void*)(uintptr_t)v[0],(half*)(uintptr_t)v[1],v[13]);else fg175q_generic<<<grid,v[12],0,x.ctx->stream()>>>((void*)(uintptr_t)v[0],(half*)(uintptr_t)v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8]);return cudaGetLastError();}
extern "C" int64_t fg175h_launch(int64_t p){auto&x=fg175_p(p);GGML_ASSERT(x.ctx);auto&v=x.v;ggml_cuda_kernel_launch_params lp(dim3(v[3][14],v[3][15],v[3][16]),dim3(v[3][30],v[3][7]),v[3][9],x.ctx->stream());ggml_cuda_kernel_launch(fg175_kernel(x),lp,(const char*)(uintptr_t)v[0][0],(const char*)(uintptr_t)v[0][7],(const char*)(uintptr_t)v[0][8],(const char*)(uintptr_t)v[0][3],(const char*)(uintptr_t)v[0][4],(const int*)(uintptr_t)v[0][10],(float*)(uintptr_t)v[0][9],(float2*)(uintptr_t)v[0][11],x.f[0],x.f[1],x.f[2],x.f[3],(uint32_t)v[5][0],x.f[4],(int32_t)v[1][0],init_fastdiv_values(v[1][1]),(int32_t)v[1][2],(int32_t)v[1][3],(int32_t)v[2][0],(int32_t)v[2][1],(int32_t)v[2][2],(int32_t)v[1][4],(int32_t)v[3][17],(int32_t)v[1][6],(int32_t)v[1][7],(int32_t)v[2][3],(int32_t)v[2][4],(int64_t)v[2][5],(int32_t)v[2][6],(int32_t)v[2][7],(int64_t)v[2][8],(int32_t)v[1][13],(int32_t)v[1][14],(int32_t)v[1][15],(int32_t)v[2][9],(int32_t)v[2][10],(int64_t)v[2][11]);return cudaGetLastError();}
extern "C" int64_t fg175h_lfix(int64_t p,int64_t kind){auto&x=fg175_p(p);GGML_ASSERT(x.ctx);auto&v=x.v;auto&a=v[7];dim3 grid(a[1],a[2],a[3]);const auto st=x.ctx->stream();switch(kind){case 0:fg175f_mask<<<grid,a[4],a[5],st>>>((const void*)(uintptr_t)v[0][3],(int*)(uintptr_t)v[0][10],a[11],a[17],a[18],v[3][2]);break;case 2:fg175f_uniform<<<grid,a[4],a[5],st>>>((float*)(uintptr_t)v[0][12],(const float2*)(uintptr_t)v[0][11],a[6],a[7],v[1][6],a[9],a[8],a[10],a[13],a[14],a[15],v[3][2],v[3][3]);break;case 3:fg175f_general<<<grid,a[4],a[5],st>>>((float*)(uintptr_t)v[0][12],(const float2*)(uintptr_t)v[0][11],a[6],a[7],a[8],a[12],a[13],a[14],a[15],a[16],v[3][2],v[3][3]);break;case 4:fg175v_combine<<<grid,a[4],a[5],st>>>((const float*)(uintptr_t)v[0][13],(const float2*)(uintptr_t)v[0][11],(float*)(uintptr_t)v[0][12],v[3][13]);break;default:GGML_ABORT("FA fixup kind");}return cudaGetLastError();}
extern "C" float fg175h_powf(float a,float b){return powf(a,b);}
extern "C" float fg175h_u2f(uint64_t v){return(float)v;}
extern "C" void fg175h_fail(int64_t code){GGML_ABORT("Goose FA validation/effect failure %lld",(long long)code);}
static void fg175fa_report();
static void fg175fa_register(){static const bool registered=[](){std::atexit(fg175fa_report);return true;}();(void)registered;}
static bool fg175_disabled(){const char*e=getenv("GGML_GOOSE_CUDA_FA");return e&&strcmp(e,"0")==0;}
size_t ggml_cuda_flash_attn_ext_get_alloc_size(int d,const ggml_tensor*t){if(fg175_disabled())return fg175_native_ggml_cuda_flash_attn_ext_get_alloc_size(d,t);fg175_packet p(0,d,t);fg175h_host_entry((int64_t)(uintptr_t)&p);if(p.v[3][28])return fg175_native_ggml_cuda_flash_attn_ext_get_alloc_size(d,t);fg175fa_host_seen.fetch_or(1,std::memory_order_relaxed);return p.v[3][0];}
bool ggml_cuda_flash_attn_ext_supported(int d,const ggml_tensor*t){if(fg175_disabled())return fg175_native_ggml_cuda_flash_attn_ext_supported(d,t);fg175_packet p(1,d,t);fg175h_host_entry((int64_t)(uintptr_t)&p);if(p.v[3][28])return fg175_native_ggml_cuda_flash_attn_ext_supported(d,t);fg175fa_host_seen.fetch_or(2,std::memory_order_relaxed);return p.v[3][29]!=0;}
void ggml_cuda_flash_attn_ext(ggml_backend_cuda_context&ctx,ggml_tensor*t){if(fg175_disabled()){fg175_native_ggml_cuda_flash_attn_ext(ctx,t);return;}fg175fa_register();fg175_packet p(2,ctx.device,t,&ctx);fg175h_host_entry((int64_t)(uintptr_t)&p);if(p.v[3][28]){fg175_native_ggml_cuda_flash_attn_ext(ctx,t);return;}fg175fa_host_seen.fetch_or(p.v[3][1]==100?4:8,std::memory_order_relaxed);}
extern "C" __attribute__((visibility("default"))) uint64_t fg175fa_cuda_host_seen(){return fg175fa_host_seen.load(std::memory_order_relaxed);}
extern "C" __attribute__((visibility("default"))) int fg175fa_cuda_device_read(uint64_t out[3]){auto s=cudaMemcpyFromSymbol(out,fg175v_paths,8);if(s==cudaSuccess)s=cudaMemcpyFromSymbol(out+1,fg175v_features,8);if(s==cudaSuccess)s=cudaMemcpyFromSymbol(out+2,fg175fa_paths,8);return(int)s;}

static void fg175fa_report(){const uint64_t h=fg175fa_host_seen.load(std::memory_order_relaxed);if(!h)return;uint64_t v[3]={};const int status=fg175fa_cuda_device_read(v);fprintf(stderr,"FG175FA_CUDA host=0x%llx,cuda_status=%d,vector=0x%llx,features=0x%llx,prefill=0x%llx\n",(unsigned long long)h,status,(unsigned long long)v[0],(unsigned long long)v[1],(unsigned long long)v[2]);}
