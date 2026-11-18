// Reference: the native recursion, transcribed, over the same symbol table.
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
extern "C" int64_t g_rs_call(const int64_t*,int64_t,int64_t,const int64_t*,const int64_t*,const int64_t*,
                             const int64_t*,const int64_t*,const char*,int64_t,const int64_t*,int64_t*);
struct Tbl { const int64_t *tok,*lc,*rc,*off,*len; const char *text; int64_t ntext; const int64_t *bid; };
static void resegment(int s, const Tbl & t, std::vector<int64_t> & out) {
    if (t.tok[s] >= 0) { out.push_back(t.tok[s]); return; }
    if (t.lc[s] >= 0 && t.rc[s] >= 0) { resegment(t.lc[s], t, out); resegment(t.rc[s], t, out); return; }
    for (int64_t b = 0; b < t.len[s]; ++b) out.push_back(t.bid[(unsigned char) t.text[t.off[s] + b]]);
}
int main(){
    int64_t bid[256]; for (int i=0;i<256;++i) bid[i]=1000+i;
    struct C{const char*n;int64_t sym[4],tok[4],lc[4],rc[4],off[4],len[4];const char*text;int64_t nsym;};
    const C cs[]={
      {"single token",   {0,0,0,0},{7,0,0,0},{-1,0,0,0},{-1,0,0,0},{0,0,0,0},{1,0,0,0},"a",1},
      {"two tokens",     {0,1,0,0},{7,8,0,0},{-1,-1,0,0},{-1,-1,0,0},{0,1,0,0},{1,1,0,0},"ab",2},
      {"merge of tokens",{0,1,2,0},{-1,7,8,0},{1,-1,-1,0},{2,-1,-1,0},{0,0,1,0},{2,1,1,0},"ab",1},
      {"merge then byte",{0,1,2,0},{-1,7,-1,0},{1,-1,-1,0},{2,-1,-1,0},{0,0,1,0},{2,1,1,0},"ab",1},
      {"plain bytes",    {0,0,0,0},{-1,0,0,0},{-1,0,0,0},{-1,0,0,0},{0,0,0,0},{2,0,0,0},"ab",1},
      {"leaf multi byte",{0,1,2,3},{-1,-1,-1,-1},{-1,-1,-1,-1},{-1,-1,-1,-1},{0,0,1,2},{2,1,1,1},"abc",3},
    };
    int fails=0,n=(int)(sizeof(cs)/sizeof(cs[0]));
    for(int k=0;k<n;++k){
        int64_t out[64]; for(int i=0;i<64;++i) out[i]=-9;
        int64_t got=g_rs_call(cs[k].sym,cs[k].nsym,4,cs[k].tok,cs[k].lc,cs[k].rc,cs[k].off,cs[k].len,
                              cs[k].text,(int64_t)__builtin_strlen(cs[k].text),bid,out);
        Tbl t{cs[k].tok,cs[k].lc,cs[k].rc,cs[k].off,cs[k].len,cs[k].text,(int64_t)__builtin_strlen(cs[k].text),bid};
        std::vector<int64_t> want;
        for(int64_t s=0;s<cs[k].nsym;++s) resegment((int)s,t,want);
        bool ok = got==(int64_t)want.size();
        for(size_t i=0; ok && i<want.size(); ++i) if(out[i]!=want[i]) ok=false;
        if(!ok){fails++;printf("MISMATCH %s got=%lld want=%zu\n  got:",cs[k].n,(long long)got,want.size());
            for(int i=0;i<got&&i<64;++i)printf(" %lld",(long long)out[i]);
            printf("\n  want:"); for(auto v:want)printf(" %lld",(long long)v); printf("\n");}
    }
    printf("resegment: cases=%d fails=%d\n",n,fails);
    return fails?1:0;
}
