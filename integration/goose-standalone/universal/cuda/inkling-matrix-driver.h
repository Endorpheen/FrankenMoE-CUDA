#include "matrix-image.h"
#include "q8-packed-image.h"

static int i2_packed_q8(DevicePtr weight, uint64_t stride, int width, int rows, int count,
                        DevicePtr jobs) {
    const bool shape = (width == 2304 && (rows == 1024 || rows == 2304 || rows == 3072)) ||
        (width == 3072 && rows == 2304);
    const char *control_setting = getenv("I2_PACK_CONTROL_SHAPES");
    const int extra = (control_setting && strcmp(control_setting, "0") == 0) ? 0 :
        width == 64 && rows == 2304 && count % 16 == 0 ? 1 :
        width == 4608 && rows == 64 ? 2 :
        width == 2304 && rows == 384 ? 3 : 0;
    if ((!shape && !extra) || count < 512 || count > 4096 || count % 8) return 1;
    static int enabled = -1;
    if (enabled < 0) {
        const char *setting = getenv("I2_Q8_PACKED_INPUT");
        enabled = setting && strcmp(setting, "1") == 0;
    }
    if (!enabled) return 1;

    static Module module;
    static Function pack, multiply, narrow4, wide2, wide4, expand, fp32w2, fp32w4;
    const char *expand_setting = getenv("I2_Q8_FP32_EXPAND");
    const bool expanded = count == 4096 && (shape || extra == 3) &&
        (!expand_setting || strcmp(expand_setting, "0") != 0);
    const char *wide_setting = getenv("I2_Q8_EXACT_TILE16");
    const bool wide = extra == 1 || (shape && count % 16 == 0 && rows != 1024 &&
        (!wide_setting || strcmp(wide_setting, "0") != 0));
    const int warps = expanded ? (width == 3072 || extra == 3 ? 2 : 4) :
                      (wide ? ((width == 3072 || extra == 1) ? 2 : 4) : (extra == 2 ? 4 : 8));
    const int tile = wide || expanded ? 16 : 8;
    int error = 0;
    if (!module) error = cuda.module_load(&module, q8_packed_cubin);
    if (!error && !pack) error = cuda.function_get(&pack, module, "i2_pack_q8_input8");
    if (!error && !multiply) error = cuda.function_get(&multiply, module, "i2_q8_packed8");
    if (!error && extra == 2 && !narrow4) error = cuda.function_get(&narrow4, module, "i2_q8_packed4");
    if (!error && wide && warps == 2 && !wide2) error = cuda.function_get(&wide2, module, "i2_q8_packed16w2");
    if (!error && wide && warps == 4 && !wide4) error = cuda.function_get(&wide4, module, "i2_q8_packed16w4");
    if (!error && expanded && !expand) error = cuda.function_get(&expand, module, "i2_q8_expand_f32");
    if (!error && expanded && warps == 2 && !fp32w2) error = cuda.function_get(&fp32w2, module, "i2_q8_fp32w2");
    if (!error && expanded && warps == 4 && !fp32w4) error = cuda.function_get(&fp32w4, module, "i2_q8_fp32w4");
    if (error) return -1;

    // The regular and grouped matrix launches reuse this work buffer on
    // the same stream. No separate persistent allocation is needed.
    const size_t pack_bytes = (size_t)count * width * sizeof(float);
    const size_t weight_bytes = expanded ? (size_t)rows * width * sizeof(float) : 0;
    error = reserve(&cuda.mmq_packed, pack_bytes + weight_bytes);
    if (error == 2) return 1;
    if (error) return -1;
    if (activations.active && activations.defer_waits) activations.pending = true;
    void *pack_args[] = {&matrix_input, &jobs, &cuda.mmq_packed.address, &width};
    DevicePtr weights_device = expanded ? cuda.mmq_packed.address + pack_bytes : weight;
    void *matrix_args[] = {&weights_device, &cuda.mmq_packed.address, &matrix_output, &jobs, &stride, &width};
    error = cuda.launch(pack, (width + 127) / 128, count / 8, 1, 128, 1, 1, 0, NULL, pack_args, NULL);
    if (!error && expanded) {
        void *args[] = {&weight, &weights_device, &stride, &width};
        error = cuda.launch(expand, (width + 127) / 128, rows, 1, 128, 1, 1, 0, NULL, args, NULL);
    }
    const Function kernel = expanded ? (warps == 2 ? fp32w2 : fp32w4) :
                            (wide ? (warps == 2 ? wide2 : wide4) : (extra == 2 ? narrow4 : multiply));
    if (!error) error = cuda.launch(kernel, rows / warps, count / tile, 1, 32 * warps, 1, 1, 0, NULL,
                                    matrix_args, NULL);
    if (!error) error = finish_matrix_launches(error);
    if (error) return -1;
    ++cuda_matrix_launches;
    cuda_matrix_kind = 8;
    static bool expand_reported;
    if (expanded && !expand_reported) {
        fputs("Inkling Q8 projection FP32 expansion active\n", stderr);
        expand_reported = true;
    }
    static unsigned extra_reported;
    if (extra && !(extra_reported & (1u << extra))) {
        extra_reported |= 1u << extra;
        fprintf(stderr, "Inkling packed control Q8 width=%d rows=%d tile=%d warps=%d\n",
                width, rows, tile, warps);
    }
    static bool wide_reported;
    if (wide && !wide_reported) {
        wide_reported = true;
        fputs("Inkling Q8: exact packed sixteen-input kernel active\n", stderr);
    }
    static bool reported;
    if (!reported) {
        reported = true;
        fputs("Inkling Q8: vector-packed eight input rows active\n", stderr);
    }
    return 0;
}


