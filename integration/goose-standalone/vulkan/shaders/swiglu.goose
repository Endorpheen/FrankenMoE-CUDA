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
@compute(local_size = (64, 1, 1))
fn main() {
    let i = gpu_global_id_x();
    if i < p.width * p.batch { y[i] = (x[i] / (1.0 + exp(-x[i]))) * w[i]; }
}
