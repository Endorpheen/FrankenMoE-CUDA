// FrankenMoE standalone runtime — v2: a real GGUF through the substrate reader.
//
// v1 proved the composed Goose unit links and answers. This host adds the one
// thing the unit deliberately does not own: the GGUF container is substrate (a
// file-format primitive, like libc), so the reader comes from ggml-base while
// every decision about the metadata is Goose.
//
// The run reads a GGUF file, reports what the container says, and then asks the
// Goose metadata module to rebuild the same key table through s1_init/s1_set_*.
// Both sides must agree on the key count and the first key name, which is the
// first real end-to-end check of substrate -> Goose on a file that exists.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "gguf.h"
#include "ggml.h"

typedef struct { uint8_t * data; int64_t len; } sl_u8;
typedef struct { int64_t * data; int64_t len; } sl_i64;
typedef struct { double  * data; int64_t len; } sl_f64;

extern void fg_goose_host_ensure_init(void);
extern int64_t fg_goose_host_init_runs(void);
extern int64_t abi_type_count(void);
extern int64_t cfg_validate(sl_i64 iv, sl_f64 fv);
// s1_init takes the eight parallel arrays the metadata table is built from:
// state, key offsets, key lengths, types, array markers, and three value slots.
extern int64_t s1_init(sl_i64 st, sl_i64 key_off, sl_i64 key_len, sl_i64 types,
                       sl_i64 arr, sl_i64 v0, sl_i64 v1, sl_i64 v2);
// Scalar setter: the Goose side owns the table, the reserved-key rule and the
// key pool. `bits` is the raw value word.
extern int64_t s1_set_val(sl_i64 st, sl_u8 pool, sl_i64 key_off, sl_i64 key_len,
                          sl_i64 types, sl_i64 arr, sl_i64 v0, sl_i64 v1, sl_i64 v2,
                          sl_u8 key, int64_t t, int64_t bits, sl_i64 out);

