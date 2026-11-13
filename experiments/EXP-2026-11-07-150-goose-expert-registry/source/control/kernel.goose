extern "fg143_ctl_meta" fn meta(context: i64, node: i64, field: i64) -> i64;
extern "fg143_ctl_extra_work" fn extra_work(context: i64, node: i64, threads: i64) -> i64;

fn tiled_work(c: i64, n: i64, count: i64) -> i64 {
    if meta(c, n, 61) == 0 || meta(c, n, 62) != 0 || meta(c, n, 63) == 0 {
        return 0;
    }
    let kind = meta(c, n, 2);

    if kind == 14 || kind == 13 || kind == 12 || kind == 11 || kind == 10 || kind == 23 || kind == 16 || kind == 17 || kind == 22 || kind == 18 || kind == 21 || kind == 19 || kind == 29 {
        return add(64, mul(count, meta(c, n, 64)));
    }
    return 0;
}
extern "fg143_ctl_goose_work" fn goose_work(context: i64, node: i64, tasks: i64) -> i64;
extern "fg143_ctl_extra_compute" fn extra_compute(context: i64) -> bool;
extern "fg143_ctl_witness" fn witness(kind: i64);

fn add(a: i64, b: i64) -> i64 {
    assert(a >= 0 && b >= 0 && a <= 9223372036854775807 - b);
    return a + b;
}

fn mul(a: i64, b: i64) -> i64 {
    assert(a >= 0 && b >= 0);

    if a == 0 || b == 0 {
        return 0;
    }
    assert(a <= 9223372036854775807 / b);
    return a * b;
}

fn maximum(a: i64, b: i64) -> i64 {
    return if a > b {
        a
    } else {
        b
    }
    ;
}

fn minimum(a: i64, b: i64) -> i64 {
    return if a < b {
        a
    } else {
        b
    }
    ;
}

fn align_size(a: i64, alignment: i64) -> i64 {
    assert(alignment > 0);
    return mul(add(a, alignment - 1) / alignment, alignment);
}

fn shape(c: i64, n: i64, s: i64, axis: i64) -> i64 {
    assert(s >= 0 && s < 3 && axis >= 0 && axis < 4);
    return meta(c, n, 20 + 4 * s + axis);
}

fn elements(c: i64, n: i64, s: i64) -> i64 {
    var total: i64 = 1;
    for axis in 4 {
        total = mul(total, shape(c, n, s, axis));
    }
    return total;
}

fn empty(c: i64, n: i64) -> bool {
    for axis in 4 {
        if meta(c, n, 10 + axis) == 0 {
            return true;
        }
    }
    return false;
}

fn tasks(c: i64, n: i64, threads: i64) -> i64 {
    assert(threads > 0);

    if empty(c, n) {
        return 1;
    }
    let op = meta(c, n, 0);
    let sub = meta(c, n, 8);

    if op == 91 {
        if sub == 8 || sub == 16 || sub == 9 || sub == 10 || sub == 17 {
            return threads;
        }
        assert(sub >= 0 && sub < meta(c, n, 60));
        return 1;
    }

    if op == 100 {
        assert(sub >= 0 && sub < meta(c, n, 59));
        return threads;
    }

    if op == 46 {
        return minimum(threads, mul(mul(shape(c, n, 0, 1), shape(c, n, 0, 2)), shape(c, n, 0, 3)));
    }

    if op == 82 || op == 84 || op == 83 {
        return minimum(threads, shape(c, n, 1, 1));
    }

    if op == 92 || op == 93 || op == 94 || op == 95 {
        let requested = meta(c, n, 7);
        let count = if requested == -1 {
            threads
        } else {
            minimum(requested, threads)
        }
        ;
        assert(count > 0);
        return count;
    }

    if op == 6 || op == 9 || op == 10 || op == 11 || op == 12 || op == 13 || op == 14 || op == 15 || op == 17 || op == 18 || op == 20 || op == 21 || op == 71 || op == 40 || op == 32 || op == 33 || op == 36 || op == 37 || op == 38 || op == 39 || op == 41 || op == 43 || op == 50 || op == 60 || op == 61 || op == 62 || op == 78 || op == 79 || op == 80 || op == 0 {
        return 1;
    }

    if op == 34 || op == 1 || op == 35 || op == 2 || op == 3 || op == 4 || op == 5 || op == 16 || op == 72 || op == 73 || op == 19 || op == 85 || op == 86 || op == 88 || op == 89 || op == 90 || op == 23 || op == 7 || op == 8 || op == 24 || op == 25 || op == 26 || op == 28 || op == 27 || op == 22 || op == 29 || op == 30 || op == 31 || op == 42 || op == 45 || op == 44 || op == 47 || op == 48 || op == 49 || op == 81 || op == 52 || op == 53 || op == 54 || op == 56 || op == 57 || op == 58 || op == 55 || op == 51 || op == 59 || op == 63 || op == 64 || op == 65 || op == 66 || op == 67 || op == 68 || op == 69 || op == 70 || op == 74 || op == 75 || op == 76 || op == 77 || op == 87 || op == 96 || op == 97 || op == 98 || op == 99 {
        return threads;
    }
    assert(false);
    return 0;
}

