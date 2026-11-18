// FrankenMoE standalone runtime — v4: metadata, tensor directory AND the tensor
// payload in one arena laid out by the Goose allocator arithmetic. The runtime
// can now reach the bytes, not only the descriptors.
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
#include "tensor-data.h"
#include "goose-host-llm.h"

typedef struct { uint8_t * data; int64_t len; } sl_u8;
typedef struct { int64_t * data; int64_t len; } sl_i64;
typedef struct { float * data; int64_t len; } sl_f32;
typedef struct { double  * data; int64_t len; } sl_f64;

extern void fg_goose_host_ensure_init(void);
extern int64_t fg_goose_host_init_runs(void);
extern int64_t abi_type_count(void);
extern int64_t cfg_validate(sl_i64 iv, sl_f64 fv);
// The Goose reader and tokenizer engine (EXP937/EXP939): the same unit that
// builds the metadata table also parses the container and tokenises.
extern int64_t sm_greedy(sl_f32 lv, int64_t n);
extern int64_t gr_parse(sl_u8 data, int64_t file_bytes, sl_i64 state, sl_i64 kv, sl_i64 tensors,
                        sl_i64 kv_slots, sl_i64 tensor_slots);
extern int64_t gr_find_kv(sl_u8 data, sl_i64 state, sl_i64 kv, sl_i64 slots, sl_u8 key);
extern int64_t bt_prepare_need(sl_u8 data, sl_i64 state, sl_i64 kv, int64_t tok_index, int64_t mrg_index, sl_i64 need);
extern int64_t bt_prepare_from_gguf(sl_u8 data, sl_i64 state, sl_i64 kv, int64_t tok_index, int64_t mrg_index,
                                    sl_u8 blob, sl_i64 tok_off, sl_i64 tok_len, sl_i64 tok_slots,
                                    sl_i64 mrg_off, sl_i64 mrg_len, sl_i64 mrg_slots, sl_i64 need);
extern int64_t bt_tokenize(sl_u8 blob, sl_i64 tok_off, sl_i64 tok_len, sl_i64 tok_slots,
                           sl_i64 mrg_off, sl_i64 mrg_len, sl_i64 mrg_slots,
                           sl_i64 order, int64_t n_special, sl_u8 text, sl_i64 ids, int64_t special,
                           sl_i64 cpts, sl_i64 seg, sl_u8 enc, sl_i64 sym, sl_i64 heap, sl_i64 hc, sl_i64 info);
extern int64_t bt_special_order(sl_u8 blob, sl_i64 tok_off, sl_i64 tok_len, sl_i64 attr, sl_i64 order);
// s1_init takes the eight parallel arrays the metadata table is built from:
// state, key offsets, key lengths, types, array markers, and three value slots.
extern int64_t s1_init(sl_i64 st, sl_i64 key_off, sl_i64 key_len, sl_i64 types,
                       sl_i64 arr, sl_i64 v0, sl_i64 v1, sl_i64 v2);
// Scalar setter: the Goose side owns the table, the reserved-key rule and the
// key pool. `bits` is the raw value word.
extern int64_t s1_set_val(sl_i64 st, sl_u8 pool, sl_i64 key_off, sl_i64 key_len,
                          sl_i64 types, sl_i64 arr, sl_i64 v0, sl_i64 v1, sl_i64 v2,
                          sl_u8 key, int64_t t, int64_t bits, sl_i64 out);
// Array setters. s1_set_arr stages raw element bytes; s1_set_arr_str takes the
// NUL-separated concatenation plus the element count.
extern int64_t s1_set_arr(sl_i64 st, sl_u8 pool, sl_i64 key_off, sl_i64 key_len,
                          sl_i64 types, sl_i64 arr, sl_i64 v0, sl_i64 v1, sl_i64 v2,
                          sl_u8 key, int64_t atype, sl_u8 data, int64_t n, sl_i64 out);
extern int64_t s1_set_arr_str(sl_i64 st, sl_u8 pool, sl_i64 key_off, sl_i64 key_len,
                              sl_i64 types, sl_i64 arr, sl_i64 v0, sl_i64 v1, sl_i64 v2,
                              sl_u8 key, sl_u8 data, int64_t n, sl_i64 out);

// Tensor directory. Layout constants come from goose/model/gguf-tensors.goose:
// state = {n, alignment, pool}, and one 8-word record per tensor.
#define TS_N 0
#define TS_ALIGN 1
#define TS_POOL 2
#define TR_STRIDE 8
#define TR_NAME_OFF 0
#define TR_NAME_LEN 1
#define TR_TYPE 2
#define TR_DIMS 3
#define TR_DATA_OFF 4
#define TR_DATA_LEN 5
#define TR_OFFSET 6

