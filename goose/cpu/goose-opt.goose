// P4-12 final slice: ggml-opt optimizer and training-loop decisions.
//
// Ported from upstream/llama.cpp/ggml/src/ggml-opt.cpp at base commit
// 98eaa70e. Only the decisions that do not need a device, a graph or real
// weights are here; tensor construction, graph building and backend
// scheduling stay native.
//
// Slice S1 (op_sched_*): dataset and epoch scheduling.
//   ggml_opt_dataset_init matrix sizes         lines 112-128
//   ggml_opt_dataset_shuffle range             lines 150-161
//   ggml_opt_dataset_get_batch shard mapping    lines 163-194
//   ggml_opt_epoch split and loop bounds        lines 895-923
//   ggml_opt_fit period and validation split    lines 1015-1053
//
// Slice S2 (op_step_*): optimizer step and gradient-accumulation policy.
//   ggml_opt_build accumulate / momenta         lines 328-332, 354-355, 472
//   ggml_opt_alloc build type for the next eval lines 727-735
//   ggml_opt_eval status updates                lines 799-826
//   loss scaling                                lines 390, 408-410, 425
//   ggml_opt_default_params                     lines 245-260
//   ggml_opt_optimizer_name                     lines 1085-1094
//
// Slice S3 (op_res_*): result aggregation.
//   ggml_opt_result_loss                        lines 658-691
//   ggml_opt_result_accuracy                    lines 699-708
//   ggml_opt_eval result increment              lines 842-876
//
// Every function is pure integer or floating-point arithmetic over caller
// buffers. No ggml handle crosses the boundary. The differential fixture in
// tests/fixture.cpp compares against the real ggml-opt.cpp compiled into the
// gate, including one tiny CPU training run per configuration.

import std;

extern "sqrt" fn sqrt(x: f64) -> f64;

// Build-type enumerators from ggml-opt.h lines 71-75.
//   GGML_OPT_BUILD_TYPE_FORWARD = 10, GRAD = 20, OPT = 30.
// Loss-type enumerators from ggml-opt.h lines 30-35.
//   MEAN = 0, SUM = 1, CROSS_ENTROPY = 2, MEAN_SQUARED_ERROR = 3.
// Optimizer enumerators from ggml-opt.h lines 77-82.
//   ADAMW = 0, SGD = 1, COUNT = 2.

fn nan64() -> f64 {
    let z: f64 = 0.0;
    return z / z;
}

// ---------- S1: dataset and epoch scheduling ----------

// ggml_opt_epoch line 899: the dataset length must be a whole number of
// batches, so the quotient is exact.
export fn op_nbatches(ndata: i64, ndata_batch: i64) -> i64 {
    return ndata / ndata_batch;
}

// ggml_opt_epoch lines 901-903. A negative split means "no validation part",
// i.e. the whole dataset is training data.
export fn op_batches_split(ndata: i64, ndata_batch: i64, idata_split: i64) -> i64 {
    var split: i64 = idata_split;
    if split < 0 {
        split = ndata;
    }
    return split / ndata_batch;
}

// ggml_opt_fit lines 1020-1026. The split is computed in f32 exactly as the
// native code does: (1.0f - val_split) * nbatches_logical, then truncated to
// int64 and multiplied by the physical opt period.
// out = [opt_period, nbatches_logical, ibatch_split, idata_split].
export fn op_epoch_split(
    ndata: i64,
    nbatch_logical: i64,
    nbatch_physical: i64,
    val_split: f32,
    out: i64[:],
) -> i64 {
    let period: i64 = nbatch_logical / nbatch_physical;
    let nbatches_logical: i64 = ndata / nbatch_logical;
    let keep: f32 = (1.0 as f32) - val_split;
    let scaled: f32 = keep * (nbatches_logical as f32);
    let ibatch_split: i64 = (scaled as i64) * period;
    out[0] = period;
    out[1] = nbatches_logical;
    out[2] = ibatch_split;
    out[3] = ibatch_split * nbatch_physical;
    return 4;
}

