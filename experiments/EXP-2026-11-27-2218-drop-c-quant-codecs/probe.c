#include "ggml.h"
#include <inttypes.h>
#include <stdio.h>

int main(void) {
    for (int i = 0; i < GGML_TYPE_COUNT; ++i) {
        const enum ggml_type type = (enum ggml_type)i;
        const int64_t block = ggml_blck_size(type);
        const char *name = ggml_type_name(type);
        printf("%d %s %" PRId64 " %zu %d %zu\n", i, name ? name : "", block,
               ggml_type_size(type), ggml_is_quantized(type),
               block > 0 ? ggml_row_size(type, block * 17) : 0);
    }
}
