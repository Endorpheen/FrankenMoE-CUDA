#include "/tmp/wlsv.c"
int64_t g_save_call(const char * arch, int64_t la, int64_t nl, int64_t tk,
                    const int64_t * pr, int64_t npr, const int64_t * ou, int64_t nou,
                    const int64_t * ro, int64_t nro, char * out) {
    gg_slice a={(void*)arch,la}, p={(void*)pr,16}, o={(void*)ou,16}, r={(void*)ro,64}, d={(void*)out,512};
    return gg_function_1(a,nl,tk,p,npr,o,nou,r,nro,d,213,0);
}
