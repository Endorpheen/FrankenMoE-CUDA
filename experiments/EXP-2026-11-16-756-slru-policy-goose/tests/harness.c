// Harness for the SLRU differential gate.
//
// Drives the Goose slru_plan over a whole request sequence and prints one line per request in the same
// format the Python reference prints. The last line is the final state, so list corruption shows up even
// when the fills happen to agree.
//
// The module addresses the int32 arrays directly through FFI, so this file supplies slru_ld_i32 /
// slru_st_i32 / slru_fail - the same three the runtime shim supplies inside libllama.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int64_t *data; int64_t len; } sl_i64;

int64_t slru_plan(uint64_t key_of, uint64_t prev, uint64_t next, uint64_t prot,
                  uint64_t head, uint64_t tail, sl_i64 seen, uint64_t slots,
                  uint64_t ids, int64_t n_ids,
                  int64_t il, int64_t n_expert, int64_t n_slots,
                  sl_i64 scal, uint64_t uniq, uint64_t fills_e, uint64_t fills_s, uint64_t evicted);
void goose_init(int argc, char **argv);

int64_t slru_ld_i32(uint64_t b, int64_t byte) {
    int32_t v;
    memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) byte), 4);
    return v;
}

void slru_st_i32(uint64_t b, int64_t byte, int64_t v) {
    int32_t x = (int32_t) v;
    memcpy((void *) (uintptr_t) (b + (uint64_t) byte), &x, 4);
}

void slru_fail(int64_t c) {
    fprintf(stderr, "SLRU validation code %lld\n", (long long) c);
    abort();
}

static char buf[1 << 22];

static const char *find_key(const char *s, const char *key) {
    const char *p = strstr(s, key);
    if (!p) { fprintf(stderr, "missing key %s\n", key); exit(2); }
    return p + strlen(key);
}

int main(int argc, char **argv) {
    goose_init(argc, argv);
    size_t n = fread(buf, 1, sizeof(buf) - 1, stdin);
    buf[n] = 0;

    const int64_t n_layer  = strtoll(find_key(buf, "\"n_layer\":"), NULL, 10);
    const int64_t n_expert = strtoll(find_key(buf, "\"n_expert\":"), NULL, 10);
    const int64_t n_slots  = strtoll(find_key(buf, "\"n_slots\":"), NULL, 10);

    const char *rq = find_key(buf, "\"requests\":");
    int64_t n_req = 0;
    {
        int depth = 0;
        for (const char *p = rq; *p; ++p) {
            if (*p == '[') { depth++; if (depth == 2) n_req++; }
            else if (*p == ']') { depth--; if (depth == 0) break; }
        }
    }
    if (n_req <= 0) { fprintf(stderr, "no requests\n"); return 2; }

    // int32 state in the native layout, i64 only for seen and the scalars
    int32_t *maps   = malloc((size_t) n_layer * n_expert * sizeof(int32_t));
    int32_t *key_of = malloc((size_t) n_slots * sizeof(int32_t));
    int32_t *prev   = malloc((size_t) n_slots * sizeof(int32_t));
    int32_t *next   = malloc((size_t) n_slots * sizeof(int32_t));
    int32_t *prot   = malloc((size_t) n_slots * sizeof(int32_t));
    int32_t *uniq   = malloc((size_t) n_slots * sizeof(int32_t));
    int32_t *ev     = malloc((size_t) (n_expert + n_slots) * sizeof(int32_t));
    int32_t *fe     = malloc((size_t) (n_expert + n_slots) * sizeof(int32_t));
    int32_t *fs     = malloc((size_t) (n_expert + n_slots) * sizeof(int32_t));
    int32_t head_arr[2] = { 0, -1 };
    int32_t tail_arr[2] = { (int32_t) (n_slots - 1), -1 };
    int64_t *seen   = malloc((size_t) n_expert * sizeof(int64_t));
    int64_t scal_arr[5] = { 0, 0, 0, 0, 1 };

    for (int64_t i = 0; i < n_layer * n_expert; ++i) maps[i] = -1;
    for (int64_t i = 0; i < n_expert; ++i) seen[i] = 0;
    for (int64_t s = 0; s < n_slots; ++s) {
        key_of[s] = -1;
        prev[s] = (int32_t) (s - 1);
        next[s] = (int32_t) ((s + 1 < n_slots) ? s + 1 : -1);
        prot[s] = 0;
    }

    const char *p = rq;
    // rq points at the '[' of the requests array; each element is [il,[ids...]]
    while (*p && *p != '[') p++;
    p++;
    for (int64_t r = 0; r < n_req; ++r) {
        while (*p && *p != '[') p++;          // '[' of this request
        p++;
        const int64_t il = strtoll(p, (char **) &p, 10);
        while (*p && *p != '[') p++;          // '[' of the ids
        p++;
        int32_t ids[512]; int64_t n_ids = 0;
        while (*p && *p != ']' && n_ids < 512) {
            ids[n_ids++] = (int32_t) strtoll(p, (char **) &p, 10);
            while (*p == ',' || *p == ' ') p++;
        }
        while (*p && *p != ']') p++;          // ids ]
        p++;
        while (*p && *p != ']') p++;          // request ]
        p++;

        const int64_t nf = slru_plan(
            (uint64_t) (uintptr_t) key_of, (uint64_t) (uintptr_t) prev,
            (uint64_t) (uintptr_t) next,   (uint64_t) (uintptr_t) prot,
            (uint64_t) (uintptr_t) head_arr, (uint64_t) (uintptr_t) tail_arr,
            (sl_i64){ seen, n_expert }, (uint64_t) (uintptr_t) (maps + il * n_expert),
            (uint64_t) (uintptr_t) ids, n_ids,
            il, n_expert, n_slots,
            (sl_i64){ scal_arr, 5 }, (uint64_t) (uintptr_t) uniq,
            (uint64_t) (uintptr_t) fe, (uint64_t) (uintptr_t) fs,
            (uint64_t) (uintptr_t) ev);

        if (nf < 0 || scal_arr[4] == 0) {
            printf("REFUSE\n");
            continue;
        }
        // the module reports the evicted key; clearing that layer's map is the caller's job
        for (int64_t i = 0; i < nf; ++i) {
            const int64_t old = ev[i];
            if (old >= 0) maps[(old / n_expert) * n_expert + old % n_expert] = -1;
        }
        printf("F");
        for (int64_t i = 0; i < nf; ++i) printf(" %d:%d", fe[i], fs[i]);
        printf(" H%lld\n", (long long) scal_arr[3]);
    }
    printf("STATE");
    for (int64_t s = 0; s < n_slots; ++s) {
        printf(" %d,%d,%d,%d", key_of[s], prev[s], next[s], prot[s]);
    }
    printf(" HEAD %d,%d TAIL %d,%d NP %lld\n",
           head_arr[0], head_arr[1], tail_arr[0], tail_arr[1], (long long) scal_arr[1]);
    return 0;
}
