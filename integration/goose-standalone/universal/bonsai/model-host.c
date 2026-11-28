#define _POSIX_C_SOURCE 200809L   // clock_gettime / CLOCK_MONOTONIC
// Generation inside the standalone runtime: HTTP in, real tokens out.
//
// The host maps the file, binds system calls and carries the model handles.
// Goose owns the inference step, including its graph and KV transaction.

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <fcntl.h>
#include <inttypes.h>
#ifdef _WIN32
#include "windows/platform.h"
#else
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#if __has_include("unit.h")
#include "unit.h"
#else
#include "main.h"
#endif
#include "goose-host-llm.h"
#include "goose-host.h"
#include "graph-execute.h"
#include "apex-cached-decode.h"
#include "inkling-backend.h"
#include "server-adapter.h"
#include "session.h"
int bonsai_session_step_phase(struct bonsai_session *, const uint32_t *, unsigned, float *, int);

extern __typeof__(gr_parse) bonsai_gr_parse;
extern void bonsai_goose_init(int argc, char **argv);

#define MAX_REQ   262144

extern bool fg_q35_checkpoint_enabled(void);

extern __typeof__(bt_tokenize) bt_tokenize_inkling2;
extern void inkling_tokenizer_init(int argc, char **argv);

const char * die(const char * what) { fprintf(stderr, "GENERATE %s\n", what); exit(1); }

double fg_q35_generation_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

typedef struct {
    uint8_t * base;
    size_t size;
    int64_t page_size;
    int64_t owner[13];
    // Goose vocabulary tables
    int64_t state[8];
    int64_t * kv, * kvsl, * tens, * tensl;
    uint8_t * blob;
    int64_t * tok_off, * tok_len, * tok_slots, * mrg_off, * mrg_len, * mrg_slots, * attr, * order;
    int64_t n_tok, n_mrg, n_special, n_tok_slots, n_mrg_slots, blob_len;
    int64_t * eog;
    int64_t n_eog, eos;
    // trunk state
    int64_t layer_count, expert_cache_limit, acfg[14], gcfg[7], mcfg[3], kv_state[6];
    int64_t handles[4];
    int64_t * records;
    struct decoder *inkling;
    bool gigachat4;
    struct bonsai_session *bonsai;
    bool bonsai_layout;
    bool inkling_tokenizer;
    int64_t kv_format;
    int64_t * scratch_cpts, * scratch_seg, * scratch_sym, * scratch_heap, * scratch_hc, * scratch_info;
    uint8_t * scratch_enc;
} host_t;

_Static_assert(offsetof(host_t, base) == 0 &&
    offsetof(host_t, size) == 8 &&
    offsetof(host_t, state) == 128 &&
    offsetof(host_t, kv) == 192 &&
    offsetof(host_t, kvsl) == 200 &&
    offsetof(host_t, tens) == 208 &&
    offsetof(host_t, tensl) == 216 &&
    offsetof(host_t, blob) == 224 &&
    offsetof(host_t, tok_off) == 232 &&
    offsetof(host_t, tok_len) == 240 &&
    offsetof(host_t, tok_slots) == 248 &&
    offsetof(host_t, mrg_off) == 256 &&
    offsetof(host_t, mrg_len) == 264 &&
    offsetof(host_t, mrg_slots) == 272 &&
    offsetof(host_t, attr) == 280 &&
    offsetof(host_t, order) == 288 &&
    offsetof(host_t, n_tok) == 296 &&
    offsetof(host_t, n_mrg) == 304 &&
    offsetof(host_t, n_special) == 312 &&
    offsetof(host_t, n_tok_slots) == 320 &&
    offsetof(host_t, n_mrg_slots) == 328 &&
    offsetof(host_t, blob_len) == 336 &&
    offsetof(host_t, eog) == 344 &&
    offsetof(host_t, n_eog) == 352 &&
    offsetof(host_t, eos) == 360 &&
    offsetof(host_t, bonsai_layout) == 688 &&
    offsetof(host_t, inkling_tokenizer) == 689 &&
    offsetof(host_t, scratch_cpts) == 704 &&
    offsetof(host_t, scratch_seg) == 712 &&
    offsetof(host_t, scratch_sym) == 720 &&
    offsetof(host_t, scratch_heap) == 728 &&
    offsetof(host_t, scratch_hc) == 736 &&
    offsetof(host_t, scratch_info) == 744 &&
    offsetof(host_t, scratch_enc) == 752 && MAX_REQ == 262144, "Goose CUDA vocabulary ABI");

