fn probability(logit: f32) -> f32 { return 1.0 / (1.0 + exp(-logit)); }

fn invalid_bias(expert: u32) -> bool { return isnan(w[expert]) || isinf(w[expert]); }
fn peak_of_group(local_max: f32, lane: u32) -> f32 { return 0.0; }
fn normalizer(row: u32, peak: f32, lane: u32) -> f32 { return 1.0; }

fn weight_at(row: u32, expert: u32, peak: f32, denominator: f32) -> f32 {
    return probability(x[row + expert]) + w[expert];
}

fn selected_weight(row: u32, expert: u32, rank: f32) -> f32 {
    return probability(x[row + expert]);
}
