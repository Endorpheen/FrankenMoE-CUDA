#include "/tmp/sm.c"
int64_t g_sm_call(int64_t n,const int64_t*pl,const int64_t*pr,const double*ps,int64_t np,
                  int64_t*nx,int64_t*pv,int64_t*al,int64_t*out){
    gg_slice a={(void*)pl,16},b={(void*)pr,16},c={(void*)ps,16},d={(void*)nx,16},
              e={(void*)pv,16},f={(void*)al,16},g={(void*)out,16};
    return gg_function_1(n,a,b,c,np,d,e,f,g,203,0);
}