fn row_size(c: i64, n: i64, count: i64) -> i64 {
    let block_width = meta(c, n, 42);
    assert(block_width > 0 && count >= 0 && count % block_width == 0);
    return mul(count / block_width, meta(c, n, 41));
}

fn workspace(c: i64, n: i64, threads: i64, count: i64) -> i64 {
    let extra = extra_work(c, n, threads);
    var cur: i64 = 0;

    if extra >= 0 {
        cur = extra;
    } else {
        let op = meta(c, n, 0);
        let kind = meta(c, n, 1);
        let x = meta(c, n, 2);
        let y = meta(c, n, 3);
        let dstwidth = meta(c, n, 10);
        let width = shape(c, n, 0, 0);

        if op == 34 || op == 1 {
            if meta(
                c,
                n,
                5) != 0 || (x == 1 && y == 30) || (x == 30 && y == 1) || (x == 0 && y == 26) || (x == 26 && y == 0) {
                cur = mul(mul(4, dstwidth), count);
            }
        } else

        if op == 2 || op == 3 || op == 4 {
            if meta(c, n, 6) != 0 {
                cur = mul(mul(4, width), count);
            }
        } else

        if op == 5 {
            if meta(c, n, 6) != 0 {
                cur = mul(mul(4, shape(c, n, 1, 0)), count);
            }
        } else

        if op == 19 {
            cur = mul(meta(c, n, 43), count);
        } else

        if op == 29 {
            if y != meta(c, n, 40) {
                cur = row_size(c, n, elements(c, n, 1));
            }
            cur = add(align_size(cur, 64), tiled_work(c, n, count));
        } else

        if op == 30 {
            if y != meta(c, n, 40) {
                cur = add(row_size(c, n, elements(c, n, 1)), 8);
            }
            let experts = shape(c, n, 0, 2);
            cur = add(cur, add(mul(experts, 8), 8));
            cur = add(cur, add(mul(mul(mul(experts, shape(c, n, 2, 0)), shape(c, n, 2, 1)), meta(c, n, 54)), 8));
            cur = add(cur, mul(meta(c, n, 53), add(experts, 1)));
            cur = add(align_size(cur, 64), tiled_work(c, n, count));
        } else

        if op == 31 {
            if meta(c, n, 6) != 0 || x == 1 {
                cur = mul(mul(4, width), count);
            }
        } else

        if op == 42 {
            if x == 1 && kind != 1 {
                cur = mul(mul(4, width), count);
            }
        } else

        if op == 46 || op == 48 || op == 49 {
            cur = mul(mul(4, dstwidth), count);
        } else

        if op == 51 {
            assert(shape(c, n, 0, 3) == 1 && shape(c, n, 1, 2) == 1 && shape(c, n, 1, 3) == 1);
            assert( (x == 1 || x == 30 || x == 0) && y == 0);
            let bytes = if x == 0 {
                4
            } else {
                2
            }
            ;
            cur = mul(
                bytes,
                add(mul(mul(width, shape(c, n, 0, 1)), shape(c, n, 0, 2)), mul(shape(c, n, 1, 0), shape(c, n, 1, 1))));
        } else

        if op == 56 || op == 57 {
            cur = meta(c, n, 58);
        } else

        if op == 59 {
            assert( (x == 0 || x == 1) && y == 0);
            cur = mul(meta(c, n, 44), add(elements(c, n, 0), elements(c, n, 1)));
        } else

        if op == 70 {
            cur = mul(mul(4, width), count);
        } else

        if op == 74 {
            let heads = shape(c, n, 0, 2);
            let dk = shape(c, n, 1, 0);
            let dv = shape(c, n, 2, 0);
            let tq = meta(c, n, 55);
            let tk = meta(c, n, 56);
            let prefill = mul(
                mul(4, count),
                add(add(mul(tq, dk), mul(mul(2, tq), tk)), add(mul(tq, dv), add(mul(tk, dv), mul(tk, dk)))));
            let decode = mul(4, add(mul(mul(heads, count), add(2, dv)), mul(count, add(dk, mul(2, dv)))));
            cur = maximum(prefill, decode);
        } else

        if op == 75 {
            if y == 0 || y == 1 || y == 30 {
                let dimension = maximum(width, align_size(shape(c, n, 1, 1), meta(c, n, 57)));
                cur = mul(mul(mul(16, dimension), count), 1);
            }
        } else

        if op == 96 {
            cur = mul(meta(c, n, 43), add(count, mul(width, count)));
        } else

        if op == 86 {
            let width_v = shape(c, n, 2, 0);
            let per = add(width_v, if sub_snapshot(c, n) > 1 {
                mul(width_v, width_v)
            } else {
                0
            }
            );
            cur = mul(mul(per, 4), count);
        } else

        if op == 87 {
            cur = mul(mul(4, shape(c, n, 1, 0)), count);
        } else {
            assert(op != 101);
        }
    }
    return maximum(cur, goose_work(c, n, count));
}

