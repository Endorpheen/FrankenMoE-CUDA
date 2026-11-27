#include <inttypes.h>
#include <stdio.h>
#include "ggml.h"
#include "tensor.h"

int main(void) {
    goose_init(0, NULL);
    size_t count = 0;
    for (int kind = 0; kind < GGML_TYPE_COUNT; ++kind) {
        const int64_t block_size = ggml_blck_size(kind);
        const size_t type_size = ggml_type_size(kind);
        if (block_size <= 0 || !type_size) continue;
        for (int variant = 0; variant < 256; ++variant) {
            struct ggml_tensor tensor = {.type = kind};
            tensor.ne[0] = block_size * (variant % 13 + 1);
            tensor.ne[1] = variant % 7 + 1;
            tensor.ne[2] = variant % 5 + 1;
            tensor.ne[3] = variant % 3 + 1;
            tensor.nb[0] = type_size * (variant % 2 + 1);
            tensor.nb[1] = tensor.nb[0] * (tensor.ne[0] / block_size) + variant % 3;
            tensor.nb[2] = tensor.nb[1] * tensor.ne[1] + variant % 5;
            tensor.nb[3] = tensor.nb[2] * tensor.ne[2] + variant % 7;
            if (variant < 4) tensor.ne[variant] = 0;
            int64_t layout[8];
            for (int axis = 0; axis < 4; ++axis) {
                layout[axis] = tensor.ne[axis];
                layout[axis + 4] = (int64_t) tensor.nb[axis];
            }
            const int64_t actual = goose_tensor_span(kind, (sl_i64){layout, 8});
            const size_t expected = ggml_nbytes(&tensor);
            if (actual < 0 || (uint64_t) actual != expected) {
                fprintf(stderr, "kind %d variant %d: expected %zu, got %" PRId64 "\n",
                        kind, variant, expected, actual);
                return 1;
            }
            ++count;
        }
    }
    printf("%zu GGML tensor layouts matched\n", count);
}