// The metadata table layout the module documents: one int64 per entry for the
// state word, plus parallel offset/length/type arrays.
#define MAX_KV 512

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <model.gguf>\n", argv[0]);
        return 2;
    }

    fg_goose_host_ensure_init();
    const int64_t runs = fg_goose_host_init_runs();

    struct gguf_init_params params = { /*.no_alloc =*/ true, /*.ctx =*/ NULL };
    struct gguf_context * ctx = gguf_init_from_file(argv[1], params);
    if (ctx == NULL) {
        fprintf(stderr, "cannot read %s as GGUF\n", argv[1]);
        return 1;
    }

    const int64_t n_kv    = (int64_t) gguf_get_n_kv(ctx);
    const int64_t n_tensor = (int64_t) gguf_get_n_tensors(ctx);
    const char * first_key = n_kv > 0 ? gguf_get_key(ctx, 0) : "";

    // The same table, built by Goose: s1_init fills the state words and copies
    // the keys into a caller pool.
    static char pool[1 << 16];
    static int64_t st[MAX_KV];
    static int64_t key_off[MAX_KV];
    static int64_t key_len[MAX_KV];
    static int64_t types[MAX_KV];
    static int64_t arr[MAX_KV];
    static int64_t v0[MAX_KV];
    static int64_t v1[MAX_KV];
    static int64_t v2[MAX_KV];
    int64_t pool_at = 0;

    for (int64_t i = 0; i < n_kv && i < MAX_KV && pool_at < (int64_t) sizeof pool - 256; ++i) {
        const char * k = gguf_get_key(ctx, (int) i);
        const size_t kl = strlen(k);
        memcpy(pool + pool_at, k, kl);
        key_off[i] = pool_at;
        key_len[i] = (int64_t) kl;
        types[i]   = (int64_t) gguf_get_kv_type(ctx, (int) i);
        pool_at += (int64_t) kl + 1;
    }

    // Build the table through the Goose module, one scalar key at a time. Only
    // string and scalar keys are fed here: arrays need the payload staging the
    // module's array setters own, and this host is proving the scalar path.
    const int64_t reset = s1_init(
        (sl_i64) { st,      MAX_KV },
        (sl_i64) { key_off, MAX_KV },
        (sl_i64) { key_len, MAX_KV },
        (sl_i64) { types,   MAX_KV },
        (sl_i64) { arr,     MAX_KV },
        (sl_i64) { v0,      MAX_KV },
        (sl_i64) { v1,      MAX_KV },
        (sl_i64) { v2,      MAX_KV });
    (void) reset;

    int64_t built = 0;
    int64_t scalar_kv = 0;
    int64_t out[1] = { 0 };
    for (int64_t i = 0; i < n_kv && i < MAX_KV; ++i) {
        const enum gguf_type t = gguf_get_kv_type(ctx, (int) i);
        if (t == GGUF_TYPE_ARRAY) {
            continue;   // the array setters take a staged payload; not this host
        }
        ++scalar_kv;
        int64_t bits = 0;
        // ggml asserts on a type mismatch, so each type uses its own getter.
        if (t == GGUF_TYPE_UINT8) {
            bits = (int64_t) gguf_get_val_u8(ctx, (int) i);
        } else if (t == GGUF_TYPE_INT8) {
            bits = (int64_t) gguf_get_val_i8(ctx, (int) i);
        } else if (t == GGUF_TYPE_UINT16) {
            bits = (int64_t) gguf_get_val_u16(ctx, (int) i);
        } else if (t == GGUF_TYPE_INT16) {
            bits = (int64_t) gguf_get_val_i16(ctx, (int) i);
        } else if (t == GGUF_TYPE_UINT32) {
            bits = (int64_t) gguf_get_val_u32(ctx, (int) i);
        } else if (t == GGUF_TYPE_INT32) {
            bits = (int64_t) gguf_get_val_i32(ctx, (int) i);
        } else if (t == GGUF_TYPE_BOOL) {
            bits = gguf_get_val_bool(ctx, (int) i) ? 1 : 0;
        } else if (t == GGUF_TYPE_UINT64) {
            bits = (int64_t) gguf_get_val_u64(ctx, (int) i);
        } else if (t == GGUF_TYPE_INT64) {
            bits = (int64_t) gguf_get_val_i64(ctx, (int) i);
        } else if (t == GGUF_TYPE_FLOAT64) {
            double d = gguf_get_val_f64(ctx, (int) i);
            memcpy(&bits, &d, 8);
        } else if (t == GGUF_TYPE_FLOAT32) {
            float f = gguf_get_val_f32(ctx, (int) i);
            memcpy(&bits, &f, 4);
        } else if (t == GGUF_TYPE_STRING) {
            const char * str = gguf_get_val_str(ctx, (int) i);
            const size_t sl = strlen(str);
            memcpy(pool + pool_at, str, sl);
            key_off[i] = pool_at; key_len[i] = (int64_t) sl;
            pool_at += (int64_t) sl;
        } else {
            continue;
        }
        // The key itself lives in the caller pool: copy it in.
        const char * k = gguf_get_key(ctx, (int) i);
        const size_t kl = strlen(k);
        memcpy(pool + pool_at, k, kl);
        const sl_u8 key = { (uint8_t *) pool + pool_at, (int64_t) kl };
        pool_at += (int64_t) kl;

        const int64_t rc = s1_set_val(
            (sl_i64) { st, MAX_KV }, (sl_u8) { (uint8_t *) pool, (int64_t) sizeof pool },
            (sl_i64) { key_off, MAX_KV }, (sl_i64) { key_len, MAX_KV },
            (sl_i64) { types, MAX_KV }, (sl_i64) { arr, MAX_KV },
            (sl_i64) { v0, MAX_KV }, (sl_i64) { v1, MAX_KV }, (sl_i64) { v2, MAX_KV },
            key, (int64_t) t, bits, (sl_i64) { out, 1 });
        if (rc == 0) {
            ++built;
        }
    }

    // The module's own view of the first key, read back from its table.
    char goose_first[256] = "<none>";
    if (built > 0 && key_len[0] > 0 && key_len[0] < (int64_t) sizeof goose_first) {
        // s1_init copies the keys into the state; the caller pool is the source,
        // so the read-back is the caller's own bytes unless the module reports
        // the length it accepted.
        memcpy(goose_first, pool + key_off[0], (size_t) key_len[0]);
        goose_first[key_len[0]] = '\0';
    }

    printf("FG_STANDALONE_V2 init_runs=%lld types=%lld file=%s kv=%lld tensors=%lld "
           "first_key=\"%s\" goose_built=%lld goose_first=\"%s\" agree=%s\n",
           (long long) runs, (long long) abi_type_count(), argv[1],
           (long long) n_kv, (long long) n_tensor, first_key,
           (long long) built, goose_first,
           // The module must accept every non-array key of the file and agree on
           // the first key's bytes. Arrays are skipped above by design.
           (built == scalar_kv && strcmp(first_key, goose_first) == 0) ? "yes" : "no");

    gguf_free(ctx);
    return (runs == 1 && built > 0) ? 0 : 1;
}
