@storage(set = 0, binding = 0, access = read)
var x: f32[];
@storage(set = 0, binding = 1, access = read_write)
var state: f32[];
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
@storage(set = 0, binding = 3, access = read)
var gates: f32[];
@compute(local_size = (64, 1, 1))
fn main() {
    let lane = gpu_local_id_x();
    let head = gpu_workgroup_id_x();
    let row = gpu_workgroup_id_y();
    let q = (head % p.kv_heads) * p.width;
    let k = q + p.kv_heads * p.width;
    let out_index = head * p.width + row;
    let value_index = 2u * p.kv_heads * p.width + out_index;
    let base = out_index * p.width;
    let decay = exp(gates[head]);
    precise var dot = 0.0;
    var i = lane;
    while i < p.width {
        dot += (state[base + i] * decay) * x[k + i];
        i += 64u;
    }
    let delta = (x[value_index] - sum_group(dot)) * gates[p.rows + head];
    precise var score = 0.0;
    i = lane;
    while i < p.width {
        precise let updated = state[base + i] * decay + delta * x[k + i];
        state[base + i] = updated;
        score += updated * x[q + i];
        i += 64u;
    }
    let result = sum_group(score) * inversesqrt(p.width as f32);
    if lane == 0u { y[out_index] = result; }
}
