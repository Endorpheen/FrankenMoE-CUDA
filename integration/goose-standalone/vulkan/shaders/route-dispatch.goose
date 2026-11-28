@compute(local_size = (64, 1, 1))
fn main() {
    let lane = gpu_local_id_x();
    let token = gpu_workgroup_id_x();
    let base = token * p.width;
    let out_base = token * (2u * p.rows);
    let neg_inf = gpu_u32_bits(0xff800000u);
    var local_max = neg_inf;
    var bad = 0.0;
    var expert = lane;
    while expert < p.width {
        let score = x[base + expert];
        if isnan(score) || isinf(score) || invalid_bias(expert) {
            bad = 1.0;
        } else {
            local_max = max(local_max, score);
        }
        expert += 64u;
    }
    if sum_group(bad) != 0.0 {
        if lane < p.rows {
            routes[out_base + lane] = 0xffffffffu;
            routes[out_base + p.rows + lane] = 0u;
        }
        return;
    }
    let peak = peak_of_group(local_max, lane);
    let denominator = normalizer(base, peak, lane);

    var slot = 0u;
    while slot < p.rows {
        var best = neg_inf;
        var id = 0xffffffffu;
        expert = lane;
        while expert < p.width {
            var used = false;
            var prior = 0u;
            while prior < slot {
                used = used || selected[prior] == expert;
                prior += 1u;
            }
            let rank = weight_at(base, expert, peak, denominator);
            if !used && rank > best {
                best = rank;
                id = expert;
            }
            expert += 64u;
        }
        partial[lane] = best;
        candidates[lane] = id;
        gpu_barrier();
        var step = 32u;
        while step != 0u {
            if lane < step {
                let other = partial[lane + step];
                let other_id = candidates[lane + step];
                if other > partial[lane] || (other == partial[lane] && other_id < candidates[lane]) {
                    partial[lane] = other;
                    candidates[lane] = other_id;
                }
            }
            gpu_barrier();
            step >>= 1u;
        }
        if lane == 0u {
            selected[slot] = candidates[0u];
            picked[slot] = selected_weight(base, candidates[0u], partial[0u]);
        }
        gpu_barrier();
        slot += 1u;
    }
    var local_sum = 0.0;
    if lane < p.rows { local_sum = picked[lane]; }
    let selected_sum = max(sum_group(local_sum), 0.00006103515625);
    if lane < p.rows {
        var scale = p.theta;
        if scale == 0.0 { scale = 1.0; }
        routes[out_base + lane] = selected[lane];
        routes[out_base + p.rows + lane] = gpu_f32_bits((picked[lane] / selected_sum) * scale);
    }
}
