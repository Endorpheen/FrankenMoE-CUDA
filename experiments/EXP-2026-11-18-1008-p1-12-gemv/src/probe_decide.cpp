#include <cstdio>
#include <cstdint>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
int main() {
    ggml_init_params ip = { 256u*1024*1024, nullptr, false };
    ggml_context * c = ggml_init(ip);
    ggml_backend_t cpu = ggml_backend_cpu_init();
    ggml_backend_dev_t dev = ggml_backend_get_device(cpu);
    ggml_backend_dev_props props; ggml_backend_dev_get_props(dev, &props);
    printf("device: %s\n", props.name);
    auto * reg = ggml_backend_dev_backend_reg(dev);
    auto get_extra = (ggml_backend_buffer_type_t * (*)(ggml_backend_dev_t))
        ggml_backend_reg_get_proc_address(reg, "ggml_backend_dev_get_extra_bufts");
    ggml_backend_buffer_type_t * extra = get_extra ? get_extra(dev) : nullptr;
    printf("extra bufts:");
    if (extra) for (int i = 0; extra[i]; ++i) printf(" %s", ggml_backend_buft_name(extra[i]));
    printf("\n");
    ggml_backend_buffer_type_t candidates[8]; int n = 0;
    if (extra) for (int i = 0; extra[i] && n < 7; ++i) candidates[n++] = extra[i];
    candidates[n++] = ggml_backend_cpu_buffer_type();
    struct { const char * name; enum ggml_type t; int ne0, ne1, ne2; } cases[] = {
        {"q4_K gate/up expert", GGML_TYPE_Q4_K, 2048, 512, 256},
    };
    for (auto & cs : cases) {
        ggml_tensor * w = ggml_new_tensor_3d(c, cs.t, cs.ne0, cs.ne1, cs.ne2);
        ggml_tensor * b = ggml_new_tensor_3d(c, GGML_TYPE_F32, cs.ne0, 4, 1);
        ggml_tensor * ids = ggml_new_tensor_2d(c, GGML_TYPE_I32, 4, 1);
        ggml_tensor * op = ggml_mul_mat_id(c, w, b, ids);
        for (int i = 0; i < n; ++i) {
            ggml_backend_buffer_t dummy = ggml_backend_buft_alloc_buffer(candidates[i], 0);
            w->buffer = dummy;
            bool sup = ggml_backend_dev_supports_op(dev, op);
            w->buffer = nullptr; ggml_backend_buffer_free(dummy);
            printf("supports_op(%s, %s) = %s\n", ggml_backend_buft_name(candidates[i]), cs.name, sup ? "true" : "false");
        }
    }
    ggml_backend_free(cpu); ggml_free(c);
    return 0;
}
