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
@storage(set = 0, binding = 3, access = read_write)
var history: f32[];
@compute(local_size = (64, 1, 1))
fn main() {
    let c = gpu_global_id_x();
    if c < p.width {
        let a = history[c];
        let b = history[p.width + c];
        let d = history[2u * p.width + c];
        let sample_value = x[c];
        let tap = c * 4u;
        precise let conv = a * w[tap] + b * w[tap + 1u] + d * w[tap + 2u] + sample_value * w[tap + 3u];
        y[c] = conv / (1.0 + exp(-conv));
        history[c] = b;
        history[p.width + c] = d;
        history[2u * p.width + c] = sample_value;
    }
}
