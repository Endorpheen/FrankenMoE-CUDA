/* Host for goose/cuda/cuda-arch-policy.goose.
 *
 * The configure-time gap P10-06 names: CMake needs CMAKE_CUDA_ARCHITECTURES before the main
 * program exists, so the decision has to be answerable by something small that runs first. This is
 * that something. It carries no decision of its own - every answer comes from the Goose module.
 *
 *   DEFAULT <cuda_major> <cuda_minor> <native> <cmake_major> <cmake_minor>  -> hex of the list
 *   RESOLVE <arch> <native_value>                                          -> hex of the list
 *   SM      <arch_entry>                                                   -> SM number, or -1
 *   VALID   <value> <list_mode>                                            -> 0|1
 *
 * Usage as a command: `arch DEFAULT 12 4 0 3 24` prints the CMake value, which is what a
 * configure-time caller feeds to -DCMAKE_CUDA_ARCHITECTURES.
 */
#include "arch.h"
#include "host_io.h"

extern void goose_init(int argc, char **argv);

#include <stdio.h>
#include <string.h>

#define HIO_ARG_CAP 4096
static uint8_t args_a[HIO_ARG_CAP];

static int64_t dec_cap(const char *field, uint8_t *dst, int64_t cap, sl_u8 *out) {
    int64_t n = hio_decode(field, dst, cap);
    if (n < 0) return -1;
    out->data = dst;
    out->len = n;
    return n;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s DEFAULT|RESOLVE|SM|VALID ...\n", argv[0]);
        return 2;
    }
    goose_init(argc, argv);
    char out[4096];
    memset(out, 0, sizeof out);
    const sl_u8 buf = { (uint8_t *) out, (int64_t) sizeof out - 1 };

    if (strcmp(argv[1], "DEFAULT") == 0) {
        if (argc < 7) { fprintf(stderr, "DEFAULT needs 5 numbers\n"); return 2; }
        const int64_t n = arch_default(strtoll(argv[2], 0, 10), strtoll(argv[3], 0, 10),
                                       strtoll(argv[4], 0, 10), strtoll(argv[5], 0, 10),
                                       strtoll(argv[6], 0, 10), buf);
        if (n < 0) return 1;
        fwrite(out, 1, (size_t) n, stdout);
        printf("\n");
        return 0;
    }
    if (strcmp(argv[1], "RESOLVE") == 0) {
        sl_u8 native = { NULL, 0 };
        if (argc < 5) { fprintf(stderr, "RESOLVE needs <arch> <native_value>\n"); return 2; }
        if (dec_cap(argv[3], args_a, HIO_ARG_CAP, &native) < 0) return 2;
        const sl_u8 arch = { (uint8_t *) argv[2], (int64_t) strlen(argv[2]) };
        const int64_t n = arch_resolve(arch, native, buf);
        if (n < 0) return 1;
        fwrite(out, 1, (size_t) n, stdout);
        printf("\n");
        return 0;
    }
    if (strcmp(argv[1], "SM") == 0) {
        if (argc < 3) { fprintf(stderr, "SM needs <arch_entry>\n"); return 2; }
        const sl_u8 s = { (uint8_t *) argv[2], (int64_t) strlen(argv[2]) };
        printf("%lld\n", (long long) arch_sm(s));
        return 0;
    }
    if (strcmp(argv[1], "VALID") == 0) {
        if (argc < 4) { fprintf(stderr, "VALID needs <value> <list_mode>\n"); return 2; }
        sl_u8 v = { NULL, 0 };
        if (dec_cap(argv[2], args_a, HIO_ARG_CAP, &v) < 0) return 2;
        printf("%lld\n", (long long) arch_valid(v, strtoll(argv[3], 0, 10)));
        return 0;
    }
    fprintf(stderr, "unknown command: %s\n", argv[1]);
    return 2;
}
