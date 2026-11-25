from pathlib import Path

root = Path(__file__).resolve().parent
old = (root / "flash-mma.cl").read_text()
anchor = "__kernel void goose_i2_flash_mma_prefill("
start = old.index(anchor)
base = old[start:]
assert base.count("__kernel") == 1
head = old[:start]
helpers = r'''
static float4 relative_load4(__global const float *address) {
    float4 value;
    __asm__("ld.global.v4.f32 {%0, %1, %2, %3}, [%4];"
        : "=f"(value.x), "=f"(value.y), "=f"(value.z), "=f"(value.w)
        : "l"((ulong)address));
    return value;
}

static float relative_adjust_vector(float score, int64_t query, int64_t key,
    int64_t window, __global const float *relative, __global const float *bias,
    float sink, float scale) {
    const int64_t distance = query - key;
    if (key < 0 || distance < 0 || (window && distance >= window)) return i2o_neg_inf();
    double dot = 0.0;
    if (distance < 1024) {
        const __global float *entry = bias + distance * 16;
        #pragma unroll
        for (int i = 0; i < 16; i += 4) {
            const float4 products = relative_load4(relative + i) * relative_load4(entry + i);
            dot += (double)products.x;
            dot += (double)products.y;
            dot += (double)products.z;
            dot += (double)products.w;
        }
    }
    return scale * score + ((float)dot - sink);
}

static uint q4_pair_product(uint nibble_pair, uint scale_pair) {
    const uint encoded = 0x64006400u | (nibble_pair & 15u) | ((nibble_pair & 0xf00u) << 8);
    uint integers, values;
    __asm__("sub.rn.f16x2 %0, %1, %2;" : "=r"(integers) : "r"(encoded), "r"(0x64086408u));
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(values) : "r"(integers), "r"(scale_pair));
    return values;
}

static uint load_q4_pair_columns(__global const uchar *data, int64_t element) {
    const __global uchar *at = data + (element >> 5) * 18;
    const uint channel = (uint)element & 31;
    const uint pair = *(__global const ushort *)(at + 2 + (channel & 15));
    const uint scale = *(__global const ushort *)at;
    return q4_pair_product(pair >> (channel >= 16 ? 4 : 0), scale | (scale << 16));
}

static uint load_q4_pair_rows(__global const uchar *data, int64_t a, int64_t b,
                               int64_t end, int64_t channel) {
    uint first_scale = 0, second_scale = 0, first = 0, second = 0;
    const uint offset = 2 + ((uint)channel & 15);
    const uint shift = (channel & 16) ? 4 : 0;
    const int64_t group = channel >> 5;
    if (a < end) {
        const __global uchar *block = data + (a * 32 + group) * 18;
        first_scale = *(__global const ushort *)block;
        first = ((uint)block[offset] >> shift) & 15;
    }
    if (b < end) {
        const __global uchar *block = data + (b * 32 + group) * 18;
        second_scale = *(__global const ushort *)block;
        second = ((uint)block[offset] >> shift) & 15;
    }
    const uint result = q4_pair_product(first | (second << 8), first_scale | (second_scale << 16));
    return result & ((a < end ? 0xffffu : 0u) | (b < end ? 0xffff0000u : 0u));
}
'''

