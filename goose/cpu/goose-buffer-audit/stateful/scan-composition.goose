struct ScanPlan {
    source: Tensor[7], ith: i64, nth: i64, snapshots: i64
}

fn softplus_state(x: f32) -> f32 {
    return if x > (20 as f32) {
        x
    } else {
        log32( (1 as f32) + exp32(x))
    }
    ;
}

fn scan_entry(
    p: const ScanPlan &,
    initial: const u8[:],
    input: const u8[:],
    dt: const u8[:],
    decay: const u8[:],
    b: const u8[:],
    c: const u8[:],
    ids: const u8[:],
    dst: u8[:]) -> bool {
    assert(p.nth > 0 && p.ith >= 0 && p.ith < p.nth && p.snapshots > 0);
    for i in 6 {
        assert(p.source[i].kind == 0);
    }
    assert(p.source[6].kind == 26);
    let state_size = p.source[0].ne[0];
    let width = p.source[0].ne[1];
    let heads = p.source[1].ne[1];
    let tokens = p.source[1].ne[2];
    let seqs = p.source[1].ne[3];
    let groups = p.source[4].ne[1];
    assert(state_size > 0 && width > 0 && groups > 0 && heads % groups == 0);
    let state_stride = p.source[0].nb[3] / 4;
    let output = mul_size(mul_size(mul_size(width, heads), tokens), seqs);
    assert(dst.len >= mul_size(add_size(output, mul_size(mul_size(state_stride, seqs), p.snapshots)), 4));
    let first = (heads * p.ith) / p.nth;
    let last = (heads * (p.ith + 1)) / p.nth;
    for seq in seqs {
        let id = read32(ids, offset(p.source[6], seq)) as !i32;
        assert(id >= 0 && (id as i64) < p.source[0].ne[3]);
        for token in tokens {
            for head_delta in last - first {
                let head = first + head_delta;
                let group = head / (heads / groups);
                let timestep = softplus_state(load_at(p.source[2], dt, head * p.source[2].nb[0] + token * p.source[2].nb[1] + seq * p.source[2].nb[2]));
                let a0 = load_at(p.source[3], decay, head * p.source[3].nb[1]);
                let shared = p.source[3].ne[0] == 1;
                let factor = exp32(timestep * a0);
                for lane in width {
                    let x = load_at(
                        p.source[1],
                        input,
                        lane * p.source[1].nb[0] + head * p.source[1].nb[1] + token * p.source[1].nb[2] + seq * p.source[1].nb[3]);
                    let scaled = x * timestep;
                    var sum: f32 = (0 as f32);
                    for state in state_size {
                        let position = state + (lane + head * width) * state_size;
                        let target = output + seq * state_stride + position;
                        let previous = if token == 0 {
                            load_at(
                                p.source[0],
                                initial,
                                state * p.source[0].nb[0] + lane * p.source[0].nb[1] + head * p.source[0].nb[2] + (id as i64) * p.source[0].nb[3])
                        } else {
                            value_at(dst, target)
                        }
                        ;
                        let ba = load_at(
                            p.source[4],
                            b,
                            state * p.source[4].nb[0] + group * p.source[4].nb[1] + token * p.source[4].nb[2] + seq * p.source[4].nb[3]);
                        let ca = load_at(
                            p.source[5],
                            c,
                            state * p.source[5].nb[0] + group * p.source[5].nb[1] + token * p.source[5].nb[2] + seq * p.source[5].nb[3]);
                        let rate = if shared {
                            factor
                        } else {
                            exp32(timestep * load_at(p.source[3], decay, state * p.source[3].nb[0] + head * p.source[3].nb[1]))
                        }
                        ;
                        let value = previous * rate + ba * scaled;
                        set_value(dst, target, value);
                        sum += value * ca;
                    }
                    set_value(dst, lane + width * (head + heads * (token + tokens * seq)), sum);
                }
                let slot = tokens - 1 - token;

                if p.snapshots > 1 && slot > 0 && slot < p.snapshots {
                    for i in state_size * width {
                        let pos = i + head * state_size * width;
                        set_value(
                            dst,
                            output + (slot * seqs + seq) * state_stride + pos,
                            value_at(dst, output + seq * state_stride + pos));
                    }
                }
            }
        }
    }
    witness(OP_SSM_SCAN, output);
    return true;
}

