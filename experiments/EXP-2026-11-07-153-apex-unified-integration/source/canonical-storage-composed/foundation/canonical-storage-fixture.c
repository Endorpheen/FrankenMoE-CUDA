/* worker-8 APX030 canonical storage: correctness, ownership and failure corpus.
 *
 * Runs the real Goose modules through their published C ABI against explicit
 * reference models (test-only, linear/sorted arrays) and checks:
 *   - signed keys 0/-1/INT64_MIN/INT64_MAX, tombstones, zero lengths;
 *   - checked growth, allocation-failure rollback, clear/free/reuse;
 *   - ordered-set AVL height bound and full ordered iteration;
 *   - byte/string storage with NUL termination;
 *   - live allocation counters return to zero.
 *
 * No model, no benchmark, no sanitizer campaign. */

#include "canonical-storage.h"
#include "canonical-storage-example.h"
#include "canonical-storage-primitives.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_errors;
static int g_cases;
static uint64_t g_hash = UINT64_C(1469598103934665603);
static uint64_t g_rng = UINT64_C(20261107);

#define CHECK(x) do { \
    if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); g_errors++; } \
} while (0)

static void mix(uint64_t v) { g_hash ^= v; g_hash *= UINT64_C(1099511628211); }
static uint64_t rnd(void) {
    g_rng = g_rng * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    return g_rng;
}
static int64_t rnd_range(int64_t lo, int64_t hi) {
    uint64_t span = (uint64_t) (hi - lo + 1);
    return lo + (int64_t) (rnd() % span);
}

/* ---------------- set reference model (test-only) ------------------------- */
#define REF_MAX 20000
static int64_t g_ref[REF_MAX];
static int g_ref_n;

static int ref_find(int64_t key) {
    int lo = 0, hi = g_ref_n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (g_ref[mid] < key) lo = mid + 1; else hi = mid;
    }
    return lo;
}
static int ref_insert(int64_t key) {
    int i = ref_find(key);
    if (i < g_ref_n && g_ref[i] == key) return 0;
    memmove(&g_ref[i + 1], &g_ref[i], (size_t)(g_ref_n - i) * sizeof(g_ref[0]));
    g_ref[i] = key;
    g_ref_n++;
    return 1;
}
static int ref_erase(int64_t key) {
    int i = ref_find(key);
    if (i >= g_ref_n || g_ref[i] != key) return 0;
    memmove(&g_ref[i], &g_ref[i + 1], (size_t)(g_ref_n - i - 1) * sizeof(g_ref[0]));
    g_ref_n--;
    return 1;
}
static int ref_contains(int64_t key) {
    int i = ref_find(key);
    return i < g_ref_n && g_ref[i] == key;
}
static int ref_lower(int64_t key, int64_t * out) {
    int i = ref_find(key);
    if (i >= g_ref_n) return 0;
    *out = g_ref[i];
    return 1;
}
static int ref_first(int64_t * out) {
    if (g_ref_n == 0) return 0;
    *out = g_ref[0];
    return 1;
}
static int ref_next(int64_t key, int64_t * out) {
    int lo = 0, hi = g_ref_n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (g_ref[mid] <= key) lo = mid + 1; else hi = mid;
    }
    if (lo >= g_ref_n) return 0;
    *out = g_ref[lo];
    return 1;
}

static int height_bound_ok(int64_t n, int64_t h) {
    if (n <= 0) return h == 0;
    int64_t x = n + 2, log = 0;
    while (x > 1) { x = (x + 1) / 2; log++; }
    return h <= 2 * log + 1;
}

static void check_set_model(fgc_handle s, const char * where) {
    CHECK(fgc_set_size(s) == g_ref_n);
    CHECK(height_bound_ok(g_ref_n, fgc_set_height(s)));

    int64_t key = 0;
    int have = fgc_set_first(s, &key);
    for (int i = 0; i < g_ref_n; ++i) {
        CHECK(have == 1);
        if (have != 1) break;
        CHECK(key == g_ref[i]);
        int64_t nxt = 0;
        have = fgc_set_next(s, key, &nxt);
        key = nxt;
    }
    CHECK(have == 0);
    (void) where;
}

/* ---------------- map reference model (test-only) ------------------------- */
#define MREF_MAX 8192
static int64_t g_mkeys[MREF_MAX];
static int64_t g_mvals[MREF_MAX];
static int g_mn;

