// Compare the Goose metadata formatter with the load log of the accepted library.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "unit.h"

int main(int argc, char ** argv) {
    goose_init(0, 0);
    int fd = open(argv[1], O_RDONLY); struct stat st; fstat(fd, &st);
    size_t cap = st.st_size < 67108864 ? (size_t) st.st_size : 67108864;
    unsigned char * base = mmap(NULL, cap, PROT_READ, MAP_PRIVATE, fd, 0);
    static int64_t st8[8], kv[512*8], kvsl[2048], tens[4096*12], tensl[16384];
    if (gr_parse((sl_u8){base, (int64_t) cap}, (int64_t) st.st_size, (sl_i64){st8,8},
                 (sl_i64){kv,512*8}, (sl_i64){tens,4096*12}, (sl_i64){kvsl,2048}, (sl_i64){tensl,16384}) != 0) {
        printf("parse failed\n"); return 1;
    }
    const int64_t n = st8[2];
    unsigned char out[4096];
    for (int64_t i = 0; i < n; ++i) {
        const int64_t at = kv[i*8+0], len = kv[i*8+1];
        int64_t w = gr_kv_to_str((sl_u8){base, (int64_t) cap}, (sl_i64){st8,8}, (sl_i64){kv,512*8}, i, 40, (sl_u8){out, sizeof out});
        if (w < 0) { printf("KV %lld <fmt failed>\n", (long long) i); continue; }
        if (w > 40) { memcpy(out + 37, "...", 3); w = 40; }
        // the loader escapes newlines in the value
        unsigned char esc[4096]; int e = 0;
        for (int k = 0; k < (int) w; ++k) { if (out[k] == '\n') { esc[e++] = '\\'; esc[e++] = 'n'; } else esc[e++] = out[k]; }
        printf("KV %lld: %.*s = %.*s\n", (long long) i, (int) len, (char *)(base + at), e, esc);
    }
    return 0;
}
