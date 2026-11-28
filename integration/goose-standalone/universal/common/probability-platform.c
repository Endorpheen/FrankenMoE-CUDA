#include "probability.h"
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(pthread_mutex_t) <= 64 && _Alignof(pthread_mutex_t) <= 8 &&
    sizeof(pthread_cond_t) <= 64 && _Alignof(pthread_cond_t) <= 8 &&
    sizeof(pthread_t) == 8 && _Alignof(pthread_t) <= 8 && sizeof(pthread_key_t) == 4,
    "Goose probability platform ABI");

typedef struct { uint64_t *data; int64_t len; } probability_words;
extern void fg_probability_init(int argc, char **argv);

static void *probability_thread(void *arg) {
    fg_probability_init(0, NULL);
    fg_prob_worker_run((uintptr_t)arg);
    return NULL;
}

int32_t fg_prob_platform_launch(uint64_t worker, uint64_t address) {
    return pthread_create((pthread_t *)(uintptr_t)worker, NULL, probability_thread, (void *)(uintptr_t)address);
}

probability_words fg_prob_platform_payload(uint64_t address) {
    return (probability_words){(uint64_t *)(uintptr_t)(address + 136), 14};
}

uint8_t fg_prob_native_avx2(void) {
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    return __builtin_cpu_supports("avx2") != 0;
#else
    return 0;
#endif
}
