#include "/tmp/wlt.c"
int64_t g_topk_call(int64_t c,int64_t r,int64_t ep,int64_t eb,int64_t ebn,int64_t el,int64_t etk,int64_t etn,
                    int64_t ap,int64_t ab,int64_t abn,int64_t al,int64_t atk,int64_t atn,int64_t*out){
    gg_slice o={(void*)out,1};
    return gg_function_1(c,r,ep,eb,ebn,el,etk,etn,ap,ab,abn,al,atk,atn,o,216,0);
}
