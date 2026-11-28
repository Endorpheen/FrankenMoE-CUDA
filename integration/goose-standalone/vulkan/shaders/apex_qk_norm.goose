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
    let row = head * p.width;
    var i = lane;
    if head < 2u * p.kv_heads {
        precise var square = 0.0;
        while i < p.width {
            square += x[row + i] * x[row + i];
            i += 64u;
        }
        let sum = sum_group(square);
        let inv = inversesqrt(sum / (p.width as f32) + p.epsilon / (p.width as f32)) * inversesqrt(p.width as f32);
        i = lane;
        while i < p.width {
            y[row + i] = x[row + i] * inv;
            i += 64u;
        }
    } else {
        while i < p.width {
            y[row + i] = x[row + i];
            i += 64u;
        }
    }
}
