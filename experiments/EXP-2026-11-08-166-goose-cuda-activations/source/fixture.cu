#include "unary.cuh"
#include "scale.cuh"
#include "clamp.cuh"
#include "ggml-backend-impl.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include "fixture-api.h"
extern "C" uint64_t fg166_cuda_host_seen();
extern "C" int fg166_cuda_device_read(uint64_t*);
static size_t checks=0,cases=0,values=0;
static void ck(cudaError_t e){if(e!=cudaSuccess){fprintf(stderr,"CUDA %s\n",cudaGetErrorString(e));std::exit(2);}}
static void req(bool v,const char*what){++checks;if(!v){fprintf(stderr,"FAIL %s\n",what);std::exit(3);}}
static ggml_type dtype(int k){return k==0?GGML_TYPE_F32:k==1?GGML_TYPE_F16:GGML_TYPE_BF16;}
static int bytes(int k){return k==0?4:2;}
static void shape(ggml_tensor&t,int kind,int cols,int rows,bool strided=false,int padding=0){std::memset(&t,0,sizeof t);t.type=dtype(kind);t.ne[0]=cols;t.ne[1]=rows;t.ne[2]=2;t.ne[3]=1;t.nb[0]=bytes(kind)*(strided?2:1);t.nb[1]=t.nb[0]*cols+padding*bytes(kind);t.nb[2]=t.nb[1]*rows;t.nb[3]=t.nb[2]*2;}
struct Allocation{char*raw;size_t size;static constexpr size_t guard=64;explicit Allocation(size_t n):size(n+2*guard){ck(cudaMalloc(&raw,size));ck(cudaMemset(raw,0xA5,size));}~Allocation(){ck(cudaFree(raw));}char*data(){return raw+guard;}};
static float sample(size_t i,int pattern){if(pattern==0)return float(int(i*31337%120001)-60000)/1024.0f;static const uint32_t bits[]={0,0x80000000,0x00000001,0x80000001,0x3f800000,0xbf800000,0x7f800000,0xff800000,0x7fc12345,0xffc12345,0x7f812345,0x3eaaaaab,0xc0000000,0x3f000000,0x00800000,0x7f7fffff};float v;uint32_t b=bits[i%(sizeof(bits)/sizeof(bits[0]))];std::memcpy(&v,&b,4);return v;}
static void fill(Allocation&a,const ggml_tensor&t,int kind,int pattern){std::vector<unsigned char>v(a.size-2*Allocation::guard,0xA5);for(int i2=0;i2<t.ne[2];i2++)for(int i1=0;i1<t.ne[1];i1++)for(int i0=0;i0<t.ne[0];i0++){size_t at=i2*t.nb[2]+i1*t.nb[1]+i0*t.nb[0];float f=sample(i0+31*i1+131*i2,pattern);if(kind==0)std::memcpy(v.data()+at,&f,4);else if(kind==1){half h=__float2half_rn(f);std::memcpy(v.data()+at,&h,2);}else{nv_bfloat16 h=__float2bfloat16_rn(f);std::memcpy(v.data()+at,&h,2);}}ck(cudaMemcpy(a.data(),v.data(),v.size(),cudaMemcpyHostToDevice));}
static void param(ggml_tensor&t,int slot,float v){std::memcpy((char*)t.op_params+slot*4,&v,4);}
static void test(ggml_backend_cuda_context&ctx,int code,int kind,int variant,int pattern){int style=styles[code];int cols=variant?257:17,rows=3;bool split=(variant&1);ggml_tensor src{},gate{},dst{},root{},mul{};int source_cols=(style==1&&!split)?cols*2:cols;bool strided=style==0&&(variant&1);int padding=(style==1||style==7)?5:0;shape(src,kind,source_cols,rows,strided,padding);shape(gate,kind,cols,rows,false,(style==1||style==7)?9:0);shape(dst,kind,cols,rows);if(style==6&&(variant&1)){shape(src,kind,cols,rows,false,13);shape(dst,kind,cols,rows,false,19);}src.op=gate.op=GGML_OP_NONE;dst.src[0]=&src;dst.src[1]=(style==1&&split)||style==3?&gate:nullptr;param(dst,0,0.125f);param(dst,1,0.03125f);param(dst,2,1.702f);param(dst,3,7.0f);param(dst,4,-0.01f);if(style==1)dst.op_params[1]=(variant>>1)&1;if(style==2){param(dst,1,0.7f);param(dst,2,0.9f);param(dst,3,0.125f);param(dst,4,-0.01f);}if(style==4)param(dst,0,0.125f);if(style==5){param(dst,0,0.125f);param(dst,1,0.03125f);}if(style==6){param(dst,0,-0.32f);param(dst,1,0.71f);}
 Allocation a(src.nb[3]),b(gate.nb[3]),n(dst.nb[3]),g(dst.nb[3]);fill(a,src,kind,pattern);fill(b,gate,kind,pattern);src.data=a.data();gate.data=b.data();dst.data=n.data();root=dst;mul=dst;
 ggml_tensor*first=&dst,*second=nullptr;
 if(style==7){root=src;root.op=GGML_OP_UNARY;root.src[0]=&src;root.op_params[0]=variant%3==0?GGML_UNARY_OP_SILU:variant%3==1?GGML_UNARY_OP_SIGMOID:GGML_UNARY_OP_SOFTPLUS;mul=dst;mul.src[0]=&root;mul.src[1]=&gate;first=&root;second=&mul;}
 if(style==8){root=src;root.op=GGML_OP_UNARY;root.src[0]=&src;mul=dst;mul.src[0]=&root;first=&root;second=&mul;}
 invoke(code,true,ctx,first,second);ck(cudaStreamSynchronize(ctx.stream()));std::vector<unsigned char>native(n.size),goose(g.size);ck(cudaMemcpy(native.data(),n.raw,n.size,cudaMemcpyDeviceToHost));bool alias=variant>=2&&(style==0||style==4||style==5||style==6)&&src.nb[3]==dst.nb[3]&&src.nb[1]==dst.nb[1]&&src.nb[0]==size_t(bytes(kind));dst.data=alias?a.data():g.data();mul.data=g.data();invoke(code,false,ctx,first,second);ck(cudaStreamSynchronize(ctx.stream()));ck(cudaMemcpy(goose.data(),alias?a.raw:g.raw,g.size,cudaMemcpyDeviceToHost));if(native!=goose){for(size_t i=0;i<native.size();i++)if(native[i]!=goose[i]){uint32_t x=0,y=0;size_t at=(i/bytes(kind))*bytes(kind);std::memcpy(&x,native.data()+at,bytes(kind));std::memcpy(&y,goose.data()+at,bytes(kind));fprintf(stderr,"MISMATCH code=%d style=%d kind=%d variant=%d pattern=%d byte=%zu native=0x%x goose=0x%x\n",code,style,kind,variant,pattern,i,x,y);break;}}req(native==goose,"bit-exact GPU output and canaries");for(size_t i=0;i<Allocation::guard;i++){req(goose[i]==0xA5&&goose[g.size-1-i]==0xA5,"allocation canaries");}++cases;values+=cols*rows*2;}
int main(){ggml_backend_cuda_context ctx(0);ck(cudaSetDevice(0));for(int code=0;code<40;code++)for(int kind=0;kind<3;kind++){int style=styles[code];if(kind==2&&(style==3||style==4||style==6||style==8||code==32))continue;if(kind!=0&&code==31)continue;if(kind==1&&style==5)continue;for(int variant=0;variant<4;variant++)for(int pattern=0;pattern<2;pattern++)test(ctx,code,kind,variant,pattern);}uint64_t mask[10]={};req(fg166_cuda_device_read(mask)==0,"device witness read");bool rollback=getenv("GGML_GOOSE_CUDA_ACTIVATIONS")&&std::strcmp(getenv("GGML_GOOSE_CUDA_ACTIVATIONS"),"0")==0;req(rollback?fg166_cuda_host_seen()==0:fg166_cuda_host_seen()==((UINT64_C(1)<<40)-1),"all40host entries/rollback witness");for(int m=0;m<10;m++)req(rollback?mask[m]==0:mask[m]!=0,"all10device modes/rollback witness");printf("PASS actual_GPU_cases=%zu values=%zu checks=%zu bit_exact=1 all40host/all10device_modes=1 rollback=%d\n",cases,values,checks,rollback);}
