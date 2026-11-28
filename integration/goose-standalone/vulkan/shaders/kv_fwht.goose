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
var tile: f32[256];

@compute(local_size = (64, 1, 1))
fn main() {
    let lane = gpu_local_id_x();
    let base = gpu_workgroup_id_y() * p.width + gpu_workgroup_id_x() * p.rows;
    var scale = 0.0625;
    if p.rows == 64u { scale = 0.125; }
    var i = lane;
    while i < p.rows {
        tile[i] = x[base + i] * scale;
        i += 64u;
    }
    gpu_barrier();
    var span = 1u;
    while span < p.rows {
        i = lane;
        while i < p.rows / 2u {
            let a = (i / span) * (2u * span) + i % span;
            let left = tile[a];
            let right = tile[a + span];
            tile[a] = left + right;
            tile[a + span] = left - right;
            i += 64u;
        }
        gpu_barrier();
        span *= 2u;
    }
    i = lane;
    while i < p.rows {
        y[base + i] = tile[i];
        i += 64u;
    }
}
