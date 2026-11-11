#include <initializer_list>
#include <cstdint>
#include <cstdio>
extern "C" int64_t fg254s_greedy(int64_t,int64_t,int64_t,int64_t);
static int64_t ref(int64_t n_probs,int64_t n_samplers,int64_t last,int64_t top_k){
    bool g = n_probs == 0 && n_samplers > 0 &&
             ((last == 0) || (last == 1 && top_k == 1));
    return g ? 1 : 0;
}
int main(){
    int cases=0, failures=0;
    for (int64_t np : {0,1,5}) for (int64_t ns : {0,1,3})
      for (int64_t last : {0,1,2}) for (int64_t k : {0,1,2,20}) {
        ++cases;
        if (fg254s_greedy(np,ns,last,k) != ref(np,ns,last,k)) {
            if (failures<3) printf("DIFF np=%lld ns=%lld last=%lld k=%lld\n",(long long)np,(long long)ns,(long long)last,(long long)k);
            ++failures;
        }
      }
    printf("FG254S_GATE cases=%d failures=%d\n",cases,failures);
    return failures?1:0;
}
