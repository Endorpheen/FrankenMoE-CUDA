/* The stage must be loadable and carry the bridge, with no model involved. */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>

int main(int argc, char ** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <stage.so>\n", argv[0]); return 2; }
    void * h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    int (*enabled)(void) = (int (*)(void)) dlsym(h, "fg345w_enabled");
    int64_t (*select)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
                      const int32_t*,const uint64_t*,int32_t*,int32_t*,int64_t) =
        (int64_t (*)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
                     const int32_t*,const uint64_t*,int32_t*,int32_t*,int64_t)) dlsym(h, "fg345w_select");
    if (!enabled || !select) { fprintf(stderr, "bridge symbols missing\n"); return 1; }
    printf("enabled=%d\n", enabled());
    /* one stream, two occupied cells, no SWA masking: one range, count 2 */
    int32_t pos[2] = { 5, 6 };
    uint64_t mask[2] = { 1, 1 };
    int32_t counts[1] = { -1 }, ranges[8] = { -1 };
    const int64_t n = select(0, 0, 0, 0, 1, 4, 0, 2, pos, mask, counts, ranges, 4);
    printf("ranges=%lld count=%d first=[%d,%d)\n", (long long) n, counts[0], ranges[0], ranges[1]);
    dlclose(h);
    return (n == 1 && counts[0] == 2 && ranges[0] == 0 && ranges[1] == 2) ? 0 : 1;
}
