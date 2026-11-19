// Differential against the REAL native registry: bmoe_core's find_moe_recipe / moe_recipe_at,
// linked into this program, compared with the ported doors in libllama loaded at run time.
// No transcription is involved on the native side, which is what the previous check could not say.
#include <dlfcn.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include "core/include/bmoe/recipe.h"

struct sl_u8  { uint8_t * data; int64_t len; };
struct sl_i64 { int64_t * data; int64_t len; };

typedef int64_t (*fn_find)(sl_u8);
typedef int64_t (*fn_arch)(int64_t, sl_u8);
typedef int64_t (*fn_suffix)(int64_t, int64_t, sl_u8);
typedef int64_t (*fn_exps)(int64_t);
typedef int64_t (*fn_count)();
typedef int64_t (*fn_match)(sl_u8, int64_t, sl_i64);

int main() {
    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
    auto find   = (fn_find)   dlsym(h, "ar_find");
    auto arch   = (fn_arch)   dlsym(h, "ar_arch");
    auto suffix = (fn_suffix) dlsym(h, "ar_suffix");
    auto exps   = (fn_exps)   dlsym(h, "ar_exps");
    auto count  = (fn_count)  dlsym(h, "ar_count");
    auto match  = (fn_match)  dlsym(h, "ar_match");
    if (!find || !arch || !suffix || !exps || !count || !match) { printf("missing door\n"); return 3; }

    int fails = 0, cases = 0;
    const int n = bmoe::n_moe_recipes();
    if (count() != n) { printf("COUNT %lld vs %d\n", (long long) count(), n); ++fails; }

    for (int i = 0; i < n; ++i) {
        const bmoe::MoeRecipe * rec = bmoe::moe_recipe_at(i);
        // row -> arch
        uint8_t buf[128] = {0};
        sl_u8 sout{ buf, (int64_t) sizeof(buf) };
        const int64_t len = arch(i, sout);
        ++cases;
        if (len != (int64_t) strlen(rec->arch) || memcmp(buf, rec->arch, (size_t) len) != 0) {
            ++fails; printf("ARCH row=%d port=%.*s native=%s\n", i, (int) len, buf, rec->arch);
        }
        // arch -> row
        sl_u8 sin{ (uint8_t *) rec->arch, (int64_t) strlen(rec->arch) };
        const int64_t row = find(sin);
        ++cases;
        if (row != i) { ++fails; printf("FIND %s port=%lld native=%d\n", rec->arch, (long long) row, i); }

        int native_exps = 0;
        for (int s = 0; s < bmoe::MoeRecipe::max_exps; ++s) if (rec->exps_suffix[s]) ++native_exps;
        ++cases;
        if (exps(i) != native_exps) { ++fails; printf("EXPS row=%d port=%lld native=%d\n", i, (long long) exps(i), native_exps); }

        for (int s = 0; s < bmoe::MoeRecipe::max_exps; ++s) {
            uint8_t sbuf[128] = {0};
            sl_u8 s2{ sbuf, (int64_t) sizeof(sbuf) };
            const int64_t sl = suffix(i, s, s2);
            ++cases;
            if (rec->exps_suffix[s] == nullptr) {
                if (sl != -1) { ++fails; printf("SUFFIX row=%d slot=%d port=%lld native=null\n", i, s, (long long) sl); }
            } else if (sl != (int64_t) strlen(rec->exps_suffix[s]) ||
                       memcmp(sbuf, rec->exps_suffix[s], (size_t) sl) != 0) {
                ++fails; printf("SUFFIX row=%d slot=%d port=%.*s native=%s\n", i, s, (int) sl, sbuf, rec->exps_suffix[s]);
            }
            // matcher: the name built from the NATIVE suffix, and its .scale companion
            if (rec->exps_suffix[s] != nullptr) {
                const std::string pos = std::string("blk.3.") + rec->exps_suffix[s] + ".weight";
                int64_t lay[2] = { -1, -1 };
                sl_i64 slay{ lay, 2 };
                sl_u8 sp{ (uint8_t *) pos.data(), (int64_t) pos.size() };
                const int64_t got = match(sp, i, slay);
                ++cases;
                if (got != s || lay[0] != 3) { ++fails; printf("MATCH row=%d slot=%d port=%lld lay=%lld\n", i, s, (long long) got, (long long) lay[0]); }
                const std::string neg = std::string("blk.3.") + rec->exps_suffix[s] + ".scale";
                sl_u8 sn{ (uint8_t *) neg.data(), (int64_t) neg.size() };
                ++cases;
                if (match(sn, i, slay) != -1) { ++fails; printf("MATCH_SCALE row=%d slot=%d\n", i, s); }
            }
        }
    }
    printf("recipe differential: cases=%d fails=%d recipes=%d\n", cases, fails, n);
    return fails ? 1 : 0;
}
