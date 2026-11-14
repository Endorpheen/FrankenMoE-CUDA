// P9-01 S3 gate: CLI flag/env/default resolution.
//
// The three native arms are transcriptions of runtime/cli/main.cpp:
//   * env_int (42-45)                      -> native_env_int;
//   * the env override block (792-802)     -> the field table below;
//   * the cache_auto default (804-810)     -> native_cache_auto;
//   * the special flag branches            -> native_cache_mb_auto,
//                                             native_dense_mode, native_draft_pick.
// The source is main.cpp, which cannot be linked without the engine, so the
// transcription is the oracle here — quoted line by line, with a pinned table
// on top so a transcription that drifts from the source is still caught.
//
// No model, no llama.cpp, no environment. Build: tests/build.sh cc.

#include "cc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int failures = 0;

static void fail(const char * what, const std::string & detail) {
    std::printf("[FAIL] %s: %s\n", what, detail.c_str());
    ++failures;
}

// main.cpp:42-45
//   static int env_int(const char * k, int dflt) {
//       const char * v = std::getenv(k);
//       return (v && *v) ? std::atoi(v) : dflt;
//   }
static long long native_env_int(bool seen, long long flag, bool has_env, const char * raw, long long dflt) {
    if (seen) return flag;
    return (has_env && raw && *raw) ? std::atoi(raw) : dflt;
}

static long long native_env_bool(bool seen, long long flag, bool has_env, const char * raw, long long dflt) {
    return native_env_int(seen, flag, has_env, raw, dflt) != 0 ? 1 : 0;
}

// main.cpp:804-810
static int native_cache_auto(bool moe_on, bool cache_auto, bool cache_mb_seen, bool env_cache_present) {
    if (moe_on && !cache_auto && !cache_mb_seen && !env_cache_present) return 1;
    return cache_auto ? 1 : 0;
}

// main.cpp:695-699
static bool native_cache_mb_auto(const char * v) { return std::string(v) == "auto"; }

// main.cpp:726-741
static int native_dense_mode(const char * m) {
    if (std::strcmp(m, "mmap") == 0) return 0;
    if (std::strcmp(m, "warm") == 0) return 1;
    if (std::strcmp(m, "anon") == 0) return 2;
    if (std::strcmp(m, "ahwb") == 0) return 3;
    return -1;
}

// main.cpp:659-668: the native prints and returns 2; the port reports -1.
static int native_draft_pick(int cur, int want) { return (cur != 0 && cur != want) ? -1 : want; }

// The field table of main.cpp:792-802, in order.
struct FieldRow {
    const char * env;
    const char * flag;
    long long dflt;
};
static const FieldRow FIELDS[11] = {
    {"BMOE_CACHE_MB", "--cache-mb", 0},
    {"BMOE_IO_THREADS", "--io-threads", 4},
    {"BMOE_PROGRESS", "--progress", 0},
    {"BMOE_OVERLAP", "--overlap", 0},
    {"BMOE_PREFETCH", "--prefetch", 0},
    {"BMOE_N_EXPERT_USED", "--n-expert-used", 0},
    {"BMOE_GPU_LAYERS", "--gpu-layers", 0},
    {"BMOE_VRAM_CACHE_MB", "--vram-cache-mb", 0},
    {"BMOE_EXPERT_HOT_SLOTS", "--expert-hot-slots", 0},
    {"BMOE_PREDICT_LOG", "--predict-log", 0},
    {"BMOE_PREDICT_PREFETCH", "--predict-prefetch", 0},
};

static std::string env_name(long long field) {
    char buf[64] = {0};
    long long n = cc_env_name(field, sl_u8{(uint8_t *) buf, (int64_t) sizeof buf});
    return n < 0 ? std::string("<none>") : std::string(buf, (size_t) n);
}

static std::string flag_name(long long field) {
    char buf[64] = {0};
    long long n = cc_flag_name(field, sl_u8{(uint8_t *) buf, (int64_t) sizeof buf});
    return n < 0 ? std::string("<none>") : std::string(buf, (size_t) n);
}

static sl_u8 slice(const std::string & s) {
    return sl_u8{(uint8_t *) s.data(), (int64_t) s.size()};
}

struct Lcg {
    unsigned long long s = 0x2545F4914F6CDD1Dull;
    unsigned long long next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return s >> 13;
    }
    int pick(int n) { return (int) (next() % (unsigned long long) n); }
};

