struct StatePlan {
    source: Tensor[7], dst: Tensor, ith: i64, nth: i64, op: i64, snapshots: i64, fp: f32[4], publish: bool
}

fn value_at(a: const u8[:], index: i64) -> f32 {
    return from_bits(read32(a, mul_size(index, 4)));
}

fn set_value(a: u8[:], index: i64, v: f32) {
    write32(a, mul_size(index, 4), bits(v));
}

extern "fg_cuda_gdn" fn cuda_gdn(plan: const i64[:], q: const f32[:], k: const f32[:],
    v: const f32[:], gate: const f32[:], beta: const f32[:], initial: const f32[:], dst: f32[:]) -> i64;
extern "fg_cuda_gdn_state_read" fn cuda_gdn_state_read(initial: const f32[:]) -> i64;

// The scalar device kernel uses the fast division descriptor from its
// original host plan. Both divisors are positive and fit in 32 bits.
fn gdn_divisor(divisor: i64, words: i64[:]) {
    let d = divisor as u64;
    var shift: i64 = 0;
    while ((1 as u64) << shift) < d { shift += 1; }
    let multiplier = ((1 as u64) << 32) * (((1 as u64) << shift) - d) / d + 1;
    words[0] = multiplier as i64;
    words[1] = shift;
    words[2] = divisor;
}

fn gdn_on_cuda(p: const StatePlan &, strides: const i64[4][6] &,
               q: const f32[:], k: const f32[:], v: const f32[:], gate: const f32[:],
               beta: const f32[:], initial: const f32[:], dst: f32[:],
               scale: f32, scores: i64, state_values: i64) -> i64 {
    let width = p.source[2].ne[0];
    let heads = p.source[2].ne[1];
    let tokens = p.source[2].ne[2];
    let sequences = p.source[2].ne[3];
    let key_heads = p.source[0].ne[1];
    let q_sequences = p.source[0].ne[3];
    let snapshots = p.snapshots;
    if p.nth != 1 || (width != 16 && width != 32 && width != 64 && width != 128) ||
       sequences > 65535 || heads > 2147483647 || tokens <= 0 || snapshots > tokens ||
       p.source[1].ne[1] != key_heads || p.source[1].ne[3] != q_sequences ||
       strides[0][0] != 1 || strides[1][0] != 1 || strides[2][0] != 1 ||
       dst.len != scores + snapshots * state_values {
        return 0;
    }
    for axis in 4 {
        if strides[0][axis] != strides[1][axis] { return 0; }
    }
    for i in 3 .. 6 {
        if !dense_scalar(p.source[i], 4) { return 0; }
    }
    for axis in 1 .. 4 {
        if p.source[3].ne[axis] != p.source[2].ne[axis] ||
           p.source[4].ne[axis] != p.source[2].ne[axis] {
            return 0;
        }
    }
    if p.source[5].ne[3] != sequences { return 0; }
    // Device packet: strides, kernel geometry, F32 scale bits, fast
    // division descriptors, and the state offset following the scores.
    var config: i64[50] = [0; 50];
    for i in 6 {
        for axis in 4 { config[4 * i + axis] = strides[i][axis]; }
    }
    config[24] = width;
    config[25] = heads;
    config[26] = tokens;
    config[27] = sequences;
    config[30] = snapshots;
    config[31] = if snapshots > 1 { 1 } else { 0 };
    config[34] = width / 16;
    config[35] = low(32, width);
    config[36] = 4;
    config[37] = 32;
    config[39] = state_values;
    config[40] = if p.source[3].ne[0] == width { 1 } else { 0 };
    config[41] = bits(scale) as i64;
    gdn_divisor(key_heads, config[42 .. 45]);
    gdn_divisor(sequences / q_sequences, config[45 .. 48]);
    config[48] = scores;
    config[49] = if p.publish { 1 } else { 0 };
    return cuda_gdn(config, q, k, v, gate, beta, initial, dst);
}

