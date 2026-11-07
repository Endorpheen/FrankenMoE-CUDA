#ifdef FG141_KERNEL_ONLY
static void fg141_quantize(int64_t c,int64_t s,int64_t d,int64_t n){(void)c;(void)s;(void)d;(void)n;__builtin_trap();}
static void fg141_reset(int64_t c,int32_t n){(void)c;(void)n;__builtin_trap();}
static void fg141_barrier(int64_t c){(void)c;__builtin_trap();}
static int32_t fg141_claim(int64_t c){(void)c;__builtin_trap();return 0;}
#else
extern void fg141_quantize(int64_t,int64_t,int64_t,int64_t);
extern void fg141_reset(int64_t,int32_t);
extern void fg141_barrier(int64_t);
extern int32_t fg141_claim(int64_t);
#endif
