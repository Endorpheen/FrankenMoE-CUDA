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
var parameters: f32[];
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
fn softplus(alpha: f32) -> f32 {
    if alpha < -16.0 { return exp(alpha); }
    return max(alpha, 0.0) + log(1.0 + exp(-gpu_u32_bits(gpu_f32_bits(alpha) & 0x7fffffffu)));
}
@compute(local_size = (64, 1, 1))
fn main() {
    let head = gpu_workgroup_id_x();
    let lane = gpu_local_id_x();
    precise var alpha_dot = 0.0;
    precise var beta_dot = 0.0;
    var i = lane;
    while i < p.width {
        alpha_dot += x[i] * w[head * p.width + i];
        beta_dot += x[i] * w[(p.rows + head) * p.width + i];
        i += 64u;
    }
    let alpha = sum_group(alpha_dot) + parameters[head];
    let beta = sum_group(beta_dot);
    if lane == 0u {
        y[head] = parameters[p.rows + head] * softplus(alpha);
        y[p.rows + head] = 1.0 / (1.0 + exp(-beta));
    }
}