// ggml_opt_dataset_init lines 112 and 116: both the data and the labels shard
// size are the tensor byte count times ndata_shard/ndata. The port returns the
// shard byte count for the data tensor and, when has_labels is set, for the
// labels tensor.
// out = [nbs_data, nbs_labels].
export fn op_shard_bytes(
    nbytes_data: i64,
    nbytes_labels: i64,
    ndata: i64,
    ndata_shard: i64,
    has_labels: i64,
    out: i64[:],
) -> i64 {
    out[0] = nbytes_data * ndata_shard / ndata;
    if has_labels != 0 {
        out[1] = nbytes_labels * ndata_shard / ndata;
    } else {
        out[1] = 0;
    }
    return 2;
}

// ggml_opt_dataset_shuffle lines 153-160. Returns the number of leading shards
// that are shuffled; -1 means the whole permutation. The native code asserts
// idata % ndata_shard == 0 and idata <= ndata.
export fn op_shuffle_limit(idata: i64, ndata_shard: i64) -> i64 {
    if idata < 0 {
        return -1;
    }
    return idata / ndata_shard;
}

// ggml_opt_dataset_get_batch lines 170-182, host variant: the batch is a
// contiguous run of shards taken from the permutation.
export fn op_batch_shard(perm: const i64[:], ibatch: i64, shards_per_batch: i64, k: i64) -> i64 {
    return perm[ibatch * shards_per_batch + k];
}

// ggml_opt_fit line 1041 and 1051: shuffling is pointless when a single batch
// covers the dataset, and the train prefix is reshuffled only when the split
// sits inside the dataset.
export fn op_should_shuffle(nbatch_logical: i64, ndata: i64) -> i64 {
    if nbatch_logical < ndata {
        return 1;
    }
    return 0;
}

export fn op_should_reshuffle(nbatch_logical: i64, idata_split: i64) -> i64 {
    if nbatch_logical < idata_split {
        return 1;
    }
    return 0;
}

// ---------- S2: optimizer step and accumulation policy ----------

// ggml_opt_fit line 1020: physical batches per logical optimizer batch.
export fn op_opt_period(nbatch_logical: i64, nbatch_physical: i64) -> i64 {
    return nbatch_logical / nbatch_physical;
}

// ggml_opt_alloc lines 731-732: after this call the graph is the optimizer
// graph when the next step index wraps to zero, otherwise a gradient graph.
export fn op_build_type_next(opt_i: i64, opt_period: i64) -> i64 {
    let nxt: i64 = (opt_i + 1) % opt_period;
    if nxt == 0 {
        return 30;
    }
    return 20;
}

// ggml_opt_eval line 825: the iteration counter advances only when the
// evaluated graph was the optimizer graph.
export fn op_iter_advance(iter: i64, is_opt: i64) -> i64 {
    return iter + is_opt;
}

// ggml_opt_eval line 826: the step index advances after every evaluation,
// including forward-only validation evaluations.
export fn op_opt_i_advance(opt_i: i64, opt_period: i64) -> i64 {
    return (opt_i + 1) % opt_period;
}

// ggml_opt_build lines 328-329: gradients are accumulated whenever the
// allocation target reaches GRAD, except for a static optimizer graph with
// period 1, which updates in place.
export fn op_accumulate(build_type_alloc: i64, static_graphs: i64, opt_period: i64) -> i64 {
    if build_type_alloc < 20 {
        return 0;
    }
    if static_graphs != 0 && build_type_alloc == 30 && opt_period == 1 {
        return 0;
    }
    return 1;
}

// ggml_opt_build lines 331-332: only an AdamW optimizer graph needs m and v.
export fn op_need_momenta(build_type_alloc: i64, optimizer: i64) -> i64 {
    if build_type_alloc == 30 && optimizer == 0 {
        return 1;
    }
    return 0;
}

// ggml_opt_build line 506: AdamW carries alpha, beta1, beta2, eps, wd and the
// two warmup-corrected betas; SGD carries alpha and wd.
export fn op_params_width(need_momenta: i64) -> i64 {
    if need_momenta != 0 {
        return 7;
    }
    return 2;
}

// Loss scaling. MEAN and MEAN_SQUARED_ERROR divide by the physical step and
// the output element count; CROSS_ENTROPY divides by the step only when it is
// above one; SUM is not scaled.
export fn op_loss_scale(loss_type: i64, opt_period: i64, nelements: i64) -> f32 {
    if loss_type == 1 {
        return 1.0;
    }
    if loss_type == 2 {
        if opt_period > 1 {
            return 1.0 / opt_period;
        }
        return 1.0;
    }
    let denom: i64 = opt_period * nelements;
    return 1.0 / denom;
}

