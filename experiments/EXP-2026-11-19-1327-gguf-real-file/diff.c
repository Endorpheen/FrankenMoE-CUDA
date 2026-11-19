/* The ported GGUF header parser against the native reader, on the real model file.
 *
 * The port parses the first 64 MiB window and reports version, key count, tensor count, alignment and
 * data offset in `state`. The native side is gguf_init_from_file from the same build, asked the same
 * questions. Nothing is transcribed: both sides are code.
 */
#include "gguf.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int64_t * data; int64_t len; } sl_i64;
typedef struct { uint8_t * data; int64_t len; } sl_u8;
typedef int64_t (*fn_parse)(sl_u8, int64_t, sl_i64, sl_i64, sl_i64, sl_i64, sl_i64);
typedef int64_t (*fn_find)(sl_u8, sl_i64, sl_i64, sl_i64, sl_u8);
typedef int64_t (*fn_meta_int)(sl_u8, sl_i64, sl_i64, int64_t, sl_i64);
typedef int64_t (*fn_kv_type)(sl_i64, sl_i64, int64_t);
typedef int64_t (*fn_span)(sl_i64, sl_i64, int64_t, sl_i64);

#define WINDOW (64u * 1024u * 1024u)

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    FILE * f = fopen(path, "rb");
    if (!f) { perror("open"); return 2; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    const size_t win = size < (long) WINDOW ? (size_t) size : WINDOW;
    uint8_t * data = malloc(win);
    if (!data || fread(data, 1, win, f) != win) { printf("read failed\n"); return 2; }
    fclose(f);

    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 3; }
    fn_parse parse = (fn_parse) dlsym(h, "gr_parse");
    if (!parse) { printf("no gr_parse\n"); return 3; }

    /* buffers sized from the native counts, so a wrong size cannot be mistaken for a wrong answer */
    struct gguf_init_params gp = { .no_alloc = true, .ctx = NULL };
    struct gguf_context * g = gguf_init_from_file(path, gp);
    if (!g) { printf("native reader refused the file\n"); return 4; }
    const int64_t n_kv = (int64_t) gguf_get_n_kv(g);
    const int64_t n_t  = (int64_t) gguf_get_n_tensors(g);
    const size_t al    = gguf_get_alignment(g);
    const size_t off   = gguf_get_data_offset(g);
    gguf_free(g);

    int64_t state[8] = {0};
    int64_t * kv = calloc((size_t) n_kv, 8 * sizeof(int64_t));
    int64_t * tn = calloc((size_t) n_t, 24 * sizeof(int64_t));
    int64_t * slots = calloc(4096, sizeof(int64_t));
    sl_u8 s_data; s_data.data = data; s_data.len = (int64_t) win;
    sl_i64 s_state; s_state.data = state; s_state.len = 8;
    sl_i64 s_kv;    s_kv.data = kv; s_kv.len = n_kv * 8;
    sl_i64 s_t;     s_t.data = tn;  s_t.len = n_t * 24;
    sl_i64 s_slots; s_slots.data = slots; s_slots.len = 4096;
    const int64_t rc = parse(s_data, (int64_t) size, s_state, s_kv, s_t, s_slots, s_slots);

    int fails = 0;
    printf("rc=%lld version=%lld nk=%lld nt=%lld align=%lld data_off=%lld\n",
           (long long) rc, (long long) state[1], (long long) state[2], (long long) state[3],
           (long long) state[4], (long long) state[5]);
    printf("native: nk=%lld nt=%lld align=%zu data_off=%zu\n",
           (long long) n_kv, (long long) n_t, al, off);
    /* the success marker is state[0], not the return value: the return is a byte count */
    if (state[0] != 919) { ++fails; printf("PARSE REFUSED state0=%lld rc=%lld\n", (long long) state[0], (long long) rc); }
    if (state[2] != n_kv) { ++fails; printf("NK MISMATCH port=%lld native=%lld\n", (long long) state[2], (long long) n_kv); }
    if (state[3] != n_t)  { ++fails; printf("NT MISMATCH port=%lld native=%lld\n", (long long) state[3], (long long) n_t); }
    if ((size_t) state[4] != al)  { ++fails; printf("ALIGN MISMATCH port=%lld native=%zu\n", (long long) state[4], al); }
    if ((size_t) state[5] != off) { ++fails; printf("OFFSET MISMATCH port=%lld native=%zu\n", (long long) state[5], off); }
    /* every key and every tensor: the port's lookup must return the same index the native reader has */
    fn_find find_kv = (fn_find) dlsym(h, "gr_find_kv");
    fn_find find_tn = (fn_find) dlsym(h, "gr_find_tensor");
    if (!find_kv || !find_tn) { printf("missing lookup doors\n"); return 3; }

    struct gguf_init_params gp2 = { .no_alloc = true, .ctx = NULL };
    struct gguf_context * g2 = gguf_init_from_file(path, gp2);
    int kv_fail = 0, tn_fail = 0;
    for (int64_t i = 0; i < n_kv; ++i) {
        const char * k = gguf_get_key(g2, i);
        sl_u8 s_key; s_key.data = (uint8_t *) k; s_key.len = (int64_t) strlen(k);
        const int64_t got = find_kv(s_data, s_state, s_kv, s_slots, s_key);
        if (got != i) {
            ++kv_fail;
            if (kv_fail <= 3) printf("KV MISMATCH key=%s native=%lld port=%lld\n", k, (long long) i, (long long) got);
        }
    }
    for (int64_t i = 0; i < n_t; ++i) {
        const char * k = gguf_get_tensor_name(g2, (int32_t) i);
        sl_u8 s_key; s_key.data = (uint8_t *) k; s_key.len = (int64_t) strlen(k);
        const int64_t got = find_tn(s_data, s_state, s_t, s_slots, s_key);
        if (got != i) {
            ++tn_fail;
            if (tn_fail <= 3) printf("TENSOR MISMATCH name=%s native=%lld port=%lld\n", k, (long long) i, (long long) got);
        }
    }
    /* integer-valued keys: the port's decoded value against the native one, key by key */
    fn_meta_int meta_int = (fn_meta_int) dlsym(h, "gr_meta_int");
    int val_cmp = 0, val_fail = 0;
    if (meta_int) {
        for (int64_t i = 0; i < n_kv; ++i) {
            enum gguf_type t = gguf_get_kv_type(g2, i);
            int64_t native = 0; int is_int = 1;
            switch (t) {
                case GGUF_TYPE_UINT8:  native = gguf_get_val_u8 (g2, i); break;
                case GGUF_TYPE_INT8:   native = gguf_get_val_i8 (g2, i); break;
                case GGUF_TYPE_UINT16: native = gguf_get_val_u16(g2, i); break;
                case GGUF_TYPE_INT16:  native = gguf_get_val_i16(g2, i); break;
                case GGUF_TYPE_UINT32: native = gguf_get_val_u32(g2, i); break;
                case GGUF_TYPE_INT32:  native = gguf_get_val_i32(g2, i); break;
                case GGUF_TYPE_UINT64: native = (int64_t) gguf_get_val_u64(g2, i); break;
                case GGUF_TYPE_INT64:  native = gguf_get_val_i64(g2, i); break;
                default: is_int = 0; break;
            }
            if (!is_int) { continue; }
            int64_t out1[1] = {0};
            sl_i64 s_out; s_out.data = out1; s_out.len = 1;
            const int64_t rc2 = meta_int(s_data, s_state, s_kv, i, s_out);
            ++val_cmp;
            if (rc2 != 0 || out1[0] != native) {
                ++val_fail;
                if (val_fail <= 3) {
                    printf("VALUE MISMATCH key=%s native=%lld port_rc=%lld port=%lld\n",
                           gguf_get_key(g2, i), (long long) native, (long long) rc2, (long long) out1[0]);
                }
            }
        }
        printf("integer keys: %d/%d equal\n", val_cmp - val_fail, val_cmp);
    }

    /* key types and string values: the type code and the bytes of every string key */
    fn_kv_type kv_type = (fn_kv_type) dlsym(h, "gr_kv_type");
    fn_span    val_span = (fn_span) dlsym(h, "gr_val_span");
    int type_cmp = 0, type_fail = 0, str_cmp = 0, str_fail = 0;
    if (kv_type && val_span) {
        for (int64_t i = 0; i < n_kv; ++i) {
            const int64_t port_t = kv_type(s_state, s_kv, i);
            const int64_t native_t = (int64_t) gguf_get_kv_type(g2, i);
            ++type_cmp;
            if (port_t != native_t) {
                ++type_fail;
                if (type_fail <= 3) printf("TYPE MISMATCH key=%s native=%lld port=%lld\n",
                                           gguf_get_key(g2, i), (long long) native_t, (long long) port_t);
            }
            if (native_t == GGUF_TYPE_STRING) {
                int64_t out2[4] = {0};   /* the door requires at least four slots */
                sl_i64 s_out2; s_out2.data = out2; s_out2.len = 4;
                const int64_t rc3 = val_span(s_state, s_kv, i, s_out2);
                const char * ns = gguf_get_val_str(g2, i);
                ++str_cmp;
                /* layout is out[0]=type, out[1]=payload offset, out[2]=payload bytes */
                const bool ok = rc3 == 0 && out2[2] == (int64_t) strlen(ns) &&
                                out2[1] >= 0 && (size_t) (out2[1] + out2[2]) <= win &&
                                memcmp(data + out2[1], ns, (size_t) out2[2]) == 0;
                if (!ok) {
                    ++str_fail;
                    if (str_fail <= 3) printf("STRING MISMATCH key=%s native_len=%zu port_rc=%lld port_off=%lld port_len=%lld\n",
                                              gguf_get_key(g2, i), strlen(ns), (long long) rc3, (long long) out2[1], (long long) out2[2]);
                }
            }
        }
        printf("key types: %d/%d equal   string values: %d/%d equal\n",
               type_cmp - type_fail, type_cmp, str_cmp - str_fail, str_cmp);
    }

    /* the negative case: a key that is not there must be reported as absent by both */
    const char * absent = "general.this.key.does.not.exist";
    sl_u8 s_absent; s_absent.data = (uint8_t *) absent; s_absent.len = (int64_t) strlen(absent);
    const int64_t port_absent  = find_kv(s_data, s_state, s_kv, s_slots, s_absent);
    const int64_t native_absent = (int64_t) gguf_find_key(g2, absent);
    if (port_absent != -1 || native_absent != -1) {
        printf("ABSENT MISMATCH port=%lld native=%lld\n", (long long) port_absent, (long long) native_absent);
        ++kv_fail;
    } else {
        printf("absent key: both report -1\n");
    }
    gguf_free(g2);
    printf("lookup: kv=%lld/%lld tensor=%lld/%lld fails=%d\n",
           (long long) (n_kv - kv_fail), (long long) n_kv, (long long) (n_t - tn_fail), (long long) n_t, kv_fail + tn_fail);
    printf("gguf header: fails=%d\n", fails + kv_fail + tn_fail + val_fail + type_fail + str_fail);
    return (fails + kv_fail + tn_fail + val_fail + type_fail + str_fail) ? 1 : 0;
}
