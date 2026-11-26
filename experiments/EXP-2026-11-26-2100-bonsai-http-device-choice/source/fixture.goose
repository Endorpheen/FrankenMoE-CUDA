extern "fg158_float" fn float_bits(x: i64) -> f32;
fn pr_hex_val(c: u8) -> i64 { return c as i64; }
fn bt_is_eog_impl(eog: const i64[:], n: i64, token: i64) -> i64 {
    for i in n { if eog[i] == token { return 1; } }
    return 0;
}

export fn test_generate(temperature: f32, ignore_eos: bool, top: i64, out: i64[:]) -> i64 {
    let prompt: i64[3] = [1, 1, 1];
    let eog: i64[1] = [3];
    var sample = q35_sample_defaults();
    sample.temperature = temperature;
    sample.ignore_eos = ignore_eos;
    var timing: f64[3] = [0; 3];
    var probabilities: f32[8] = [0; 8];
    var ids: i64[8] = [0; 8];
    return q35_sample_generate(prompt[..], 4, 8, eog[..], 4, sample, out, timing[..],
                               top, ids[..], probabilities[..]);
}
