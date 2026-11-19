#include "/tmp/mp.c"
int64_t g_mp_call(const int64_t* inp,int64_t ni,const int64_t* key,int64_t nk,int64_t n,int64_t m,
                  int64_t cl,int64_t slb,int64_t kml){
    gg_slice a={(void*)inp,ni},b={(void*)key,nk};
    return gg_function_1(a,b,n,m,cl,slb,kml,206,0);
}