fn sub_snapshot(c: i64, n: i64) -> i64 {
    return meta(c, n, 8);
}

fn plan(c: i64, count: i64, requested: i64, out: i64[:]) {
    assert(count >= 0 && out.len >= 2);
    var threads = requested;

    if threads <= 0 {
        let pool = meta(c, -1, 50);
        threads = if pool > 0 {
            pool
        } else {
            meta(c, -1, 51)
        }
        ;
    }

    if meta(c, -1, 52) != 0 {
        threads = 1;
    }
    assert(threads > 0);
    var max_tasks: i64 = 1;
    var work: i64 = 0;
    for node in count {
        let nt = tasks(c, node, threads);
        assert(nt > 0);
        max_tasks = maximum(max_tasks, nt);
        work = maximum(work, workspace(c, node, threads, nt));
    }

    if work > 0 {
        work = add(work, mul(meta(c, -1, 53), threads));
    }
    out[0] = work;
    out[1] = minimum(max_tasks, threads);
    witness(0);
}
extern "fg143_ctl_leaf_dup" fn leaf_dup(context: i64);
extern "fg143_ctl_leaf_add" fn leaf_add(context: i64);
extern "fg143_ctl_leaf_add_id" fn leaf_add_id(context: i64);
extern "fg143_ctl_leaf_add1" fn leaf_add1(context: i64);
extern "fg143_ctl_leaf_acc" fn leaf_acc(context: i64);
extern "fg143_ctl_leaf_sub" fn leaf_sub(context: i64);
extern "fg143_ctl_leaf_mul" fn leaf_mul(context: i64);
extern "fg143_ctl_leaf_div" fn leaf_div(context: i64);
extern "fg143_ctl_leaf_sqr" fn leaf_sqr(context: i64);
extern "fg143_ctl_leaf_sqrt" fn leaf_sqrt(context: i64);
extern "fg143_ctl_leaf_log" fn leaf_log(context: i64);
extern "fg143_ctl_leaf_sin" fn leaf_sin(context: i64);
extern "fg143_ctl_leaf_cos" fn leaf_cos(context: i64);
extern "fg143_ctl_leaf_sum" fn leaf_sum(context: i64);
extern "fg143_ctl_leaf_sum_rows" fn leaf_sum_rows(context: i64);
extern "fg143_ctl_leaf_cumsum" fn leaf_cumsum(context: i64);
extern "fg143_ctl_leaf_mean" fn leaf_mean(context: i64);
extern "fg143_ctl_leaf_argmax" fn leaf_argmax(context: i64);
extern "fg143_ctl_leaf_count_equal" fn leaf_count_equal(context: i64);
extern "fg143_ctl_leaf_repeat" fn leaf_repeat(context: i64);
extern "fg143_ctl_leaf_repeat_back" fn leaf_repeat_back(context: i64);
extern "fg143_ctl_leaf_concat" fn leaf_concat(context: i64);
extern "fg143_ctl_leaf_silu_back" fn leaf_silu_back(context: i64);
extern "fg143_ctl_leaf_norm" fn leaf_norm(context: i64);
extern "fg143_ctl_leaf_rms_norm" fn leaf_rms_norm(context: i64);
extern "fg143_ctl_leaf_rms_norm_back" fn leaf_rms_norm_back(context: i64);
extern "fg143_ctl_leaf_group_norm" fn leaf_group_norm(context: i64);
extern "fg143_ctl_leaf_l2_norm" fn leaf_l2_norm(context: i64);
extern "fg143_ctl_leaf_mul_mat" fn leaf_mul_mat(context: i64);
extern "fg143_ctl_leaf_mul_mat_id" fn leaf_mul_mat_id(context: i64);
extern "fg143_ctl_leaf_out_prod" fn leaf_out_prod(context: i64);
extern "fg143_ctl_leaf_scale" fn leaf_scale(context: i64);
extern "fg143_ctl_leaf_set" fn leaf_set(context: i64);
extern "fg143_ctl_leaf_cpy" fn leaf_cpy(context: i64);
extern "fg143_ctl_leaf_cont" fn leaf_cont(context: i64);
extern "fg143_ctl_leaf_get_rows" fn leaf_get_rows(context: i64);
extern "fg143_ctl_leaf_get_rows_back" fn leaf_get_rows_back(context: i64);
extern "fg143_ctl_leaf_set_rows" fn leaf_set_rows(context: i64);
extern "fg143_ctl_leaf_diag" fn leaf_diag(context: i64);
extern "fg143_ctl_leaf_diag_mask_inf" fn leaf_diag_mask_inf(context: i64);
extern "fg143_ctl_leaf_diag_mask_zero" fn leaf_diag_mask_zero(context: i64);
extern "fg143_ctl_leaf_soft_max" fn leaf_soft_max(context: i64);
extern "fg143_ctl_leaf_soft_max_back" fn leaf_soft_max_back(context: i64);
extern "fg143_ctl_leaf_rope" fn leaf_rope(context: i64);
extern "fg143_ctl_leaf_rope_back" fn leaf_rope_back(context: i64);
extern "fg143_ctl_leaf_clamp" fn leaf_clamp(context: i64);
extern "fg143_ctl_leaf_conv_transpose_1d" fn leaf_conv_transpose_1d(context: i64);
extern "fg143_ctl_leaf_im2col" fn leaf_im2col(context: i64);
extern "fg143_ctl_leaf_im2col_back" fn leaf_im2col_back(context: i64);
extern "fg143_ctl_leaf_im2col_3d" fn leaf_im2col_3d(context: i64);
extern "fg143_ctl_leaf_col2im_1d" fn leaf_col2im_1d(context: i64);
extern "fg143_ctl_leaf_conv_2d" fn leaf_conv_2d(context: i64);
extern "fg143_ctl_leaf_conv_3d" fn leaf_conv_3d(context: i64);
extern "fg143_ctl_leaf_conv_2d_dw" fn leaf_conv_2d_dw(context: i64);
extern "fg143_ctl_leaf_conv_transpose_2d" fn leaf_conv_transpose_2d(context: i64);
extern "fg143_ctl_leaf_pool_1d" fn leaf_pool_1d(context: i64);
extern "fg143_ctl_leaf_pool_2d" fn leaf_pool_2d(context: i64);
extern "fg143_ctl_leaf_pool_2d_back" fn leaf_pool_2d_back(context: i64);
extern "fg143_ctl_leaf_upscale" fn leaf_upscale(context: i64);
extern "fg143_ctl_leaf_pad" fn leaf_pad(context: i64);
extern "fg143_ctl_leaf_pad_reflect_1d" fn leaf_pad_reflect_1d(context: i64);
extern "fg143_ctl_leaf_roll" fn leaf_roll(context: i64);
extern "fg143_ctl_leaf_arange" fn leaf_arange(context: i64);
extern "fg143_ctl_leaf_timestep_embedding" fn leaf_timestep_embedding(context: i64);
extern "fg143_ctl_leaf_argsort" fn leaf_argsort(context: i64);
extern "fg143_ctl_leaf_top_k" fn leaf_top_k(context: i64);
extern "fg143_ctl_leaf_leaky_relu" fn leaf_leaky_relu(context: i64);
extern "fg143_ctl_leaf_tri" fn leaf_tri(context: i64);
extern "fg143_ctl_leaf_fill" fn leaf_fill(context: i64);
extern "fg143_ctl_leaf_flash_attn_ext" fn leaf_flash_attn_ext(context: i64);
extern "fg143_ctl_leaf_flash_attn_back" fn leaf_flash_attn_back(context: i64, masked: i64);
extern "fg143_ctl_leaf_ssm_conv" fn leaf_ssm_conv(context: i64);
extern "fg143_ctl_leaf_ssm_scan" fn leaf_ssm_scan(context: i64);
extern "fg143_ctl_leaf_win_part" fn leaf_win_part(context: i64);
extern "fg143_ctl_leaf_win_unpart" fn leaf_win_unpart(context: i64);
extern "fg143_ctl_leaf_unary" fn leaf_unary(context: i64);
extern "fg143_ctl_leaf_glu" fn leaf_glu(context: i64);
extern "fg143_ctl_leaf_get_rel_pos" fn leaf_get_rel_pos(context: i64);
extern "fg143_ctl_leaf_add_rel_pos" fn leaf_add_rel_pos(context: i64);
extern "fg143_ctl_leaf_rwkv_wkv6" fn leaf_rwkv_wkv6(context: i64);
extern "fg143_ctl_leaf_gated_linear_attn" fn leaf_gated_linear_attn(context: i64);
extern "fg143_ctl_leaf_rwkv_wkv7" fn leaf_rwkv_wkv7(context: i64);
extern "fg143_ctl_leaf_solve_tri" fn leaf_solve_tri(context: i64);
extern "fg143_ctl_leaf_gated_delta_net" fn leaf_gated_delta_net(context: i64);
extern "fg143_ctl_leaf_lightning_indexer" fn leaf_lightning_indexer(context: i64);
extern "fg143_ctl_leaf_dsv4_hc_comb" fn leaf_dsv4_hc_comb(context: i64);
extern "fg143_ctl_leaf_dsv4_hc_pre" fn leaf_dsv4_hc_pre(context: i64);
extern "fg143_ctl_leaf_dsv4_hc_post" fn leaf_dsv4_hc_post(context: i64);
extern "fg143_ctl_leaf_map_custom1" fn leaf_map_custom1(context: i64);
extern "fg143_ctl_leaf_map_custom2" fn leaf_map_custom2(context: i64);
extern "fg143_ctl_leaf_map_custom3" fn leaf_map_custom3(context: i64);
extern "fg143_ctl_leaf_custom" fn leaf_custom(context: i64);
extern "fg143_ctl_leaf_cross_entropy_loss" fn leaf_cross_entropy_loss(context: i64);
extern "fg143_ctl_leaf_cross_entropy_loss_back" fn leaf_cross_entropy_loss_back(context: i64);
extern "fg143_ctl_leaf_opt_step_adamw" fn leaf_opt_step_adamw(context: i64);
extern "fg143_ctl_leaf_opt_step_sgd" fn leaf_opt_step_sgd(context: i64);