// ggml_opt_build lines 393, 399, 412, 428: whether the loss is reported per
// datapoint or as an unscaled sum.
export fn op_loss_per_dp(loss_type: i64) -> i64 {
    if loss_type == 1 {
        return 0;
    }
    return 1;
}

// ggml_opt_optimizer_name lines 1085-1094: 0 adamw, 1 sgd, -1 for any other
// enumerator, which the native switch maps to "undefined".
export fn op_optimizer_code(id: i64) -> i64 {
    if id == 0 {
        return 0;
    }
    if id == 1 {
        return 1;
    }
    return -1;
}

// ggml_opt_default_params lines 245-260.
// out = [loss_type, build_type, opt_period, optimizer, has_default_pars].
export fn op_defaults(loss_type: i64, out: i64[:]) -> i64 {
    out[0] = loss_type;
    out[1] = 30;
    out[2] = 1;
    out[3] = 0;
    out[4] = 1;
    return 5;
}

// ---------- S3: result aggregation ----------

// ggml_opt_result_loss lines 658-691. Per-datapoint losses were scaled down by
// the opt period for every physical batch, so they are multiplied back before
// the mean. unc is the standard error; it is NaN for fewer than two batches.
// out = [loss, unc].
export fn op_result_loss(
    losses: const f32[:],
    loss_per_dp: i64,
    opt_period: i64,
    out: f64[:],
) -> i64 {
    let nbatches: i64 = losses.len;
    if nbatches == 0 {
        out[0] = 0.0;
        out[1] = nan64();
        return 2;
    }
    var sum: f64 = 0.0;
    var sum_squared: f64 = 0.0;
    var i: i64 = 0;
    while i < nbatches {
        // The native rescale is a float multiply before the double sum.
        var vf: f32 = losses[i];
        if loss_per_dp != 0 {
            vf = vf * opt_period;
        }
        let v: f64 = vf;
        // The squared term is a float multiply in the native code.
        let sq: f32 = vf * vf;
        sum = sum + v;
        sum_squared = sum_squared + sq;
        i = i + 1;
    }
    let n: f64 = nbatches;
    let mean: f64 = sum / n;
    if loss_per_dp != 0 {
        out[0] = mean;
    } else {
        out[0] = sum;
    }
    if nbatches < 2 {
        out[1] = nan64();
        return 2;
    }
    let var_sum: f64 = sum_squared / n - mean * mean;
    let denom: f64 = n - 1.0;
    if loss_per_dp != 0 {
        out[1] = sqrt(var_sum / denom);
    } else {
        out[1] = sqrt(var_sum * n / denom);
    }
    return 2;
}

// ggml_opt_result_accuracy lines 699-708.
// out = [accuracy, unc].
export fn op_result_accuracy(ncorrect: i64, ndata: i64, out: f64[:]) -> i64 {
    if ncorrect < 0 {
        out[0] = nan64();
        out[1] = nan64();
        return 2;
    }
    let nc: f64 = ncorrect;
    let nd: f64 = ndata;
    let acc: f64 = nc / nd;
    out[0] = acc;
    if ndata < 2 {
        out[1] = nan64();
        return 2;
    }
    out[1] = sqrt(acc * (1.0 - acc) / (ndata - 1));
    return 2;
}

// ggml_opt_eval lines 850-852: the new total is the old total plus one batch of
// datapoints, and a mismatch against the already recorded batches is an error.
// Returns the new total or -1 on a mismatch.
export fn op_ndata_next(prev: i64, ndata: i64, nlosses: i64) -> i64 {
    if prev != ndata * nlosses {
        return -1;
    }
    return prev + ndata;
}

// ggml_opt_eval lines 862-864: the prediction vector grows by one batch.
export fn op_pred_len(prev: i64, ndata: i64) -> i64 {
    return prev + ndata;
}

// ggml_opt_eval lines 867-876: -1 is the "accuracy undefined" sentinel and it
// is sticky; otherwise the correct counts accumulate.
export fn op_ncorrect_next(cur: i64, ncorrect: i64, has_ncorrect: i64) -> i64 {
    if has_ncorrect == 0 || cur < 0 {
        return -1;
    }
    return cur + ncorrect;
}

export fn op_accuracy_defined(ncorrect: i64) -> i64 {
    if ncorrect >= 0 {
        return 1;
    }
    return 0;
}
