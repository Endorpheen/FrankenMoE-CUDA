// bmoe::validate (runtime/core/src/config.cpp) as an ordered rule index.
// Returns 0 when the configuration passes, otherwise the 1-based index of the first failing rule
// in the native order. Messages are not ported: the harness maps the native message back to its
// rule, and comparing indices is what makes the two sides comparable at all.
fn g_cfg_rule(has_model_path: i64, n_predict: i64, n_threads: i64, n_ctx: i64, n_ubatch: i64,
              n_expert_used: i64, has_wl_capture: i64, has_wl_replay: i64, moe_enabled: i64,
              spec_enabled: i64, spec_is_mtp: i64, spec_is_ngram: i64, draft_max: i64,
              draft_p_min: f64, ngram_max_match: i64, ngram_min_match: i64,
              vram_cache_mb: i64, expert_hot_slots: i64, n_gpu_layers: i64,
              temp: f64, top_k: i64, top_p: f64, overlap: i64, predict_log: i64,
              predict_prefetch: i64, route_ahead: i64, marker: i64) -> i64 {
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
    // SpecConfig::draft_max_limit = 8, ngram_match_limit = 64 (config.h)
    if spec_enabled == 1 && (draft_max < 1 || draft_max > 8) { return 17; }
    if spec_is_mtp == 1 && (draft_p_min < 0.0 || draft_p_min > 1.0) { return 18; }
    if spec_is_ngram == 1 && draft_p_min != 0.0 { return 19; }
    if spec_is_ngram == 1 && (ngram_max_match < 1 || ngram_max_match > 64) { return 20; }
    if spec_is_ngram == 1 && (ngram_min_match < 1 || ngram_min_match > ngram_max_match) { return 21; }
    if spec_enabled == 1 && n_ubatch > 0 && n_ubatch < draft_max + 1 { return 22; }
    if overlap == 1 && moe_enabled == 0 { return 23; }
    if predict_log == 1 && moe_enabled == 0 { return 24; }
    if predict_prefetch == 1 && moe_enabled == 0 { return 25; }
    if route_ahead > 0 && moe_enabled == 0 { return 26; }
    return 0;
}

fn main() {
    let r = g_cfg_rule(1, 128, 4, 2048, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0.0, 0, 0, 0, 0, 0, 0.0, 40, 0.9, 0, 0, 0, 0, 210);
    if r < 0 {
        return;
    }
}
