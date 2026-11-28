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

_Static_assert(offsetof(__typeof__(decode_plan), enabled) == 4 &&
    offsetof(__typeof__(decode_plan), words) == 8 &&
    Q35_DECODE_PLAN_WORDS == 10 && HOSE_DEFAULT_PREFILL_BATCH == 128, "Goose decode state ABI");

uint64_t fg_cuda_decode_borrow(size_t bytes) { return (uintptr_t)borrow_decode_exchange(bytes); }
void fg_cuda_decode_release(void) { decode_exchange.borrowed = false; }
uint64_t fg_cuda_decode_plan_address(void) { return (uintptr_t)&decode_plan; }

uint64_t fg_cuda_generation_ids_address(void) {
    static int64_t ids[MAX_REQ];
    return (uintptr_t)ids;
}
