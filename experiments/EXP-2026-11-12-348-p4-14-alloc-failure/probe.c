#include "ggml.h"
#include <stdio.h>
#include <string.h>
int main(void){ setvbuf(stdout,NULL,_IONBF,0);
    struct ggml_init_params p = { 4u*1024u*1024u, NULL, false };
    struct ggml_context * c = ggml_init(p);
    printf("init=%s\n", c ? "ok" : "null");
    if (!c) return 3;
    for (int i = 0; i < 200; ++i) {
        struct ggml_tensor * t = ggml_new_tensor_1d(c, GGML_TYPE_F32, 1024);
        if (!t) { printf("tensor %d null\n", i); return 4; }
    }
    printf("all tensors ok, used=%zu\n", ggml_used_mem(c));
    ggml_free(c);
    return 0;
}