def variant(name, vector, prune, half=False, softmax=False):
    code = base.replace("goose_i2_flash_mma_prefill(", name + "(", 1)
    old_adjust = '''            i2_online_adjust_key_g((uint64_t)(scores + row * 32), (uint64_t)key_pos,
                (uint64_t)relative, (uint64_t)bias, (uint64_t)sinks, missing, start,
                query_pos[query + row], qhead + row * 24, at & 31, window, scale);'''
    new_adjust = '''            const uint lane_key = at & 31;
            const int64_t relative_head = qhead + row * 24;
            scores[at] = lane_key < (uint)missing ?
                relative_adjust_vector(scores[at], query_pos[query + row], key_pos[start + lane_key],
                    window, relative + relative_head * 16, bias, sinks[relative_head], scale) :
                i2o_neg_inf();'''
    if vector:
        assert code.count(old_adjust) == 1
        code = code.replace(old_adjust, new_adjust)
    if prune:
        anchor = "    for (int64_t start=begin; start<end; start+=32) {"
        assert code.count(anchor) == 1
        code = code.replace(anchor, '''    const int64_t earliest_query = query_pos[query];
    const int64_t latest_query = query_pos[query + tail - 1];
''' + anchor)
        anchor = "        int missing = (end-start < 32 ? (int)(end-start) : 32);"
        code = code.replace(anchor,anchor+'''
        const int64_t first_key = key_pos[start];
        const int64_t last_key = key_pos[start + missing - 1];
        if (first_key > latest_query || (window && earliest_query - last_key >= window)) continue;
        const bool no_relative = !window && earliest_query - last_key >= 1024 && missing == 32;''')
        anchor = "        scores[idx] = qk.x;"
        code = code.replace(anchor, '''        if (no_relative) {
            const float sink0 = group < (uint)tail ? sinks[qhead + group * 24] : 0;
            const float sink1 = group + 8 < (uint)tail ? sinks[qhead + (group + 8) * 24] : 0;
            qk.x = scale * qk.x + (0.0f - sink0);
            qk.y = scale * qk.y + (0.0f - sink0);
            qk.z = scale * qk.z + (0.0f - sink1);
            qk.w = scale * qk.w + (0.0f - sink1);
        }
''' + anchor)
        code = code.replace('        for (uint at = thread;', '        if (!no_relative) for (uint at = thread;')
        anchor = '        sync_group();\n        if (thread < 16) {'
        assert code.count(anchor) == 1
        code = code.replace(anchor,'        if (!no_relative) sync_group();\n        if (thread < 16) {')
    if half:
        start = code.index("                if (q4) {")
        end = code.index("\n            }", start)
        code = code[:start] + """                b = (uint2)(load_q4_pair_columns((__global const uchar *)k, offset),
                             load_q4_pair_columns((__global const uchar *)k, offset + 8));""" + code[end:]
        start = code.index("                ushort v0=")
        end = code.index("\n            }", start)
        code = code[:start] + """                acc[i] = mma(acc[i], a, (uint2)(
                    load_q4_pair_rows((__global const uchar *)v, i0, i1, end, c),
                    load_q4_pair_rows((__global const uchar *)v, i2, i3, end, c)));""" + code[end:]
    if softmax:
        start = code.index("        if (thread < 16) {")
        end = code.index("        const float4 weights", start)
        code = code[:start] + """        if (thread < 16) {
            if (thread < (uint)tail) {
                const float before = maxval[thread];
                float maximum = before;
                #pragma unroll
                for (uint i = 0; i < 32; ++i) {
                    const float score = scores[thread * 32 + i];
                    if (score > maximum) maximum = score;
                }
                alpha[thread] = i2o_exp(before - maximum);
                maxval[thread] = maximum;
                sum[thread] *= alpha[thread];
            } else alpha[thread] = 0;
        }
        sync_group();
        #pragma unroll
        for (uint index = thread; index < 16 * 32; index += 128) {
            const uint row = index >> 5;
            const float weight = row < (uint)tail ? i2o_exp(scores[index] - maxval[row]) : 0.0f;
            scores[index] = weight;
            probs[index] = half_bits(weight);
        }
        sync_group();
        if (thread < (uint)tail) {
            float total = sum[thread];
            #pragma unroll
            for (uint i = 0; i < 32; ++i) total += scores[thread * 32 + i];
            sum[thread] = total;
        }
""" + code[end:]
    return code

(root / "prefill-variants.cl").write_text(head + helpers + base +
    variant("i2_flash_prefill_vector", True, False) +
    variant("i2_flash_prefill_prune", False, True) +
    variant("i2_flash_prefill_combined", True, True) +
    variant("i2_flash_prefill_half", False, False, True) +
    variant("i2_flash_prefill_all", True, True, True) +
    variant("i2_flash_prefill_softmax", False, False, False, True) +
    variant("i2_flash_prefill_all_softmax", True, True, True, True))
