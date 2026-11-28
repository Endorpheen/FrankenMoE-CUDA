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
    let qbase = head * 128u;
    let rbase = 3072u + head * 16u;
    let kvhead = head / 3u;
    let sink = x[3456u + head];
    var peak = 0.0;
    var denominator = 1.0;
    var output_lo = 0.0;
    var output_hi = 0.0;
    var key = 0u;
    if p.rotary != 0u { key = max(p.position + 1u, p.rotary) - p.rotary; }
    while key <= p.position {
        let kv = ((key % p.context) * 8u + kvhead) * 128u;
        let distance = p.position - key;
        precise let dot = x[qbase + lane] * cache_value(kv + lane, false)
            + x[qbase + lane + 64u] * cache_value(kv + lane + 64u, false);
        var score = sum_group(dot) * 0.08838834765;
        var relative = 0.0;
        if distance < 1024u {
            let bias = 3480u + distance * 16u;
            var product = 0.0;
            if lane < 16u { product = x[rbase + lane] * x[bias + lane]; }
            relative = sum_group(product);
        }
        score += relative - sink;
        let next = max(peak, score);
        let old_weight = exp(peak - next);
        let weight = exp(score - next);
        denominator = denominator * old_weight + weight;
        output_lo = output_lo * old_weight + weight * cache_value(kv + lane, true);
        output_hi = output_hi * old_weight + weight * cache_value(kv + lane + 64u, true);
        peak = next;
        key += 1u;
    }
    y[qbase + lane] = output_lo / denominator;
    y[qbase + lane + 64u] = output_hi / denominator;
}