_Static_assert(offsetof(host_t, page_size) == 16 &&
    offsetof(host_t, owner) == 24 &&
    offsetof(host_t, layer_count) == 368 &&
    offsetof(host_t, expert_cache_limit) == 376 &&
    offsetof(host_t, acfg) == 384 &&
    offsetof(host_t, gcfg) == 496 &&
    offsetof(host_t, mcfg) == 552 &&
    offsetof(host_t, kv_state) == 576 &&
    offsetof(host_t, handles) == 624 &&
    offsetof(host_t, records) == 656 &&
    offsetof(host_t, inkling) == 664 &&
    offsetof(host_t, gigachat4) == 672 &&
    offsetof(host_t, bonsai) == 680 &&
    offsetof(host_t, kv_format) == 696 && HOSE_DEFAULT_CONTEXT == 2048 &&
    BONSAI_SESSION_VOCAB == 248320 && INKLING_TEXT_VOCAB == 200058 &&
    INKLING_KV_F16 == 0 && INKLING_KV_Q4_0 == 1, "Goose CUDA model host ABI");
#ifndef _WIN32
_Static_assert(sizeof(struct stat) == 144 && offsetof(struct stat, st_mode) == 24 &&
    offsetof(struct stat, st_size) == 48 && _SC_PAGESIZE == 30 && O_CLOEXEC == 524288 &&
    S_IFREG == 0x8000 && PROT_READ == 1 && MAP_PRIVATE == 2, "Goose Linux model mapping ABI");
#endif

host_t H;
uint64_t fg_cuda_model_host_address(void) { return (uintptr_t)&H; }

static int64_t prefill_batch = HOSE_DEFAULT_PREFILL_BATCH;
static int64_t prefill_ubatch = HOSE_DEFAULT_PREFILL_BATCH;
static int64_t expert_cache_request;

int64_t fg_cuda_model_expert_request(void) { return expert_cache_request; }

int64_t fg_model_mincore(uint64_t address, int64_t bytes, sl_u8 flags) {
    #ifdef _WIN32
    return fg_win_resident((void *)(uintptr_t)address, (size_t)bytes, flags.data);
    #else
    return mincore((void *) (uintptr_t) address, (size_t) bytes, flags.data);
    #endif
}

int64_t fg_model_populate(uint64_t address, int64_t bytes) {
#ifdef MADV_POPULATE_READ
    if (madvise((void *) (uintptr_t) address, (size_t) bytes, MADV_POPULATE_READ) == 0) return 0;
    if (errno != EINVAL && errno != ENOSYS && errno != EOPNOTSUPP) return -errno;
#else
    (void) address;
    (void) bytes;
#endif
    return 1;
}

void fg_model_touch_page(uint64_t address) {
    (void) *(const volatile uint8_t *) (uintptr_t) address;
}

extern bool fg_standalone_option_enabled(const char * name);

// The caller owns this span in a private anonymous tensor arena. Only discard
// pages entirely inside the span, as boundary pages may contain other objects.
int64_t fg_model_zero_host_pages(uint64_t address, int64_t length) {
#if defined(__linux__) && defined(MADV_DONTNEED)
    if (!fg_standalone_option_enabled("GOOSE_HOST_KV_DECOMMIT") ||
        !address || length <= 0 || (uint64_t) length > UINTPTR_MAX - address) return 0;
    const long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) return 0;
    const size_t bytes = (size_t) length;
    const size_t leading = ((size_t) page - address % (size_t) page) % (size_t) page;
    if (leading >= bytes) return 0;
    const size_t whole = (bytes - leading) / (size_t) page * (size_t) page;
    uint8_t * const start = (void *) (uintptr_t) address;
    if (!whole || madvise(start + leading, whole, MADV_DONTNEED) != 0) return 0;
    memset(start, 0, leading);
    memset(start + leading + whole, 0, bytes - leading - whole);
    return 1;
