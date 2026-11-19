// Reference: the native scan transcribed without the hash shortcut.
#include <cstdio>
#include <cstdint>
#include <vector>
#include <random>
extern "C" int64_t g_mp_call(const int64_t*,int64_t,const int64_t*,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
static int64_t ref(const std::vector<int64_t>& inp,const std::vector<int64_t>& key,int n,int m,
                   int64_t cur_len,int64_t slb,int64_t kml){
    int64_t match_pos = -1;
    if (slb > (size_t)(n+m+1)) {
        for (int64_t j = (int64_t) slb - n - m - 1; j > kml; --j) {
            bool ok = true;
            for (int k = 0; k < n; ++k) if (inp[j+k] != key[k]) { ok=false; break; }
            if (ok) { match_pos = j; break; }
        }
    }
    if (match_pos < 0) {
        for (int64_t j = cur_len - n - m - 1; j > slb && j > kml; --j) {
            bool ok = true;
            for (int k = 0; k < n; ++k) if (inp[j+k] != key[k]) { ok=false; break; }
            if (ok) { match_pos = j; break; }
        }
    }
    return match_pos;
}
int main(){
    std::mt19937 rng(9);
    int fails=0, trials=2000;
    for (int t=0;t<trials;++t){
        int len = 4 + (int)(rng()%20);
        std::vector<int64_t> inp(len);
        for (auto& v: inp) v = (int64_t)(rng()%3);
        int n = 1 + (int)(rng()%3), m = 1 + (int)(rng()%3);
        const int ks = (int)(rng()%(len-n));
        std::vector<int64_t> key(inp.begin() + ks, inp.begin() + ks + n);
        int64_t cur_len = len;
        int64_t slb = (int64_t)(rng() % (len+2));
        int64_t kml = (int64_t)(rng() % (len+1));
        int64_t want = ref(inp,key,n,m,cur_len,slb,kml);
        int64_t got = g_mp_call(inp.data(),len,key.data(),n,n,m,cur_len,slb,kml);
        if (got != want) { fails++; if (fails<=4) printf("MISMATCH t=%d got=%lld want=%lld (n=%d m=%d slb=%lld kml=%lld)\n",
            t,(long long)got,(long long)want,n,m,(long long)slb,(long long)kml); }
    }
    printf("match pos: trials=%d fails=%d\n", trials, fails);
    return fails?1:0;
}