static int mref_find(int64_t key) {
    for (int i = 0; i < g_mn; ++i) if (g_mkeys[i] == key) return i;
    return -1;
}
static void mref_put(int64_t key, int64_t value) {
    int i = mref_find(key);
    if (i >= 0) { g_mvals[i] = value; return; }
    if (g_mn >= MREF_MAX) return;
    g_mkeys[g_mn] = key;
    g_mvals[g_mn] = value;
    g_mn++;
}
static int mref_erase(int64_t key) {
    int i = mref_find(key);
    if (i < 0) return 0;
    g_mkeys[i] = g_mkeys[g_mn - 1];
    g_mvals[i] = g_mvals[g_mn - 1];
    g_mn--;
    return 1;
}
static int mref_get(int64_t key, int64_t * out) {
    int i = mref_find(key);
    if (i < 0) return 0;
    *out = g_mvals[i];
    return 1;
}

/* ---------------- tests --------------------------------------------------- */

static void test_vector(void) {
    fgc_handle v = fgc_vec_new(8);
    CHECK(v != 0);
    CHECK(fgc_vec_len(v) == 0);
    CHECK(fgc_vec_data(v) == NULL);
    CHECK(fgc_vec_item_bytes(v) == 8);
    CHECK(fgc_vec_resize(v, 4) == 0);
    CHECK(fgc_vec_len(v) == 4);
    int64_t * p = (int64_t *) fgc_vec_data(v);
    CHECK(p != NULL);
    for (int i = 0; i < 4; ++i) CHECK(p[i] == 0); /* zero-filled */
    for (int i = 0; i < 4; ++i) p[i] = 100 + i;
    CHECK(fgc_vec_reserve(v, 100) == 0);
    CHECK(fgc_vec_len(v) == 4);
    p = (int64_t *) fgc_vec_data(v);
    for (int i = 0; i < 4; ++i) CHECK(p[i] == 100 + i);
    CHECK(fgc_vec_resize(v, 2) == 0);
    CHECK(fgc_vec_len(v) == 2);
    CHECK(fgc_vec_resize(v, -1) == FGC_ERR_INVAL);
    CHECK(fgc_vec_resize(v, 2) == 0);
    CHECK(fgc_vec_clear(v) == 0);
    CHECK(fgc_vec_len(v) == 0);
    CHECK(fgc_vec_resize(v, 3) == 0);
    CHECK(((int64_t *) fgc_vec_data(v))[2] == 0);

    /* checked overflow: item_bytes = INT64_MAX leaves max count 1 */
    fgc_handle big = fgc_vec_new(INT64_MAX);
    CHECK(big != 0);
    CHECK(fgc_vec_reserve(big, 2) == FGC_ERR_OVERFLOW);
    fgc_vec_free(big);

    /* allocation failure rollback: capacity/data/len/content unchanged */
    fgc_handle f = fgc_vec_new(4);
    CHECK(f != 0);
    CHECK(fgc_vec_resize(f, 4) == 0);
    int32_t * fp = (int32_t *) fgc_vec_data(f);
    for (int i = 0; i < 4; ++i) fp[i] = (int32_t)(20 + i);
    fgc_test_alloc_reset();
    fgc_test_fail_alloc_at(1);
    CHECK(fgc_vec_reserve(f, 4096) == FGC_ERR_NOMEM);
    CHECK(fgc_vec_len(f) == 4);
    fp = (int32_t *) fgc_vec_data(f);
    for (int i = 0; i < 4; ++i) CHECK(fp[i] == (int32_t)(20 + i));
    fgc_test_fail_alloc_at(0);
    CHECK(fgc_vec_reserve(f, 4096) == 0);
    CHECK(((int32_t *) fgc_vec_data(f))[3] == 23);

    /* two independent instances */
    fgc_handle a = fgc_vec_new(8), b = fgc_vec_new(8);
    CHECK(a != 0 && b != 0);
    CHECK(fgc_vec_resize(a, 1) == 0 && fgc_vec_resize(b, 1) == 0);
    ((int64_t *) fgc_vec_data(a))[0] = 11;
    ((int64_t *) fgc_vec_data(b))[0] = 22;
    fgc_vec_free(a);
    CHECK(((int64_t *) fgc_vec_data(b))[0] == 22);

    fgc_vec_free(v);
    fgc_vec_free(f);
    fgc_vec_free(b);
    fgc_vec_free(0);
    g_cases++;
}

