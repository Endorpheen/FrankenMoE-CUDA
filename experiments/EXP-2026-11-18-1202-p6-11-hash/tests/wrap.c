#include "/tmp/hs.c"
int64_t g_hash_call(const int64_t* t,int64_t nt,int64_t st,int64_t len,int64_t lcg){
    gg_slice a={(void*)t,nt};
    return gg_function_1(a,st,len,lcg,207,0);
}
int64_t g_slot_call(int64_t h,int64_t sl){ return gg_function_2(h,sl,0); }