fn dispatch(c: i64) {
    let op = meta(c, 0, 0);

    if op == 0 || empty(c, 0) {
        return;
    }

    if extra_compute(c) {
        return;
    }

    if op == 1 {
        leaf_dup(c);
    } else

    if op == 2 {
        leaf_add(c);
    } else

    if op == 3 {
        leaf_add_id(c);
    } else

    if op == 4 {
        leaf_add1(c);
    } else

    if op == 5 {
        leaf_acc(c);
    } else

    if op == 6 {
        leaf_sub(c);
    } else

    if op == 7 {
        leaf_mul(c);
    } else

    if op == 8 {
        leaf_div(c);
    } else

    if op == 9 {
        leaf_sqr(c);
    } else

    if op == 10 {
        leaf_sqrt(c);
    } else

    if op == 11 {
        leaf_log(c);
    } else

    if op == 12 {
        leaf_sin(c);
    } else

    if op == 13 {
        leaf_cos(c);
    } else

    if op == 14 {
        leaf_sum(c);
    } else

    if op == 15 {
        leaf_sum_rows(c);
    } else

    if op == 16 {
        leaf_cumsum(c);
    } else

    if op == 17 {
        leaf_mean(c);
    } else

    if op == 18 {
        leaf_argmax(c);
    } else

    if op == 19 {
        leaf_count_equal(c);
    } else

    if op == 20 {
        leaf_repeat(c);
    } else

    if op == 21 {
        leaf_repeat_back(c);
    } else

    if op == 22 {
        leaf_concat(c);
    } else

    if op == 23 {
        leaf_silu_back(c);
    } else

    if op == 24 {
        leaf_norm(c);
    } else

    if op == 25 {
        leaf_rms_norm(c);
    } else

    if op == 26 {
        leaf_rms_norm_back(c);
    } else

    if op == 27 {
        leaf_group_norm(c);
    } else

    if op == 28 {
        leaf_l2_norm(c);
    } else

    if op == 29 {
        leaf_mul_mat(c);
    } else

    if op == 30 {
        leaf_mul_mat_id(c);
    } else

    if op == 31 {
        leaf_out_prod(c);
    } else

    if op == 32 {
        leaf_scale(c);
    } else

    if op == 33 {
        leaf_set(c);
    } else

    if op == 34 {
        leaf_cpy(c);
    } else

    if op == 35 {
        leaf_cont(c);
    } else

    if op == 40 {
        leaf_get_rows(c);
    } else

    if op == 41 {
        leaf_get_rows_back(c);
    } else

    if op == 42 {
        leaf_set_rows(c);
    } else

    if op == 43 {
        leaf_diag(c);
    } else

    if op == 44 {
        leaf_diag_mask_inf(c);
    } else

    if op == 45 {
        leaf_diag_mask_zero(c);
    } else

    if op == 46 {
        leaf_soft_max(c);
    } else

    if op == 47 {
        leaf_soft_max_back(c);
    } else

    if op == 48 {
        leaf_rope(c);
    } else

    if op == 49 {
        leaf_rope_back(c);
    } else

    if op == 50 {
        leaf_clamp(c);
    } else

    if op == 51 {
        leaf_conv_transpose_1d(c);
    } else

    if op == 52 {
        leaf_im2col(c);
    } else

    if op == 53 {
        leaf_im2col_back(c);
    } else

    if op == 54 {
        leaf_im2col_3d(c);
    } else

    if op == 55 {
        leaf_col2im_1d(c);
    } else

    if op == 56 {
        leaf_conv_2d(c);
    } else

    if op == 57 {
        leaf_conv_3d(c);
    } else

    if op == 58 {
        leaf_conv_2d_dw(c);
    } else

    if op == 59 {
        leaf_conv_transpose_2d(c);
    } else

    if op == 60 {
        leaf_pool_1d(c);
    } else

    if op == 61 {
        leaf_pool_2d(c);
    } else

    if op == 62 {
        leaf_pool_2d_back(c);
    } else

    if op == 63 {
        leaf_upscale(c);
    } else

    if op == 64 {
        leaf_pad(c);
    } else

    if op == 65 {
        leaf_pad_reflect_1d(c);
    } else

    if op == 66 {
        leaf_roll(c);
    } else

    if op == 67 {
        leaf_arange(c);
    } else

    if op == 68 {
        leaf_timestep_embedding(c);
    } else

    if op == 69 {
        leaf_argsort(c);
    } else

    if op == 70 {
        leaf_top_k(c);
    } else

    if op == 71 {
        leaf_leaky_relu(c);
    } else

    if op == 72 {
        leaf_tri(c);
    } else

    if op == 73 {
        leaf_fill(c);
    } else

    if op == 74 {
        leaf_flash_attn_ext(c);
    } else

    if op == 75 {
        let mask = meta(c, 0, 8);
        assert(mask == 0 || mask == 1);
        leaf_flash_attn_back(c, mask);
    } else

    if op == 76 {
        leaf_ssm_conv(c);
    } else

    if op == 77 {
        leaf_ssm_scan(c);
    } else

    if op == 78 {
        leaf_win_part(c);
    } else

    if op == 79 {
        leaf_win_unpart(c);
    } else

    if op == 91 {
        leaf_unary(c);
    } else

    if op == 100 {
        leaf_glu(c);
    } else

    if op == 80 {
        leaf_get_rel_pos(c);
    } else

    if op == 81 {
        leaf_add_rel_pos(c);
    } else

    if op == 82 {
        leaf_rwkv_wkv6(c);
    } else

    if op == 83 {
        leaf_gated_linear_attn(c);
    } else

    if op == 84 {
        leaf_rwkv_wkv7(c);
    } else

    if op == 85 {
        leaf_solve_tri(c);
    } else

    if op == 86 {
        leaf_gated_delta_net(c);
    } else

    if op == 87 {
        leaf_lightning_indexer(c);
    } else

    if op == 88 {
        leaf_dsv4_hc_comb(c);
    } else

    if op == 89 {
        leaf_dsv4_hc_pre(c);
    } else

    if op == 90 {
        leaf_dsv4_hc_post(c);
    } else

    if op == 92 {
        leaf_map_custom1(c);
    } else

    if op == 93 {
        leaf_map_custom2(c);
    } else

    if op == 94 {
        leaf_map_custom3(c);
    } else

    if op == 95 {
        leaf_custom(c);
    } else

    if op == 96 {
        leaf_cross_entropy_loss(c);
    } else

    if op == 97 {
        leaf_cross_entropy_loss_back(c);
    } else

    if op == 98 {
        leaf_opt_step_adamw(c);
    } else

    if op == 99 {
        leaf_opt_step_sgd(c);
    } else

    if op == 36 || op == 37 || op == 39 || op == 38 {
    } else {
        assert(false);
    }
    witness(2);
}
extern "fg143_job_get" fn job_get(context: i64, field: i64) -> i64;
extern "fg143_job_set" fn job_set(context: i64, field: i64, value: i64);
extern "fg143_job_create" fn job_create(context: i64, threads: i64);
extern "fg143_job_free" fn job_free(context: i64);
extern "fg143_job_team" fn job_team(context: i64, threads: i64);
extern "fg143_job_barrier" fn job_barrier();
extern "fg143_job_place" fn job_place(context: i64, thread: i64);
extern "fg143_job_compute" fn job_compute(context: i64, thread: i64);
extern "fg143_job_clear_affinity" fn job_clear_affinity();
extern "fg149_job_kickoff" fn job_kickoff(context: i64, threads: i64);

