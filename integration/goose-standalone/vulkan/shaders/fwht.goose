@storage(set = 0, binding = 0, access = read)
var x: f32[];
@storage(set = 0, binding = 1, access = read)
var signs: i32[];
@storage(set = 0, binding = 2, access = write)
var y: f32[];
struct Params {
    width: u32, rows: u32, batch: u32, epsilon: f32,
    position: u32, kv_heads: u32, context: u32, theta: f32, rotary: u32
}
@push_constant
var p: Params;
@workgroup
var tile: f32[1024];

@compute(local_size = (64, 1, 1))
fn main() {
    let lane = gpu_local_id_x();
    let base = gpu_workgroup_id_x() * 1024u;
    let token = gpu_workgroup_id_y() * p.width;
    let inverse = p.position != 0u;
    var i = lane;
    while i < 1024u {
        let column = base + i;
        var source = column;
        if p.context > 1u {
            let head = column / p.rows;
            source = (head % p.context * p.kv_heads + head / p.context) * p.rows + column % p.rows;
        }
        var value = x[token + source];
        if !inverse { value *= signs[column] as f32; }
        tile[i] = value;
        i += 64u;
    }
    gpu_barrier();
    var span = 1u;
    while span < 1024u {
        i = lane;
        while i < 512u {
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
    while i < 1024u {
        var value = tile[i] * 0.03125;
        if inverse { value *= signs[base + i] as f32; }
        y[token + base + i] = value;
        i += 64u;
    }
}
