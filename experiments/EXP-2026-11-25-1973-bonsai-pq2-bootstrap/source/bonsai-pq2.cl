static uint local_id(void) {
    uint n;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(n));
    return n;
}

static uint work_group(void) {
    uint n;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(n));
    return n;
}

static void sync_threads(void) {
    __asm__ volatile("bar.sync 0;" ::: "memory");
}

static float pq2_element(__global const uchar *row, uint column) {
    const __global uchar *quant = row + (column / 128) * 34;
    const uint index = column % 128;
    const uint code = (quant[2 + index / 4] >> (2 * (index % 4))) & 3;
    float scale;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*(__global const ushort *)quant));
    return scale * (float)((int)code - 1);
}

__kernel void bonsai_pq2_row(__global const uchar *weights, __global float *output,
                            uint width, uint row) {
    const uint column = work_group() * 256 + local_id();
    if (column < width) output[column] = pq2_element(weights + (ulong)row * (width / 128) * 34, column);
}

__kernel void bonsai_fwht1024(__global const float *input, __global const int *signs,
                             __global float *output, uint inverse,
                             uint head_width, uint key_heads, uint repeat) {
    __local float values[1024];
    const uint lane = local_id(), base = work_group() * 1024;
    for (uint i = lane; i < 1024; i += 256) {
        const uint column = base + i;
        uint source = column;
        if (repeat > 1) {
            const uint head = column / head_width;
            source = (head % repeat * key_heads + head / repeat) * head_width + column % head_width;
        }
        const float value = input[source];
        values[i] = inverse ? value : value * (float)signs[column];
    }
    sync_threads();
    for (uint span = 1; span < 1024; span *= 2) {
        for (uint i = lane; i < 512; i += 256) {
            const uint a = (i / span) * (2 * span) + i % span;
            const float x = values[a], y = values[a + span];
            values[a] = x + y;
            values[a + span] = x - y;
        }
        sync_threads();
    }
    for (uint i = lane; i < 1024; i += 256) {
        const uint column = base + i;
        const float value = values[i] * 0.03125f;
        output[column] = inverse ? value * (float)signs[column] : value;
    }
}

__kernel void bonsai_pq2_matvec(__global const uchar *matrix, __global const float *input,
                               __global float *output, uint width, uint rows) {
    const uint thread = local_id(), lane = thread & 31, row = work_group() * 4 + thread / 32;
    if (row >= rows) return;
    const __global uchar *weights = matrix + (ulong)row * (width / 128) * 34;
    float sum = 0.0f;
    for (uint column = lane; column < width; column += 32) {
        const float w = pq2_element(weights, column);
        __asm__("fma.rn.f32 %0, %1, %2, %0;" : "+f"(sum) : "f"(w), "f"(input[column]));
    }
    for (int bit = 16; bit > 0; bit /= 2) {
        float peer;
        __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=f"(peer) : "f"(sum), "r"(bit));
        sum += peer;
    }
    if (lane == 0) output[row] = sum;
}

extern float __nv_expf(float);

__kernel void bonsai_swiglu(__global const float *gate, __global const float *up,
                            __global float *output, uint width) {
    const uint i = work_group() * 256 + local_id();
    if (i < width) {
        const float x = gate[i];
        output[i] = (x / (1.0f + __nv_expf(-x))) * up[i];
    }
}