fn job_worker(c: i64, single: bool) {
    let thread = if single {
        0
    } else {
        job_get(c, 10)
    }
    ;
    let actual = if single {
        1
    } else {
        job_get(c, 11)
    }
    ;
    assert(actual > 0 && thread >= 0 && thread < actual && actual <= job_get(c, 4));

    if thread == 0 {
        job_set(c, 5, actual);
    }

    if !single {
        job_barrier();
    }

    if !single {
        job_place(c, thread);
    }
    job_compute(c, thread);
    witness(4);
}

fn graph_job(c: i64, out: i64[:]) {
    assert(out.len >= 1 && job_get(c, 0) != 0);
    let requested = job_get(c, 1);
    assert(requested > 0 && job_get(c, 2) >= 0 && (job_get(c, 2) == 0 || job_get(c, 3) != 0));
    let created = job_get(c, 6) == 0;

    if created {
        job_create(c, requested);

        if job_get(c, 6) == 0 {
            out[0] = job_get(c, 14);
            witness(3);
            return;
        }
    } else {
        job_set(c, 0, 0);
        job_set(c, 1, 0);
        job_set(c, 2, 0);
        job_set(c, 3, -1);
        job_set(c, 4, job_get(c, 12));
    }
    assert(job_get(c, 6) != 0);

    if job_get(c, 13) != 0 {
        assert(requested <= job_get(c, 4));

        if requested > 1 {
            job_team(c, requested);
        } else {
            job_worker(c, true);
        }
    } else {
        let actual = minimum(requested, job_get(c, 4));
        job_kickoff(c, actual);
        job_compute(c, 0);
    }
    job_clear_affinity();
    let status = job_get(c, 7);

    if created {
        job_free(c);
    }
    out[0] = status;
    witness(3);
}

fn entry(mode: i64, c: i64, count: i64, requested: i64, out: i64[:]) {
    if mode == 0 {
        plan(c, count, requested, out);
    } else

    if mode == 1 {
        assert(out.len >= 1);
        out[0] = tasks(c, 0, requested);
        witness(1);
    } else

    if mode == 2 {
        dispatch(c);
    } else

    if mode == 3 {
        graph_job(c, out);
    } else

    if mode == 4 {
        job_worker(c, false);
    } else {
        assert(mode == 5 && out.len >= 1);
        out[0] = tiled_work(c, 0, requested);
    }
}

fn main() {
    var out: i64[2] = default < i64[2] > ();
    entry(0, 0, 0, 1, out);
}

