#include <stdint.h>
#include "io-qualified.c"
// const declarations become module globals; initialise them before use
__attribute__((constructor)) static void fg458_boot(void){ fg458_init_globals(0, 0); }
int64_t fg458_fits(int64_t s,int64_t r){return fg458_function_1(s,r,0);}
int64_t fg458_arr(int64_t n,int64_t e,int64_t r,int64_t *out){return fg458_function_2(n,e,r,(uint64_t)(uintptr_t)out,0);}
int64_t fg458_str(int64_t s,int64_t r,int64_t *out){return fg458_function_5(s,r,(uint64_t)(uintptr_t)out,0);}
int64_t fg458_seek(int64_t a,int64_t o,int64_t r,int64_t *out){return fg458_function_6(a,o,r,(uint64_t)(uintptr_t)out,0);}
int64_t fg458_raw(int64_t s,int64_t c,int64_t o,int64_t r,int64_t end,int64_t *out){ return fg458_function_7(s,c,o,r,end,(uint64_t)(uintptr_t)out,0);}
int64_t fg458_fremain(int64_t end,int64_t cur,int64_t *out){return fg458_function_8(end,cur,(uint64_t)(uintptr_t)out,0);}
