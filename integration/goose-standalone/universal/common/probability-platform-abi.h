#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(pthread_mutex_t) <= 64 && _Alignof(pthread_mutex_t) <= 8 &&
    sizeof(pthread_cond_t) <= 64 && _Alignof(pthread_cond_t) <= 8 &&
    sizeof(pthread_t) == 8 && _Alignof(pthread_t) <= 8 && sizeof(pthread_key_t) == 4,
    "Goose probability platform ABI");
