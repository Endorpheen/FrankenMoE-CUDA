#include "/tmp/cu.c"
int64_t g_cu_call(const int64_t* inp, int64_t n, int64_t nn, int64_t mn, int64_t mx,
                  int64_t* keys, int64_t* vals, int64_t* cnt, int64_t* szs, int64_t maxp) {
    gg_slice a={(void*)inp,n},b={(void*)keys,maxp*4},c={(void*)vals,maxp},d={(void*)cnt,maxp},e={(void*)szs,maxp};
    return gg_function_1(a,nn,mn,mx,b,c,d,e,maxp,205,0);
}