static void test_bytes_string(void) {
    fgc_handle b = fgc_bytes_new();
    CHECK(b != 0);
    CHECK(fgc_bytes_append(b, "abc", 3) == 0);
    CHECK(fgc_bytes_append(b, "de", 2) == 0);
    CHECK(fgc_bytes_len(b) == 5);
    CHECK(memcmp(fgc_bytes_data(b), "abcde", 5) == 0);
    CHECK(fgc_bytes_resize(b, 2) == 0);
    CHECK(fgc_bytes_len(b) == 2);

    fgc_handle s = fgc_bytes_new();
    CHECK(fgc_str_set(s, "hello", 5) == 0);
    CHECK(fgc_str_len(s) == 5);
    CHECK(strcmp(fgc_str_data(s), "hello") == 0);
    CHECK(fgc_str_set(s, "", 0) == 0);
    CHECK(fgc_str_len(s) == 0);
    CHECK(fgc_str_data(s)[0] == '\0');
    fgc_bytes_free(b);
    fgc_str_free(s);
    fgc_bytes_free(0);
    g_cases++;
}

static void test_record(void) {
    const int64_t keys[] = { 0, -1, INT64_MIN, INT64_MAX, 7, -7 };
    fgc_handle r = fgc_record_new(42);
    CHECK(r != 0);
    CHECK(fgc_record_kind(r) == 42);
    CHECK(fgc_record_get(r, 0) == 0); /* absent */
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        CHECK(fgc_record_set(r, keys[i], (int64_t)(i * 10 + 1)) == 0);
    }
    CHECK(fgc_record_count(r) == (int64_t)(sizeof(keys) / sizeof(keys[0])));
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        CHECK(fgc_record_get(r, keys[i]) == (int64_t)(i * 10 + 1));
    }
    CHECK(fgc_record_set(r, keys[1], -999) == 0); /* update in place */
    CHECK(fgc_record_get(r, keys[1]) == -999);

    /* growth failure rollback: cap is 4 after the first four fields */
    fgc_handle g = fgc_record_new(1);
    CHECK(g != 0);
    for (int i = 0; i < 4; ++i) CHECK(fgc_record_set(g, i, 100 + i) == 0);
    fgc_test_alloc_reset();
    fgc_test_fail_alloc_at(1);
    CHECK(fgc_record_set(g, 4, 104) == FGC_ERR_NOMEM);
    CHECK(fgc_record_count(g) == 4);
    for (int i = 0; i < 4; ++i) CHECK(fgc_record_get(g, i) == 100 + i);
    fgc_test_fail_alloc_at(0);
    CHECK(fgc_record_set(g, 4, 104) == 0);
    CHECK(fgc_record_get(g, 4) == 104);

    /* clear/free/reuse */
    CHECK(fgc_record_clear(r) == 0);
    CHECK(fgc_record_count(r) == 0);
    CHECK(fgc_record_get(r, keys[0]) == 0);
    CHECK(fgc_record_set(r, keys[0], 5) == 0);
    CHECK(fgc_record_get(r, keys[0]) == 5);

    /* kind mismatch: a map handle is not a record */
    fgc_handle m = fgc_map_new();
    CHECK(m != 0);
    CHECK(fgc_record_kind(m) == -1);
    CHECK(fgc_record_set(m, 1, 2) < 0);
    CHECK(fgc_record_get(m, 1) == 0);
    fgc_map_free(m);

    fgc_record_free(r);
    fgc_record_free(g);
    fgc_record_free(0);
    g_cases++;
}