int main() {
    goose_init(0, nullptr);

    // 1. The field table: names, flags and defaults, port against pin.
    for (int f = 0; f < 11; ++f) {
        if (env_name(f) != FIELDS[f].env) fail("cc_env_name", std::to_string(f) + " -> " + env_name(f));
        if (flag_name(f) != FIELDS[f].flag) fail("cc_flag_name", std::to_string(f) + " -> " + flag_name(f));
        if (cc_env_default(f) != FIELDS[f].dflt) fail("cc_env_default", std::to_string(f));
    }
    if (env_name(11) != "<none>" || env_name(-1) != "<none>") fail("cc_env_name range", "");
    if (flag_name(11) != "<none>") fail("cc_flag_name range", "");
    if (cc_env_default(11) != -1) fail("cc_env_default range", "");

    // 2. atoi, against the real std::atoi.
    const char * atoi_cases[] = {
        "", "0", "1", "-1", "12", " 12", "\t12", "\n12", " +7", "-007", "12x", "x12", "  ", "+", "-",
        "auto", "12.5", "0x10", "999999999", "-2147483647", "2147483647", "1500abc",
    };
    long long atoi_checks = 0;
    for (const char * s : atoi_cases) {
        long long want = (long long) std::atoi(s);
        long long got = cc_atoi(slice(s));
        ++atoi_checks;
        if (want != got) {
            char d[160];
            std::snprintf(d, sizeof d, "`%s` native=%lld port=%lld", s, want, got);
            fail("cc_atoi", d);
        }
    }

    // 3. The int/bool precedence matrix: flag wins even when it repeats the
    //    default; an empty variable is absent; a non-numeric value is 0.
    const char * envs[] = {"", "0", "1", "4", "-3", "12", " 7", "12x", "+5", "auto", "  ", "1500", "-0"};
    long long matrix = 0;
    for (const char * raw : envs) {
        for (int seen = 0; seen <= 1; ++seen) {
            for (int has = 0; has <= 1; ++has) {
                for (long long flag : {0LL, 4LL, 9LL}) {
                    for (long long dflt : {0LL, 4LL, -2LL}) {
                        long long want = native_env_int(seen != 0, flag, has != 0, raw, dflt);
                        long long got = cc_env_int(seen, flag, has, slice(raw), dflt);
                        ++matrix;
                        if (want != got) {
                            char d[200];
                            std::snprintf(d, sizeof d, "raw=`%s` seen=%d has=%d flag=%lld dflt=%lld native=%lld port=%lld",
                                          raw, seen, has, flag, dflt, want, got);
                            fail("cc_env_int", d);
                        }
                        long long wb = native_env_bool(seen != 0, flag, has != 0, raw, dflt);
                        long long gb = cc_env_bool(seen, flag, has, slice(raw), dflt);
                        ++matrix;
                        if (wb != gb) fail("cc_env_bool", std::string(raw) + " bool");
                    }
                }
            }
        }
    }

    // 4. cache_auto: all 16 combinations, transcription and pin.
    for (int moe = 0; moe <= 1; ++moe)
        for (int ca = 0; ca <= 1; ++ca)
            for (int seen = 0; seen <= 1; ++seen)
                for (int env = 0; env <= 1; ++env) {
                    int want = native_cache_auto(moe != 0, ca != 0, seen != 0, env != 0);
                    long long got = cc_cache_auto(moe, ca, seen, env);
                    if (want != got) {
                        char d[160];
                        std::snprintf(d, sizeof d, "moe=%d auto=%d seen=%d env=%d native=%d port=%lld", moe, ca,
                                      seen, env, want, got);
                        fail("cc_cache_auto", d);
                    }
                }
    // Pins for the two cases the rule exists for.
    if (cc_cache_auto(1, 0, 0, 0) != 1) fail("cache_auto pin", "--moe-stream defaults to auto");
    if (cc_cache_auto(1, 0, 1, 0) != 0) fail("cache_auto pin", "typed --cache-mb 0 wins");
    if (cc_cache_auto(1, 0, 0, 1) != 0) fail("cache_auto pin", "BMOE_CACHE_MB wins");
    if (cc_cache_auto(0, 0, 0, 0) != 0) fail("cache_auto pin", "no streaming, no auto");

    // 5. --cache-mb auto and the dense vocabulary.
    const char * cache_vals[] = {"auto", "Auto", "AUTO", "auto ", " auto", "0", "1500", "", "aut"};
    for (const char * v : cache_vals) {
        if ((long long) native_cache_mb_auto(v) != cc_cache_mb_kind(slice(v))) fail("cc_cache_mb_kind", v);
    }
    if (cc_cache_mb_kind(slice("auto")) != 1) fail("cc_cache_mb_kind pin", "auto");
    if (cc_cache_mb_kind(slice("Auto")) != 0) fail("cc_cache_mb_kind pin", "Auto is not auto");

    const char * dense_vals[] = {"mmap", "warm", "anon", "ahwb", "", "MMAP", "Anon", "ahw", "pinned",
                                 "dense", "mmap ", " mmap"};
    for (const char * v : dense_vals) {
        long long want = native_dense_mode(v);
        long long got = cc_dense_mode(slice(v));
        if (want != got) fail("cc_dense_mode", std::string("`") + v + "` native=" + std::to_string(want) +
                                                     " port=" + std::to_string(got));
    }
    for (int m = 0; m < 4; ++m) {
        const char * names[] = {"mmap", "warm", "anon", "ahwb"};
        if (cc_dense_mode(slice(names[m])) != m) fail("cc_dense_mode pin", names[m]);
    }

    // 6. --mtp / --ngram: same source twice is fine, the other source is a
    //    contradiction.
    int draft_checks = 0;
    for (int cur = 0; cur <= 2; ++cur)
        for (int want = 0; want <= 2; ++want) {
            long long n = native_draft_pick(cur, want);
            long long p = cc_draft_pick(cur, want);
            ++draft_checks;
            if (n != p) {
                char d[96];
                std::snprintf(d, sizeof d, "cur=%d want=%d native=%lld port=%lld", cur, want, n, p);
                fail("cc_draft_pick", d);
            }
        }
    if (cc_draft_pick(1, 2) != -1) fail("cc_draft_pick pin", "mtp then ngram");
    if (cc_draft_pick(2, 1) != -1) fail("cc_draft_pick pin", "ngram then mtp");
    if (cc_draft_pick(1, 1) != 1) fail("cc_draft_pick pin", "mtp twice");

    // 7. Random precedence sweep: field, seen, env, raw, flag, default.
    Lcg rng;
    const int FUZZ = 20000;
    long long fuzz_ok = 0;
    for (int k = 0; k < FUZZ; ++k) {
        int seen = rng.pick(2);
        int has = rng.pick(2);
        int flag = rng.pick(21) - 5;
        int dflt = rng.pick(9) - 4;
        std::string raw;
        switch (rng.pick(10)) {
        case 0: raw = ""; break;
        case 1: raw = std::to_string(rng.pick(5000) - 1000); break;
        case 2: raw = " " + std::to_string(rng.pick(100)); break;
        case 3: raw = std::to_string(rng.pick(100)) + "x"; break;
        case 4: raw = "auto"; break;
        case 5: raw = (rng.pick(2) ? "+" : "-") + std::to_string(rng.pick(50)); break;
        case 6: raw = "0"; break;
        case 7: raw = "\t" + std::to_string(rng.pick(9)); break;
        case 8: raw = "  "; break;
        default: raw = std::to_string(rng.pick(2)); break;
        }
        long long want = native_env_int(seen != 0, flag, has != 0, raw.c_str(), dflt);
        long long got = cc_env_int(seen, flag, has, slice(raw), dflt);
        if (want == got) {
            ++fuzz_ok;
        } else {
            fail("env fuzz", raw + " seen=" + std::to_string(seen));
            if (failures > 10) break;
        }
        int moe = rng.pick(2), ca = rng.pick(2), cs = rng.pick(2), ep = rng.pick(2);
        if (native_cache_auto(moe != 0, ca != 0, cs != 0, ep != 0) != cc_cache_auto(moe, ca, cs, ep))
            fail("cache_auto fuzz", std::to_string(k));
    }

    std::printf("FG622_S3 fields=11 atoi=%lld matrix=%lld draft=%d fuzz=%d passes=%lld failures=%d\n", atoi_checks,
                matrix, draft_checks, FUZZ, fuzz_ok, failures);
    if (failures == 0) {
        std::printf("all config-resolve checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", failures);
    return 1;
}