fn gdn(
    p: const StatePlan &,
    q: const f32[:],
    k: const f32[:],
    v: const f32[:],
    g: const f32[:],
    beta: const f32[:],
    initial: const f32[:],
    dst: f32[:]) -> bool {
    let width = p.source[2].ne[0];
    let heads = p.source[2].ne[1];
    let tokens = p.source[2].ne[2];
    let seqs = p.source[2].ne[3];
    let snapshots = p.snapshots;
    assert(width > 0 && heads > 0 && tokens >= 0 && seqs > 0 && snapshots >= 1 && p.nth > 0 && p.ith >= 0 && p.ith < p.nth);
    assert(p.dst.kind == 0 && dense_scalar(p.dst, 4));
    validate(p.dst, mul_size(dst.len, 4));
    validate(p.source[0], mul_size(q.len, 4));
    validate(p.source[1], mul_size(k.len, 4));
    validate(p.source[2], mul_size(v.len, 4));
    validate(p.source[3], mul_size(g.len, 4));
    validate(p.source[4], mul_size(beta.len, 4));
    validate(p.source[5], mul_size(initial.len, 4));

    var strides: i64[4][6] = default <i64[4][6]>();
    for source in 6 {
        assert(p.source[source].kind == 0);
        for axis in 4 {
            assert(p.source[source].nb[axis] % 4 == 0);
            strides[source][axis] = p.source[source].nb[axis] / 4;
        }
    }
    for source in 2 {
        let t = p.source[source];
        assert(t.ne[0] == width && t.ne[1] > 0 && heads % t.ne[1] == 0 &&
            t.ne[2] >= tokens && t.ne[3] > 0 && seqs % t.ne[3] == 0);
    }
    assert((p.source[3].ne[0] == 1 || p.source[3].ne[0] == width) && p.source[4].ne[0] == 1);
    for source in 3 .. 5 {
        let t = p.source[source];
        assert(t.ne[1] >= heads && t.ne[2] >= tokens && t.ne[3] >= seqs);
    }
    assert(p.source[5].ne[0] == width && p.source[5].ne[1] == width && p.source[5].ne[2] == heads && p.source[5].ne[3] >= seqs);

    let square = mul_size(width, width);
    let tasks = mul_size(heads, seqs);
    let scores = mul_size(mul_size(width, tokens), tasks);
    let snapshot_values = mul_size(square, tasks);
    assert(add_size(scores, mul_size(snapshot_values, snapshots)) <= dst.len);
    let scale = (1 as f32) / sqrt32(width as f32);
    let device = gdn_on_cuda(p, strides, q, k, v, g, beta, initial, dst, scale, scores, snapshot_values);
    if device != 0 {
        if device > 0 { witness(OP_GATED_DELTA_NET, scores); }
        return device > 0;
    }
    if activation_read_f32(q) != 0 || activation_read_f32(k) != 0 ||
       activation_read_f32(v) != 0 || activation_read_f32(g) != 0 ||
       activation_read_f32(beta) != 0 || activation_read_f32(initial) != 0 { return false; }
    if cuda_gdn_state_read(initial) != 0 { return false; }
    let gate_width = p.source[3].ne[0];
    var decay: f32[>..] = [];
    decay.resize(gate_width, 0.0);
    var task = p.ith;

    while task < tasks {
        let head = task % heads;
        let seq = task / heads;
        let qhead = head % p.source[0].ne[1];
        let khead = head % p.source[1].ne[1];
        let qseq = seq / (seqs / p.source[0].ne[3]);
        let kseq = seq / (seqs / p.source[1].ne[3]);
        let statebase = scores + task * square;
        let state = dst[statebase .. statebase + square];

        if tokens > 0 || snapshots == 1 {
            let init = head * strides[5][2] + seq * strides[5][3];
            for j in width {
                let row = state[j * width .. (j + 1) * width];
                for i in row.len {
                    row[i] = initial[init + j * strides[5][1] + i * strides[5][0]];
                }
            }
        }
        for token in tokens {
            let qb = qhead * strides[0][1] + token * strides[0][2] + qseq * strides[0][3];
            let kb = khead * strides[1][1] + token * strides[1][2] + kseq * strides[1][3];
            let vb = head * strides[2][1] + token * strides[2][2] + seq * strides[2][3];
            let gb = head * strides[3][1] + token * strides[3][2] + seq * strides[3][3];
            let bb = head * strides[4][1] + token * strides[4][2] + seq * strides[4][3];
            let query = q[qb .. qb + (width - 1) * strides[0][0] + 1];
            let key = k[kb .. kb + (width - 1) * strides[1][0] + 1];
            let rate = beta[bb];
            let out = ((seq * tokens + token) * heads + head) * width;
            let score = dst[out .. out + width];
            for lane in gate_width {
                decay[lane] = exp32(g[gb + lane * strides[3][0]]);
            }

            for j in width {
                let row = state[j * width .. (j + 1) * width];
                if gate_width == 1 {
                    let factor = decay[0];
                    for i in row.len { row[i] *= factor; }
                } else {
                    for i in row.len { row[i] *= decay[i]; }
                }
                let predicted = f32_row_dot(row, key, strides[1][0]);
                let delta = (v[vb + j * strides[2][0]] - predicted) * rate;
                for i in row.len {
                    row[i] = fma32(key[i * strides[1][0]], delta, row[i]);
                }
                score[j] = f32_row_dot(row, query, strides[0][0]) * scale;
            }

            let slot = tokens - 1 - token;
            if snapshots > 1 && slot > 0 && slot < snapshots {
                let at = scores + slot * snapshot_values + task * square;
                let saved = dst[at .. at + square];
                for i in saved.len { saved[i] = state[i]; }
            }
        }
        task += p.nth;
    }
    witness(OP_GATED_DELTA_NET, scores);
    return true;
}

