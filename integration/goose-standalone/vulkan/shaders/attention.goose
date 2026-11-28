@storage(set = 0, binding = 0, access = read)
var x: f32[];
@storage(set = 0, binding = 1, access = read)
var w: f32[];
@storage(set = 0, binding = 2, access = write)
var y: f32[];
struct Params {
    width: u32, rows: u32, batch: u32, epsilon: f32,
    position: u32, kv_heads: u32, context: u32, theta: f32, rotary: u32
}
@push_constant
var p: Params;
@storage(set = 0, binding = 3, access = read)
var v: f32[];
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
            dot += x[query + i] * w[kv + i];
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
            let value = v[kv + i];
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
