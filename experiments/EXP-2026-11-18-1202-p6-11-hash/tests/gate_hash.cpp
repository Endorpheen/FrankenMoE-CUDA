// Reference: the native uint32_t arithmetic, verbatim.
#include <cstdio>
#include <cstdint>
#include <vector>
#include <random>
#define LCG_FACTOR 2654435761UL
extern "C" int64_t g_hash_call(const int64_t*,int64_t,int64_t,int64_t,int64_t);
extern "C" int64_t g_slot_call(int64_t,int64_t);
static uint32_t ref_hash(const std::vector<int64_t>& t, size_t start, size_t len) {
    uint32_t hash = 0;
    for (size_t i = 0; i < len; ++i) hash = hash * LCG_FACTOR + (uint32_t) t[start + i];
    return hash;
}
int main(){
    std::mt19937 rng(13);
    int fails = 0, cases = 0;
    for (int it = 0; it < 500; ++it) {
        int len = 1 + (int)(rng()%6);
        std::vector<int64_t> t(len);
        for (auto& v : t) v = (int64_t)(rng()%200000);   // vocabulary-sized ids
        uint32_t want = ref_hash(t, 0, len);
        int64_t got = g_hash_call(t.data(), len, 0, len, (int64_t) LCG_FACTOR);
        cases++;
        if (got != (int64_t) want) { fails++; if (fails<=3) printf("HASH MISMATCH len=%d got=%lld want=%u\n", len, (long long) got, want); }
        // slot
        int64_t slots = 1 + (int64_t)(rng()%4096);
        int64_t wslot = (int64_t)(want % (uint32_t) slots);
        int64_t gslot = g_slot_call(got, slots);
        if (gslot != wslot) { fails++; if (fails<=3) printf("SLOT MISMATCH got=%lld want=%lld\n", (long long) gslot, (long long) wslot); }
    }
    // boundary cases where the product overflows a signed 64-bit integer
    for (int64_t v : {(int64_t)0xFFFFFFFFLL, (int64_t)0xFFFFFFFELL, (int64_t)0x80000000LL}) {
        std::vector<int64_t> t = {v, v, v};
        uint32_t want = ref_hash(t, 0, 3);
        int64_t got = g_hash_call(t.data(), 3, 0, 3, (int64_t) LCG_FACTOR);
        cases++;
        if (got != (int64_t) want) { fails++; printf("BOUNDARY MISMATCH v=%lld got=%lld want=%u\n", (long long) v, (long long) got, want); }
    }
    printf("ngram hash: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
