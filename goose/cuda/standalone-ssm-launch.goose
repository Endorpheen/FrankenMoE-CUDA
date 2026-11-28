extern "fg_attn_word" fn ssm_config(cfg: u64, index: i64) -> i64;

export fn ssm_launch_direct(history: u64, mixed: u64, weights: u64, output: u64,
    channels: u64, tokens: u64, group: u64, time: u64, lane: u64, silu: bool) {
    ssm_conv_direct(history, mixed, weights, output, channels, tokens, group * 128 + lane, time * 32, silu);
}

export fn ssm_launch_serial(input: u64, weights: u64, output: u64, cfg: u64, lane: i32, sequence: i32, group: i32) {
    if (group as i64) * 128 + lane >= ssm_config(cfg, 5) { return; }
    ssm_conv_serial(input, weights, 0, output,
        ssm_config(cfg, 8) as i32, ssm_config(cfg, 9) as i32, ssm_config(cfg, 10) as i32,
        ssm_config(cfg, 11) as i32, ssm_config(cfg, 12) as i32, ssm_config(cfg, 13) as i32, ssm_config(cfg, 14) as i32,
        ssm_config(cfg, 7) as i32, ssm_config(cfg, 6) as i32, ssm_config(cfg, 15) != 0, false, lane, sequence, group);
}

export fn ssm_launch_decode(input: u64, weights: u64, output: u64, cfg: u64, lane: u64, sequence: u64, group: u64) {
    let channel = group * 128 + lane;
    if channel >= (ssm_config(cfg, 5) as u64) { return; }
    ssm_conv_decode(input + sequence * (ssm_config(cfg, 10) as u64), weights,
        output + sequence * (ssm_config(cfg, 14) as u64),
        ssm_config(cfg, 9) as u64, ssm_config(cfg, 11) as u64, channel, ssm_config(cfg, 15) != 0);
}

export fn ssm_launch_long(input: u64, weights: u64, output: u64, cfg: u64, shared: u64,
    lane: i32, sequence: i32, group: i32, tokens: i32) {
    ssm_conv_long(input, weights, 0, output, shared,
        ssm_config(cfg, 8) as i32, ssm_config(cfg, 9) as i32, ssm_config(cfg, 10) as i32,
        ssm_config(cfg, 11) as i32, ssm_config(cfg, 12) as i32, ssm_config(cfg, 13) as i32, ssm_config(cfg, 14) as i32,
        ssm_config(cfg, 7) as i32, 4, ssm_config(cfg, 15) != 0, false, true, lane, sequence, group, tokens);
}
