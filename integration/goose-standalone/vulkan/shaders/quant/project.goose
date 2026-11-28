@storage(set = 0, binding = 0, access = read)
var x: f32[];
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
    let row = gpu_workgroup_id_x();
    let token = gpu_workgroup_id_y();
    precise var sum = 0.0;
    var i = lane;
    while i < p.width {
        sum += decode_weight(row, i) * x[token * p.width + i];
        i += 64u;
    }
    let result = sum_group(sum);
    var stride = p.rows;
    if p.context != 0u { stride = p.context; }
    if lane == 0u { y[token * stride + p.position + row] = result; }
}
