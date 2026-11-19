// Regression gate for the real Apex failing case (cne1 = 1).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>

struct fg_args {
    const uint8_t * src0; int64_t ne00, ne01, nb01;
    const uint8_t * act;  int64_t nbw1;
    const int32_t * routes; int64_t cne1;
    int64_t nb0, nb1, nb2;
    float * dst; int32_t ith, nth;
};

extern "C" int32_t fg_cpu_simd_mmid_tiled_q4_k(const void * a, uint8_t * scratch);

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "evidence-mmid/failing_case.bin";
    FILE * f = fopen(path, "rb");
    if (!f) { perror(path); return 2; }
    int64_t h[11];
    if (fread(h, 8, 10, f) != 10) { printf("short header\n"); return 2; }
    const int64_t K = h[0], R = h[1], nb01 = h[2], nbw1 = h[3], cne1 = h[4];
    const int64_t nb0 = h[5], nb1 = h[6], nb2 = h[7], count = h[9];

    std::vector<uint8_t> src0((size_t) (R * nb01));
    std::vector<uint8_t> act((size_t) (cne1 * nbw1));
    if (fread(src0.data(), 1, src0.size(), f) != src0.size()) { printf("short src0\n"); return 2; }
    if (fread(act.data(), 1, act.size(), f) != act.size()) { printf("short act\n"); return 2; }
    std::vector<int32_t> routes(2 * cne1, 0);
    if (fread(routes.data(), 4, 2 * cne1, f) != (size_t) (2 * cne1)) { printf("short routes\n"); return 2; }
    std::vector<float> expected((size_t) count);
    std::vector<uint8_t> rec((size_t) count * 8);
    if (fread(rec.data(), 1, rec.size(), f) != rec.size()) { printf("short records\n"); return 2; }
    for (int64_t i = 0; i < count; i++) memcpy(&expected[(size_t) i], rec.data() + i * 8 + 4, 4);
    fclose(f);

    const int64_t dst_rows = R;
    const int64_t dst_size = (dst_rows - 1) * nb0 + (int64_t) routes[0] * nb1 + (int64_t) routes[1] * nb2 + 4;
    std::vector<float> dst((size_t) (dst_size / 4 + 4), 0.0f);
    uint8_t * scratch = (uint8_t *) aligned_alloc(64, 512u * 1024u);
    memset(scratch, 0, 512u * 1024u);

    for (int t = 0; t < 1; t++) {
        fg_args a{ src0.data(), K, R, nb01, act.data(), nbw1, routes.data(), cne1,
                   nb0, nb1, nb2, dst.data(), t, 1 };
        fg_cpu_simd_mmid_tiled_q4_k(&a, scratch);
    }

    int64_t same = 0;
    for (int64_t i = 0; i < count; i++) {
        uint32_t x, y;
        const int64_t off = i * nb0 + (int64_t) routes[0] * nb1 + (int64_t) routes[1] * nb2;
        const float got = *(const float *) ((const char *) dst.data() + off);
        memcpy(&x, &got, 4);
        memcpy(&y, &expected[(size_t) i], 4);
        if (x == y) same++;
        else if (i < 8) printf("row %lld expected=%.9g got=%.9g\n", (long long) i, expected[(size_t) i], got);
    }
    printf("case cne1=%lld rows=%lld compared=%lld equal=%lld differ=%lld\n",
           (long long) cne1, (long long) dst_rows, (long long) count, (long long) same,
           (long long) (count - same));
    free(scratch);
    return same == count ? 0 : 1;
}