#else
    (void) address;
    (void) length;
    return 0;
#endif
}

extern int64_t find_key(const char *key);
extern bool metadata_is(const char *key, const char *value);
extern void load_vocab(void);
extern int64_t tokenize(const unsigned char *text, int64_t len, int64_t *ids, int64_t capacity);
extern int64_t detokenize(const int64_t *ids, int64_t count, unsigned char *out, int64_t capacity);

void fg_q35_write_ids(uint64_t address, sl_i32 tokens) {
    memcpy((void *) (uintptr_t) address, tokens.data, (size_t) tokens.len * sizeof(*tokens.data));
}

void fg_q35_read_logits(uint64_t address, sl_f32 output) {
    memcpy(output.data, (const void *) (uintptr_t) address, (size_t) output.len * sizeof(*output.data));
}

void fg_q35_release_graph(uint64_t address) {
    free((void *) (uintptr_t) address);
}

extern int64_t fg_cuda_model_step(sl_i32 tokens, int64_t rows, sl_f32 output);

int64_t hose_tokenize(const unsigned char * text, int64_t len, int64_t * ids, int64_t cap) {
    return tokenize(text, len, ids, cap);
}

int64_t hose_detokenize(const int64_t * ids, int64_t n, unsigned char * out, int64_t cap) {
    return detokenize(ids, n, out, cap);
}

int64_t hose_eog_is(int64_t id) {
    return bt_is_eog((sl_i64){ H.eog, H.n_eog }, H.n_eog, id);
}

void hose_stats(int64_t * out5) {
    out5[0] = H.n_tok;
    out5[1] = H.n_special;
    out5[2] = H.n_eog;
    out5[3] = H.eos;
    out5[4] = H.layer_count;
}

int64_t hose_context_capacity(void) { return H.owner[10]; }

int64_t hose_configure_prefill(int64_t batch, int64_t ubatch) {
    fg_goose_host_ensure_init();
    if (!q35_prefill_valid(batch, ubatch)) return -1;
    prefill_batch = batch;
    prefill_ubatch = ubatch;
    return 0;
}

void hose_prefill_profile(int64_t profile[2]) {
    profile[0] = prefill_batch;
    profile[1] = prefill_ubatch;
}

int64_t hose_configure_expert_cache(int64_t mib) {
    const int64_t mib_bytes = 1024 * 1024;
    if (mib <= 0 || mib > INT64_MAX / mib_bytes) return -1;
    expert_cache_request = mib * mib_bytes;
    return 0;
}

int64_t hose_expert_cache_limit(void) {
    return H.expert_cache_limit;
}

void fg_q35_generation_profile(sl_i64 profile) {
    hose_prefill_profile(profile.data);
    if (profile.len >= 3) {
        const char * tail = getenv("GOOSE_PREFILL_CHECKPOINT_TAIL");
        profile.data[2] = !H.bonsai && !H.inkling && !H.gigachat4 && tail && strcmp(tail, "1") == 0;
    }
}

int64_t fg_http_eog(sl_i64 ids) {
    if (ids.len < H.n_eog) return -1;
    if (H.n_eog) memcpy(ids.data, H.eog, (size_t) H.n_eog * sizeof(*ids.data));
    return H.n_eog;
}

static pthread_once_t exchange_once = PTHREAD_ONCE_INIT;
static pthread_key_t exchange_key;
static bool exchange_key_available;
static _Thread_local struct {
    uint8_t *data;
    size_t capacity;
    bool borrowed;
} decode_exchange;

static void init_exchange_key(void) {
    exchange_key_available = pthread_key_create(&exchange_key, free) == 0;
}

