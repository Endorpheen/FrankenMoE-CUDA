#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint8_t  fg443_ld_u8 (uint64_t p, uint64_t o) { return ((const uint8_t  *)(uintptr_t)p)[o]; }
uint32_t fg443_ld_u32(uint64_t p, uint64_t o) { return *(const uint32_t *)((const char *)(uintptr_t)p + o); }
uint64_t fg443_ld_u64(uint64_t p, uint64_t o) { return *(const uint64_t *)((const char *)(uintptr_t)p + o); }
void     fg443_st_u64(uint64_t p, uint64_t o, uint64_t v) { *(uint64_t *)((char *)(uintptr_t)p + o) = v; }
int64_t fg443_parse(uint64_t buf, int64_t len, uint64_t out, int64_t cap);

int main(void) {
    const char * path = "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    FILE * fp = fopen(path, "rb");
    if (!fp) { perror("open"); return 1; }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char * buf = malloc((size_t) n);
    if (!buf || fread(buf, 1, (size_t) n, fp) != (size_t) n) { perror("read"); return 1; }
    fclose(fp);

    const int64_t cap = 8 * 1024 * 1024;
    uint64_t * out = calloc((size_t) cap / 8, 8);
    int64_t words = fg443_parse((uint64_t) (uintptr_t) buf, (int64_t) n,
                                (uint64_t) (uintptr_t) out, cap);
    printf("FG443_GGUF words=%lld\n", (long long) words);
    if (words <= 0) return 2;

    FILE * rf = fopen("records.bin", "wb");
    if (!rf) { perror("records"); return 1; }
    fwrite(out, 8, (size_t) words, rf);
    fclose(rf);

    int trunc_ok = 0, trunc_total = 0;
    const int64_t cuts[] = {0, 3, 4, 8, 16, 23, 24, 64, 1000, 1000000, 5000000, 10000000, 10990000, 10990918};
    for (size_t i = 0; i < sizeof(cuts) / sizeof(cuts[0]); ++i) {
        ++trunc_total;
        int64_t r = fg443_parse((uint64_t) (uintptr_t) buf, cuts[i],
                                (uint64_t) (uintptr_t) out, cap);
        if (r < 0) ++trunc_ok;
        else printf("TRUNC ACCEPTED cut=%lld r=%lld\n", (long long) cuts[i], (long long) r);
    }

    int bad = 0;
    unsigned char save = buf[0];
    buf[0] = 'X';
    if (fg443_parse((uint64_t) (uintptr_t) buf, (int64_t) n,
                    (uint64_t) (uintptr_t) out, cap) >= 0) ++bad;
    buf[0] = save;
    memset(buf + 8, 0xFF, 8);
    if (fg443_parse((uint64_t) (uintptr_t) buf, (int64_t) n,
                    (uint64_t) (uintptr_t) out, cap) >= 0) ++bad;
    printf("FG443_GGUF_TRUNC ok=%d/%d corrupt_bad=%d\n", trunc_ok, trunc_total, bad);
    return (trunc_ok == trunc_total && bad == 0) ? 0 : 3;
}
