// Smoke call for the GGUF set (metadata + writer + tensors + split): write an
// empty GGUF v3 header and print its size and magic.
#include "gguf.h"

#include <stdio.h>
#include <string.h>

void goose_init_gguf(int argc, char **argv);
int64_t s2_write(sl_i64 st, sl_u8 pool, sl_i64 key_off, sl_i64 key_len,
                 sl_i64 types, sl_i64 arr, sl_i64 v0, sl_i64 v1, sl_i64 v2,
                 sl_u8 out, int64_t version, int64_t alignment, int64_t only_meta);

int run_gguf(void) {
    static int64_t st[8];        /* st[S_N] = 0: no metadata keys */
    static int64_t empty[8];
    static uint8_t pool[8];
    static uint8_t out[256];
    char *argv[] = {(char *)"smoke", NULL};

    goose_init_gguf(1, argv);
    sl_i64 sst = {st, 8}, sempty = {empty, 0};
    sl_u8 spool = {pool, 0}, sout = {out, (int64_t)sizeof out};
    int64_t n = s2_write(sst, spool, sempty, sempty, sempty, sempty, sempty,
                         sempty, sempty, sout, 3, 32, 1);

    printf("gguf:   s2_write(version=3, alignment=32, kv=0) -> %lld bytes, magic=",
           (long long)n);
    if (n >= 4) printf("%c%c%c%c", out[0], out[1], out[2], out[3]);
    printf("\n");
    return (n == 32 && memcmp(out, "GGUF", 4) == 0) ? 0 : 1;
}