static bool i2_matrix_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) {
        const char *setting = getenv("I2_SPECIALIZED_MATVEC");
        enabled = setting && strcmp(setting, "1") == 0;
    }
    return enabled != 0;
}

static int i2_launch_matrix(DevicePtr weight, uint64_t stride, int width,
                            int kind, int rows, int count, int words, DevicePtr job_address) {
    if (kind == 8 && words == 2) {
        const int selected = i2_packed_q8(weight, stride, width, rows, count, job_address);
        if (selected <= 0) return selected;
    }
    static Module module;
    static Function kernels[4];
    static unsigned reported;
    const bool verify_expert = i2_exact_verify && words == 3 && (kind == 20 || kind == 23);
    const bool xs_blocks = kind == 23 && (count <= 8 || verify_expert) && width % 256 == 0 &&
        fg_standalone_option_enabled("I2_DECODE_IQ4_XS_BLOCKS");
    const int index = xs_blocks ? 3 : kind == 8 ? 0 : kind == 20 ? 1 : 2;
    static const char *const names[] = {
        "goose_i2_matrix_q8_0", "goose_i2_matrix_iq4_nl", "goose_i2_matrix_iq4_xs", "goose_i2_matrix_iq4_xs_blocks"
    };
    int error = module ? 0 : cuda.module_load(&module, matrix_cubin);
    if (!error && !kernels[index]) {
        error = cuda.function_get(&kernels[index], module, names[index]);
    }

    int tile = count >= 16 && !verify_expert ? 4 : 1;
    const char *wide = getenv("I2_Q8_TILE8");
    if (kind == 8 && words == 2 && count >= 16 && wide && strcmp(wide, "1") == 0) tile = 8;
    for (int base = 0; base < count && !error;) {
        int size = count - base;
        if (size > 65535 * tile) {
            size = 65535 * tile;
        }
        void *args[] = {&weight, &matrix_input, &matrix_output, &job_address,
                        &stride, &width, &rows, &size, &base, &words, &tile};
        error = cuda.launch(kernels[index], ((unsigned)rows + 3) / 4, (size + tile - 1) / tile,
                            1, 128, 1, 1, 0, NULL, args, NULL);
        if (!error) {
            ++cuda_matrix_launches;
        }
        base += size;
    }
    if (!error) {
        error = finish_matrix_launches(error);
    }
    if (error != 0) {
        fprintf(stderr, "Inkling specialized matrix launch failed: %d\n", error);
        return -1;
    }
    cuda_matrix_kind = kind;
    static bool reported_verify;
    if (verify_expert && !reported_verify) {
        fputs("Inkling verifier expert single-job dispatch active\n", stderr);
        reported_verify = true;
    }
    if ((reported & (1u << index)) == 0) {
        reported |= 1u << index;
        fprintf(stderr, "Inkling specialized matrix kind %d (tile %d) active\n", kind, tile);
    }
    return 0;
}
