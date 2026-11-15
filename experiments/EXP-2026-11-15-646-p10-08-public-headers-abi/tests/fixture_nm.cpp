// Differential gate for P10-08 S3 (the text side of the public ABI).
//
// Native arm: tests/extract_native.py parses the switch tables out of
// src/llama-model-loader.cpp (llama_ftype_name), src/llama.cpp
// (llama_flash_attn_type_name, llama_load_mode_name, llama_load_mode_from_str),
// src/llama-model.cpp (llama_model_meta_key_str) and ggml/src/ggml.c
// (ggml_status_to_string) on every build. This file re-applies the two rules
// that are code rather than data, each cited at its native line:
//   - the "(guessed) " prefix of llama_ftype_name (loader 31-76);
//   - strcmp-only matching of llama_load_mode_from_str (llama.cpp 65-74).
//
// Port arm: the nm_ exports of source/public-name-tables.goose.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "nm.h"
#include "native_nm.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_str(const char * what, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-32s want=[%s] got=[%s]\n", what, want.c_str(), got.c_str());
    }
}

static void eq_int(const char * what, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-32s want=%lld got=%lld\n", what, (long long) want, (long long) got);
    }
}

template <typename F>
static std::string port_text(F fn, int64_t key) {
    char buf[256];
    memset(buf, 0x5a, sizeof buf);
    int64_t n = fn(key, sl_u8{(uint8_t *) buf, (int64_t) sizeof buf - 1});
    if (n < 0) {
        return "!";
    }
    return std::string(buf, (size_t) n);
}

// ── native rules that are code, not tables ───────────────────────────────────
// llama_ftype_name: mask with ~LLAMA_FTYPE_GUESSED, look up, keep the prefix
// only when the bit was set (src/llama-model-loader.cpp 33-76).
static std::string nat_ftype_name(int64_t ftype) {
    const int64_t masked = ftype & ~(int64_t) LLAMA_FTYPE_GUESSED;
    std::string suffix = NAT_FTYPE_DEFAULT;
    for (int i = 0; i < NAT_FTYPE_N; i++) {
        if (NAT_FTYPE[i].key == masked) {
            suffix = NAT_FTYPE[i].text;
            break;
        }
    }
    if ((ftype & LLAMA_FTYPE_GUESSED) != 0) {
        return std::string(NAT_FTYPE_PREFIX) + suffix;
    }
    return suffix;
}

static bool nat_named(const gs_nat_named * tab, int n, int64_t key, std::string & out) {
    for (int i = 0; i < n; i++) {
        if (tab[i].key == key) {
            out = tab[i].text;
            return true;
        }
    }
    return false;
}

// llama_load_mode_from_str matches whole strings with strcmp (llama.cpp 65-74).
static bool nat_from_str(const std::string & s, int64_t & out) {
    for (int i = 0; i < NAT_FROM_STR_N; i++) {
        if (s == NAT_FROM_STR[i].text) {
            out = NAT_FROM_STR[i].value;
            return true;
        }
    }
    return false;
}

