// Differential gate for P10-08 S2 (the numeric surface of the public headers).
//
// Native arm: include/llama.h, ggml/include/ggml.h and ggml/include/ggml-backend.h
// compiled directly, one entry per identifier in tests/native_keys.inc. The two
// limits that the API exposes as functions are compiled from
// tests/native_limits.h, which the extractor fills from src/llama.cpp.
//
// Port arm: the hdr_ exports of source/public-enum-values.goose.
//
// The port drives the comparison: the gate asks the port for its key list and
// looks every key up in the native map. It then walks the native map back and
// requires every native identifier to be present in the port, so neither side
// can shrink silently.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "hdr.h"
#include "llama.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "native_limits.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static void note(const char * fmt, const char * key, int64_t want, int64_t got) {
    fails++;
    printf("FAIL ");
    printf(fmt, key, (long long) want, (long long) got);
    printf("\n");
}

static std::map<std::string, int64_t> native_map() {
    std::map<std::string, int64_t> m;
    // One line per frozen identifier; the compiler resolves each value from the
    // real header, so a changed header changes the native side of the gate.
#define HDR(x) m[#x] = (int64_t) (x);
#include "native_keys.inc"
#undef HDR
    // The two limits that are functions rather than header constants.
    m["llama_max_devices"] = NAT_LLAMA_MAX_DEVICES;
    m["llama_max_tensor_buft_overrides"] = NAT_LLAMA_MAX_TENSOR_BUFT_OVERRIDES;
    return m;
}

static std::string port_key(int64_t i) {
    char buf[96];
    memset(buf, 0, sizeof buf);
    int64_t n = hdr_key_at(i, sl_u8{(uint8_t *) buf, (int64_t) sizeof buf - 1});
    if (n < 0) {
        return "";
    }
    return std::string(buf, (size_t) n);
}

static bool port_value(const std::string & key, int64_t & out) {
    int64_t found = -1;
    int64_t v = hdr_value(sl_u8{(uint8_t *) (void *) key.data(), (int64_t) key.size()},
                          sl_i64{&found, 1});
    out = v;
    return found == 1;
}

int main() {
    goose_init(0, nullptr);

    const std::map<std::string, int64_t> nat = native_map();

    // ── the port enumerates its own table; every key must resolve natively ───
    const int64_t n = hdr_count();
    cases++;
    if ((size_t) n != nat.size()) {
        fails++;
        printf("FAIL hdr_count want=%zu got=%lld\n", nat.size(), (long long) n);
    }
    values++;

    int64_t seen = 0;
    for (int64_t i = 0; i < n; i++) {
        std::string key = port_key(i);
        cases++;
        auto it = nat.find(key);
        if (it == nat.end()) {
            fails++;
            printf("FAIL port key without native mapping: [%s]\n", key.c_str());
            continue;
        }
        int64_t got = 0;
        if (!port_value(key, got)) {
            fails++;
            printf("FAIL port key not found by port: [%s]\n", key.c_str());
            continue;
        }
        seen++;
        values++;
        if (got != it->second) {
            note("%s: want=%lld got=%lld", key.c_str(), it->second, got);
        }
    }

    // ── and every native identifier must be reachable from the port ──────────
    cases++;
    for (const auto & kv : nat) {
        int64_t got = 0;
        values++;
        if (!port_value(kv.first, got)) {
            fails++;
            printf("FAIL native key missing from the port: [%s]\n", kv.first.c_str());
        }
    }
    // ── the refusal boundary ─────────────────────────────────────────────────
    const char * unknown[] = {"", "GGML_MAX_DIMS ", "ggml_max_dims", "GGML_TYPE_Q4_0", "LLAMA_MAX_DEVICES"};
    for (const char * u : unknown) {
        cases++;
        std::string s(u);
        int64_t got = 0;
        values++;
        if (port_value(s, got)) {
            fails++;
            printf("FAIL unknown key was accepted: [%s] -> %lld\n", u, (long long) got);
        }
    }
    cases++;
    values += 2;
    if (hdr_key_at(-1, sl_u8{(uint8_t *) nullptr, 0}) != -1) {
        fails++;
        printf("FAIL hdr_key_at(-1) did not refuse\n");
    }
    if (hdr_key_at(n, sl_u8{(uint8_t *) nullptr, 0}) != -1) {
        fails++;
        printf("FAIL hdr_key_at(count) did not refuse\n");
    }

    // a buffer shorter than the key: the port reports what it wrote
    cases++;
    {
        char tiny[4];
        memset(tiny, 0, sizeof tiny);
        int64_t w = hdr_key_at(0, sl_u8{(uint8_t *) tiny, (int64_t) sizeof tiny});
        std::string first = port_key(0);
        values++;
        if (w != 4 || first.size() <= 4 || memcmp(tiny, first.data(), 4) != 0) {
            fails++;
            printf("FAIL short key buffer: wanted 4 bytes of [%s], wrote %lld\n", first.c_str(),
                   (long long) w);
        }
    }

    // ── the canon freeze: layout facts that do not travel through the port ───
    // Sizes of the public structures an FFI caller allocates. They are not
    // portable to another ABI, so they are asserted only on a 64-bit build.
    if (sizeof(void *) == 8) {
        static_assert(sizeof(llama_batch) == 56, "llama_batch layout moved");
        static_assert(sizeof(llama_token_data) == 12, "llama_token_data layout moved");
        static_assert(sizeof(llama_token_data_array) == 32, "llama_token_data_array layout moved");
        static_assert(sizeof(llama_model_kv_override) == 264, "llama_model_kv_override layout moved");
        static_assert(sizeof(llama_logit_bias) == 8, "llama_logit_bias layout moved");
        static_assert(sizeof(ggml_tensor) == 336, "ggml_tensor layout moved");
        static_assert(sizeof(llama_context_params) == 160, "llama_context_params layout moved");
        static_assert(sizeof(llama_model_params) == 80, "llama_model_params layout moved");
        cases++;
        values++;
    }

    printf("FG645_HDR cases=%d values=%d failures=%d seen=%lld\n", cases, values, fails,
           (long long) seen);
    return fails == 0 ? 0 : 1;
}
