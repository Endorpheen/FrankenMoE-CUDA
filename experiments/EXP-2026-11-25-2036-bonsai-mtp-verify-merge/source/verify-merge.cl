static inline void verify_merge_store(__global float *dest, float2 value, float mass) {
    if (mass > 0) {
        float a, b;
        __asm__("div.rn.f32 %0, %1, %2;" : "=f"(a) : "f"(value.x), "f"(mass));
        __asm__("div.rn.f32 %0, %1, %2;" : "=f"(b) : "f"(value.y), "f"(mass));
        value = (float2)(a, b);
    } else {
        value = (float2)(0.0f);
    }
    *(__global float2 *)dest = value;
}

static inline __attribute__((always_inline)) void verify_merge_impl(
    __global const float *scratch, __global float *out, uint first_keys, uint max_tiles,
    uint parallel, __local float *work) {
    const uint job = work_group(), thread = local_id(), offset = thread * 2;
    const uint token = job / 24, head = job % 24;
    const uint tiles = (first_keys + token + 255) / 256;
    const __global float *part = scratch + ((size_t)token * 24 * max_tiles + head * tiles) * 258;
    __global float *dest = out + (size_t)job * 256 + offset;
    float2 values = (float2)(0.0f);
    float maximum = as_float(0xff800000u), mass = 0.0f;

    if (!parallel) {
        for (uint t = 0; t < tiles; ++t) {
            const __global float *tile = part + t * 258;
            const float m = tile[1];
            if (m == 0.0f) continue;
            const float mx = tile[0];
            const float next = mx > maximum ? mx : maximum;
            const float keep = attention_exp(maximum - next), add = attention_exp(mx - next);
            maximum = next;
            mass = mass * keep + m * add;
            values = values * keep + *(__global const float2 *)(tile + 2 + offset) * add;
        }
        verify_merge_store(dest, values, mass);
        return;
    }

    __local float *prev = work, *next_weight = work + 32, *tile_mass = work + 64;
    for (uint base = 0; base < tiles; base += 32) {
        const uint active = tiles - base < 32 ? tiles - base : 32;
        if (thread < 32) {
            const __global float *tile = part + (base + thread) * 258;
            const float m = thread < active ? tile[1] : 0.0f;
            const float mx = m == 0.0f ? as_float(0xff800000u) : tile[0];
            float seen = mx > maximum ? mx : maximum;
            #pragma unroll
            for (uint bit = 1; bit < 32; bit *= 2) {
                const float peer = bonsai_shuffle_up(seen, bit);
                if (thread >= bit) seen = seen > peer ? seen : peer;
            }
            const float before = bonsai_shuffle_up(seen, 1);
            tile_mass[thread] = m;
            if (m != 0.0f) {
                prev[thread] = attention_exp((thread == 0 ? maximum : before) - seen);
                next_weight[thread] = attention_exp(mx - seen);
            }
            maximum = bonsai_last_lane(seen);
        }
        sync_threads();
        for (uint i = 0; i < active; ++i) {
            const float m = tile_mass[i];
            if (m == 0.0f) continue;
            const float keep = prev[i], add = next_weight[i];
            const __global float *tile = part + (base + i) * 258;
            if (thread == 0) mass = mass * keep + m * add;
            values = values * keep + *(__global const float2 *)(tile + 2 + offset) * add;
        }
        sync_threads();
    }
    if (thread == 0) work[96] = mass;
    sync_threads();
    verify_merge_store(dest, values, work[96]);
}

__kernel void bonsai_verify_merge_direct(__global const float *scratch, __global float *out,
    uint first_keys, uint max_tiles) {
    verify_merge_impl(scratch, out, first_keys, max_tiles, 0, (__local float *)0);
}

__kernel void bonsai_verify_merge_parallel(__global const float *scratch, __global float *out,
    uint first_keys, uint max_tiles) {
    __local float work[97];
    verify_merge_impl(scratch, out, first_keys, max_tiles, 1, work);
}
