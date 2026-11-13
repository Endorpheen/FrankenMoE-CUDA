#include "norm-goose-block.cu"
#include <vector>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
static void check(cudaError_t e) { if(e!=cudaSuccess){fprintf(stderr,"CUDA: %s\n",cudaGetErrorString(e));std::exit(2);} }
static int cases=0; static unsigned long long values=0;
template<int BS,int VPT,bool VEC,bool MUL,bool ADD,bool SCALE,bool GOOSE>
static void launch(const float*x,float*y,const float*m,const float*a,int n,int rows,int channels,int samples,int sr,int sc,int ss,int mc,int ac,float eps,cudaStream_t stream=0) {
    const auto mp=init_fastdiv_values(mc), ap=init_fastdiv_values(ac);
    const auto r=init_fastdiv_values(2),c=init_fastdiv_values(2),s=init_fastdiv_values(1);
    const dim3 grid(rows,1,1); const int shared=BS>32?128:0;
    if constexpr(GOOSE) rms_norm_goose_f32<BS,VPT,VEC,MUL,ADD,SCALE><<<grid,BS,shared,stream>>>(x,y,n,channels,samples,sr,sc,ss,eps,m,mc,2*mc,4*mc,mp,r,c,s,a,ac,2*ac,4*ac,ap,r,c,s,0.125f);
    else rms_norm_f32<BS,VPT,VEC,MUL,ADD,SCALE><<<grid,BS,shared,stream>>>(x,y,n,channels,samples,sr,sc,ss,eps,m,mc,2*mc,4*mc,mp,r,c,s,a,ac,2*ac,4*ac,ap,r,c,s,0.125f);
}
template<int BS,int VPT,bool VEC,bool MUL,bool ADD,bool SCALE>
static void test(int n,int pattern) {
    const int rows=3,channels=3,samples=2,sr=((n+19)/4)*4,sc=sr*rows+16,ss=sc*channels+16;
    const int mc=VEC?n:3,ac=VEC?n:5,guard=64;
    const size_t count=size_t(rows)*channels*samples*n,total=count+2*guard;
    std::vector<float>x(ss*samples),m(4*mc),a(4*ac),initial(total,-12345.5f),native(total),goose(total);
    for(size_t i=0;i<x.size();++i)x[i]=pattern==0?float(int(i*31337ULL%200003)-100001)/1024.0f:pattern==1?0.0f:float(int(i%17)-8)*1e-19f;
    for(size_t i=0;i<m.size();++i)m[i]=float(int(i%13)-6)*0.125f;
    for(size_t i=0;i<a.size();++i)a[i]=float(int(i%19)-9)*0.03125f;
    float *dx,*dn,*dg,*dm,*da;check(cudaMalloc(&dx,x.size()*4));check(cudaMalloc(&dn,total*4));check(cudaMalloc(&dg,total*4));check(cudaMalloc(&dm,m.size()*4));check(cudaMalloc(&da,a.size()*4));
    check(cudaMemcpy(dx,x.data(),x.size()*4,cudaMemcpyHostToDevice));check(cudaMemcpy(dm,m.data(),m.size()*4,cudaMemcpyHostToDevice));check(cudaMemcpy(da,a.data(),a.size()*4,cudaMemcpyHostToDevice));
    check(cudaMemcpy(dn,initial.data(),total*4,cudaMemcpyHostToDevice));check(cudaMemcpy(dg,initial.data(),total*4,cudaMemcpyHostToDevice));
    launch<BS,VPT,VEC,MUL,ADD,SCALE,false>(dx,dn+guard,dm,da,n,rows,channels,samples,sr,sc,ss,mc,ac,1e-6f);
    launch<BS,VPT,VEC,MUL,ADD,SCALE,true>(dx,dg+guard,dm,da,n,rows,channels,samples,sr,sc,ss,mc,ac,1e-6f);
    check(cudaGetLastError());check(cudaMemcpy(native.data(),dn,total*4,cudaMemcpyDeviceToHost));check(cudaMemcpy(goose.data(),dg,total*4,cudaMemcpyDeviceToHost));
    for(size_t i=0;i<total;++i) {
        if(std::memcmp(&native[i],&goose[i],4)){fprintf(stderr,"FAIL bs=%d vpt=%d vec=%d mul=%d add=%d scale=%d n=%d pattern=%d i=%zu native=%a goose=%a\n",BS,VPT,VEC,MUL,ADD,SCALE,n,pattern,i,native[i],goose[i]);std::exit(1);}
        if((i<guard||i>=guard+count)&&goose[i]!=initial[i]){fprintf(stderr,"CANARY FAIL\n");std::exit(1);}
    }
    std::vector<float>after(x.size());check(cudaMemcpy(after.data(),dx,x.size()*4,cudaMemcpyDeviceToHost));if(std::memcmp(after.data(),x.data(),x.size()*4)){fprintf(stderr,"INPUT MUTATED\n");std::exit(1);}
    check(cudaFree(dx));check(cudaFree(dn));check(cudaFree(dg));check(cudaFree(dm));check(cudaFree(da));++cases;values+=count;
}
template<int BS,int VPT,bool VEC>
static void variants(int n){for(int p=0;p<3;++p){test<BS,VPT,VEC,false,false,false>(n,p);test<BS,VPT,VEC,true,false,false>(n,p);test<BS,VPT,VEC,true,true,false>(n,p);test<BS,VPT,VEC,false,false,true>(n,p);}}
template<bool GOOSE>
static float timed(const float*x,float*y,const float*m,int n,int rows) {
 cudaEvent_t a,b;check(cudaEventCreate(&a));check(cudaEventCreate(&b));check(cudaEventRecord(a));
 for(int i=0;i<40;++i)launch<512,1,true,true,false,false,GOOSE>(x,y,m,m,n,rows,1,1,n,n*rows,n*rows,n,n,1e-6f);
 check(cudaEventRecord(b));check(cudaEventSynchronize(b));float ms;check(cudaEventElapsedTime(&ms,a,b));check(cudaEventDestroy(a));check(cudaEventDestroy(b));return ms/40;
}
static void bench(int rows) {
 int n=2048;size_t count=size_t(rows)*n;std::vector<float>x(count,0.5f);float *dx,*dy,*dm;check(cudaMalloc(&dx,count*4));check(cudaMalloc(&dy,count*4));check(cudaMalloc(&dm,n*4*4));check(cudaMemcpy(dx,x.data(),count*4,cudaMemcpyHostToDevice));check(cudaMemset(dm,0,n*4*4));
 timed<false>(dx,dy,dm,n,rows);timed<true>(dx,dy,dm,n,rows);
 for(int r=0;r<5;++r){float native,goose;if(r%2){goose=timed<true>(dx,dy,dm,n,rows);native=timed<false>(dx,dy,dm,n,rows);}else{native=timed<false>(dx,dy,dm,n,rows);goose=timed<true>(dx,dy,dm,n,rows);}printf("{\"kind\":\"micro\",\"rows\":%d,\"ncols\":%d,\"repeat\":%d,\"native_ms\":%.9g,\"goose_ms\":%.9g,\"preliminary\":true}\n",rows,n,r,native,goose);}
 check(cudaFree(dx));check(cudaFree(dy));check(cudaFree(dm));
}
int main(){variants<32,1,true>(4);variants<32,1,true>(128);variants<128,1,true>(132);variants<128,1,true>(512);variants<256,1,true>(516);variants<256,1,true>(1024);variants<512,1,true>(2048);variants<1024,1,true>(4096);variants<1024,4,true>(4100);variants<1024,4,true>(16384);variants<256,1,false>(1);variants<256,1,false>(31);variants<256,1,false>(257);variants<1024,1,false>(1025);variants<1024,1,false>(17003);printf("{\"kind\":\"correctness\",\"cases\":%d,\"values_bitwise_checked\":%llu,\"input_and_output_canaries\":true,\"status\":\"PASS\"}\n",cases,values);for(int rows:{1,512,4096})bench(rows);return 0;}
