// LOCAL CHECK ONLY link stubs for backend tensor transfer symbols pulled by
// the graph-services reset helpers. Creation fixture never calls them.
#include "ggml.h"

#include <string.h>

void ggml_backend_tensor_memset(struct ggml_tensor * tensor, int value, size_t offset, size_t size) {
    if (tensor && tensor->data && size) {
        memset((char *) tensor->data + offset, value, size);
    }
}

void ggml_backend_tensor_set(struct ggml_tensor * tensor, const void * data, size_t offset, size_t size) {
    if (tensor && tensor->data && data && size) {
        memcpy((char *) tensor->data + offset, data, size);
    }
}
