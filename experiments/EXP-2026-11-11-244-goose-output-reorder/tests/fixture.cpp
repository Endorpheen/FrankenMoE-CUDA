// Differential gate for the swap-minimising selection sort and its inverse map.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <random>
#include <algorithm>
extern "C" { int64_t fg244s_reorder(int64_t,int32_t*,int32_t*,int32_t*,int64_t); }
static int checks, failures;
static void ck(bool ok,const char*w,long long a,long long b,int at){++checks;
  if(!ok){if(failures<12)printf("FAIL %s at %d goose=%lld ref=%lld\n",w,at,a,b);++failures;}}

int main(void){
    std::mt19937 rng(244011);
    auto run=[&](std::vector<int> ids,int invlen,int at){
        const int64_t n=(int64_t)ids.size();
        std::vector<int32_t> ref=ids;
        std::vector<std::pair<int,int>> refsw;
        if (n>1){
            for (int64_t i=0;i<n-1;++i){
                int64_t jm=i;
                for (int64_t j=i+1;j<n;++j) if (ref[(size_t)j]<ref[(size_t)jm]) jm=j;
                if (jm==i) continue;
                std::swap(ref[(size_t)i],ref[(size_t)jm]);
                refsw.emplace_back((int)i,(int)jm);
            }
        }
        std::vector<int> refinv((size_t)invlen,-1);
        for (int64_t i=0;i<n;++i) refinv[(size_t)ref[(size_t)i]]=(int)i;

        std::vector<int32_t> got=ids, sw(2*(size_t)n+4,-7), inv((size_t)invlen+2,-7);
        const int64_t ns=fg244s_reorder(n,got.data(),sw.data(),inv.data(),invlen);
        ck(ns==(int64_t)refsw.size(),"nswaps",ns,(long long)refsw.size(),at);
        for (size_t k=0;k<refsw.size();++k){
            ck(sw[k*2]==refsw[k].first,"swap i",sw[k*2],refsw[k].first,at);
            ck(sw[k*2+1]==refsw[k].second,"swap j",sw[k*2+1],refsw[k].second,at);
        }
        for (int64_t i=0;i<n;++i) ck(got[(size_t)i]==ref[(size_t)i],"sorted",got[(size_t)i],ref[(size_t)i],at);
        for (int k=0;k<invlen;++k) ck(inv[(size_t)k]==refinv[(size_t)k],"inv",inv[(size_t)k],refinv[(size_t)k],at);
        ck(inv[(size_t)invlen]==-7,"inv no overrun",inv[(size_t)invlen],-7,at);
    };
    run({},4,0);
    run({0},4,1);
    run({0,1},4,2);              // already sorted -> zero swaps
    run({1,0},4,3);              // one swap
    run({3,2,1,0},4,4);          // reverse
    run({2,2,1},4,5);            // duplicate: first minimum wins
    run({5,5,5},6,6);            // all equal -> zero swaps
    for (int t=0;t<20000;++t){
        const int n=1+(int)(rng()%8);
        const int invlen=n+(int)(rng()%3);
        // ids must stay inside the inverse-map range: native indexes output_ids[out_ids[i]] without a
        // bound test, so an id >= invlen would write past the array. The module traps on that instead of
        // corrupting memory, which is why the generator has to respect the same precondition.
        std::vector<int> ids((size_t)n);
        for (auto& x:ids) x=(int)(rng()%(unsigned)invlen);   // duplicates on purpose
        run(ids,invlen,100+t);
    }
    printf("FG244S_FIXTURE checks=%d failures=%d\n",checks,failures);
    if(failures){printf("EXP244_LOCAL_GATES_FAIL\n");return 1;}
    printf("EXP244_LOCAL_GATES_PASS\n"); return 0;
}
