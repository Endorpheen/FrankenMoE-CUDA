#include "ggml.h"
#include <stdio.h>
#include <string.h>

static void dump(const char * tag, struct ggml_tensor * t) {
    if (!t) { printf("%s NULL\n", tag); return; }
    printf("%s type=%s ne=[%ld,%ld,%ld,%ld] nb=[%ld,%ld,%ld,%ld] nbytes=%zu contig=%d view=%d op=%d\n",
        tag, ggml_type_name(t->type), (long)t->ne[0],(long)t->ne[1],(long)t->ne[2],(long)t->ne[3],
        (long)t->nb[0],(long)t->nb[1],(long)t->nb[2],(long)t->nb[3],
        ggml_nbytes(t), (int)ggml_is_contiguous(t), t->view_src != NULL, (int)t->op);
}

int main(void) {
    struct ggml_init_params p = { 64u*1024u*1024u, NULL, false };
    struct ggml_context * ctx = ggml_init(p);
    printf("blck f32=%zu f16=%zu q4_0=%zu q4_K=%zu iq4_nl=%zu\n",
        ggml_blck_size(GGML_TYPE_F32), ggml_blck_size(GGML_TYPE_F16),
        ggml_blck_size(GGML_TYPE_Q4_0), ggml_blck_size(GGML_TYPE_Q4_K), ggml_blck_size(GGML_TYPE_IQ4_NL));
    printf("tsize f32=%zu f16=%zu q4_0=%zu q4_K=%zu iq4_nl=%zu bf16=%zu\n",
        ggml_type_size(GGML_TYPE_F32), ggml_type_size(GGML_TYPE_F16), ggml_type_size(GGML_TYPE_Q4_0),
        ggml_type_size(GGML_TYPE_Q4_K), ggml_type_size(GGML_TYPE_IQ4_NL), ggml_type_size(GGML_TYPE_BF16));
    struct ggml_tensor * a = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 100);
    struct ggml_tensor * b = ggml_new_tensor_2d(ctx, GGML_TYPE_F16, 64, 32);
    struct ggml_tensor * c = ggml_new_tensor_3d(ctx, GGML_TYPE_Q4_K, 256, 8, 3);
    struct ggml_tensor * d = ggml_new_tensor_4d(ctx, GGML_TYPE_IQ4_NL, 32, 4, 2, 5);
    dump("1d", a); dump("2d", b); dump("3d_q4k", c); dump("4d_iq4nl", d);
    struct ggml_tensor * v1 = ggml_view_1d(ctx, a, 10, 4*7);
    struct ggml_tensor * v2 = ggml_view_2d(ctx, b, 32, 16, b->nb[1], 2*16);
    struct ggml_tensor * v3 = ggml_view_3d(ctx, c, 128, 4, 2, c->nb[1], c->nb[2], 8);
    struct ggml_tensor * p4 = ggml_permute(ctx, d, 1, 0, 2, 3);
    struct ggml_tensor * tr = ggml_transpose(ctx, b);
    struct ggml_tensor * rs = ggml_reshape_2d(ctx, a, 10, 10);
    struct ggml_tensor * rs4 = ggml_reshape_4d(ctx, a, 5, 5, 2, 2);
    dump("view1d", v1); dump("view2d", v2); dump("view3d", v3);
    dump("permute", p4); dump("transpose", tr); dump("reshape2d", rs); dump("reshape4d", rs4);
    printf("row_size f32 100=%zu f16 64=%zu q4k 256=%zu iq4nl 32=%zu\n",
        ggml_row_size(GGML_TYPE_F32, 100), ggml_row_size(GGML_TYPE_F16, 64),
        ggml_row_size(GGML_TYPE_Q4_K, 256), ggml_row_size(GGML_TYPE_IQ4_NL, 32));
    printf("elsize f32=%zu f16=%zu iq4nl=%zu\n", ggml_element_size(a), ggml_element_size(b), ggml_element_size(d));
    ggml_free(ctx);
    return 0;
}
