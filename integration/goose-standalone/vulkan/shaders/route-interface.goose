@storage(set = 0, binding = 0, access = read)
var x: f32[];
@storage(set = 0, binding = 1, access = read)
var w: f32[];
@storage(set = 0, binding = 2, access = write)
var routes: u32[];
struct Params {
    width: u32, rows: u32, batch: u32, epsilon: f32,
    position: u32, kv_heads: u32, context: u32, theta: f32, rotary: u32
}
@push_constant
var p: Params;
@workgroup
var partial: f32[64];
@workgroup
var candidates: u32[64];
@workgroup
var selected: u32[64];
@workgroup
var picked: f32[64];
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
