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
    let token = gpu_workgroup_id_y();
    let head = gpu_workgroup_id_x();
    let lane = gpu_local_id_x();
    let base = (token * p.rows + head) * p.width;
    var rotated = p.width;
    if p.rotary != 0u { rotated = p.rotary; }
    let half_width = rotated / 2u;
    var i = lane;
    while i < half_width {
        let frequency = pow(p.theta, -(i as f32) / (half_width as f32));
        let angle = ((p.position + token) as f32) * frequency;
        let a = x[base + i];
        let b = x[base + i + half_width];
        let c = cos(angle);
        let s = sin(angle);
        y[base + i] = a * c - b * s;
        y[base + i + half_width] = b * c + a * s;
        i += 64u;
    }
    i = rotated + lane;
    while i < p.width {
        y[base + i] = x[base + i];
        i += 64u;
    }
}
