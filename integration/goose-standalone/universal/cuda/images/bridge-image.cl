#include "depth-expf.h"

#define WIDTH 2304
#define LANES 128
#define MAX_HISTORY 11

static uint lane_id(void) {
    uint id;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(id));
    return id;
}

static uint block_id(void) {
    uint id;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(id));
    return id;
}

static void block_barrier(void) {
    __asm__ volatile("bar.sync 0;" ::: "memory");
}

static float divide_rn(float a, float b) {
    float result;
    __asm__("div.rn.f32 %0, %1, %2;" : "=f"(result) : "f"(a), "f"(b));
    return result;
}

static float sqrt_rn(float x) {
    float result;
    __asm__("sqrt.rn.f32 %0, %1;" : "=f"(result) : "f"(x));
    return result;
}

// Bound the error of both double sums by more than 2304 rounding units.
// Only use the parallel result if the whole interval rounds to one float.
static float ordered_result(__local const float *products, double total, double magnitude,
                            double divisor) {
    const double margin = magnitude * 0x1p-39;
    const float lower = (float)((total - margin) / divisor);
    const float upper = (float)((total + margin) / divisor);
    if (as_uint(lower) == as_uint(upper)) return lower;
    double exact_order = 0.0;
    for (int i = 0; i < WIDTH; ++i) exact_order += (double)products[i];
    return (float)(exact_order / divisor);
}

static void sum_local(__local const float *values, __local double *sums,
                      __local double *magnitudes, uint lane) {
    double sum = 0.0, magnitude = 0.0;
    for (uint i = lane; i < WIDTH; i += LANES) {
        const double x = values[i];
        sum += x;
        magnitude += x < 0.0 ? -x : x;
    }
    sums[lane] = sum;
    magnitudes[lane] = magnitude;
    block_barrier();
    for (uint stride = LANES / 2; stride; stride /= 2) {
        if (lane < stride) {
            sums[lane] += sums[lane + stride];
            magnitudes[lane] += magnitudes[lane + stride];
        }
        block_barrier();
    }
}

// One visit's attention convolution, residual, and exact-order RMS.
__kernel __attribute__((reqd_work_group_size(LANES, 1, 1)))
void i2_bridge_prepare(__global const float *attention, __global const float *weights,
                       __global float *history, __global const float *residual,
                       __global float *next, __global float *normalized, int add_residual) {
    __local float squares[WIDTH];
    __local double sums[LANES], magnitudes[LANES];
    __local float inverse;
    const int lane = lane_id();
    for (int i = lane; i < WIDTH; i += LANES) {
        const int h = 3 * i, w = 4 * i;
        const float current = attention[i];
        float sum = 0.0f;
        sum += history[h] * weights[w];
        sum += history[h + 1] * weights[w + 1];
        sum += history[h + 2] * weights[w + 2];
        sum += current * weights[w + 3];
        float x = (current + sum) * 0x1.c9f25cp-2f;
        if (add_residual) x += residual[i];
        history[h] = history[h + 1];
        history[h + 1] = history[h + 2];
        history[h + 2] = current;
        next[i] = x;
        squares[i] = x * x;
    }
    block_barrier();
    sum_local(squares, sums, magnitudes, lane);
    if (lane == 0) {
        const float mean = ordered_result(squares, sums[0], magnitudes[0], (double)WIDTH);
        inverse = divide_rn(1.0f, sqrt_rn(mean + 1e-6f));
    }
    block_barrier();
    for (int i = lane; i < WIDTH; i += LANES) normalized[i] = next[i] * inverse;
}

// Match the reference product rounding and chronological reversal.
__kernel __attribute__((reqd_work_group_size(LANES, 1, 1)))
void i2_bridge_score(__global const float *past, __global const float *current,
                     __global const float *query, __global float *scores, int rows) {
    __local float products[WIDTH];
    __local double sums[LANES], magnitudes[LANES];
    const int lane = lane_id();
    const int row = block_id();
    __global const float *values = row == rows ? current : past + (rows - 1 - row) * WIDTH;
    for (int i = lane; i < WIDTH; i += LANES) products[i] = values[i] * query[i];
    block_barrier();
    sum_local(products, sums, magnitudes, lane);
    if (lane == 0) {
        const float dot = ordered_result(products, sums[0], magnitudes[0], 1.0);
        scores[row] = dot * divide_rn(1.0f, sqrt_rn((float)WIDTH));
    }
}

__kernel __attribute__((reqd_work_group_size(LANES, 1, 1)))
void i2_bridge_mix(__global const float *past, __global const float *current,
                   __global const float *scores, __global float *output, int rows) {
    __local float weights[MAX_HISTORY + 1];
    const int lane = lane_id();
    if (lane == 0) {
        float maximum = -0x1.fffffep127f;
        for (int row = 0; row <= rows; ++row) {
            if (scores[row] > maximum) maximum = scores[row];
        }
        double total = 0.0;
        for (int row = 0; row <= rows; ++row) {
            const float delta = scores[row] - maximum;
            weights[row] = depth_expf(delta);
            total += (double)weights[row];
        }
        const float inverse = divide_rn(1.0f, (float)total);
        for (int row = 0; row <= rows; ++row) weights[row] *= inverse;
    }
    block_barrier();
    const int i = block_id() * LANES + lane;
    double sum = 0.0;
    for (int row = 0; row < rows; ++row) {
        const float term = weights[row] * past[(rows - 1 - row) * WIDTH + i];
        sum += (double)term;
    }
    const float term = weights[rows] * current[i];
    sum += (double)term;
    output[i] = (float)sum;
}
