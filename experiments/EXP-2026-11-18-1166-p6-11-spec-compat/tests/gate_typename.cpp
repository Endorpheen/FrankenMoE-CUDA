// Compared against the native map itself, not against a re-typed list.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdint>
enum common_speculative_type { NONE_t, DRAFT_SIMPLE_t, DRAFT_EAGLE3_t, DRAFT_MTP_t, DRAFT_DFLASH_t, DRAFT_DSPARK_t, NGRAM_SIMPLE_t, NGRAM_MAP_K_t, NGRAM_MAP_K4V_t, NGRAM_MOD_t, NGRAM_CACHE_t, COUNT_t };
static const int COMMON_SPECULATIVE_TYPE_COUNT = 11;
extern "C" int64_t g_typename_call(const char*,int64_t);
static const char * native_name(int t) {
    switch (t) {
      case NONE_t: return "none";
      case DRAFT_SIMPLE_t: return "draft-simple";
      case DRAFT_EAGLE3_t: return "draft-eagle3";
      case DRAFT_MTP_t: return "draft-mtp";
      case DRAFT_DFLASH_t: return "draft-dflash";
      case DRAFT_DSPARK_t: return "draft-dspark";
      case NGRAM_SIMPLE_t: return "ngram-simple";
      case NGRAM_MAP_K_t: return "ngram-map-k";
      case NGRAM_MAP_K4V_t: return "ngram-map-k4v";
      case NGRAM_MOD_t: return "ngram-mod";
      case NGRAM_CACHE_t: return "ngram-cache";
      default: return nullptr;
    }
}
int main() {
    int fails = 0;
    for (int t = 0; t < COMMON_SPECULATIVE_TYPE_COUNT; ++t) {
        const char * n = native_name(t);
        if (!n) continue;
        int64_t r = g_typename_call(n, (int64_t) strlen(n));
        if (r != t) { fails++; printf("MISMATCH %s got=%lld want=%d\n", n, (long long) r, t); }
    }
    // near misses must not be accepted
    const char * bad[] = { "", "None", "draft-simpl", "draft-simple-", "ngram-map-k4", "xg", "unknown" };
    for (const char * b : bad) {
        int64_t r = g_typename_call(b, (int64_t) strlen(b));
        if (r != 11) { fails++; printf("MISMATCH reject %s got=%lld want=11\n", b, (long long) r); }
    }
    printf("spec type name: fails=%d\n", fails);
    return fails ? 1 : 0;
}
