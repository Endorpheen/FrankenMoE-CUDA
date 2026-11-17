#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <execinfo.h>
static int64_t calls = 0;
static int64_t (*real_find)(const void *, const char *);
int64_t gguf_find_key(const void * ctx, const char * key) {
    if (!real_find) real_find = dlsym(RTLD_NEXT, "gguf_find_key");
    if (calls < 3) {
        void * bt[6]; int n = backtrace(bt, 6);
        fprintf(stderr, "COUNT key(%s) from:\n", key ? key : "(null)");
        backtrace_symbols_fd(bt, n, 2);
    }
    calls++;
    return real_find(ctx, key);
}
__attribute__((destructor)) static void report(void) { fprintf(stderr, "COUNT gguf_find_key calls=%lld\n", (long long) calls); }
