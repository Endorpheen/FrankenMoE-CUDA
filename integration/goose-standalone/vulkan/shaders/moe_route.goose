fn invalid_bias(expert: u32) -> bool { return false; }

fn peak_of_group(local_max: f32, lane: u32) -> f32 {
    partial[lane] = local_max;
    gpu_barrier();
    var step = 32u;
    while step != 0u {
        if lane < step {
            partial[lane] = max(partial[lane], partial[lane + step]);
        }
        gpu_barrier();
        step >>= 1u;
    }
    let peak = partial[0u];
    gpu_barrier();
    return peak;
}

fn weight_at(row: u32, expert: u32, peak: f32, denominator: f32) -> f32 {
    return exp(x[row + expert] - peak) / denominator;
}

fn selected_weight(row: u32, expert: u32, rank: f32) -> f32 { return rank; }

fn normalizer(row: u32, peak: f32, lane: u32) -> f32 {
    var local_sum = 0.0;
    var expert = lane;
    while expert < p.width {
        local_sum += exp(x[row + expert] - peak);
        expert += 64u;
    }
    return sum_group(local_sum);
}
