#define _POSIX_C_SOURCE 200809L   // clock_gettime / CLOCK_MONOTONIC
// Generation inside the standalone runtime: HTTP in, real tokens out.
//
// The host maps the file, binds system calls and carries the model handles.
// Goose owns the inference step, including its graph and KV transaction.

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
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
#include "inkling-backend.h"
#include "session.h"
_Static_assert(HOSE_DEFAULT_CONTEXT == 2048 && BONSAI_SESSION_VOCAB == 248320 && INKLING_TEXT_VOCAB == 200058 &&
    INKLING_KV_F16 == 0 && INKLING_KV_Q4_0 == 1, "Goose CUDA model host constants");

#ifndef _WIN32
_Static_assert(sizeof(struct stat) == 144 && offsetof(struct stat, st_mode) == 24 &&
    offsetof(struct stat, st_size) == 48 && _SC_PAGESIZE == 30 && O_CLOEXEC == 524288 &&
    S_IFREG == 0x8000 && PROT_READ == 1 && MAP_PRIVATE == 2, "Goose Linux model mapping ABI");
#endif

_Static_assert(sizeof(struct timespec) == 16 && offsetof(struct timespec, tv_sec) == 0 &&
    offsetof(struct timespec, tv_nsec) == 8 && CLOCK_MONOTONIC == 1, "Goose monotonic time ABI");
#ifdef _WIN32
_Static_assert(sizeof(((struct timespec *)0)->tv_nsec) == 4, "Goose Windows time ABI");
#else
_Static_assert(sizeof(((struct timespec *)0)->tv_nsec) == 8 && MADV_DONTNEED == 4 &&
    EINVAL == 22 && ENOSYS == 38 && EOPNOTSUPP == 95, "Goose Linux residency ABI");
#ifdef MADV_POPULATE_READ
_Static_assert(MADV_POPULATE_READ == 22, "Goose Linux populate ABI");
#endif
#endif

struct decode_exchange_abi {
    uint8_t *data;
    size_t capacity;
    bool borrowed;
};

struct decode_plan_abi {
    unsigned depth;
    bool enabled;
    int64_t words[10];
};

_Static_assert(sizeof(struct decode_exchange_abi) == 24 &&
    offsetof(struct decode_exchange_abi, borrowed) == 16 &&
    sizeof(struct decode_plan_abi) == 88 && offsetof(struct decode_plan_abi, enabled) == 4 &&
    offsetof(struct decode_plan_abi, words) == 8 && sizeof(pthread_key_t) == 4 &&
    HOSE_DEFAULT_PREFILL_BATCH == 128, "Goose decode state ABI");
