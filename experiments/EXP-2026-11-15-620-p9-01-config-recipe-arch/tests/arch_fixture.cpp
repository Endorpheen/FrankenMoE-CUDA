// P9-01 S2 gate: the Goose architecture registry against the native table.
//
// Native arms:
//   * find_moe_recipe / n_moe_recipes / moe_recipe_at from
//     runtime/core/src/moe/arch_registry.cpp, compiled and linked into the gate
//     together with the real recipe.h — the registry is compared row by row;
//   * match_expert from runtime/core/src/moe/router_hook.cpp:22-39, which is
//     static and pulls llama.cpp headers, so it is transcribed here byte for
//     byte (identical body, same sscanf format) and the Goose port is compared
//     against the transcription over every row and hundreds of names.
//
// The expected table is also pinned as a literal, so a change in the native
// table cannot move both arms together and still pass.
//
// No model, no llama.cpp, no I/O. Build: tests/build.sh ar.

#include "bmoe/recipe.h"
#include "ar.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace bmoe;

static int failures = 0;

static const int N_ROWS = 9;
static const char * EXP_ARCH[N_ROWS] = {
    "qwen3moe", "qwen2moe", "qwen35moe", "gemma4", "gpt-oss",
    "lfm2moe", "deepseek4", "bailingmoe3", "qwen4exp",
};
static const char * EXP_SUF[N_ROWS][3] = {
    {"ffn_gate_exps", "ffn_up_exps", "ffn_down_exps"},
    {"ffn_gate_exps", "ffn_up_exps", "ffn_down_exps"},
    {"ffn_gate_exps", "ffn_up_exps", "ffn_down_exps"},
    {"ffn_gate_up_exps", "ffn_down_exps", nullptr},
    {"ffn_gate_exps", "ffn_up_exps", "ffn_down_exps"},
    {"ffn_gate_exps", "ffn_up_exps", "ffn_down_exps"},
    {"ffn_gate_exps", "ffn_up_exps", "ffn_down_exps"},
    {"ffn_gate_exps", "ffn_up_exps", "ffn_down_exps"},
    {"ffn_gate_exps", "ffn_up_exps", "ffn_down_exps"},
};

static void fail(const char * what, const std::string & detail) {
    std::printf("[FAIL] %s: %s\n", what, detail.c_str());
    ++failures;
}

static int row_of(const MoeRecipe * r) {
    if (!r) return -1;
    for (int k = 0; k < n_moe_recipes(); ++k) {
        if (moe_recipe_at(k) == r) return k;
    }
    return -2; // a pointer into no row: the native accessor is broken
}

static std::string port_str(int64_t (*fn)(int64_t, sl_u8), int64_t row) {
    char buf[64] = {0};
    int64_t n = fn(row, sl_u8{(uint8_t *) buf, (int64_t) sizeof buf});
    if (n < 0) return std::string("<none>");
    return std::string(buf, (size_t) n);
}

static std::string port_suffix(int64_t row, int64_t slot) {
    char buf[64] = {0};
    sl_u8 out{(uint8_t *) buf, (int64_t) sizeof buf};
    int64_t n = ar_suffix(row, slot, out);
    if (n < 0) return std::string("<none>");
    return std::string(buf, (size_t) n);
}

// router_hook.cpp:22-39, verbatim (the static native function).
static int native_match(const char * name, const MoeRecipe & r, int & il_out) {
    int il = -1;
    int consumed = 0;
    if (std::sscanf(name, "blk.%d.%n", &il, &consumed) != 1 || il < 0) return -1;
    const char * rest = name + consumed;
    for (int p = 0; p < MoeRecipe::max_exps; ++p) {
        const char * suffix = r.exps_suffix[p];
        if (!suffix) continue;
        const size_t sl = std::strlen(suffix);
        if (std::strncmp(rest, suffix, sl) == 0 && std::strcmp(rest + sl, ".weight") == 0) {
            il_out = il;
            return p;
        }
    }
    return -1;
}

struct Match {
    int slot;
    int layer;
};

static Match native_named(const std::string & name, int row) {
    int il = -1;
    int slot = native_match(name.c_str(), *moe_recipe_at(row), il);
    return Match{slot, slot >= 0 ? il : -3};
}

static Match port_named(const std::string & name, int row) {
    int64_t lay[1] = {-9};
    sl_u8 s{(uint8_t *) name.data(), (int64_t) name.size()};
    int64_t slot = ar_match(s, row, sl_i64{lay, 1});
    return Match{(int) slot, slot >= 0 ? (int) lay[0] : -3};
}

// One comparison, with the first few mismatches printed in full.
static long long match_checks = 0;
static void cmp_match(const std::string & name, int row) {
    Match n = native_named(name, row);
    Match p = port_named(name, row);
    ++match_checks;
    if (n.slot != p.slot || n.layer != p.layer) {
        char d[256];
        std::snprintf(d, sizeof d, "name=`%s` row=%d native=(%d,%d) port=(%d,%d)", name.c_str(), row, n.slot,
                      n.layer, p.slot, p.layer);
        fail("match", d);
    }
}

