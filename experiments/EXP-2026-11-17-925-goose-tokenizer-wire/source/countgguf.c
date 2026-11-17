#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <execinfo.h>
static int64_t calls = 0;
static const void * (*real_find)(const void *, const char *);
int64_t gguf_find_tensor(const void * ctx, const char * name) {
    if (!real_find) real_find = dlsym(RTLD_NEXT, "gguf_find_tensor");
    if (calls < 3) {
        void * bt[8];
        int n = backtrace(bt, 8);
        fprintf(stderr, "COUNT gguf_find_tensor(%s) from:\n", name ? name : "(null)");
        backtrace_symbols_fd(bt, n, 2);
    }
    calls++;
    return (int64_t) real_find(ctx, name);
}
__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "COUNT total gguf_find_tensor calls=%lld\n", (long long) calls);
}
