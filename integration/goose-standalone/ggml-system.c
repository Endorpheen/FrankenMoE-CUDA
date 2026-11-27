#ifndef GOOSEINFER_TYPE_METADATA_ONLY
#include "ggml-backend-impl.h"
#endif

#include <pthread.h>

static pthread_mutex_t ggml_critical_lock = PTHREAD_MUTEX_INITIALIZER;

void ggml_critical_section_start(void) {
    pthread_mutex_lock(&ggml_critical_lock);
}

void ggml_critical_section_end(void) {
    pthread_mutex_unlock(&ggml_critical_lock);
}

#ifndef GOOSEINFER_TYPE_METADATA_ONLY
#include "ggml-system-reference.inc"
#endif
