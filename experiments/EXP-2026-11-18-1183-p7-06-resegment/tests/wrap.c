#include <string.h>
#include "/tmp/rs.c"
int64_t g_rs_call(const int64_t*sym,int64_t nchain,int64_t ntab,const int64_t*tok,const int64_t*lc,const int64_t*rc,
                  const int64_t*off,const int64_t*len,const char*text,int64_t ntext,
                  const int64_t*bid,int64_t*out){
    gg_slice a={(void*)sym,4},b={(void*)tok,4},c={(void*)lc,4},d={(void*)rc,4},
              e={(void*)off,4},f={(void*)len,4},g={(void*)text,ntext},h={(void*)bid,256},
              i={(void*)out,64};
    return gg_function_1(a,nchain,ntab,b,c,d,e,f,g,h,i,204,0);
}
