#include "ggml.h"
#include "ggml-cpu.h"
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <sys/resource.h>
extern "C" void quantize_row_q8_K(const float *,void *,int64_t);
extern "C" void ggml_vec_dot_q4_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);
extern "C" float fg_selfhost_q4dot(int,const void*,const void*);
using Fn=float(*)(int,const void*,const void*);
__attribute__((noinline)) static float original(int n,const void*x,const void*y) {
 float out;ggml_vec_dot_q4_K_q8_K(n,&out,0,x,0,y,0,1);return out;
}
__attribute__((noinline)) static float generated(int n,const void*x,const void*y) {
 return fg_selfhost_q4dot(n,x,y);
}
int main(int argc,char**argv) {
 if(argc!=2)return 2;
 ggml_cpu_init();const int n=2048,rows=256,passes=64;const size_t stride=n/256*144;
 std::vector<unsigned char>w(rows*stride),y(n/256*292);
 std::ifstream f(argv[1],std::ios::binary);f.read((char*)w.data(),w.size());if(!f)return 3;
 std::vector<float>a(n),ref(rows),out(rows);
 for(int i=0;i<n;++i)a[i]=std::sin(i*.023f)*.75f;
 quantize_row_q8_K(a.data(),y.data(),n);
 for(int i=0;i<rows;++i){ref[i]=original(n,w.data()+i*stride,y.data());out[i]=generated(n,w.data()+i*stride,y.data());}
 if(memcmp(ref.data(),out.data(),rows*4)){fprintf(stderr,"real weight mismatch\n");return 1;}
 for(int nt:{1,8})for(int repeat=0;repeat<3;++repeat)for(int step=0;step<2;++step) {
  int mode=repeat%2?1-step:step;Fn fn=mode?generated:original;
  auto run=[&](int rank){for(int p=0;p<passes;++p)for(int i=rank;i<rows;i+=nt)out[i]=fn(n,w.data()+i*stride,y.data());};
  auto start=std::chrono::steady_clock::now();
  if(nt==1)run(0);else{std::vector<std::thread>t;for(int i=0;i<nt;++i)t.emplace_back(run,i);for(auto&x:t)x.join();}
  double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  if(memcmp(ref.data(),out.data(),rows*4))return 4;
  rusage u;getrusage(RUSAGE_SELF,&u);
  printf("{\"mode\":%d,\"threads\":%d,\"repeat\":%d,\"ms\":%.9g,\"ns_per_row\":%.9g,\"peak_rss_kib\":%ld,\"bitwise_equal\":true,\"preliminary\":true}\n",mode,nt,repeat,ms,ms*1e6/(rows*passes),u.ru_maxrss);
 }
}