static uint8_t *borrow_decode_exchange(size_t bytes) {
    if (decode_exchange.borrowed) return NULL;
    if (decode_exchange.capacity < bytes) {
        if (pthread_once(&exchange_once, init_exchange_key) != 0 || !exchange_key_available) return NULL;
        uint8_t *data = malloc(bytes);
        if (!data) return NULL;
        if (pthread_setspecific(exchange_key, data) != 0) {
            free(data);
            return NULL;
        }
        free(decode_exchange.data);
        decode_exchange.data = data;
        decode_exchange.capacity = bytes;
    }
    decode_exchange.borrowed = true;
    return decode_exchange.data;
}

enum { Q35_DECODE_PLAN_WORDS = 10 };
static _Thread_local struct {
    unsigned depth;
    bool enabled;
    int64_t words[Q35_DECODE_PLAN_WORDS];
} decode_plan;

void fg_q35_generation_scope_enter(void) {
    if (H.bonsai || H.inkling || H.gigachat4) return;
    if (decode_plan.depth++ == 0) {
        const char *reuse = getenv("GOOSE_DECODE_PLAN_REUSE");
        decode_plan.enabled = (!reuse || strcmp(reuse, "0") != 0) && !fg_q35_checkpoint_enabled();
    }
}

void fg_q35_generation_scope_leave(void) {
    if (H.bonsai || H.inkling || H.gigachat4) return;
    if (--decode_plan.depth == 0 &&
        q35_decode_plan_close((sl_i64) { H.owner, 13 },
            (sl_i64) { decode_plan.words, Q35_DECODE_PLAN_WORDS }) != 0) {
        die("decode plan release");
    }
}

static int64_t generation_step(sl_i32 tokens, sl_f32 logits, bool prefill) {
    if (H.gigachat4 || prefill || tokens.len != 1 || logits.len != H.n_tok ||
        decode_plan.depth != 1 || !decode_plan.enabled) {
        return fg_cuda_model_step(tokens, logits.len != 0, logits);
    }
    const int64_t result = q35_model_step_cached((sl_i64) { H.owner, 13 },
        (sl_i64) { H.handles, 4 }, (sl_i64) { H.records, H.layer_count * 32 },
        (sl_i64) { H.acfg, 14 }, (sl_i64) { H.gcfg, 7 }, (sl_i64) { H.mcfg, 3 },
        (sl_i64) { H.kv_state, 6 }, tokens, logits, (sl_i64) { decode_plan.words, Q35_DECODE_PLAN_WORDS });
    if (result == -2) die("model step recovery");
    return result;
}

int64_t fg_q35_generation_eval(sl_i32 tokens, sl_f32 output, uint8_t prefill) {
    if (!tokens.data || tokens.len <= 0 || output.len < 0 ||
        (output.len != 0 && !output.data) ||
        (uint64_t) tokens.len > SIZE_MAX / sizeof(int32_t) ||
        (uint64_t) output.len > SIZE_MAX / sizeof(float)) return -1;
    const size_t token_bytes = (size_t) tokens.len * sizeof(int32_t);
    const size_t logit_bytes = (size_t) output.len * sizeof(float);
    if (token_bytes > SIZE_MAX - logit_bytes) return -1;
    // Nested Goose exports start at frame zero and can reuse the caller's
    // arena slots. Pin both borrowed slices outside that arena until return.
    const size_t total_bytes = token_bytes + logit_bytes;
    uint8_t * exchange = !prefill && tokens.len == 1 ?
        borrow_decode_exchange(total_bytes) : NULL;
    const bool reused = exchange != NULL;
    if (!reused) exchange = malloc(total_bytes);
    if (!exchange) return -1;
    memcpy(exchange, tokens.data, token_bytes);
    int64_t result;
    if (H.bonsai) {
        float *row = output.len == H.n_tok ? (float *)(exchange + token_bytes) : NULL;
        const int status = (row || !output.len) && tokens.len <= UINT32_MAX ?
            bonsai_session_step_phase(H.bonsai, (const uint32_t *)exchange, (unsigned)tokens.len, row, !prefill) : -1;
        result = status == 0 ? output.len : -1;
    } else if (H.inkling) {
        result = inkling_backend_eval(H.inkling, (const int32_t *)exchange, tokens.len,
            prefill != 0, (float *)(exchange + token_bytes), output.len);
    } else {
        const bool previous = fg_standalone_set_cpu_prefill(prefill != 0);
        result = generation_step((sl_i32) { (int32_t *) exchange, tokens.len },
            (sl_f32) { (float *) (exchange + token_bytes), output.len }, prefill != 0);
        fg_standalone_set_cpu_prefill(previous);
    }
    if (result > 0) memcpy(output.data, exchange + token_bytes, logit_bytes);
    if (reused) decode_exchange.borrowed = false;
    else free(exchange);
    return result;
}

