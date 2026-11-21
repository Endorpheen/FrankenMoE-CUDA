extern "fg_attn_word" fn rp_word(cfg: u64, i: i64) -> i64;
extern "fg_rope_float" fn rp_float(cfg: u64, i: i64) -> f32;
extern "fg_rope_position" fn rp_position(positions: u64, index: i64) -> i32;
extern "fg_device_global_load" fn rp_load_float(address: u64) -> f32;
extern "fg_device_global_store" fn rp_store_float(address: u64, value: f32);
extern "fg_rope_load" fn rp_load(address: u64, size: i64) -> f32;
extern "fg_rope_store" fn rp_store(address: u64, size: i64, value: f32);
extern "fg_rope_copy_pair" fn rp_copy(src: u64, dst: u64, size: i64);
extern "fg_rope_sin" fn sin32(x: f32) -> f32;
extern "fg_rope_cos" fn cos32(x: f32) -> f32;
extern "fg_vector_log" fn log32(x: f32) -> f32;
extern "fg_vector_min" fn min32(a: f32, b: f32) -> f32;
extern "fg_vector_max" fn max32(a: f32, b: f32) -> f32;

// One table per token, shared by its heads and constituent sequences.
export fn fg_rope_phases(positions: u64, factors: u64, table: u64, cfg: u64, token: i64) {
    let tokens = rp_word(cfg, 7);
    if token >= tokens { return; }
    let width = rp_word(cfg, 5);
    let mode = rp_word(cfg, 10);
    let first = rp_word(cfg, 12);
    let second = rp_word(cfg, 13);
    let third = rp_word(cfg, 14);
    let total = first + second + third + rp_word(cfg, 15);
    let multi = (mode & 8) != 0;
    let step = rp_float(cfg, 21);
    let scale = rp_float(cfg, 22);
    let ext = rp_float(cfg, 23);
    let magnitude = rp_float(cfg, 24);
    let corr: f32[2] = [rp_float(cfg, 25), rp_float(cfg, 26)];
    let sign: f32 = if rp_word(cfg, 20) != 0 { 1.0 } else { -1.0 };
    var initial: f32[4] = [0.0; 4];
    var theta: f32[4] = [0.0; 4];
    for axis in 4 {
        let index = if multi { token + axis * tokens } else { token };
        initial[axis] = rp_position(positions, index) as f32;
        theta[axis] = initial[axis];
    }
    let dst = table + ((4 * token * width) as u64);
    for pair in width / 2 {
        var axis: i64 = 0;
        if multi {
            let sector = pair % total;
            if mode == 24 {
                let reset = rope_reset_axis(sector, first, second, third);
                if reset >= 0 { theta[reset] = initial[reset]; }
            }
            axis = rope_sector_axis(mode, sector, first, second, third);
        }
        let ff = if factors == 0 { 1.0 } else { rp_load_float(factors + ((4 * pair) as u64)) };
        let cs = yarn_pair(theta[axis] / ff, scale, corr, 2 * pair, ext, magnitude);
        rp_store_float(dst + ((8 * pair) as u64), cs[0]);
        rp_store_float(dst + ((8 * pair + 4) as u64), cs[1] * sign);
        for i in 4 { theta[i] *= step; }
    }
}

fn rotate_pair(x: u64, out: u64, size: i64, first: i64, distance: i64, c: f32, s: f32) {
    let a = (first * size) as u64;
    let b = ((first + distance) * size) as u64;
    let x0 = rp_load(x + a, size);
    let x1 = rp_load(x + b, size);
    rp_store(out + a, size, x0 * c - x1 * s);
    rp_store(out + b, size, x0 * s + x1 * c);
}

export fn fg_rope_apply(x: u64, out: u64, table: u64, cfg: u64, task: i64) {
    let width = rp_word(cfg, 5);
    let heads = rp_word(cfg, 6);
    let tokens = rp_word(cfg, 7);
    let row = task / (width / 2);
    if row >= heads * tokens * rp_word(cfg, 8) { return; }
    let size = rp_word(cfg, 16);
    let head = row % heads;
    let token = (row / heads) % tokens;
    let seq = row / (heads * tokens);
    let xpos = x + ((head * rp_word(cfg, 17) + token * rp_word(cfg, 18) + seq * rp_word(cfg, 19)) as u64);
    let dst = out + ((row * width * size) as u64);
    let mode = rp_word(cfg, 10);
    let dims = rp_word(cfg, 9);
    let offs = rp_word(cfg, 11);
    let col = task % (width / 2) * 2;
    if mode != 24 && (col < offs || col >= offs + dims) {
        let at = (col * size) as u64;
        rp_copy(xpos + at, dst + at, size);
        return;
    }
    let rel = col - offs;
    let first = offs + (if mode == 0 { rel } else { rel / 2 });
    let distance = if mode == 0 { 1 } else if mode == 24 { dims } else { dims / 2 };
    let angle = table + ((4 * (token * width + rel)) as u64);
    rotate_pair(xpos, dst, size, first, distance, rp_load_float(angle), rp_load_float(angle + 4));
}

// Single-token IMRoPE. Distribute the pairs across lanes while keeping the
// same rounded theta recurrence as the table producer for each selected axis.
export fn fg_rope_decode(x: u64, positions: u64, factors: u64, out: u64, cfg: u64, head: i64, lane: i64) {
    let width = rp_word(cfg, 5);
    let col = 2 * lane;
    if col >= width { return; }
    let size = rp_word(cfg, 16);
    let src = x + ((head * rp_word(cfg, 17)) as u64);
    let dst = out + ((head * width * size) as u64);
    let dims = rp_word(cfg, 9);
    let offs = rp_word(cfg, 11);
    if col < offs || col >= offs + dims {
        let at = (col * size) as u64;
        rp_copy(src + at, dst + at, size);
        return;
    }
    let pair = (col - offs) / 2;
    let first = rp_word(cfg, 12);
    let second = rp_word(cfg, 13);
    let third = rp_word(cfg, 14);
    let total = first + second + third + rp_word(cfg, 15);
    let axis = rope_sector_axis(40, pair % total, first, second, third);
    let step = rp_float(cfg, 21);
    var theta = rp_position(positions, axis) as f32;
    for i in pair { theta *= step; }
    let ff = if factors == 0 { 1.0 } else { rp_load_float(factors + ((4 * pair) as u64)) };
    let corr: f32[2] = [rp_float(cfg, 25), rp_float(cfg, 26)];
    let cs = yarn_pair(theta / ff, rp_float(cfg, 22), corr, 2 * pair, rp_float(cfg, 23), rp_float(cfg, 24));
    let sign: f32 = if rp_word(cfg, 20) != 0 { 1.0 } else { -1.0 };
    rotate_pair(src, dst, size, offs + pair, dims / 2, cs[0], cs[1] * sign);
}
