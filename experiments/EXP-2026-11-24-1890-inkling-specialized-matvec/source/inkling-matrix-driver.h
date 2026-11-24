#include "matrix-image.h"

static bool i2_matrix_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) {
        const char *setting = getenv("I2_SPECIALIZED_MATVEC");
        enabled = setting && strcmp(setting, "1") == 0;
    }
    return enabled != 0;
}

static int i2_launch_matrix(DevicePtr weight, uint64_t stride, int width,
                            int kind, int rows, int count, int words) {
    static Module module;
    static Function kernels[3];
    static unsigned reported;
    const int index = kind == 8 ? 0 : kind == 20 ? 1 : 2;
    static const char *const names[] = {
        "goose_i2_matrix_q8_0", "goose_i2_matrix_iq4_nl", "goose_i2_matrix_iq4_xs"
    };
    int error = module ? 0 : cuda.module_load(&module, matrix_cubin);
    if (!error && !kernels[index]) {
        error = cuda.function_get(&kernels[index], module, names[index]);
    }

    int tile = count >= 16 ? 4 : 1;
    for (int base = 0; base < count && !error;) {
        int size = count - base;
        if (size > 65535 * tile) {
            size = 65535 * tile;
        }
        void *args[] = {&weight, &matrix_input, &matrix_output, &cuda.jobs.address,
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
    if ((reported & (1u << index)) == 0) {
        reported |= 1u << index;
        fprintf(stderr, "Inkling specialized matrix kind %d (tile %d) active\n", kind, tile);
    }
    return 0;
}
