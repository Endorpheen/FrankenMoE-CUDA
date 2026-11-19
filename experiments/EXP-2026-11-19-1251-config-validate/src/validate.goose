// Numeric half of bmoe::validate (runtime/core/src/config.cpp), as an ordered rule index.
// Returns 0 when the configuration passes, otherwise the 1-based index of the first failing
// rule in the native order. Messages are not ported: the harness identifies the native rule by
// its message, and comparing indices is what makes the two sides comparable at all.
// The rules here are the ones that read integers or floats only; the string-valued ones
// (model_path, workload paths) are reported as separate indices and are passed as flags.
fn g_cfg_rule(has_model_path: i64, n_predict: i64, n_threads: i64, n_ctx: i64, n_ubatch: i64,
              n_expert_used: i64, has_wl_capture: i64, has_wl_replay: i64, moe_enabled: i64,
              spec_enabled: i64, vram_cache_mb: i64, expert_hot_slots: i64, n_gpu_layers: i64,
              temp: f64, top_k: i64, top_p: f64, overlap: i64, marker: i64) -> i64 {
    if marker != 210 {
        return -1;
    }
    if has_model_path == 0 { return 1; }
    if n_predict <= 0 { return 2; }
    if n_threads <= 0 { return 3; }
    if n_ctx <= 0 { return 4; }
    if n_ubatch < 0 { return 5; }
    if n_ubatch > n_ctx { return 6; }
    if n_expert_used < 0 { return 7; }
    if has_wl_capture == 1 && has_wl_replay == 1 { return 8; }
    if (has_wl_capture == 1 || has_wl_replay == 1) && moe_enabled == 0 { return 9; }
    if (has_wl_capture == 1 || has_wl_replay == 1) && spec_enabled == 1 { return 10; }
    if vram_cache_mb < 0 { return 11; }
    if expert_hot_slots < 0 { return 12; }
    if (vram_cache_mb > 0 || expert_hot_slots > 0) && n_gpu_layers == 0 { return 13; }
    if temp > 0.0 {
        if top_k < 0 { return 14; }
        if top_p <= 0.0 || top_p > 1.0 { return 15; }
    }
    if spec_enabled == 1 && temp > 0.0 { return 16; }
    if overlap == 1 && moe_enabled == 0 { return 17; }
    return 0;
}

fn main() {
    let r = g_cfg_rule(1, 128, 4, 2048, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, 40, 0.9, 0, 210);
    if r < 0 {
        return;
    }
}