struct Lcg {
    unsigned long long s = 0xD1B54A32D192ED03ull;
    unsigned long long next() {
        s = s * 2862933555777941757ull + 3037000493ull;
        return s >> 17;
    }
    int pick(int n) { return (int) (next() % (unsigned long long) n); }
};

int main() {
    goose_init(0, nullptr);

    // 1. The row count and the full table, native and pinned.
    if (ar_count() != n_moe_recipes() || n_moe_recipes() != N_ROWS) {
        char d[128];
        std::snprintf(d, sizeof d, "ar_count=%lld native=%d pin=%d", (long long) ar_count(), n_moe_recipes(),
                      N_ROWS);
        fail("row count", d);
    }
    for (int i = 0; i < N_ROWS; ++i) {
        const MoeRecipe * r = moe_recipe_at(i);
        if (!r) {
            fail("recipe_at", "null row " + std::to_string(i));
            continue;
        }
        if (std::string(r->arch) != EXP_ARCH[i]) fail("arch pin", std::string("row ") + std::to_string(i));
        if (port_str(ar_arch, i) != EXP_ARCH[i]) fail("ar_arch", std::string("row ") + std::to_string(i));
        int exps = 0;
        for (int slot = 0; slot < MoeRecipe::max_exps; ++slot) {
            const char * want = EXP_SUF[i][slot];
            std::string got = port_suffix(i, slot);
            std::string nat = r->exps_suffix[slot] ? r->exps_suffix[slot] : "<none>";
            if (want ? got != want : got != "<none>") {
                fail("ar_suffix", std::string("row ") + std::to_string(i) + " slot " + std::to_string(slot) +
                                       " got " + got);
            }
            if (got != nat) {
                fail("ar_suffix vs native",
                     std::string("row ") + std::to_string(i) + " slot " + std::to_string(slot) + " port " + got +
                         " native " + nat);
            }
            if (want) ++exps;
        }
        if (ar_exps(i) != exps) {
            fail("ar_exps", std::string("row ") + std::to_string(i));
        }
        // Out-of-range slot is "<none>" for both arms.
        if (port_suffix(i, MoeRecipe::max_exps) != "<none>") fail("ar_suffix range", std::to_string(i));
    }
    if (port_str(ar_arch, N_ROWS) != "<none>" || port_str(ar_arch, -1) != "<none>") fail("ar_arch range", "");

    // 2. Exact lookup on every row, through the native function and by pin.
    for (int i = 0; i < N_ROWS; ++i) {
        const MoeRecipe * r = find_moe_recipe(EXP_ARCH[i]);
        int64_t p = ar_find(sl_u8{(uint8_t *) EXP_ARCH[i], (int64_t) std::strlen(EXP_ARCH[i])});
        if (row_of(r) != i || p != i) {
            char d[128];
            std::snprintf(d, sizeof d, "row %d native=%d port=%lld", i, row_of(r), p);
            fail("ar_find exact", d);
        }
    }

    // 3. Everything the exact strcmp must refuse.
    const char * variants[] = {
        "", " ", "qwen", "qwen3", "Qwen3moe", "QWEN3MOE", "qwen3moe ", " qwen3moe", "qwen3moe\n",
        "qwen3moex", "qwen3mo", "qwen3moe.gguf", "qwen3-moe", "qwen3_moe", "qwen2", "qwen2moe2",
        "gemma", "gemma4b", "gemma3", "gemma3n", "gpt-oss-20b", "gpt_oss", "GPT-OSS", "gpt-oss ",
        "lfm2", "lfm2moe2", "deepseek", "deepseek3", "deepseek4x", "bailingmoe", "bailingmoe2",
        "qwen4", "qwen4ex", "qwen4exp2", "qwen35", "qwen35moe2", "moe", "0", "\x01qwen3moe",
    };
    for (const char * v : variants) {
        const MoeRecipe * r = find_moe_recipe(v);
        int64_t p = ar_find(sl_u8{(uint8_t *) v, (int64_t) std::strlen(v)});
        if (row_of(r) != -1 || p != -1) {
            char d[160];
            std::snprintf(d, sizeof d, "`%s` native=%d port=%lld", v, row_of(r), p);
            fail("ar_find refuse", d);
        }
    }

    // 4. The tensor matcher, row by row, slot by slot, over common and hostile
    //    layer numbers, and with the companion/near-miss tails.
    const int layers[] = {0, 1, 7, 12, 127, 1234};
    for (int row = 0; row < N_ROWS; ++row) {
        for (int slot = 0; slot < MoeRecipe::max_exps; ++slot) {
            const char * suf = EXP_SUF[row][slot];
            if (!suf) continue;
            for (int layer : layers) {
                std::string n = "blk." + std::to_string(layer) + "." + suf + ".weight";
                cmp_match(n, row);
                // The tag pins the intended answer, independent of both arms.
                Match m = native_named(n, row);
                if (m.slot != slot || m.layer != layer) {
                    fail("match pin", n + " -> (" + std::to_string(m.slot) + "," + std::to_string(m.layer) + ")");
                }
            }
            // Hostile and near-miss name shapes for this suffix.
            const std::string base = std::string("blk.3.") + suf;
            const std::string shapes[] = {
                base + ".weight",
                base + ".scale",
                base + ".bias",
                base + ".weight ",
                base + ".weights",
                base + ".weigh",
                base + ".weight2",
                base + "weight",
                base,
                std::string("blk.3.") + suf + "x.weight",
                std::string("blk.3.0") + suf + ".weight",
                std::string("blk3.") + suf + ".weight",
                std::string("blk.") + suf + ".weight",
                std::string("blk..") + suf + ".weight",
                std::string("blk.3") + suf + ".weight",
                std::string("block.3.") + suf + ".weight",
                std::string("blk.3.") + suf + ".Weight",
            };
            for (const std::string & n : shapes) cmp_match(n, row);
        }
    }
    const char * number_forms[] = {
        "blk.0.ffn_gate_exps.weight",     "blk.00.ffn_gate_exps.weight",
        "blk.007.ffn_gate_exps.weight",   "blk.+12.ffn_gate_exps.weight",
        "blk. 12.ffn_gate_exps.weight",   "blk.\t12.ffn_gate_exps.weight",
        "blk.-0.ffn_gate_exps.weight",    "blk.-1.ffn_gate_exps.weight",
        "blk.--1.ffn_gate_exps.weight",   "blk.+.ffn_gate_exps.weight",
        "blk.12 .ffn_gate_exps.weight",   "blk.12x.ffn_gate_exps.weight",
        "blk.1 2.ffn_gate_exps.weight",   "blk.999999.ffn_gate_exps.weight",
        "blk.2147483647.ffn_gate_exps.weight",
    };
    for (const char * n : number_forms) {
        cmp_match(n, 0);             // split row
        cmp_match(n, 3);             // fused row: gate_exps is not a gemma4 suffix
    }
    // Cross-layout rows: the same name must resolve differently, or not at all.
    cmp_match("blk.4.ffn_gate_up_exps.weight", 0);
    cmp_match("blk.4.ffn_gate_up_exps.weight", 3);
    cmp_match("blk.4.ffn_down_exps.weight", 3);
    cmp_match("blk.4.ffn_up_exps.weight", 3);
    // Out-of-range row is the caller's error.
    {
        int64_t lay[1] = {-9};
        const char * n = "blk.1.ffn_gate_exps.weight";
        int64_t p = ar_match(sl_u8{(uint8_t *) n, (int64_t) std::strlen(n)}, -1, sl_i64{lay, 1});
        int64_t q = ar_match(sl_u8{(uint8_t *) n, (int64_t) std::strlen(n)}, N_ROWS, sl_i64{lay, 1});
        if (p != -1 || q != -1) fail("match row range", "");
        int64_t r = ar_match(sl_u8{(uint8_t *) n, (int64_t) std::strlen(n)}, 0, sl_i64{lay, 0});
        if (r != -1) fail("match short layer buffer", "");
    }

    // 5. Differential fuzz over composed names: native transcription vs port.
    Lcg rng;
    const char * prefixes[] = {"blk.", "blk", "block.", "blk..", "Blk.", "blk. ", "blk.+"};
    const char * tails[] = {".weight", ".scale", ".bias", ".weight ", ".weights", ".weigh", "", ".weight\n"};
    const int FUZZ = 20000;
    long long hits = 0;
    std::vector<std::string> suf_pool;
    for (int row = 0; row < N_ROWS; ++row)
        for (int slot = 0; slot < MoeRecipe::max_exps; ++slot)
            if (EXP_SUF[row][slot]) suf_pool.push_back(EXP_SUF[row][slot]);
    suf_pool.push_back("ffn_gate_exps2");
    suf_pool.push_back("ffn_gat_exps");
    suf_pool.push_back("ffn_gate_up_exps");
    suf_pool.push_back("ffn_down_exps.scale");
    for (int k = 0; k < FUZZ; ++k) {
        std::string n = prefixes[rng.pick(7)];
        switch (rng.pick(7)) {
        case 0: n += std::to_string(rng.pick(64)); break;
        case 1: n += std::to_string(rng.pick(100000)); break;
        case 2: n += "0" + std::to_string(rng.pick(3000)); break;
        case 3: n += (rng.pick(2) ? "+" : "-") + std::to_string(rng.pick(20)); break;
        case 4: n += std::string(rng.pick(2) ? " " : "\t") + std::to_string(rng.pick(40)); break;
        case 5: n += std::to_string(rng.pick(40)) + "x"; break;
        case 6: n += std::to_string(rng.pick(40)) + " "; break;
        }
        n += ".";
        n += suf_pool[rng.pick((int) suf_pool.size())];
        n += tails[rng.pick(8)];
        int row = rng.pick(N_ROWS);
        cmp_match(n, row);
        if (native_named(n, row).slot >= 0) ++hits;
    }

    std::printf("FG622_S2 rows=%d variants=%d match_names=%lld fuzz=%d fuzz_hits=%lld failures=%d\n", N_ROWS,
                (int) (sizeof(variants) / sizeof(variants[0])), match_checks, FUZZ, hits, failures);
    if (failures == 0) {
        std::printf("all arch-registry checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", failures);
    return 1;
}