int main() {
    goose_init(0, nullptr);

    // ── llama_ftype_name: every entry, both with and without the guessed bit ──
    for (int i = 0; i < NAT_FTYPE_N; i++) {
        cases++;
        const int64_t v = NAT_FTYPE[i].key;
        char what[64];
        snprintf(what, sizeof what, "ftype %lld", (long long) v);
        eq_str(what, nat_ftype_name(v), port_text(nm_ftype_name, v));
        snprintf(what, sizeof what, "ftype %lld guessed", (long long) v);
        eq_str(what, nat_ftype_name(v | LLAMA_FTYPE_GUESSED),
               port_text(nm_ftype_name, v | LLAMA_FTYPE_GUESSED));
    }

    // the catch-all branch, both variants
    const int64_t unknown_ftype[] = {999, 4, 5, 6, 33, 34, 35, 1023, 1025, 2048, 3072};
    for (int64_t v : unknown_ftype) {
        cases++;
        char what[64];
        snprintf(what, sizeof what, "ftype %lld fallback", (long long) v);
        eq_str(what, nat_ftype_name(v), port_text(nm_ftype_name, v));
        snprintf(what, sizeof what, "ftype %lld fallback guessed", (long long) v);
        eq_str(what, nat_ftype_name(v | LLAMA_FTYPE_GUESSED),
               port_text(nm_ftype_name, v | LLAMA_FTYPE_GUESSED));
    }

    // ── llama_load_mode_name: the six names and the refusal ──────────────────
    for (int i = 0; i < NAT_LOAD_N; i++) {
        cases++;
        std::string want;
        nat_named(NAT_LOAD, NAT_LOAD_N, NAT_LOAD[i].key, want);
        char what[64];
        snprintf(what, sizeof what, "load mode %lld", (long long) NAT_LOAD[i].key);
        eq_str(what, want, port_text(nm_load_mode_name, NAT_LOAD[i].key));
    }
    for (int64_t v : {-5, -2, 5, 6, 100}) {
        cases++;
        std::string want;
        bool found = nat_named(NAT_LOAD, NAT_LOAD_N, v, want);
        char what[64];
        snprintf(what, sizeof what, "load mode %lld refused", (long long) v);
        eq_int(what, found ? 0 : -1,
               port_text(nm_load_mode_name, v) == "!" ? -1 : 0);
    }

    // ── llama_load_mode_from_str: exact strings only ─────────────────────────
    for (int i = 0; i < NAT_FROM_STR_N; i++) {
        cases++;
        std::string text = NAT_FROM_STR[i].text;
        int64_t want = -1;
        nat_from_str(text, want);
        char what[64];
        snprintf(what, sizeof what, "from_str [%s]", text.c_str());
        eq_int(what, want,
               nm_load_mode_from_str(sl_u8{(uint8_t *) (void *) text.data(), (int64_t) text.size()}));
    }
    const char * bad_modes[] = {"", "AUTO", "Mmap", "mmap ", " mmap", "mmap+mlock ", "dio\n",
                                "direct", "mmap+mlockx", "mlock+mmap"};
    for (const char * b : bad_modes) {
        cases++;
        int64_t natv = -1;
        bool found = nat_from_str(b, natv);
        std::string s(b);
        char what[64];
        snprintf(what, sizeof what, "from_str bad [%s]", b);
        eq_int(what, found ? natv : -99,
               nm_load_mode_from_str(sl_u8{(uint8_t *) s.data(), (int64_t) s.size()}));
    }

    // ── llama_flash_attn_type_name ───────────────────────────────────────────
    for (int i = 0; i < NAT_FLASH_N; i++) {
        cases++;
        std::string want;
        nat_named(NAT_FLASH, NAT_FLASH_N, NAT_FLASH[i].key, want);
        char what[64];
        snprintf(what, sizeof what, "flash %lld", (long long) NAT_FLASH[i].key);
        eq_str(what, want, port_text(nm_flash_attn_type_name, NAT_FLASH[i].key));
    }
    for (int64_t v : {-3, -2, 2, 3}) {
        cases++;
        std::string want;
        bool found = nat_named(NAT_FLASH, NAT_FLASH_N, v, want);
        char what[64];
        snprintf(what, sizeof what, "flash %lld refused", (long long) v);
        eq_int(what, found ? 0 : -1,
               port_text(nm_flash_attn_type_name, v) == "!" ? -1 : 0);
    }

    // ── llama_model_meta_key_str ─────────────────────────────────────────────
    for (int i = 0; i < NAT_META_N; i++) {
        cases++;
        std::string want;
        nat_named(NAT_META, NAT_META_N, NAT_META[i].key, want);
        char what[64];
        snprintf(what, sizeof what, "meta key %lld", (long long) NAT_META[i].key);
        eq_str(what, want, port_text(nm_meta_key_str, NAT_META[i].key));
    }
    for (int64_t v : {-1, 12, 13, 100}) {
        cases++;
        std::string want;
        bool found = nat_named(NAT_META, NAT_META_N, v, want);
        char what[64];
        snprintf(what, sizeof what, "meta key %lld refused", (long long) v);
        eq_int(what, found ? 0 : -1, port_text(nm_meta_key_str, v) == "!" ? -1 : 0);
    }

    // ── ggml_status_to_string ────────────────────────────────────────────────
    for (int i = 0; i < NAT_STATUS_N; i++) {
        cases++;
        std::string want;
        nat_named(NAT_STATUS, NAT_STATUS_N, NAT_STATUS[i].key, want);
        char what[64];
        snprintf(what, sizeof what, "status %lld", (long long) NAT_STATUS[i].key);
        eq_str(what, want, port_text(nm_status_text, NAT_STATUS[i].key));
    }
    for (int64_t v : {-3, 2, 3, 1000}) {
        cases++;
        std::string want;
        bool found = nat_named(NAT_STATUS, NAT_STATUS_N, v, want);
        char what[64];
        snprintf(what, sizeof what, "status %lld catch-all", (long long) v);
        eq_str(what, found ? want : std::string(NAT_STATUS_DEFAULT), port_text(nm_status_text, v));
    }

    // ── a buffer that cannot hold the text reports what it wrote ─────────────
    cases++;
    {
        char tiny[4];
        memset(tiny, 0, sizeof tiny);
        int64_t n = nm_meta_key_str(LLAMA_MODEL_META_KEY_SAMPLING_SEQUENCE,
                                    sl_u8{(uint8_t *) tiny, (int64_t) sizeof tiny});
        values++;
        if (n != 4 || memcmp(tiny, "gene", 4) != 0) {
            fails++;
            printf("FAIL short meta buffer wrote %lld\n", (long long) n);
        }
    }

    printf("FG645_NM cases=%d values=%d failures=%d ftype=%d load=%d from_str=%d flash=%d meta=%d status=%d\n",
           cases, values, fails, NAT_FTYPE_N, NAT_LOAD_N, NAT_FROM_STR_N, NAT_FLASH_N, NAT_META_N,
           NAT_STATUS_N);
    return fails == 0 ? 0 : 1;
}