static void test_map(void) {
    const int64_t keys[] = { 0, -1, INT64_MIN, INT64_MAX, 1, -2, 7 };
    fgc_handle m = fgc_map_new();
    CHECK(m != 0);
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        CHECK(fgc_map_put(m, keys[i], (int64_t)(i + 1)) == 0);
    }
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        int64_t v = 0;
        CHECK(fgc_map_get(m, keys[i], &v) == 1);
        CHECK(v == (int64_t)(i + 1));
        mix((uint64_t) v);
    }
    CHECK(fgc_map_size(m) == (int64_t)(sizeof(keys) / sizeof(keys[0])));
    int64_t v = 0;
    CHECK(fgc_map_get(m, 12345, &v) == 0);
    CHECK(fgc_map_get(m, 12345, NULL) == 0);
    CHECK(fgc_map_put(m, keys[0], 77) == 0);
    CHECK(fgc_map_get(m, keys[0], &v) == 1 && v == 77);
    CHECK(fgc_map_erase(m, keys[0]) == 0);
    CHECK(fgc_map_get(m, keys[0], &v) == 0);
    CHECK(fgc_map_erase(m, keys[0]) == 0); /* absent no-op */
    CHECK(fgc_map_put(m, keys[0], 78) == 0); /* tombstone reuse */
    CHECK(fgc_map_get(m, keys[0], &v) == 1 && v == 78);

    /* allocation-failure rollback on the 6th insert (triggers rehash) */
    fgc_handle f = fgc_map_new();
    CHECK(f != 0);
    for (int i = 0; i < 5; ++i) CHECK(fgc_map_put(f, i * 101, i) == 0);
    fgc_test_alloc_reset();
    fgc_test_fail_alloc_at(1);
    CHECK(fgc_map_put(f, 999, 999) == FGC_ERR_NOMEM);
    CHECK(fgc_map_size(f) == 5);
    for (int i = 0; i < 5; ++i) {
        int64_t got = -1;
        CHECK(fgc_map_get(f, i * 101, &got) == 1 && got == i);
    }
    fgc_test_fail_alloc_at(0);
    CHECK(fgc_map_put(f, 999, 999) == 0);
    CHECK(fgc_map_get(f, 999, &v) == 1 && v == 999);
    CHECK(fgc_map_clear(f) == 0);
    CHECK(fgc_map_size(f) == 0);
    CHECK(fgc_map_get(f, 999, &v) == 0);
    CHECK(fgc_map_put(f, 999, 1) == 0);

    /* randomized differential against the reference model */
    CHECK(fgc_map_clear(m) == 0);
    g_mn = 0;
    for (int step = 0; step < 30000; ++step) {
        int64_t key;
        switch (rnd() % 5) {
            case 0: key = 0; break;
            case 1: key = -1; break;
            case 2: key = INT64_MIN; break;
            case 3: key = INT64_MAX; break;
            default: key = rnd_range(-300, 300); break;
        }
        uint64_t op = rnd() % 3;
        if (op == 0) {
            int64_t value = (int64_t)(rnd() & 0x7fffffff);
            CHECK(fgc_map_put(m, key, value) == 0);
            mref_put(key, value);
        } else if (op == 1) {
            CHECK(fgc_map_erase(m, key) == 0);
            mref_erase(key);
        } else {
            int64_t got = 0, want = 0;
            int g = fgc_map_get(m, key, &got);
            int w = mref_get(key, &want);
            CHECK(g == w);
            if (g && w) CHECK(got == want);
        }
        if ((step & 1023) == 0) CHECK(fgc_map_size(m) == g_mn);
        mix((uint64_t) key);
    }
    CHECK(fgc_map_size(m) == g_mn);

    fgc_map_free(m);
    fgc_map_free(f);
    fgc_map_free(0);
    g_cases++;
}

static void test_set(void) {
    fgc_handle s = fgc_set_new();
    CHECK(s != 0);
    g_ref_n = 0;
    int64_t out = 0;
    CHECK(fgc_set_first(s, &out) == 0);
    CHECK(fgc_set_contains(s, 0) == 0);

    const int64_t seed[] = { 0, -1, INT64_MIN, INT64_MAX, 1, 2, -2, 7, -7 };
    for (size_t i = 0; i < sizeof(seed) / sizeof(seed[0]); ++i) {
        CHECK(fgc_set_insert(s, seed[i]) == 0);
        ref_insert(seed[i]);
    }
    CHECK(fgc_set_insert(s, seed[0]) == 0); /* duplicate is success */
    check_set_model(s, "seed");
    for (size_t i = 0; i < sizeof(seed) / sizeof(seed[0]); ++i) {
        CHECK(fgc_set_contains(s, seed[i]) == 1);
    }

    /* lower_bound / next bounds on extremes */
    CHECK(fgc_set_lower_bound(s, INT64_MIN, &out) == 1 && out == INT64_MIN);
    CHECK(fgc_set_lower_bound(s, INT64_MAX, &out) == 1 && out == INT64_MAX);
    CHECK(fgc_set_next(s, INT64_MAX, &out) == 0);
    CHECK(fgc_set_lower_bound(s, 3, &out) == 1 && out == 7);
    CHECK(fgc_set_next(s, 2, &out) == 1 && out == 7);

    /* sequential insert/erase = AVL rotation coverage */
    for (int i = 0; i < 2000; ++i) {
        int64_t key = i * 3 - 1000;
        CHECK(fgc_set_insert(s, key) == 0);
        ref_insert(key);
    }
    check_set_model(s, "sequential");
    for (int i = 0; i < 2000; i += 2) {
        int64_t key = i * 3 - 1000;
        CHECK(fgc_set_erase(s, key) == 0);
        ref_erase(key);
    }
    check_set_model(s, "erase-even");

    /* randomized mixed operations against the sorted reference */
    for (int step = 0; step < 30000; ++step) {
        int64_t key;
        switch (rnd() % 6) {
            case 0: key = 0; break;
            case 1: key = -1; break;
            case 2: key = INT64_MIN; break;
            case 3: key = INT64_MAX; break;
            default: key = rnd_range(-400, 400); break;
        }
        uint64_t op = rnd() % 4;
        if (op == 0) {
            CHECK(fgc_set_insert(s, key) == 0);
            ref_insert(key);
        } else if (op == 1) {
            CHECK(fgc_set_erase(s, key) == 0);
            ref_erase(key);
        } else if (op == 2) {
            CHECK(fgc_set_contains(s, key) == ref_contains(key));
        } else {
            int64_t got = 0, want = 0;
            int g = fgc_set_lower_bound(s, key, &got);
            int w = ref_lower(key, &want);
            CHECK(g == w);
            if (g && w) CHECK(got == want);
        }
        if ((step & 1023) == 0) check_set_model(s, "random");
        mix((uint64_t) key);
    }
    check_set_model(s, "random-final");

    /* allocation-failure rollback on the 9th insert (arena grows to 8) */
    fgc_handle f = fgc_set_new();
    CHECK(f != 0);
    for (int i = 0; i < 8; ++i) CHECK(fgc_set_insert(f, i * 17) == 0);
    CHECK(fgc_set_size(f) == 8);
    fgc_test_alloc_reset();
    fgc_test_fail_alloc_at(1);
    CHECK(fgc_set_insert(f, 999) == FGC_ERR_NOMEM);
    CHECK(fgc_set_size(f) == 8);
    for (int i = 0; i < 8; ++i) CHECK(fgc_set_contains(f, i * 17) == 1);
    CHECK(fgc_set_contains(f, 999) == 0);
    fgc_test_fail_alloc_at(0);
    CHECK(fgc_set_insert(f, 999) == 0);
    CHECK(fgc_set_contains(f, 999) == 1);

    /* clear/free/reuse */
    CHECK(fgc_set_clear(s) == 0);
    CHECK(fgc_set_size(s) == 0);
    CHECK(fgc_set_first(s, &out) == 0);
    CHECK(fgc_set_insert(s, 5) == 0);
    CHECK(fgc_set_contains(s, 5) == 1);

    fgc_set_free(s);
    fgc_set_free(f);
    fgc_set_free(0);
    g_cases++;
}