fn recurrent_attention(
    p: const StatePlan &,
    a0: const u8[:],
    a1: const u8[:],
    a2: const u8[:],
    a3: const u8[:],
    a4: const u8[:],
    a5: const u8[:],
    a6: const u8[:],
    dst: u8[:]) -> bool {
    let v7 = p.op == OP_RWKV_WKV7;
    let linear = p.op == OP_GATED_LINEAR_ATTN;

    if !v7 && !linear && p.op != OP_RWKV_WKV6 {
        return false;
    }
    let state_index = if v7 {
        6
    } else {
        if linear {
            4
        } else {
            5
        }
    }
    ;
    let width = p.source[1].ne[0];
    let heads = p.source[1].ne[1];
    let tokens = p.source[1].ne[2];
    let seqs = p.source[state_index].ne[1];
    assert(width > 0 && heads > 0 && tokens >= 0 && seqs > 0 && tokens % seqs == 0 && p.nth > 0 && p.ith >= 0 && p.ith < p.nth);
    assert(p.dst.kind == 0);
    validate(p.dst, dst.len);
    for input in state_index + 1 {
        let shape = p.source[input];
        assert(shape.kind == 0);
        var stride: i64 = 4;
        for axis in 4 {
            assert(shape.nb[axis] == stride);
            stride = mul_size(stride, shape.ne[axis]);
        }
        assert(shape.bytes >= stride);
    }
    validate(p.source[0], a0.len);
    validate(p.source[1], a1.len);
    validate(p.source[2], a2.len);
    validate(p.source[3], a3.len);
    validate(p.source[4], a4.len);

    if !linear {
        validate(p.source[5], a5.len);
    }

    if v7 {
        validate(p.source[6], a6.len);
        assert(width % 32 == 0);
    }
    let channels = mul_size(width, heads);
    let scores = mul_size(channels, tokens);
    let square = mul_size(width, width);
    let state_values = mul_size(mul_size(square, heads), seqs);
    assert(elements(p.source[state_index]) == state_values && mul_size(add_size(scores, state_values), 4) <= dst.len);
    for input in state_index {
        if !v7 && input == 3 && !linear {
            assert(elements(p.source[input]) >= channels);
        } else {
            assert(elements(p.source[input]) >= scores);
        }
    }
    let begin = heads * p.ith / p.nth;
    let finish = heads * (p.ith + 1) / p.nth;
    let simd_end = width / 8 * 8;
    for head in finish - begin {
        let h = begin + head;
        for token in tokens {
            let seq = token / (tokens / seqs);
            let output = token * channels + h * width;
            let state_base = scores + (seq * heads + h) * square;
            let initial = (seq * heads + h) * square;

            if token % (tokens / seqs) == 0 {
                for i in square {
                    let value = if v7 {
                        value_at(a6, initial + i)
                    } else {
                        if linear {
                            value_at(a4, initial + i)
                        } else {
                            value_at(a5, initial + i)
                        }
                    }
                    ;
                    set_value(dst, state_base + i, value);
                }
            }

            if v7 {
                for row in width {
                    let base = state_base + row * width;
                    let sa = vector_dot(p.source[4], a4, output * 4, 4, p.dst, dst, base * 4, 4, width);
                    let value = value_at(a3, output + row);
                    for col in width {
                        let at = output + col;
                        let kv = value * value_at(a2, at);
                        let decayed = fma32(value_at(dst, base + col), value_at(a1, at), kv);
                        set_value(dst, base + col, fma32(sa, value_at(a5, at), decayed));
                    }
                    set_value(
                        dst,
                        output + row,
                        vector_dot(p.dst, dst, base * 4, 4, p.source[0], a0, output * 4, 4, width));
                }
            } else {
                for col in width {
                    set_value(dst, output + col, 0 as f32);
                }
                for row in width {
                    let key = value_at(a0, output + row);
                    let query = value_at(a2, output + row) * (if linear {
                        p.fp[0]
                    } else {
                        1 as f32
                    }
                    );
                    let decay = if linear {
                        value_at(a3, output + row)
                    } else {
                        value_at(a4, output + row)
                    }
                    ;
                    let factor = if linear {
                        0 as f32
                    } else {
                        value_at(a3, h * width + row)
                    }
                    ;
                    for col in width {
                        let at = state_base + row * width + col;
                        let kv = value_at(a1, output + col) * key;
                        let previous = value_at(dst, at);
                        let next = if col < simd_end {
                            fma32(previous, decay, kv)
                        } else {
                            previous * decay + kv
                        }
                        ;
                        let weighted = if linear {
                            next
                        } else {
                            if col < simd_end {
                                fma32(kv, factor, previous)
                            } else {
                                kv * factor + previous
                            }
                        }
                        ;
                        let result = if col < simd_end {
                            fma32(weighted, query, value_at(dst, output + col))
                        } else {
                            value_at(dst, output + col) + weighted * query
                        }
                        ;
                        set_value(dst, at, next);
                        set_value(dst, output + col, result);
                    }
                }
            }
        }
    }
    witness(p.op, scores);
    return true;
}

