// Differential gate for the three KV scanning helpers.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
#include <algorithm>
extern "C" {
int64_t fg233c_has_shift(int64_t,const int32_t*);
int64_t fg233c_pos_min(int64_t,const int32_t*);
int64_t fg233c_used_max_p1(int64_t,const int32_t*);
}
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b){++checks;
  if(!ok){if(failures<15)printf("FAIL %s goose=%lld ref=%lld\n",w,a,b);++failures;}}

int main(void){
    std::mt19937 rng(233011);
    // has_shift: OR over streams
    for (int t=0;t<3000;++t) {
        const int n=(int)(rng()%5);
        std::vector<int32_t> f(n);
        bool any=false;
        for (int i=0;i<n;++i){ f[i]=(int32_t)(rng()%2); any = any || f[i]; }
        const int64_t got=fg233c_has_shift(n, n?f.data():nullptr);
        ck(got == (any?1:0), "has_shift", got, any?1:0);
    }
    // pos_min: first element or -1
    for (int t=0;t<3000;++t) {
        const int n=(int)(rng()%4);
        std::vector<int32_t> p(n);
        for (int i=0;i<n;++i) p[i]=(int32_t)(rng()%5000)-100;
        std::sort(p.begin(), p.end());           // native keeps the set ordered
        const int64_t got=fg233c_pos_min(n, n?p.data():nullptr);
        const int64_t ref = n? (int64_t)p[0] : -1;
        ck(got==ref, "pos_min", got, ref);
    }
    // used_max_p1: max+1, with the set sorted so the last element dominates
    for (int t=0;t<4000;++t) {
        const int n=(int)(rng()%6);
        std::vector<int32_t> u(n);
        for (int i=0;i<n;++i) u[i]=(int32_t)(rng()%4096);
        std::sort(u.begin(), u.end());
        u.erase(std::unique(u.begin(),u.end()),u.end());
        const int64_t got=fg233c_used_max_p1((int64_t)u.size(), u.empty()?nullptr:u.data());
        const int64_t ref = u.empty()? 0 : (int64_t)u.back()+1;
        ck(got==ref, "used_max_p1", got, ref);
    }
    // explicit edges
    ck(fg233c_pos_min(0,nullptr)==-1, "empty pos_min", fg233c_pos_min(0,nullptr), -1);
    ck(fg233c_used_max_p1(0,nullptr)==0, "empty used", fg233c_used_max_p1(0,nullptr), 0);
    int32_t z={0}; ck(fg233c_used_max_p1(1,&z)==1, "used {0} -> 1", fg233c_used_max_p1(1,&z), 1);
    printf("FG233C_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP233_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP233_LOCAL_GATES_PASS\n"); return 0;
}
