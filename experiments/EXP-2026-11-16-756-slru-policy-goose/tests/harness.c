// Harness for the SLRU differential gate.
//
// Reads one case on stdin (the shape gate.py emits) and drives the Goose slru_plan over the whole
// request sequence, printing one line per request in the same format the Python reference prints.
// The last line is the final state, so list corruption shows up even when the fills happen to agree.
//
// The Goose ABI is the one --standalone generates: slices are passed as {data,len} structs.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int64_t *data; int64_t len; } sl_i64;

int64_t slru_plan(sl_i64 key_of, sl_i64 prev, sl_i64 next, sl_i64 prot,
                  sl_i64 head, sl_i64 tail, sl_i64 seen, sl_i64 slots,
                  sl_i64 ids, int64_t n_ids,
                  int64_t il, int64_t n_expert, int64_t n_slots,
                  sl_i64 scal, sl_i64 uniq, sl_i64 fills_e, sl_i64 fills_s, sl_i64 evicted);
void goose_init(int argc, char **argv);

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

    int64_t *maps   = malloc((size_t) n_layer * n_expert * sizeof(int64_t));
    int64_t *key_of = malloc((size_t) n_slots * sizeof(int64_t));
    int64_t *prev   = malloc((size_t) n_slots * sizeof(int64_t));
    int64_t *next   = malloc((size_t) n_slots * sizeof(int64_t));
    int64_t *prot   = malloc((size_t) n_slots * sizeof(int64_t));
    int64_t *seen   = malloc((size_t) n_expert * sizeof(int64_t));
    int64_t *uniq   = malloc((size_t) n_slots * sizeof(int64_t));
    int64_t *ev     = malloc((size_t) (n_expert + n_slots) * sizeof(int64_t));
    int64_t *fe     = malloc((size_t) (n_expert + n_slots) * sizeof(int64_t));
    int64_t *fs     = malloc((size_t) (n_expert + n_slots) * sizeof(int64_t));
    int64_t head_arr[2] = { 0, -1 };
    int64_t tail_arr[2] = { n_slots - 1, -1 };
    int64_t scal_arr[5] = { 0, 0, 0, 0, 1 };

    for (int64_t i = 0; i < n_layer * n_expert; ++i) maps[i] = -1;
    for (int64_t s = 0; s < n_slots; ++s) {
        key_of[s] = -1;
        prev[s] = s - 1;
        next[s] = (s + 1 < n_slots) ? s + 1 : -1;
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
        int64_t ids[512]; int64_t n_ids = 0;
        while (*p && *p != ']' && n_ids < 512) {
            ids[n_ids++] = strtoll(p, (char **) &p, 10);
            while (*p == ',' || *p == ' ') p++;
        }
        while (*p && *p != ']') p++;          // ids ]
        p++;
        while (*p && *p != ']') p++;          // request ]
        p++;

        const int64_t nf = slru_plan(
            (sl_i64){ key_of, n_slots }, (sl_i64){ prev, n_slots },
            (sl_i64){ next, n_slots },   (sl_i64){ prot, n_slots },
            (sl_i64){ head_arr, 2 },     (sl_i64){ tail_arr, 2 },
            (sl_i64){ seen, n_expert },  (sl_i64){ maps + il * n_expert, n_expert },
            (sl_i64){ ids, n_ids }, n_ids,
            il, n_expert, n_slots,
            (sl_i64){ scal_arr, 5 }, (sl_i64){ uniq, n_slots },
            (sl_i64){ fe, n_expert + n_slots }, (sl_i64){ fs, n_expert + n_slots },
            (sl_i64){ ev, n_expert + n_slots });

        if (nf < 0 || scal_arr[4] == 0) {
            printf("REFUSE\n");
        } else {
            // the module reports the evicted key; clearing that layer's map is the caller's job
            for (int64_t i = 0; i < nf; ++i) {
                const int64_t old = ev[i];
                if (old >= 0) maps[(old / n_expert) * n_expert + old % n_expert] = -1;
            }
            printf("F");
            for (int64_t i = 0; i < nf; ++i) printf(" %lld:%lld", (long long) fe[i], (long long) fs[i]);
            printf(" H%lld\n", (long long) scal_arr[3]);
        }
    }
    printf("STATE");
    for (int64_t s = 0; s < n_slots; ++s) {
        printf(" %lld,%lld,%lld,%lld", (long long) key_of[s], (long long) prev[s],
               (long long) next[s], (long long) prot[s]);
    }
    printf(" HEAD %lld,%lld TAIL %lld,%lld NP %lld\n",
           (long long) head_arr[0], (long long) head_arr[1],
           (long long) tail_arr[0], (long long) tail_arr[1], (long long) scal_arr[1]);
    return 0;
}
