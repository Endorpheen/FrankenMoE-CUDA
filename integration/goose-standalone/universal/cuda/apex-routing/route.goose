extern "route_score" fn load(base: u64, index: i32) -> f32;
extern "route_write_score" fn write_score(base: u64, index: i32, value: f32);
extern "route_write_id" fn write_id(base: u64, index: i32, value: i32);
extern "route_neg_inf" fn neg_inf() -> f32;
extern "route_clamp" fn clamp(value: f32) -> f32;
extern "route_scaled" fn scaled(value: f32, scale: f32) -> f32;

export fn goose_compact_route(x: u64, ids: u64, weights: u64, scale: f32, lane: i32) {
    var scores: f32[8] = [0.0; 8];
    for i in 8 { scores[i] = load(x, 32 * (i as i32) + lane); }
    var picked: f32 = 0.0;
    for slot in 8 {
        var key = scores[0];
        var expert = lane;
        for i in 1 .. 8 {
            if scores[i] > key {
                key = scores[i];
                expert = lane + 32 * (i as i32);
            }
        }
        for step in 5 {
            let offset = (16 >> step) as i32;
            let alt = swap_f32(key, offset);
            let other = swap_i32(expert, offset);
            if alt > key || (alt == key && other < expert) {
                key = alt;
                expert = other;
            }
        }
        if (expert & 31) == lane {
            for i in 8 {
                if (expert >> 5) == i as i32 { scores[i] = neg_inf(); }
            }
        }
        if lane == slot as i32 {
            picked = key;
            write_id(ids, lane, expert);
        }
    }
    var sum: f64 = 0.0;
    if lane < 8 { sum += picked as f64; }
    for step in 5 { sum += swap_f64(sum, (16 >> step) as i32); }
    if lane < 8 {
        let weight = picked / clamp(sum as f32);
        write_score(weights, lane, if scale == 0.0 || scale == 1.0 { weight } else { scaled(weight, scale) });
    }
}
