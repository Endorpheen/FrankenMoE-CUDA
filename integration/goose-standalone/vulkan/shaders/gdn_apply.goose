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
fn softplus(alpha: f32) -> f32 {
    if alpha < -16.0 { return exp(alpha); }
    return max(alpha, 0.0) + log(1.0 + exp(-gpu_u32_bits(gpu_f32_bits(alpha) & 0x7fffffffu)));
}
@compute(local_size = (64, 1, 1))
fn main() {
    let head = gpu_global_id_x();
    if head < p.width {
        let alpha = x[head] + parameters[head];
        y[head] = parameters[p.width + head] * softplus(alpha);
        y[p.width + head] = 1.0 / (1.0 + exp(-w[head]));
    }
}
