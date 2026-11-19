// Reference: the native loop, transcribed. okmask is built from the same length set.
#include <cstdio>
#include <cstdint>
#include <algorithm>
extern "C" int64_t g_wpm_call(int64_t,int64_t,int64_t,int64_t);
static int64_t ref(int64_t n,int64_t i,int64_t maxlen,int64_t okmask){
    int64_t j = std::min(n, i + maxlen + 1);
    for (; j > i; --j) if ((okmask >> j) & 1) return j - i;
    return -1;
}
struct C{const char*name;int64_t len,i,ml,mask,want;};
int main(){
    const C cs[]={
      {"longest wins",              10,0,3, (1<<3)|(1<<1), 3},
      {"shorter when longer absent",10,0,3, (1<<2)|(1<<1), 2},
      {"bounded by n",               4,0,10,(1<<4)|(1<<3), 4},
      {"maxlen+1 bound",            10,0,2, (1<<3)|(1<<1), 3},
      {"start offset",              10,5,2, (1<<7)|(1<<6), 2},
      {"nothing",                   10,0,3, 0, -1},
      {"single",                     3,0,0, (1<<1), 1},
    };
    int fails=0,n=(int)(sizeof(cs)/sizeof(cs[0]));
    for(int k=0;k<n;++k){
        int64_t r=g_wpm_call(cs[k].len,cs[k].i,cs[k].ml,cs[k].mask);
        int64_t e=ref(cs[k].len,cs[k].i,cs[k].ml,cs[k].mask);
        if(r!=e||r!=cs[k].want){fails++;printf("MISMATCH %s goose=%lld ref=%lld want=%lld\n",cs[k].name,(long long)r,(long long)e,(long long)cs[k].want);}
    }
    printf("wpm match: cases=%d fails=%d\n",n,fails);
    return fails?1:0;
}
