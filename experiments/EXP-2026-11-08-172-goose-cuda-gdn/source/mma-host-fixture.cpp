#include "mma-host-ffi.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <climits>
extern "C" uint8_t fg174m_host_entry(int64_t);
struct Packet{int64_t dev=0,H=16,Hk=16,nt=64,ns=1,rq3=1,eligible=1,out=0x1000,cc=89,nsm=80,mem=98304,maxdev=2,ampere=1,status=0;};
static std::atomic<int64_t> memo[2][4]{};static std::mutex locks[2][4];static std::atomic<int> inits{0},launches{0},clears{0};static std::atomic<int64_t> lastpath{0};static std::atomic<uint64_t> lastbytes{0},lastgrid{0};static std::atomic<int> checks{0};static int cases=0;
static void req(bool ok,const char*why){++checks;if(!ok){fprintf(stderr,"FAIL MMA host %s\n",why);exit(3);}}
extern "C" int64_t fg174m_query(int64_t pp,int64_t k){const auto&p=*(const Packet*)(uintptr_t)pp;const int64_t v[]={p.dev,p.H,p.Hk,p.nt,p.ns,p.rq3,p.eligible,p.out,p.cc,p.nsm,p.mem,p.maxdev,p.ampere};req(k>=0&&k<13,"query key");return v[k];}
extern "C" int64_t fg174m_cc(int64_t k){const int64_t v[]={86,89,100,121};return v[k];}
extern "C" int64_t fg174m_memo_get(int64_t d,int64_t p){return memo[d][p].load();}
extern "C" void fg174m_memo_set(int64_t d,int64_t p,int64_t f){memo[d][p].store(f);}
extern "C" void fg174m_lock(int64_t d,int64_t p){locks[d][p].lock();}
extern "C" void fg174m_unlock(int64_t d,int64_t p){locks[d][p].unlock();}
extern "C" int64_t fg174m_init(int64_t pp,int64_t,uint64_t){++inits;return((Packet*)(uintptr_t)pp)->status;}
extern "C" void fg174m_clear_error(){++clears;}
extern "C" int64_t fg174m_launch_raw(int64_t,int64_t p,uint64_t b,uint64_t g){++launches;lastpath=p;lastbytes=b;lastgrid=g;return 0;}
extern "C" void fg174m_fail(int64_t c){fprintf(stderr,"Unexpected MMA host fail %lld\n",(long long)c);exit(4);}
static void reset(){for(auto&r:memo)for(auto&v:r)v=0;inits=0;launches=0;clears=0;lastpath=0;lastbytes=0;lastgrid=0;}
static bool run(Packet&p){return fg174m_host_entry((int64_t)(uintptr_t)&p)!=0;}
int main(){
 for(int path=1;path<=3;path++){reset();Packet p;if(path==2)p.nsm=46;if(path==3){p.nsm=46;p.H=32;}req(run(p),"valid path");req(lastpath==path&&inits==1&&launches==1,"path/init selection");req(lastbytes==(path==3?93312:38016)&&lastgrid==(uint64_t)(p.H*p.ns)*(path==3?1:4),"shared bytes/grid");req(run(p)&&inits==1&&launches==2,"once memo reuse");++cases;}
 for(int which=0;which<12;which++){reset();Packet p;switch(which){case 0:p.eligible=0;break;case 1:p.Hk=15;break;case 2:p.H=17;break;case 3:p.nt=63;break;case 4:p.nt=INT64_MAX-31;break;case 5:p.rq3=0;break;case 6:p.ns=0;break;case 7:p.ns=INT_MAX/(p.H*4)+1;break;case 8:p.ampere=0;break;case 9:p.cc=90;break;case 10:p.out=0;break;case 11:p.dev=-1;p.out=0;break;}req(!run(p)&&inits==0&&launches==0,"decline without effects");++cases;}
 reset();Packet p;p.mem=0;req(!run(p)&&inits==0&&memo[0][1]==1,"shared limit cached unavailable");p.mem=98304;req(!run(p)&&inits==0,"unavailable stays cached");++cases;
 reset();p=Packet{};p.status=7;req(!run(p)&&inits==1&&clears==1&&launches==0,"SDK error cleanup");p.status=0;req(!run(p)&&inits==1&&clears==1,"SDK failure stays cached");++cases;
 reset();p=Packet{};std::vector<std::thread>ts;std::atomic<int>ok{0};for(int i=0;i<16;i++)ts.emplace_back([&](){if(run(p))++ok;});for(auto&t:ts)t.join();req(ok==16&&inits==1&&launches==16,"concurrent once/init+launch");++cases;
 printf("PASS MMA host cases=%d checks=%d concurrent_threads=16 init_exactly_once=1\n",cases,checks.load());
}
