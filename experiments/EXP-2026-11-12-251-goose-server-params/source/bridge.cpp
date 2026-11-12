// Bridge for EXP251: exposes the JSON fields the /completion path reads, and nothing more.
// JSON parsing stays native; the decisions that follow are Goose.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

extern "C" void fg251s_fail(int64_t c) { fprintf(stderr, "FG251S_FAIL code=%lld\n", (long long) c); abort(); }
extern "C" void fg251s_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t) p)[k * 16 + i] = (int64_t) v; }
extern "C" uint64_t fg251s_get(int64_t p, int64_t k, int64_t i) { return (uint64_t) ((const int64_t *)(uintptr_t) p)[k * 16 + i]; }

static const char * key_name(int64_t k) {
    switch (k) { case 1: return "n_predict"; case 2: return "top_k"; case 3: return "top_p"; default: return "?"; }
}

extern "C" int64_t fg251s_present(int64_t req, int64_t key) {
    const json & j = *(const json *)(uintptr_t) req;
    return j.contains(key_name(key)) ? 1 : 0;
}

extern "C" int64_t fg251s_int(int64_t req, int64_t key, int64_t def) {
    const json & j = *(const json *)(uintptr_t) req;
    const char * k = key_name(key);
    if (!j.contains(k) || !j[k].is_number_integer()) return def;
    return j[k].get<int64_t>();
}

extern "C" int64_t fg251s_flt_bits(int64_t req, int64_t key, int64_t defbits) {
    const json & j = *(const json *)(uintptr_t) req;
    const char * k = key_name(key);
    if (!j.contains(k) || !j[k].is_number()) return defbits;
    float f = j[k].get<float>();
    int32_t bits; memcpy(&bits, &f, 4);
    return (int64_t) bits;
}

extern "C" uint8_t fg251ds_run(int64_t);

extern "C" void fg251s_decide(int64_t req, int64_t out /* int64_t[4] */) {
    int64_t pkt[2 * 16] = {0};
    pkt[0] = req;
    (void) fg251ds_run((int64_t)(uintptr_t) pkt);
    ((int64_t *)(uintptr_t) out)[0] = pkt[16 + 0];
    ((int64_t *)(uintptr_t) out)[1] = pkt[16 + 1];
    ((int64_t *)(uintptr_t) out)[2] = pkt[16 + 2];
    ((int64_t *)(uintptr_t) out)[3] = pkt[16 + 3];
}

// mode 1: bounds applied to already-parsed values (n_predict >= -1, top_k >= 0)
extern "C" void fg251s_apply(int64_t n_predict, int64_t top_k, int64_t top_p_bits, int64_t out[2]) {
    int64_t pkt[2 * 16] = {0};
    pkt[0] = 0;            // json handle unused in this mode
    pkt[1] = 1;            // mode
    pkt[2] = n_predict;
    pkt[3] = top_k;
    pkt[4] = top_p_bits;
    (void) fg251ds_run((int64_t)(uintptr_t) pkt);
    out[0] = pkt[16 + 0];
    out[1] = pkt[16 + 1];
}
