#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint8_t  fg443_ld_u8 (uint64_t p, uint64_t o) { return ((const uint8_t  *)(uintptr_t)p)[o]; }
uint32_t fg443_ld_u32(uint64_t p, uint64_t o) { return *(const uint32_t *)((const char *)(uintptr_t)p + o); }
uint64_t fg443_ld_u64(uint64_t p, uint64_t o) { return *(const uint64_t *)((const char *)(uintptr_t)p + o); }
void     fg443_st_u64(uint64_t p, uint64_t o, uint64_t v) { *(uint64_t *)((char *)(uintptr_t)p + o) = v; }
int64_t fg443_parse(uint64_t buf, int64_t len, uint64_t out, int64_t cap);
int64_t fg443_lookup(uint64_t buf, int64_t len, uint64_t kp, int64_t kl, uint64_t out, int64_t cap);


static uint64_t fnv(const unsigned char * p, size_t n) {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}
static size_t tsize(uint64_t t) {
    if (t == 0 || t == 1 || t == 7) return 1;
    if (t == 2 || t == 3) return 2;
    if (t == 4 || t == 5 || t == 6) return 4;
    if (t == 10 || t == 11 || t == 12) return 8;
    return 0;
}
static uint64_t value_hash(const unsigned char * b, uint64_t type, uint64_t off, uint64_t len, uint64_t et, uint64_t cnt) {
    if (type != 9) return fnv(b + off, (size_t) len);
    if (et == 8) {
        uint64_t h = 14695981039346656037ULL;
        size_t p = (size_t) off;
        for (uint64_t i = 0; i < cnt; ++i) {
            uint64_t sl = *(const uint64_t *) (b + p);
            uint64_t eh = fnv(b + p + 8, (size_t) sl);
            for (int k = 0; k < 8; ++k) h = (h ^ (eh & 0xff)) * 1099511628211ULL, eh >>= 8;
            p += 8 + (size_t) sl;
        }
        return h;
    }
    size_t es = tsize(et);
    return fnv(b + off, (size_t) cnt * es);
}

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


    const char * keys[] = {"general.architecture", "llama.context_length", "tokenizer.ggml.model",
                           "tokenizer.ggml.tokens", "tokenizer.ggml.scores", "general.alignment"};
    FILE * lf = fopen("lookup.txt", "w");
    if (!lf) { perror("lookup"); return 1; }
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) {
        uint64_t rec[5] = {0,0,0,0,0};
        int64_t ok = fg443_lookup((uint64_t)(uintptr_t)buf, (int64_t)n,
            (uint64_t)(uintptr_t)keys[i], (int64_t)strlen(keys[i]),
            (uint64_t)(uintptr_t)rec, 40);
        if (ok == 1) {
            uint64_t h = value_hash(buf, rec[0], rec[1], rec[2], rec[3], rec[2]);
            fprintf(lf, "%s type=%llu off=%llu len=%llu et=%llu cnt=%llu hash=%016llx\n",
                keys[i], (unsigned long long)rec[0], (unsigned long long)rec[1],
                (unsigned long long)rec[2], (unsigned long long)rec[3],
                (unsigned long long)(rec[0] == 9 ? rec[2] : 0), (unsigned long long)h);
        } else {
            fprintf(lf, "%s absent rc=%lld\n", keys[i], (long long)ok);
        }
    }
    fclose(lf);
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
