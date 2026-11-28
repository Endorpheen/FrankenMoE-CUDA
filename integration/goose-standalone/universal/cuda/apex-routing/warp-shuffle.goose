extern "route_shuffle" fn shuffle_word(input: u32, delta: i32) -> u32;
extern "as_uint" fn float_word(input: f32) -> u32;
extern "as_float" fn word_float(input: u32) -> f32;
extern "as_ulong" fn double_word(input: f64) -> u64;
extern "as_double" fn word_double(input: u64) -> f64;

fn swap_f32(value: f32, delta: i32) -> f32 {
    return word_float(shuffle_word(float_word(value), delta));
}

fn swap_i32(value: i32, delta: i32) -> i32 {
    return shuffle_word(value as! u32, delta) as! i32;
}

fn swap_f64(value: f64, delta: i32) -> f64 {
    let bits = double_word(value);
    let low = shuffle_word(bits as! u32, delta);
    let high = shuffle_word((bits >> 32) as u32, delta);
    return word_double(((high as u64) << 32) | (low as u64));
}