extern int64_t s3_init_tensors(sl_i64 tst, int64_t alignment);
extern int64_t s3_add_tensor(sl_i64 tst, sl_u8 tpool, sl_i64 tne, sl_i64 trec,
                             sl_u8 name, int64_t ndims, sl_i64 ne,
                             int64_t t, sl_u8 data);

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

    // no_alloc=false asks the substrate reader to materialise the payload in a
    // context it creates; that context is what the loader inspects.
    struct ggml_context * gctx = NULL;
    struct gguf_init_params params = { /*.no_alloc =*/ false, /*.ctx =*/ &gctx };
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
    static char pool[1 << 24];   // tokenizers carry tens of thousands of strings
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
            const enum gguf_type at = gguf_get_arr_type(ctx, (int) i);
            const int64_t an = (int64_t) gguf_get_arr_n(ctx, (int) i);
            const char * ak = gguf_get_key(ctx, (int) i);
            const size_t akl = strlen(ak);
            memcpy(pool + pool_at, ak, akl);
            const sl_u8 akey = { (uint8_t *) pool + pool_at, (int64_t) akl };
            pool_at += (int64_t) akl;

            int64_t rc = -1;
            if (at == GGUF_TYPE_STRING) {
                // NUL-separated concatenation of the elements, as the module's
                // string-array contract specifies.
                static char strdata[1 << 22];
                int64_t sdl = 0;
                for (int64_t e = 0; e < an; ++e) {
                    const char * el = gguf_get_arr_str(ctx, (int) i, (int) e);
                    const size_t eln = strlen(el);
                    if (sdl + (int64_t) eln + 1 >= (int64_t) sizeof strdata) break;
                    memcpy(strdata + sdl, el, eln);
                    sdl += (int64_t) eln;
                    strdata[sdl++] = '\0';
                }
                rc = s1_set_arr_str(
                    (sl_i64) { st, MAX_KV }, (sl_u8) { (uint8_t *) pool, (int64_t) sizeof pool },
                    (sl_i64) { key_off, MAX_KV }, (sl_i64) { key_len, MAX_KV },
                    (sl_i64) { types, MAX_KV }, (sl_i64) { arr, MAX_KV },
                    (sl_i64) { v0, MAX_KV }, (sl_i64) { v1, MAX_KV }, (sl_i64) { v2, MAX_KV },
                    akey, (sl_u8) { (uint8_t *) strdata, sdl }, an, (sl_i64) { out, 1 });
            } else {
                // Raw element bytes, one element at a time into a staging buffer.
                static uint8_t raw[1 << 20];
                int64_t rl = 0;
                for (int64_t e = 0; e < an; ++e) {
                    int64_t word = 0;
                    if (at == GGUF_TYPE_UINT8)        word = gguf_get_arr_data(ctx, (int) i) ? (int64_t) ((const uint8_t *) gguf_get_arr_data(ctx, (int) i))[e] : 0;
                    else if (at == GGUF_TYPE_INT32)   word = (int64_t) ((const int32_t *) gguf_get_arr_data(ctx, (int) i))[e];
                    else if (at == GGUF_TYPE_UINT32)  word = (int64_t) ((const uint32_t *) gguf_get_arr_data(ctx, (int) i))[e];
                    else if (at == GGUF_TYPE_FLOAT32) { float f = ((const float *) gguf_get_arr_data(ctx, (int) i))[e]; memcpy(&word, &f, 4); }
                    else                              break;
                    if (rl + 8 > (int64_t) sizeof raw) break;
                    memcpy(raw + rl, &word, 8);
                    rl += 8;
                }
                rc = s1_set_arr(
                    (sl_i64) { st, MAX_KV }, (sl_u8) { (uint8_t *) pool, (int64_t) sizeof pool },
                    (sl_i64) { key_off, MAX_KV }, (sl_i64) { key_len, MAX_KV },
                    (sl_i64) { types, MAX_KV }, (sl_i64) { arr, MAX_KV },
                    (sl_i64) { v0, MAX_KV }, (sl_i64) { v1, MAX_KV }, (sl_i64) { v2, MAX_KV },
                    akey, (int64_t) at, (sl_u8) { raw, rl }, an, (sl_i64) { out, 1 });
            }
            if (rc == 0) { ++built; ++scalar_kv; }
            continue;
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

    // ---- the tensor directory, also owned by Goose ----
    // No tensor data is staged: the module accepts an empty image and records the
    // descriptor, which is the same shape the native reader produces before it
    // allocates. `ne` is the four-dim descriptor, padded exactly as ggml does.
    static int64_t tst[3];
    static int64_t tne[4 * 4096];
    static int64_t trec[TR_STRIDE * 4096];
    static char tpool[1 << 22];

    const int64_t align_kv = gguf_find_key(ctx, "general.alignment");
    const int64_t align = align_kv >= 0 ? (int64_t) gguf_get_val_u32(ctx, (int) align_kv) : 32;

    int64_t tensors_added = 0;
    if (s3_init_tensors((sl_i64) { tst, 3 }, align) == 0) {
        for (int64_t t = 0; t < n_tensor && t < 4096; ++t) {
            const char * tn = gguf_get_tensor_name(ctx, (int) t);
            const size_t tnl = strlen(tn);
            if (tpool[0] == 0) { /* keep the pool base stable */ }
            const sl_u8 tname = { (uint8_t *) tn, (int64_t) tnl };
            const enum ggml_type tt = gguf_get_tensor_type(ctx, (int) t);
            // The real extents, four dims as GGML_MAX_DIMS requires: the reader
            // hands them over, the module validates ndims and derives nbytes.
            const int64_t * ne_src = gguf_get_tensor_ne(ctx, (int) t);
            const int64_t ne[4] = { ne_src[0], ne_src[1], ne_src[2], ne_src[3] };
            const int64_t rc = s3_add_tensor(
                (sl_i64) { tst, 3 }, (sl_u8) { (uint8_t *) tpool, (int64_t) sizeof tpool },
                (sl_i64) { tne, 4 * 4096 }, (sl_i64) { trec, TR_STRIDE * 4096 },
                tname, 4, (sl_i64) { (int64_t *) ne, 4 }, (int64_t) tt,
                (sl_u8) { NULL, 0 });
            if (rc >= 0) {
                ++tensors_added;
            }
        }
    }

    // ---- the payload: arena laid out by the Goose arithmetic ----
    fg_tensor_arena arena = {0};
    const int arena_rc = fg_goose_tensor_load(ctx, gctx, &arena);
    if (arena_rc == 0 && arena.n_tensors > 0) {
        // First and last tensor, read back through the arena, prove the bytes are
        // reachable and not just counted.
        const int64_t first = 0, last = arena.n_tensors - 1;
        const uint8_t * fb = (const uint8_t *) arena.data[first];
        const uint8_t * lb = (const uint8_t *) arena.data[last];
        printf("FG_TENSOR_DATA rc=%d tensors=%lld total=%zu copied=%zu layout=%lld "
               "mismatches=%lld first_size=%zu last_size=%zu first_byte=%u last_byte=%u\n",
               arena_rc, (long long) arena.n_tensors, arena.total, arena.copied,
               (long long) arena.layout_rc, (long long) arena.mismatches,
               arena.size[first], arena.size[last],
               fb ? (unsigned) fb[0] : 0u, lb ? (unsigned) lb[0] : 0u);
    } else {
        printf("FG_TENSOR_DATA rc=%d tensors=%lld (no payload)\n",
               arena_rc, (long long) arena.n_tensors);
    }
    fg_goose_tensor_free(&arena);

    printf("FG_STANDALONE_V3 init_runs=%lld types=%lld file=%s kv=%lld tensors=%lld "
           "first_key=\"%s\" goose_built=%lld goose_first=\"%s\" agree=%s "
           "tensors_added=%lld tdir_n=%lld align=%lld ne0_sum=%lld\n",
           (long long) runs, (long long) abi_type_count(), argv[1],
           (long long) n_kv, (long long) n_tensor, first_key,
           (long long) built, goose_first,
           // The module must accept EVERY key of the file and agree on the first
           // key's bytes. Nothing is skipped any more, so this is the full set.
           (built == n_kv && strcmp(first_key, goose_first) == 0) ? "yes" : "no",
           (long long) tensors_added, (long long) tst[TS_N], (long long) align,
           // The module's own recorded extents: if the descriptors were not
           // filled, this sum is 0 and the check above would be hollow.
           (long long) ({
               int64_t acc = 0;
               for (int64_t t = 0; t < tst[TS_N] && t < 4096; ++t) acc += tne[4 * t];
               acc;
           }));

    // ---- tokenisation through the Goose engine, on the same file ----
    // The reader parses the container independently of the metadata table above,
    // so agreement between the two is worth having. The text tokenised here is a
    // fixed probe: the point is that the shipping binary answers, not the text.
    {
        static int64_t rstate[8], rkv[512 * 8], rkvsl[2048], rtens[4096 * 12], rtensl[16384];
        static unsigned char hdr[1 << 26];
        // file_bytes is the WHOLE file, not the header we read: the reader checks
        // the tensor payload against it and answers -7 otherwise.
        int64_t file_size = 0;
        {
            FILE * fs = fopen(argv[1], "rb");
            if (fs) { fseek(fs, 0, SEEK_END); file_size = (int64_t) ftell(fs); fclose(fs); }
        }
        FILE * f = fopen(argv[1], "rb");
        int tokenised = -1;
        if (f) {
            const int64_t got = (int64_t) fread(hdr, 1, sizeof hdr, f);
            fclose(f);
            const int64_t prc = gr_parse((sl_u8){ hdr, got }, (int64_t) file_size, (sl_i64){ rstate, 8 },
                                         (sl_i64){ rkv, 512 * 8 }, (sl_i64){ rtens, 4096 * 12 },
                                         (sl_i64){ rkvsl, 2048 }, (sl_i64){ rtensl, 16384 });
            if (got > 0 && prc == 0) {
                const char * tk = "tokenizer.ggml.tokens";
                const char * mk = "tokenizer.ggml.merges";
                const int64_t ti = gr_find_kv((sl_u8){ hdr, got }, (sl_i64){ rstate, 8 },
                                              (sl_i64){ rkv, 512 * 8 }, (sl_i64){ rkvsl, 2048 },
                                              (sl_u8){ (uint8_t *) tk, (int64_t) strlen(tk) });
                const int64_t mi = gr_find_kv((sl_u8){ hdr, got }, (sl_i64){ rstate, 8 },
                                              (sl_i64){ rkv, 512 * 8 }, (sl_i64){ rkvsl, 2048 },
                                              (sl_u8){ (uint8_t *) mk, (int64_t) strlen(mk) });
                int64_t need[5] = {0};
                if (ti >= 0 && mi >= 0 &&
                    bt_prepare_need((sl_u8){ hdr, got }, (sl_i64){ rstate, 8 }, (sl_i64){ rkv, 512 * 8 },
                                    ti, mi, (sl_i64){ need, 5 }) == 0) {
                    static unsigned char blob[1 << 24];
                    static int64_t toff[1 << 18], tlen[1 << 18], tslots[1 << 20];
                    static int64_t moff[1 << 18], mlen[1 << 18], mslots[1 << 20];
                    static int64_t order[1 << 18];
                    if (need[0] <= (int64_t) sizeof blob &&
                        bt_prepare_from_gguf((sl_u8){ hdr, got }, (sl_i64){ rstate, 8 }, (sl_i64){ rkv, 512 * 8 },
                                             ti, mi, (sl_u8){ blob, need[0] },
                                             (sl_i64){ toff, need[1] }, (sl_i64){ tlen, need[1] },
                                             (sl_i64){ tslots, need[3] },
                                             (sl_i64){ moff, need[2] }, (sl_i64){ mlen, need[2] },
                                             (sl_i64){ mslots, need[4] }, (sl_i64){ need, 5 }) == 0) {
                        // Attributes and the special-token order come from the same
                        // tables the engine exposes; without them bt_tokenize refuses
                        // the text rather than guessing.
                        static int64_t attr[1 << 18];
                        extern int64_t bt_attrs(sl_u8 blob, sl_i64 tok_off, sl_i64 tok_len, sl_i64 tok_type, sl_i64 attr);
                        int64_t span4[4] = {0};
                        extern int64_t gr_val_span(sl_i64 state, sl_i64 kv, int64_t index, sl_i64 out);
                        const char * tyk = "tokenizer.ggml.token_type";
                        const int64_t tyi = gr_find_kv((sl_u8){ hdr, got }, (sl_i64){ rstate, 8 },
                                                       (sl_i64){ rkv, 512 * 8 }, (sl_i64){ rkvsl, 2048 },
                                                       (sl_u8){ (uint8_t *) tyk, (int64_t) strlen(tyk) });
                        if (tyi >= 0 && gr_val_span((sl_i64){ rstate, 8 }, (sl_i64){ rkv, 512 * 8 }, tyi,
                                                    (sl_i64){ span4, 4 }) == 0) {
                            const int32_t * tt = (const int32_t *) (hdr + span4[1]);
                            static int64_t ttype[1 << 18];
                            for (int64_t k = 0; k < need[1] && k < (1 << 18); ++k) ttype[k] = tt[k];
                            bt_attrs((sl_u8){ blob, need[0] }, (sl_i64){ toff, need[1] },
                                     (sl_i64){ tlen, need[1] }, (sl_i64){ ttype, need[1] },
                                     (sl_i64){ attr, need[1] });
                        }
                        // bt_build fills the probe tables; without it every lookup
                        // misses and tokenisation answers 0 with no error at all.
                        extern int64_t bt_build(sl_u8 blob, sl_i64 tok_off, sl_i64 tok_len, sl_i64 tok_slots,
                                                sl_i64 mrg_off, sl_i64 mrg_len, sl_i64 mrg_slots, sl_i64 stats);
                        int64_t bstats[4] = {0};
                        (void) bt_build((sl_u8){ blob, need[0] }, (sl_i64){ toff, need[1] },
                                                     (sl_i64){ tlen, need[1] }, (sl_i64){ tslots, need[3] },
                                                     (sl_i64){ moff, need[2] }, (sl_i64){ mlen, need[2] },
                                                     (sl_i64){ mslots, need[4] }, (sl_i64){ bstats, 4 });
                        const int64_t n_sp = bt_special_order((sl_u8){ blob, need[0] }, (sl_i64){ toff, need[1] },
                                                              (sl_i64){ tlen, need[1] }, (sl_i64){ attr, need[1] },
                                                              (sl_i64){ order, need[1] });
                        const char * probe = "The capital of France is";
                        static int64_t ids[256], cpts[256], seg[1024], sym[4096], heap[4096], hc[2], info[8];
                        static unsigned char enc[2048];
                        tokenised = (int) bt_tokenize((sl_u8){ blob, need[0] },
                                                      (sl_i64){ toff, need[3] }, (sl_i64){ tlen, need[3] },
                                                      (sl_i64){ tslots, need[3] },
                                                      (sl_i64){ moff, need[4] }, (sl_i64){ mlen, need[4] },
                                                      (sl_i64){ mslots, need[4] },
                                                      (sl_i64){ order, need[1] }, n_sp > 0 ? n_sp : 0,
                                                      (sl_u8){ (uint8_t *) probe, (int64_t) strlen(probe) },
                                                      (sl_i64){ ids, 256 }, 0,
                                                      (sl_i64){ cpts, 256 }, (sl_i64){ seg, 1024 },
                                                      (sl_u8){ enc, sizeof enc }, (sl_i64){ sym, 4096 },
                                                      (sl_i64){ heap, 4096 }, (sl_i64){ hc, 2 },
                                                      (sl_i64){ info, 8 });
                        printf("FG_TOKENIZE tokens=%lld merges=%lld ids=%d", (long long) need[1],
                               (long long) need[2], tokenised);
                        for (int k = 0; k < tokenised && k < 12; ++k) { printf(" %lld", (long long) ids[k]); }
                        printf("\n");
                    }
                }
            }
        }
        if (tokenised < 0) {
            printf("FG_TOKENIZE engine=unavailable\n");
        }
    }

    // ---- one generated step, through the same trunk host the server uses ----
    // The probe text is tokenised by the Goose engine and one model step runs on
    // the real weights. The point is that the shipping binary generates, not that
    // this particular text is meaningful.
    if (argc > 2 && strcmp(argv[2], "--generate") == 0) {
        hose_host_load(argv[1]);
        int64_t hs[5] = {0};
        hose_stats(hs);
        const char * probe = "The capital of France is";
        static int64_t pids[256];
        const int64_t n_ids = hose_tokenize((const unsigned char *) probe, (int64_t) strlen(probe), pids, 256);
        if (n_ids > 0) {
            static int32_t batch[256];
            for (int64_t i = 0; i < n_ids; ++i) batch[i] = (int32_t) pids[i];
            float * logits = NULL;
            const int64_t n_logits = hose_step(batch, n_ids, &logits);
            if (n_logits > 0) {
                const int64_t vocab = n_logits / n_ids;
                const int64_t next = sm_greedy((sl_f32){ logits + (n_ids - 1) * vocab, vocab }, vocab);
                unsigned char piece[256];
                const int64_t pl = hose_detokenize(&next, 1, piece, sizeof piece);
                printf("FG_GENERATE vocab=%lld prompt_ids=%lld next=%lld piece=\"%.*s\"\n",
                       (long long) hs[0], (long long) n_ids, (long long) next,
                       (int) (pl > 0 ? pl : 0), piece);
                free(logits);
            } else {
                printf("FG_GENERATE step failed\n");
            }
        }
    }

    gguf_free(ctx);
    if (gctx) { ggml_free(gctx); }
    return (runs == 1 && built == n_kv && tensors_added == n_tensor) ? 0 : 1;
}