int64_t hose_generate_ids(const int64_t * prompt, int64_t count, int64_t max_tokens,
                          bool ignore_eos, int64_t * out_ids, double * timing) {
    if (!prompt || count <= 0 || !out_ids || max_tokens <= 0) return -1;
    return q35_generate((sl_i64) { (int64_t *) prompt, count }, H.n_tok, H.owner[10],
        (sl_i64) { H.eog, H.n_eog }, max_tokens, ignore_eos, (sl_i64) { out_ids, max_tokens },
        (sl_f64) { timing, timing ? 3 : 0 });
}

int64_t hose_generate_text(const unsigned char * text, int64_t len, int64_t max_tokens,
                           bool ignore_eos, int64_t * out_ids, double * timing) {
    if (timing) { timing[0] = 0.0; timing[1] = 0.0; timing[2] = 0.0; }
    if (!text || len <= 0 || len > MAX_REQ || max_tokens <= 0 || !out_ids) return -1;
    static int64_t ids[MAX_REQ];
    const int64_t n_ids = hose_tokenize(text, len, ids, MAX_REQ);
    return hose_generate_ids(ids, n_ids, max_tokens, ignore_eos, out_ids, timing);
}

int64_t hose_generate(const unsigned char * text, int64_t len, int64_t max_tokens,
                      int64_t * out_ids, unsigned char * out_text, int64_t text_cap,
                      double * timing) {
    const int64_t produced = hose_generate_text(text, len, max_tokens, false, out_ids, timing);
    if (produced >= 0 && out_text && detokenize(out_ids, produced, out_text, text_cap) < 0) return -1;
    return produced;
}

extern int bonsai_session_choice_available(const struct bonsai_session *, const int64_t *, int64_t);
extern int64_t bonsai_session_choose_next(struct bonsai_session *, uint32_t, int);
extern int bonsai_session_read_choice_logits(struct bonsai_session *, float *);

uint8_t fg_bonsai_choice_available(sl_i64 eog) {
    return H.bonsai && bonsai_session_choice_available(H.bonsai, eog.data, eog.len);
}

extern int64_t bonsai_session_mtp_next(struct bonsai_session *, uint32_t, int, int64_t);

int64_t fg_bonsai_choose_next(int32_t token, sl_f32 output, uint8_t ignore_eos, int64_t remaining) {
    if (!H.bonsai || token < 0 || !output.data || output.len != H.n_tok) return -1;
    const int64_t id = bonsai_session_mtp_next(H.bonsai, (uint32_t)token, ignore_eos != 0, remaining);
    if (id != -2) return id;
    const size_t bytes = (size_t)output.len * sizeof(float);
    float *safe = malloc(bytes);
    if (!safe) return -1;
    const int status = bonsai_session_read_choice_logits(H.bonsai, safe);
    if (!status) memcpy(output.data, safe, bytes);
    free(safe);
    return status ? -1 : -2;
}
extern uint8_t fg_q35_topk_scan_enabled(void);

uint8_t fg_bonsai_host_scan_enabled(void) {
    if (!H.bonsai) return 0;
    const char *enabled = getenv("BONSAI_HTTP_HOST_SCAN");
    return enabled && !strcmp(enabled, "1") && fg_q35_topk_scan_enabled();
}

extern void bonsai_session_mtp_prepare(struct bonsai_session *, int);
extern int64_t bonsai_session_mtp_next(struct bonsai_session *, uint32_t, int, int64_t);

void fg_bonsai_mtp_prepare(uint8_t allowed) {
    if (H.bonsai) bonsai_session_mtp_prepare(H.bonsai, allowed);
}
