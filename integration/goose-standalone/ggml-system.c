#include "ggml-backend-impl.h"

#include <pthread.h>

static pthread_mutex_t ggml_critical_lock = PTHREAD_MUTEX_INITIALIZER;

void ggml_critical_section_start(void) {
    pthread_mutex_lock(&ggml_critical_lock);
}

void ggml_critical_section_end(void) {
    pthread_mutex_unlock(&ggml_critical_lock);
}

void ggml_backend_tensor_memset(struct ggml_tensor *tensor, uint8_t value, size_t offset, size_t size) {
    GGML_ASSERT(tensor != NULL);
    if (size == 0) {
        return;
    }
    const size_t bytes = ggml_nbytes(tensor);
    GGML_ASSERT(offset <= bytes && size <= bytes - offset);
    ggml_backend_buffer_t buffer = tensor->view_src ? tensor->view_src->buffer : tensor->buffer;
    GGML_ASSERT(tensor->data != NULL && buffer != NULL && buffer->iface.memset_tensor != NULL);
    buffer->iface.memset_tensor(buffer, tensor, value, offset, size);
}