fn state_entry(
    p: const StatePlan &,
    a0: const u8[:],
    a1: const u8[:],
    a2: const u8[:],
    a3: const u8[:],
    a4: const u8[:],
    a5: const u8[:],
    a6: const u8[:],
    dst: u8[:]) -> bool {
    if recurrent_attention(p, a0, a1, a2, a3, a4, a5, a6, dst) {
        return true;
    }
    validate(p.dst, dst.len);
    validate(p.source[0], a0.len);
    validate(p.source[1], a1.len);
    validate(p.source[2], a2.len);
    validate(p.source[3], a3.len);
    validate(p.source[4], a4.len);
    validate(p.source[5], a5.len);

    if p.op == OP_SSM_SCAN {
        validate(p.source[6], a6.len);
        var scan: ScanPlan = default < ScanPlan > ();
        scan.source = p.source;
        scan.ith = p.ith;
        scan.nth = p.nth;
        scan.snapshots = p.snapshots;
        assert(p.source[3].ne[0] == 1 || p.snapshots == 1);
        return scan_entry(scan, a0, a1, a2, a3, a4, a5, a6, dst);
    }
    return false;
}

fn main() {
    var p: StatePlan = default < StatePlan > ();
    let a: u8[1] = default < u8[1] > ();
    var d: u8[1] = default < u8[1] > ();
    let ok = state_entry(p, a, a, a, a, a, a, a, d);
    print(ok);
}

