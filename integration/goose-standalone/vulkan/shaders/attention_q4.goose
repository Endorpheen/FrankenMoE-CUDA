@storage(set = 0, binding = 0, access = read)
var x: f32[];
@storage(set = 0, binding = 1, access = read)
var packed_keys: u32[];
@storage(set = 0, binding = 3, access = read)
var packed_values: u32[];
@storage(set = 0, binding = 2, access = write)
var y: f32[];
struct Params {
    width: u32, rows: u32, batch: u32, epsilon: f32,
    position: u32, kv_heads: u32, context: u32, theta: f32, rotary: u32
}
@push_constant
var p: Params;
@workgroup
var partial: f32[64];

fn sum_group(value: f32) -> f32 {
    let lane = gpu_local_id_x();
    partial[lane] = value;
    gpu_barrier();
    var step = 32u;
    while step != 0u {
        if lane < step {
            partial[lane] += partial[lane + step];
        }
        gpu_barrier();
        step >>= 1u;
    }
    let total = partial[0u];
    gpu_barrier();
    return total;
}
fn cache_byte(offset: u32, value: bool) -> u32 {
    if value { return (packed_values[offset >> 2u] >> ((offset & 3u) * 8u)) & 255u; }
    return (packed_keys[offset >> 2u] >> ((offset & 3u) * 8u)) & 255u;
}

fn cache_value(index: u32, value: bool) -> f32 {
    let block = (index >> 5u) * 18u;
    let j = index & 31u;
    let data = cache_byte(block + 2u + (j & 15u), value);
    let quant = (((data >> ((j >> 4u) * 4u)) & 15u) as i32) - 8;
    let bits = cache_byte(block, value) | (cache_byte(block + 1u, value) << 8u);
    return (quant as f32) * gpu_unpack_half_low(bits);
}

@compute(local_size = (64, 1, 1))
fn main() {
    let lane = gpu_local_id_x();
    let head = gpu_workgroup_id_x();
    let token = gpu_workgroup_id_y();
    let kv_head = head / (p.rows / p.kv_heads);
    let query = (token * p.rows + head) * p.width;
    var peak = gpu_u32_bits(0xff800000u);
    var denominator = 0.0;
    var output_row: f32[4];
    output_row[0u] = 0.0;
    output_row[1u] = 0.0;
    output_row[2u] = 0.0;
    output_row[3u] = 0.0;
    let scale = inversesqrt(p.width as f32);
    var k = 0u;
    while k <= p.position + token {
        let kv = (k * p.kv_heads + kv_head) * p.width;
        precise var dot = 0.0;
        var i = lane;
        while i < p.width {
            dot += x[query + i] * cache_value(kv + i, false);
            i += 64u;
        }
        let score = sum_group(dot) * scale;
        let next = max(peak, score);
        let old_weight = exp(peak - next);
        let weight = exp(score - next);
        denominator = denominator * old_weight + weight;
        i = lane;
        while i < p.width {
            let j = i / 64u;
            let value = cache_value(kv + i, true);
            output_row[j] = output_row[j] * old_weight + value * weight;
            i += 64u;
        }
        peak = next;
        k += 1u;
    }
    var i = lane;
    while i < p.width {
        y[query + i] = output_row[i / 64u] / denominator;
        i += 64u;
    }
}
