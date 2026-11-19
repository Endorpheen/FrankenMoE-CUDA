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
    printf("gguf header: fails=%d\n", fails + kv_fail + tn_fail);
    return (fails + kv_fail + tn_fail) ? 1 : 0;
}