/* Schema/API example for other workers: a model record plus an ordered key
 * set.  Field ids are owned by the model schema; the storage layer only stores
 * the signed i64 keys/values it is given. */
static void test_example_schema(void) {
    fgc_handle model = fgc_record_new(FGC_EXAMPLE_KIND_MODEL);
    CHECK(model != 0);
    CHECK(fgc_record_set(model, FGC_EXAMPLE_KEY_N_LAYER, 40) == 0);
    CHECK(fgc_record_set(model, FGC_EXAMPLE_KEY_N_EMBD, 2048) == 0);
    CHECK(fgc_record_get(model, FGC_EXAMPLE_KEY_N_LAYER) == 40);
    CHECK(fgc_record_get(model, FGC_EXAMPLE_KEY_N_EMBD) == 2048);
    CHECK(fgc_record_kind(model) == FGC_EXAMPLE_KIND_MODEL);

    fgc_handle deps = fgc_set_new();
    CHECK(deps != 0);
    CHECK(fgc_set_insert(deps, FGC_EXAMPLE_KEY_N_LAYER) == 0);
    CHECK(fgc_set_insert(deps, FGC_EXAMPLE_KEY_N_EMBD) == 0);
    int64_t first = 0;
    CHECK(fgc_set_first(deps, &first) == 1 && first == FGC_EXAMPLE_KEY_N_EMBD);
    int64_t next = 0;
    CHECK(fgc_set_next(deps, first, &next) == 1 && next == FGC_EXAMPLE_KEY_N_LAYER);

    fgc_record_free(model);
    fgc_set_free(deps);
    g_cases++;
}

int main(void) {
    fgc_test_alloc_reset();

    test_vector();
    test_bytes_string();
    test_record();
    test_map();
    test_set();
    test_example_schema();

    fgc_vec_free(0);
    fgc_bytes_free(0);
    fgc_record_free(0);
    fgc_map_free(0);
    fgc_set_free(0);

    CHECK(fgc_live_allocs() == 0);
    CHECK(fgc_live_bytes() == 0);

    if (g_errors) {
        fprintf(stderr, "{\"status\":\"FAIL_LOCAL_CANONICAL_STORAGE\",\"errors\":%d,\"cases\":%d}\n",
                g_errors, g_cases);
        return 1;
    }
    printf("{\"status\":\"PASS_LOCAL_CANONICAL_STORAGE\",\"cases\":%d,\"live_allocs\":%lld,"
           "\"hash\":\"%016llx\"}\n",
           g_cases, (long long) fgc_live_allocs(), (unsigned long long) g_hash);
    return 0;
}
